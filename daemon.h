/*!
 * \file   daemon.h
 * \brief  Functions related to the server's daemon.
 * \author Henrique Nascimento Gouveia <h.gouveia@icloud.com>
 */

#ifndef _AKWBS_MT_DAEMON_H_
#define _AKWBS_MT_DAEMON_H_

#include <pthread.h>
#include <stdint.h>
#include <netinet/in.h>

#include "io.h"


#define AKWBS_WORKING_THREADS 10  /*!< Number of working threads.                        */


/*!
 * Represent a daemon server. Contains all information about
 * the server and its structures.
 */
struct akwbs_daemon
{
  struct akwbs_connection
    *active_connections_head;   /*!< Head of active connections double-linked list.     */

  struct akwbs_connection
    *active_connections_tail;   /*!< Tail of active connections double-linked list.     */

  struct akwbs_connection
    *cleanup_connections_head;  /*!< Head of clean up connections double-linked list.   */

  struct akwbs_connection
    *cleanup_connections_tail;  /*!< Tail of clean up connections double-linked list.   */

  int listen_fd;                /*!< Descriptor listening incoming connections.         */

  int shutdown;                 /*!< Is the server being shutting down.                 */

  int has_new_conf;             /*!< New server's configuration has been set.           */

  int request_io_queue[2];      /*!< Queue of I/O requests.                             */

  pthread_mutex_t
    request_io_queue_mutex;     /*!< Mutex variable for the request I/O queue.          */

  pthread_cond_t
    request_io_queue_cond;      /*!< Condition variable for the request I/O queue.      */

  int result_io_queue[2];       /*!< Queue of I/O results.                              */

  char *root_path;              /*!< Server's root path.                                */

  uint16_t port;                /*!< Server's port.                                     */

  fd_set master_read_set;       /*!< Master read set for descriptors.                   */

  fd_set master_write_set;      /*!< Master write set for descriptors.                  */

  fd_set temp_read_set;         /*!< Temporary read set for ready descriptors.          */

  fd_set temp_write_set;        /*!< Temporary write set for ready descriptors.         */

  int fds_ready;                /*!< Number of fds ready after a select call.           */

  int max_fds;                  /*!< Greater fd being handle.                           */

  struct sockaddr_in serv_addr; /*!< Server address.                                    */

  unsigned long send_rate;      /*!< Send rate for out going transmissions.             */

  pthread_t thread_ids
    [AKWBS_WORKING_THREADS];    /*!< Array containing threads' IDs.                     */

  void *tree_opened_files;      /*!< Tree root of opened files.                         */
};

/*
 * Public Interface.
 */
int akwbs_start_daemon(uint16_t port, char *root_path, unsigned long send_rate);


#endif  /* END OF daemon.h */
