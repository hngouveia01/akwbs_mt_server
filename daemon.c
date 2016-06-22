/*!
 * \file   daemon.c
 * \brief  Functions related to the server's daemon.
 * \author Henrique Nascimento Gouveia <h.gouveia@icloud.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <string.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <errno.h>
#include <signal.h>
#include <sys/param.h>

#include "daemon.h"
#include "internal.h"
#include "requestio.h"
#include "ringbuffer.h"
#include "connection.h"
#include "resultio.h"
#include "thread_io.h"
#include "http.h"


/* GLOBAL VARIABLES FOR SIGNALS USED BY THE SERVER'S DAEMON. */


/*!
 * Global variable for SIGTERM signal. This indicates whether the server should start to
 * shutdown.
 */
static volatile sig_atomic_t shutdown_flag = 0;

/*!
 * Global variable for SIGUSR1 signal. This indicates whether there is new configurations
 * to be applied to this server.
 */
static volatile sig_atomic_t new_conf_flag = 0;


/* FUNCTIONS THAT HANDLE SIGNALS */


/*!
 * Handler for signal SIGTERM, flag the server to initiate the shutting down.
 *
 * \param sig signal caught.
 */
static void handler_shutdown()
{
  shutdown_flag = 1;
}

/*!
 * Handler for signal SIGUSR1, this signal flags the server to indicate that new configur
 * ation has been placed and must be applied.
 */
static void handler_new_conf()
{
  new_conf_flag = 1;
}


/*!
 * Register signal handlers to the respective signal.
 *
 * \param  None.
 * \return AKWBS_ERROR on error if a handler could not be registered to handle the signal.
 *         One should check the global variable 'errno' for a more detailed description of
 *         the given error.
 *
 * \details Here, the SIGTERM is set to be handled and make the server do a gracious exit.
 *          The SIGUSR1 is important to set up the flag that indicates whether or not
 *          there is a new configuration for the server.
 *          Other signals, such as SIGUSR2 and SIGPIPE are ignored. For the reason that
 *          those signals will shutdown this application unexpected.
 */
static int setup_signal_handlers(void)
{
  if (signal(SIGTERM, handler_shutdown) == SIG_ERR)
    return AKWBS_ERROR;

  if (signal(SIGUSR1, handler_new_conf) == SIG_ERR)
    return AKWBS_ERROR;

  if (signal(SIGUSR2, SIG_IGN) == SIG_ERR)
    return AKWBS_ERROR;

  if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
    return AKWBS_ERROR;

  return AKWBS_SUCCESS;
}


static void check_new_conf(struct akwbs_daemon *daemon_p)
{
  if (new_conf_flag == AKWBS_NO)
    return;

  new_conf_flag = 0;

  FILE *file_new_conf;
  char root_path[PATH_MAX];
  char port[7];
  char send_rate[BUFSIZ];
  int index = 0;
  char character;


  file_new_conf = fopen("akwbs.conf", "rt");

  if (file_new_conf == NULL)
    return;

  while (! feof(file_new_conf))
  {
    character = getc(file_new_conf);

    if (character == '|')
    {
      root_path[index] = '\0';
      break;
    }
    root_path[index] = character;
    index++;
  }

  index = 0;

  while (! feof(file_new_conf))
  {
    character = getc(file_new_conf);

    if (character == '|')
    {
      port[index] = '\0';
      break;
    }
    port[index] = character;
    index++;
  }

  index = 0;


  while (! feof(file_new_conf))
  {
    character = getc(file_new_conf);

    if (character == '|')
    {
      send_rate[index] = '\0';
      break;
    }
    send_rate[index] = character;
    index++;
  }

  daemon_p->send_rate = atol(send_rate);

  if (access(root_path, R_OK | W_OK) == AKWBS_ERROR)
    return;
  else
  {
    free(daemon_p->root_path);
    daemon_p->root_path = strdup(root_path);
  }

  daemon_p->serv_addr.sin_port = htons(atol(port));

  int new_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

  if (bind(new_sock,
           (struct sockaddr *)&daemon_p->serv_addr,
           (socklen_t)sizeof(daemon_p->serv_addr)) == AKWBS_ERROR)
    return;

  close(daemon_p->listen_fd);

  if (listen(new_sock, SOMAXCONN) == AKWBS_ERROR)
    puts(strerror(errno));

  FD_CLR(daemon_p->listen_fd, &daemon_p->master_read_set);

  daemon_p->listen_fd = new_sock;

  if (new_sock > daemon_p->max_fds)
    daemon_p->max_fds = new_sock;

  FD_SET(new_sock, &daemon_p->master_read_set);

}


/*!
 * Turn this process into a daemon.
 *
 * \param root_path daemon's root path.
 *
 * \return AKWBS_SUCCESS on success.
 *         AKWBS_ERROR on error.
 */
static int daemonize(const char *root_path)
{
  pid_t pid;
  int i;


  if ((pid = fork()) == AKWBS_ERROR)
    return -1;
  else if (pid)
    _exit(EXIT_SUCCESS);

  if (setsid() == AKWBS_ERROR)
    return AKWBS_ERROR;

  signal(SIGHUP, SIG_IGN);
  if ( (pid = fork()) == AKWBS_ERROR)
    return AKWBS_ERROR;
  else if (pid)
    _exit(EXIT_SUCCESS);

  chdir(root_path);

  for (i = 0; i < FD_SETSIZE; i++)
    close(i);

  open("/dev/null", O_RDONLY);
  open("/dev/null", O_RDWR);
  open("/dev/null", O_RDWR);

  return AKWBS_SUCCESS;
}


/*!
 * Get file descriptors ready for some input or output.
 *
 * \param daemon_p param-return pointer to the daemon holding the master and temporary
 *        descriptors sets.
 *
 * \return AKWBS_SUCCESS on success getting ready file descriptors.
 *         AKWBS_ERROR on error while getting ready file descriptors.
 */
static int get_ready_fds(struct akwbs_daemon *daemon_p)
{
  struct timeval tv;
  struct timeval *should_wait_or_not = NULL;
  tv.tv_sec = 0;
  tv.tv_usec = 0;


  memcpy(&daemon_p->temp_read_set, &daemon_p->master_read_set, sizeof(fd_set));
  memcpy(&daemon_p->temp_write_set, &daemon_p->master_write_set, sizeof(fd_set));

  if ((daemon_p->active_connections_head == NULL)
      && (daemon_p->cleanup_connections_head == NULL))
    should_wait_or_not = NULL;
  else
    should_wait_or_not = &tv;

  daemon_p->fds_ready = select(daemon_p->max_fds + 1,
                               &daemon_p->temp_read_set,
                               &daemon_p->temp_write_set,
                               NULL,
                               should_wait_or_not);

  if (daemon_p->fds_ready == AKWBS_ERROR)
    return AKWBS_ERROR;

  return AKWBS_SUCCESS;
}


/*!
 * Perform actions in order to check and, if there is a connection waiting to be accepted,
 * create connection's object.
 *
 * \param daemon_p pointer to the daemon structure holding the file descriptor listening
 *        for incoming connections.
 *
 * \return AKWBS_SUCCESS on succes either handling this new connection or none connection
 *         was waiting for being accepted.
 */
static int handle_incoming_connections(struct akwbs_daemon *daemon_p)
{
  int new_socket                      = AKWBS_ERROR;
  struct akwbs_connection *connection = NULL;
  int sk_flags = 0;


  if (! FD_ISSET(daemon_p->listen_fd, &daemon_p->temp_read_set))
    return AKWBS_SUCCESS;

  daemon_p->fds_ready--;

  new_socket = accept(daemon_p->listen_fd, NULL, NULL);

  if (new_socket == AKWBS_ERROR)
    switch (errno)
    {
      case ECONNABORTED:
      case EINTR:
      case EMFILE:
      case EAGAIN:
        return AKWBS_SUCCESS;
      default:
        return AKWBS_ERROR;
    }

  if (akwbs_create_new_connection(&connection) == AKWBS_ERROR)
    return (close(new_socket), AKWBS_ERROR);

  connection->daemon_ref = daemon_p;

  if (new_socket > daemon_p->max_fds)
    daemon_p->max_fds = new_socket;

  connection->client_socket = new_socket;

  DLL_insert(daemon_p->active_connections_head,
             daemon_p->active_connections_tail,
             connection);

  FD_SET(connection->client_socket, &daemon_p->temp_read_set);

  sk_flags = fcntl(new_socket, F_GETFD);

  if (fcntl(new_socket, F_SETFD, O_NONBLOCK | sk_flags) == AKWBS_ERROR)
    return AKWBS_ERROR;

  return AKWBS_SUCCESS;
}


/*!
 * Search for the given socket in the active connections list.
 *
 * \param connection param-return that will be pointing to the found connection.
 * \param daemon_p pointer to the daemon that holds all information about connections.
 * \param socket_to_find the socket descriptor to perform the search.
 *
 * \return AKWBS_SUCCESS in case we did found the connection with the given socket
 *         descriptor. The param <u>connection</u> is set to point to found connection.
 *         AKWBS_ERROR if we did not found the given socket descriptor in the active
 *         connections list.
 */
static int search_connection_by_socket(struct akwbs_connection **connection,
                                      struct akwbs_daemon *daemon_p,
                                      int socket_to_find)
{
  struct akwbs_connection *next = NULL;
  struct akwbs_connection *pos  = NULL;


  next = daemon_p->active_connections_head;

  while (NULL != (pos = next))
  {
    next = pos->next;

    if (pos->client_socket != socket_to_find)
      continue;

    *connection = pos;

    return AKWBS_SUCCESS;
  }

  return AKWBS_ERROR;
}


/*!
 * Get I/O result from the queue and update the related connection accounting.
 *
 * \param daemon_p pointer to the daemon structure that holds all information about
 *        connections.
 *
 * \return AKWBS_SUCCESS on success.
 *         AKWBS_ERROR on error while trying to get results or finding the connection.
 *
 * \details This function starts by checking if the daemon pointer is not NULL and if the
 *          result queue descriptor is ready or reading.
 */
static int handle_results(struct akwbs_daemon *daemon_p)
{
  struct akwbs_result_io result_msg;
  struct akwbs_connection *connection = NULL;

  if (daemon_p == NULL)
    return AKWBS_ERROR;

  if (! FD_ISSET(daemon_p->result_io_queue[AKWBS_READ_INDEX], &daemon_p->temp_read_set))
    return AKWBS_SUCCESS;

  daemon_p->fds_ready--;

  akwbs_result_io_init_msg(&result_msg);

  if (akwbs_result_io_recv_msg(&result_msg,
                               daemon_p->result_io_queue[AKWBS_READ_INDEX])
      == AKWBS_ERROR)
    return AKWBS_ERROR;

  if (search_connection_by_socket(&connection,
                                  daemon_p,
                                  result_msg.connection_fd) == AKWBS_ERROR)
    return AKWBS_ERROR;

  if (connection->io_type == AKWBS_IO_GET_TYPE)
    ring_buffer_write_advance(&connection->buffer, result_msg.bytes_read);
  else
    ring_buffer_read_advance(&connection->buffer, result_msg.bytes_read);

  connection->file_cur_offset += result_msg.bytes_read;
  connection->is_waiting_result = 0;

  return AKWBS_SUCCESS;
}


/*!
 * Pass through all connections and check which is the maximum
 * value for file descriptor, in this case, socket descriptor.
 *
 * \param daemon_p pointer to the daemon structure containing the list
 *        of active connections.
 *
 * \return AKWBS_SUCCESS on success updating the maximum file descriptor.
 *         AKWBS_ERROR on error while trying to update the maximum file descriptor.
 */
static int update_max_fds(struct akwbs_daemon *daemon_p)
{
  struct akwbs_connection *next = NULL;
  struct akwbs_connection *pos  = NULL;


  next = daemon_p->active_connections_head;

  if (daemon_p->result_io_queue[AKWBS_WRITE_INDEX] > daemon_p->listen_fd)
    daemon_p->max_fds = daemon_p->result_io_queue[AKWBS_WRITE_INDEX];

  if (daemon_p->listen_fd > daemon_p->result_io_queue[AKWBS_WRITE_INDEX])
    daemon_p->max_fds = daemon_p->listen_fd;

  while (NULL != (pos = next))
  {
    next = pos->next;

    if (pos->client_socket > daemon_p->max_fds)
      daemon_p->max_fds = pos->client_socket;
  }

  return AKWBS_SUCCESS;
}


/*!
 * Clean up the given connection.
 *
 * \param connection pointer to the connection that must be cleaned up.
 */
static void cleanup_connections_list(struct akwbs_connection **list_head,
                                     struct akwbs_connection **list_tail)
{
  struct akwbs_connection *next = NULL;
  struct akwbs_connection *pos  = NULL;


  next = *list_head;

  while (NULL != (pos = next))
  {
    next = pos->next;

    ring_buffer_free(&pos->buffer);
    free(pos->file_name);

    DLL_remove((*list_head), (*list_tail), pos);
    FD_CLR(pos->client_socket, &pos->daemon_ref->master_read_set);
    FD_CLR(pos->client_socket, &pos->daemon_ref->master_write_set);
    free(pos);
  }
}


/*!
 * Clean up all active connections in the 'active' double-linked list.
 *
 * \param daemon_p pointer to the daemon structure that holds the active connections.
 */
static void clean_active_connections_list(struct akwbs_daemon *daemon_p)
{
  cleanup_connections_list(&daemon_p->active_connections_head,
                           &daemon_p->active_connections_tail);
}


/*!
 * Clean up all connections in the 'cleanup' double-linked list.
 *
 * \param daemon_p pointer to the daemon that holds the list.
 */
void akwbs_clean_cleanup_connections_list(struct akwbs_daemon *daemon_p)
{
  cleanup_connections_list(&daemon_p->cleanup_connections_head,
                           &daemon_p->cleanup_connections_tail);

  update_max_fds(daemon_p);
}


/*!
 * Clean up all connections handled by the given daemon.
 *
 * \param daemon_p pointer to the daemon that handles all connections.
 */
void akwbs_cleanup_connections(struct akwbs_daemon *daemon_p)
{
  if (daemon_p->cleanup_connections_head != NULL)
    akwbs_clean_cleanup_connections_list(daemon_p);

  if (daemon_p->active_connections_head != NULL)
    clean_active_connections_list(daemon_p);
}


/*!
 * Pass through all connections in active connections list and cleanup connections list.
 *
 * \param daemon_p pointer to the daemon structure that holds all the connections lists.
 *
 * \return AKWBS_SUCCESS on success handling all connection at this time.
 *         AKWBS_ERROR on serious error while handling connections.
 */
static int handle_connections(struct akwbs_daemon *daemon_p)
{
  struct akwbs_connection *next = NULL;
  struct akwbs_connection *pos  = NULL;


  next = daemon_p->active_connections_head;

  while (NULL != (pos = next))
  {
    next = pos->next;

    if (akwbs_handle_connection(pos) == AKWBS_ERROR)
      return (update_max_fds(daemon_p), AKWBS_ERROR);
  }

  if (daemon_p->cleanup_connections_head != NULL)
    akwbs_clean_cleanup_connections_list(daemon_p);

  return AKWBS_SUCCESS;
}


/*!
 * Main routine performed by the server's daemon.
 *
 * \param daemon_p pointer to the daemon structure containing all information needed to
 *        execute the server.
 *
 * \return AKWBS_SUCCESS on success when terminating the daemon routine.
 *         AKWBS_ERROR on fatal errors.
 *
 * \details The server is flagged as being shutting down when receives a TERM signal.
 *          After this flag have been set, the server will be shutdown gracefully by
 *          deallocating previous allocated structures and closing used resources.
 */
static int daemon_routine(struct akwbs_daemon *daemon_p)
{
  while (shutdown_flag == AKWBS_NO)
  {
    check_new_conf(daemon_p);

    if (get_ready_fds(daemon_p) == AKWBS_ERROR)
    {
      if (errno == EINTR)
        continue;
      return AKWBS_ERROR;
    }

    if (handle_incoming_connections(daemon_p) == AKWBS_ERROR)
      return AKWBS_ERROR;

    if (handle_results(daemon_p) == AKWBS_ERROR)
      return AKWBS_ERROR;

    if (handle_connections(daemon_p) == AKWBS_ERROR)
      return AKWBS_ERROR;
  }

  return AKWBS_SUCCESS;
}


/*!
 * Setup daemon structure.
 *
 * \param daemon_p pointer to the daemon structure.
 * \param serv_conf pointer to the server configuration structure.
 *
 * \return AKWBS_SUCCESS on success, AKWBS_ERROR on error.
 */
static int setup_daemon(struct akwbs_daemon *daemon_p,
                        struct akwbs_server_conf *serv_conf_p)
{
  int ret_setsock_opt = -1;
  int opt_reuse       = AKWBS_YES;
  int i;


  if ((daemon_p == NULL) || (serv_conf_p == NULL))
    return AKWBS_ERROR;

  if (setup_signal_handlers() == AKWBS_ERROR)
    return AKWBS_ERROR;

  if(pthread_mutex_init(&daemon_p->request_io_queue_mutex, NULL) == AKWBS_ERROR)
    return AKWBS_ERROR;

  if (pthread_cond_init(&daemon_p->request_io_queue_cond, NULL) == AKWBS_ERROR)
    return AKWBS_ERROR;

  daemon_p->tree_opened_files = NULL;

  daemon_p->root_path = strdup(serv_conf_p->root_path);
  daemon_p->send_rate = serv_conf_p->send_rate;
  daemon_p->port      = serv_conf_p->port;

  if (akwbs_request_io_create_queue() == AKWBS_ERROR)
    return AKWBS_ERROR;

  daemon_p->listen_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

  if (daemon_p->listen_fd == AKWBS_ERROR)
    return AKWBS_ERROR;

  ret_setsock_opt = setsockopt(daemon_p->listen_fd,
                               SOL_SOCKET,
                               SO_REUSEADDR,
                               &opt_reuse,
                               (socklen_t)sizeof(opt_reuse));


  if (ret_setsock_opt == AKWBS_ERROR)
    return AKWBS_ERROR;

  daemon_p->serv_addr.sin_family        = AF_INET;
  daemon_p->serv_addr.sin_port          = htons(serv_conf_p->port);
  daemon_p->serv_addr.sin_addr.s_addr   = htonl(INADDR_ANY);

  if (bind(daemon_p->listen_fd,
           (struct sockaddr *)&daemon_p->serv_addr,
           (socklen_t)sizeof(daemon_p->serv_addr)) == AKWBS_ERROR)
    return AKWBS_ERROR;

  if (akwbs_request_io_open_for_read(&daemon_p->request_io_queue[AKWBS_READ_INDEX])
      == AKWBS_ERROR)
    return AKWBS_ERROR;

  if (akwbs_request_io_open_for_write(&daemon_p->request_io_queue[AKWBS_WRITE_INDEX])
      == AKWBS_ERROR)
    return AKWBS_ERROR;

  if (socketpair(AF_LOCAL, SOCK_DGRAM, 0, daemon_p->result_io_queue) == AKWBS_ERROR)
    return AKWBS_ERROR;

  for (i = 0; i < AKWBS_WORKING_THREADS; i++)
    if (pthread_create(&daemon_p->thread_ids[i],
                   NULL,
                   akwbs_thread_io_routine,
                   daemon_p)
        != AKWBS_SUCCESS)
      return AKWBS_ERROR;

  if (listen(daemon_p->listen_fd, SOMAXCONN) == AKWBS_ERROR)
    return AKWBS_ERROR;

  daemon_p->max_fds = daemon_p->result_io_queue[AKWBS_WRITE_INDEX];

  FD_SET(daemon_p->listen_fd, &daemon_p->master_read_set);
  FD_SET(daemon_p->result_io_queue[AKWBS_READ_INDEX], &daemon_p->master_read_set);


  return AKWBS_SUCCESS;
}


/*!
 * Shutdown this daemon by deallocating previous allocated structures and closing precious
 * opened resources.
 *
 * \param daemon_p pointer to the daemon structure.
 */
static void shutdown_daemon(struct akwbs_daemon *daemon_p)
{
  int i;
  void *res = NULL;


  close(daemon_p->listen_fd);
  free(daemon_p->root_path);

  for (i = 0; i < AKWBS_WORKING_THREADS; i++)
  {
    pthread_cancel(daemon_p->thread_ids[i]);
    pthread_join(daemon_p->thread_ids[i], &res);
  }

  close(daemon_p->request_io_queue[AKWBS_READ_INDEX]);
  close(daemon_p->request_io_queue[AKWBS_WRITE_INDEX]);
  unlink(AKWBS_REQUEST_IO_FIFO_PATH);

  close(daemon_p->result_io_queue[AKWBS_READ_INDEX]);
  close(daemon_p->result_io_queue[AKWBS_WRITE_INDEX]);

  pthread_mutex_destroy(&daemon_p->request_io_queue_mutex);
  pthread_cond_destroy(&daemon_p->request_io_queue_cond);

  akwbs_cleanup_connections(daemon_p);

  tdestroy(daemon_p->tree_opened_files, free);
}


/*!
 * Start server's daemon.
 *
 * \param port port to bind to.
 * \param root_path root path for this daemon.
 * \param rate_send the rate of transmission.
 *
 * \return AKWBS_SUCCESS on success.
 *         AKWBS_ERROR on error.
 */
int akwbs_start_daemon(uint16_t port, char *root_path, unsigned long send_rate)
{
  struct akwbs_daemon daemon_s;
  struct akwbs_server_conf conf;
  int ret = AKWBS_ERROR;


  //if (daemonize(root_path) == AKWBS_ERROR)
  //  return AKWBS_ERROR;

  bzero(&daemon_s, sizeof(struct akwbs_daemon));

  conf.port = port;
  conf.root_path = root_path;
  conf.send_rate = send_rate;


  if (setup_daemon(&daemon_s, &conf) == AKWBS_ERROR)
    ret = AKWBS_ERROR;
  else if (daemon_routine(&daemon_s) == AKWBS_SUCCESS)
    ret = AKWBS_SUCCESS;

  shutdown_daemon(&daemon_s);

  return ret;
}
