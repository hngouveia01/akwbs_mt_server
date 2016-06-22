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
