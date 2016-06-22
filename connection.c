/*!
 * \file   connection.c
 * \brief  Implementation functions related to managing connections.
 * \author Henrique Nascimento Gouveia <h.gouveia@icloud.com>
 */

#define _BSD_SOURCE
#define _XOPEN_SOURCE 600

#include <stdio.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <libgen.h>
#include <fcntl.h>
#include <search.h>

#include "ringbuffer.h"
#include "connection.h"
#include "daemon.h"
#include "internal.h"
#include "io.h"
#include "http.h"
#include "file_tree.h"


/*!
 * Manage issues related to the send rate, or 'speed control' if you wish.
 *
 * \param connection pointer to the connection.
 * \param bytes_to_send param-return to be sent.
 *
 * \return AKWBS_SUCCESS on success while performing the send rate analysis.
 */
static int manage_send_rate(struct akwbs_connection *connection, ssize_t *bytes_to_send)
{
  struct timeval current_time = {0, 0};
  struct timeval diff_time    = {0, 0};


  if (*bytes_to_send == 0)
    return -2;

  if (gettimeofday(&current_time, NULL) == AKWBS_ERROR)
    return AKWBS_ERROR;

  timersub(&current_time, &connection->last_time_io, &diff_time);

  if ((diff_time.tv_sec == 0)
     && (diff_time.tv_usec >= 0))
  {
    if (connection->bytes_sent_last_io >= connection->daemon_ref->send_rate)
      return -2;

    size_t bytes_that_can_be_sent = connection->daemon_ref->send_rate
                                    - connection->bytes_sent_last_io;

    if (*bytes_to_send > bytes_that_can_be_sent)
      *bytes_to_send = bytes_that_can_be_sent;

    return AKWBS_SUCCESS;
  }

  if (diff_time.tv_sec > 0)
  {
    connection->bytes_sent_last_io = 0;

    if (gettimeofday(&connection->last_time_io, NULL) == AKWBS_ERROR)
      return AKWBS_ERROR;

    if (*bytes_to_send > connection->daemon_ref->send_rate)
      *bytes_to_send = connection->daemon_ref->send_rate;

    return AKWBS_SUCCESS;
  }

  return AKWBS_SUCCESS;
}

/*!
 * Verify if this connection has reached the timeout limit.
 *
 * \param connection connection object.
 *
 * \return AKWBS_SUCCESS if it did NOT REACH the limit.
 *         AKWBS_ERROR if it REACHED the limit.
 */
static int get_timeout(struct akwbs_connection *connection)
{
  struct timeval diff_time = {0, 0};
  struct timeval current_time = {0, 0};


  gettimeofday(&current_time, NULL);

  timersub(&current_time, &connection->last_activity, &diff_time);

  if (diff_time.tv_sec > AKWBS_TIMEOUT_SECONDS)
    return AKWBS_ERROR;

  return AKWBS_SUCCESS;
}

/*!
 * Receive more data from a socket and put into the given buffer.
 *
 * \param buffer buffer to store data into.
 * \param socket_descriptor socket descriptor to get data from.
 *
 * \return AKWBS_SUCCESS on success receiving data from the given socket.
 *         AKWBS_ERROR on error while trying to get data from the given socket.
 */
static int recv_data_from_socket(struct akwbs_connection *connection)
{
  ssize_t bytes_read     = 0;
  size_t  free_space     = 0;
  void    *write_address = NULL;


  free_space = ring_buffer_count_free_bytes(&connection->buffer);

  if (free_space == 0)
    return RING_BUFFER_IS_FULL;  /* This should never occur while receiving headers. */

  write_address = ring_buffer_write_address(&connection->buffer);

  bytes_read    = recv(connection->client_socket, write_address, free_space, 0);

  if (bytes_read == AKWBS_ERROR)
    return AKWBS_ERROR;

  gettimeofday(&connection->last_activity, NULL);

  ring_buffer_write_advance(&connection->buffer, bytes_read);

  return AKWBS_SUCCESS;
}


/*!
 * Send data to a socket and update the buffer accounting.
 *
 */
static int send_data_to_socket(struct akwbs_connection *connection)
{
  ssize_t bytes_sent      = 0;
  size_t  bytes_to_send   = 0;
  void    *read_address   = NULL;
  int     ret             = 0;


  bytes_to_send = ring_buffer_count_bytes(&connection->buffer);

  if (bytes_to_send == 0)
    return AKWBS_SUCCESS;

  ret = manage_send_rate(connection, &bytes_to_send);

  if (ret == -2)
    return AKWBS_SUCCESS;

  read_address = ring_buffer_read_address(&connection->buffer);

  bytes_sent = send(connection->client_socket, read_address, bytes_to_send, 0);

  if (bytes_sent == AKWBS_ERROR)
    return AKWBS_ERROR;

  connection->bytes_sent_last_io += bytes_sent;

  ring_buffer_read_advance(&connection->buffer, bytes_sent);

  return AKWBS_SUCCESS;
}

static void make_real_file_path(char *root_path, char *file_name, char *real_path)
{
  char *index = root_path;
  int i = 0;


  while (*index != '\0')
  {
    real_path[i] = *index;
    i++;
    index++;
  }

  index = file_name;

  while (*index != '\0')
  {
    real_path[i] = *index;
    i++;
    index++;
  }

  real_path[i] = '\0';
}

/*!
 * \brief Decrease the number of references that a file
 *        has.
 * \param connection connection containing the information
 *        about the file that is being written or reading.
 * \return AKWBS_ERROR on error.
 * \return AKWBS_SUCCESS if the number of references was
 *         increased  or reached zero.
 */
static int decrease_file_stat_reference(struct akwbs_connection *connection)
{
  struct stat stat_buf;
  struct akwbs_file_stat *result = NULL;
  struct akwbs_file_stat key_to_search;


  if (fstat(connection->file_descriptor, &stat_buf) == AKWBS_ERROR)
    return AKWBS_ERROR;

  if (connection->connection_state != AKWBS_CONNECTION_CLOSED)
    return AKWBS_ERROR;

  bzero(&key_to_search, sizeof(struct akwbs_file_stat));

  key_to_search.inode_number = stat_buf.st_ino;

  result = (struct akwbs_file_stat *) tfind(&key_to_search,
                                            &connection->daemon_ref->tree_opened_files,
                                            akwbs_compare_file_stat);

  if (result == NULL)
    return AKWBS_ERROR;

  (*((struct akwbs_file_stat **)result))->number_of_references--;

  if ((*((struct akwbs_file_stat **)result))->number_of_references == 0)
  {
    struct akwbs_file_stat *to_be_freed = NULL;


    close((*((struct akwbs_file_stat **)result))->file_descriptor);
    to_be_freed = tdelete(result,
                  &connection->daemon_ref->tree_opened_files,
                  akwbs_compare_file_stat);

    free(to_be_freed);

    return AKWBS_SUCCESS;
  }

  return AKWBS_SUCCESS;
}

/*!
 * \brief Create a leaf in the binary tree representing
 *        a reference and status about a file.
 * \param connection connection that is requesting operation
 *        on file.
 * \return AKWBS_ERROR on error.
 * \return AKWBS_SUCCESS if a reference already exists or
 *         it has been created.
 */
static int create_file_stat(struct akwbs_connection *connection)
{
  char real_path[PATH_MAX];
  struct stat stat_buf;
  struct akwbs_file_stat key_to_search;
  struct akwbs_file_stat *search_result = NULL;
  struct akwbs_file_stat *file_stat_to_insert = NULL;
  struct akwbs_file_stat *insert_result = NULL;


  make_real_file_path(connection->daemon_ref->root_path,
                      connection->file_name,
                      real_path);

  if (stat(real_path, &stat_buf) == AKWBS_ERROR)
    return AKWBS_ERROR;

  key_to_search.inode_number         = stat_buf.st_ino;
  key_to_search.file_descriptor      = -1;
  key_to_search.number_of_references = 0;

  search_result = (struct akwbs_file_stat *) tfind(&key_to_search,
                                                   &connection->daemon_ref->tree_opened_files,
                                                   akwbs_compare_file_stat);

  if (search_result != NULL)
  {
    (* (struct akwbs_file_stat **) search_result)->number_of_references++;
    connection->file_descriptor = (* (struct akwbs_file_stat **)search_result)->file_descriptor;
    connection->file_total_offset = stat_buf.st_size;
    return AKWBS_SUCCESS;
  }

  file_stat_to_insert = (struct akwbs_file_stat *) malloc(sizeof(struct akwbs_file_stat));

  if (file_stat_to_insert == NULL)
    return AKWBS_ERROR;

  file_stat_to_insert->inode_number = key_to_search.inode_number;
  file_stat_to_insert->number_of_references = 1;
  file_stat_to_insert->file_descriptor = open(real_path, O_RDONLY | O_NONBLOCK);

  if (file_stat_to_insert->file_descriptor == AKWBS_ERROR)
    goto free_and_fail;

  insert_result = tsearch((void *) file_stat_to_insert,
                          &connection->daemon_ref->tree_opened_files,
                          akwbs_compare_file_stat);

  if ((* (struct akwbs_file_stat **) insert_result) != file_stat_to_insert)
    goto free_and_fail;

  connection->file_descriptor   = (* (struct akwbs_file_stat **)insert_result)->file_descriptor;
  connection->file_total_offset = stat_buf.st_size;

  return AKWBS_SUCCESS;

free_and_fail:
  free(file_stat_to_insert);
  return AKWBS_ERROR;

}

/* Decides if we should create or update references about files. */
static int manage_file_stat_tree(struct akwbs_connection *connection)
{
  if (connection->file_descriptor != AKWBS_ERROR)
  {
    if (decrease_file_stat_reference(connection) == AKWBS_ERROR)
      return AKWBS_ERROR;
  }
  else
  {
    if (create_file_stat(connection) == AKWBS_ERROR)
      return AKWBS_ERROR;
  }

  return AKWBS_SUCCESS;
}

static int open_file_for_writing(struct akwbs_connection *connection)
{
  char real_path[PATH_MAX];


  make_real_file_path(connection->daemon_ref->root_path,
                      connection->file_name,
                      real_path);


  connection->file_descriptor = open(basename(real_path),
                                     O_CREAT | O_WRONLY | O_NONBLOCK,
                                     S_IRWXU | S_IRWXG | S_IRWXO);

  if (connection->file_descriptor == AKWBS_ERROR)
    return AKWBS_ERROR;

  return AKWBS_SUCCESS;
}

/*!
 * Open the requested file for this connection.
 *
 * \param connection connection holding file name.
 *
 * \return AKWBS_SUCCESS on success opening the requested file.
 *         AKWBS_ERROR on error while trying to open the requested file.
 */
static int open_resource(struct akwbs_connection *connection)
{
  struct stat stat_buf;


  switch (connection->io_type)
  {
  case AKWBS_IO_GET_TYPE:
    if (manage_file_stat_tree(connection) == AKWBS_ERROR)
      return AKWBS_ERROR;
    break;
  case AKWBS_IO_PUT_TYPE:
    if (open_file_for_writing(connection) == AKWBS_ERROR)
      return AKWBS_ERROR;
    break;
  default:
    /* AKWBS_IO_UNKNOWN_TYPE. Obviously an error. */
    return AKWBS_ERROR;
  }

  if (connection->file_descriptor == AKWBS_ERROR)
    return AKWBS_ERROR;

  return AKWBS_SUCCESS;
}

/*!
 * Prepare I/O message for an I/O request.
 *
 * \param connection connection requesting this I/O.
 *
 * \return AKWBS_SUCCESS on success preparing the message.
 *         AKWBS_ERROR on error while preparing the message.
 */
static int prepare_io_request(struct akwbs_connection *connection)
{
  switch (connection->has_request_pending)
  {
  case AKWBS_NO:
    switch (connection->io_type)
    {
    case AKWBS_IO_GET_TYPE:
      connection->pending_io_msg.address = ring_buffer_write_address(&connection->buffer);
      connection->pending_io_msg.bytes   = ring_buffer_count_free_bytes(&connection->buffer);
      break;
    case AKWBS_IO_PUT_TYPE:
      connection->pending_io_msg.address = ring_buffer_read_address(&connection->buffer);
      connection->pending_io_msg.bytes   = ring_buffer_count_bytes(&connection->buffer);
      break;
    case AKWBS_IO_UNKNOWN_TYPE:
      return AKWBS_ERROR;
    }
    connection->pending_io_msg.fd      = connection->file_descriptor;
    connection->pending_io_msg.sd      = connection->client_socket;
    connection->pending_io_msg.type    = connection->io_type;
    connection->pending_io_msg.offset  = connection->file_cur_offset;
    break;
  case AKWBS_YES:
    /* We will send the request that is pending and was previously prepared */
    break;
  default:
    /* We must never get here. */
    return AKWBS_ERROR;
  }
  return AKWBS_SUCCESS;
}


/*!
 * Open the requested file and send the first request I/O of this connection.
 *
 * \param connection connection that made the request.
 * \param daemon_p pointer to the daemon containing the socket of requests.
 *
 * \return AKWBS_SUCCESS on success. AKWBS_ERROR on error.
 */
static int do_handle_request(struct akwbs_connection *connection)
{
  if (connection->is_waiting_result == AKWBS_YES)
    return AKWBS_SUCCESS;

  if (connection->file_cur_offset == connection->file_total_offset)
  {
    if (connection->io_type == AKWBS_IO_PUT_TYPE)
      send(connection->client_socket, AKWBS_HTTP_201, strlen(AKWBS_HTTP_201), 0);

    close(connection->client_socket);
    connection->connection_state = AKWBS_CONNECTION_CLOSED;
    manage_file_stat_tree(connection);
    FD_CLR(connection->client_socket, &connection->daemon_ref->master_read_set);
    FD_CLR(connection->client_socket, &connection->daemon_ref->master_write_set);

    return AKWBS_SUCCESS;
  }

  if (prepare_io_request(connection) == AKWBS_ERROR)
    return AKWBS_ERROR;

  if (akwbs_request_io_send_msg(&connection->pending_io_msg,
                                connection->daemon_ref->request_io_queue[AKWBS_WRITE_INDEX])
      == AKWBS_ERROR)
    connection->has_request_pending = AKWBS_YES;
  else
  {
    connection->has_request_pending = AKWBS_NO;
    connection->is_waiting_result   = AKWBS_YES;
  }

  posix_fadvise(connection->file_descriptor,
                connection->file_cur_offset,
                connection->pending_io_msg.bytes,
                POSIX_FADV_SEQUENTIAL);

  pthread_cond_signal(&connection->daemon_ref->request_io_queue_cond);

  return AKWBS_SUCCESS;
}

/*!
 * This function tries to find header's delimiters. Header lines are splitted
 * into multiple lines by \r\n (aka CRLF), so when we find this, it indicates
 * that we have a header line. This line is parsed and information extracted.
 * As soon as we find the \r\n\r\n (aka CRLFCRLF), we indicate that situation
 * to the caller of this function.
 *
 * \param connection the connection containing the buffer.
 *
 * \return AKWBS_REACHED_HEADER_END if we find the end of the header.
 *         AKWBS_NOT_A_RELEVANT_EVENT if we do not find any of the flags that
 *         we are looking for.
 *         AKWBS_ERROR if this header is malformed.
 */
static int check_end_of_header(struct akwbs_connection *connection)
{
  char *initial_address = ring_buffer_read_address(&connection->buffer);
  size_t total_bytes_in_buffer = ring_buffer_count_bytes(&connection->buffer);
  size_t bytes_read = 0;
  char *variable_position = NULL;


  for (variable_position = initial_address;
       ((bytes_read != total_bytes_in_buffer)
        && (bytes_read < AKWBS_SIZE_HEADER_TOO_BIG));
       bytes_read++, variable_position++)
  {
    switch (connection->header_state)
    {
      case AKWBS_HEADER_INITIAL:
        if (*variable_position == '\r')
          connection->header_state = AKWBS_HEADER_FIRST_CARRIAGE_RETURN;
        break;
      case AKWBS_HEADER_FIRST_CARRIAGE_RETURN:
        if (*variable_position == '\n')
        {
          connection->header_state = AKWBS_HEADER_FIRST_LINEFEED;
          if (connection->end_of_first_header_line == NULL)
            connection->end_of_first_header_line = variable_position;
        }
        else
          connection->header_state = AKWBS_HEADER_INITIAL;
        break;
      case AKWBS_HEADER_FIRST_LINEFEED:
        if (*variable_position == '\r')
          connection->header_state = AKWBS_HEADER_LAST_CARRIAGE_RETURN;
        else
          connection->header_state = AKWBS_HEADER_INITIAL;
        break;
      case AKWBS_HEADER_LAST_CARRIAGE_RETURN:
        if (*variable_position == '\n')
        {
          connection->header_state = AKWBS_HEADER_LAST_LINEFEED;
          connection->end_of_header = ++variable_position;
          return AKWBS_SUCCESS;
        }
        else
          connection->header_state = AKWBS_HEADER_INITIAL;
        break;
      default:
        connection->header_state = AKWBS_HEADER_INITIAL;
    }
  }

  if ((bytes_read >= AKWBS_SIZE_HEADER_TOO_BIG)
      && (connection->header_state != AKWBS_HEADER_LAST_LINEFEED))
    return AKWBS_ERROR;

  return AKWBS_YES;
}

/*!
 * Receive more data from the client connection.
 *
 * \param daemon_p pointer to the daemon structure.
 * \param connection connection which will receive more data.
 *
 * \return AKWBS_SUCCESS on success receiving the data or if there is no space available
 *         to hold any more data. This case in particular should never occur while
 *         receiving the request header, because the buffer is greater than the limit of
 *         bytes considered as header too big. Thus, this is taken as an error.
 */
static int recv_header(struct akwbs_connection *connection)
{
  if (! FD_ISSET(connection->client_socket, &connection->daemon_ref->temp_read_set))
  {
    if (get_timeout(connection) == AKWBS_ERROR)
      goto close_and_error;
    return AKWBS_SUCCESS;
  }

  if (recv_data_from_socket(connection) != AKWBS_SUCCESS)
    goto close_and_error;

  if (check_end_of_header(connection) == AKWBS_ERROR)
    goto close_and_error;

  if (connection->header_state == AKWBS_HEADER_LAST_LINEFEED)
  {
    connection->connection_state = AKWBS_CONNECTION_HEADERS_RECEIVED;
    FD_CLR(connection->client_socket, &connection->daemon_ref->master_read_set);
    FD_SET(connection->client_socket, &connection->daemon_ref->master_write_set);
  }
  else
    connection->connection_state = AKWBS_CONNECTION_HEADERS_RECEIVING;

  return AKWBS_SUCCESS;

close_and_error:
  send(connection->client_socket, AKWBS_HTTP_400, strlen(AKWBS_HTTP_400), 0);
  close(connection->client_socket);
  connection->connection_state = AKWBS_CONNECTION_CLOSED;
  return AKWBS_ERROR;
}

/*!
 * Start the transmission for this connection.
 *
 * \param connection connection to handle.
 * \param daemon_p pointer to the daemon structure containing data about the request I/O
 *        queue.
 *
 * \return AKWBS_SUCCESS on success. AKWBS_ERROR on error.
 */
static int init_transmission(struct akwbs_connection *connection)
{
  struct stat s_stat;
  int ret = AKWBS_ERROR;


  if (open_resource(connection) == AKWBS_ERROR)
  {
    send(connection->client_socket, AKWBS_HTTP_404, strlen(AKWBS_HTTP_404), 0);
    close(connection->client_socket);
    connection->connection_state = AKWBS_CONNECTION_CLOSED;
    FD_CLR(connection->client_socket, &connection->daemon_ref->master_read_set);
    FD_CLR(connection->client_socket, &connection->daemon_ref->master_write_set);
    return AKWBS_SUCCESS;
  }

  ret = do_handle_request(connection);

  connection->connection_state = AKWBS_CONNECTION_ON_TRANSMISSION;

  return ret;
}


/*!
 * Handle the given connection on transmission state.
 *
 * \param connection connection on transmission state that will be handled.
 *
 * \return AKWBS_SUCCESS on success handling this connection.
 *         AKWBS_ERROR on error serious error while handling this connection.
 */
static int handle_transmission(struct akwbs_connection *connection)
{
  if (connection == NULL)
    return AKWBS_ERROR;

  switch (connection->io_type)
  {
    case AKWBS_IO_GET_TYPE:
      if (! FD_ISSET(connection->client_socket, &connection->daemon_ref->temp_write_set))
        return AKWBS_SUCCESS;

      if (send_data_to_socket(connection) == AKWBS_ERROR)
      {
        manage_file_stat_tree(connection);
        close(connection->client_socket);
        connection->connection_state = AKWBS_CONNECTION_CLOSED;

        return AKWBS_ERROR;
      }
      do_handle_request(connection);
      break;
    case AKWBS_IO_PUT_TYPE:
      if (FD_ISSET(connection->client_socket, &connection->daemon_ref->temp_read_set))
        recv_data_from_socket(connection);
      do_handle_request(connection);
      break;
    default:
      /* If we got here, the genius programmer is missing something... I assume.        */
      return AKWBS_ERROR;
  }
  return AKWBS_SUCCESS;
}
