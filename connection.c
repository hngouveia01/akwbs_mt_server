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
