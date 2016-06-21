/*!
 * \file resultio.c
 * \brief Implementation of functions related to passing messages of I/O results.
 * \author Henrique Nascimento Gouveia <henrique.gouveia@aker.com.br>
 */

#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "resultio.h"
#include "internal.h"

/*!
 * Initialize the result I/O result message.
 *
 * \param msg message to be initialized.
 *
 * \return AKWBS_SUCCESS on success.
 */
int akwbs_result_io_init_msg(struct akwbs_result_io *msg)
{
  msg->connection_fd = 0;
  msg->bytes_read    = 0;

  return AKWBS_SUCCESS;
}

/*!
 * Send the result of the I/O performed as a message through a socketpair.
 *
 * \param msg message to be sent.
 * \param sock socket descriptor representing the write side of the socketpair.
 *
 * \return AKWBS_SUCCESS on success sending the message.
 *         AKWBS_ERROR on error while sending the message.
 *
 * \see send(2).
 */
int akwbs_result_io_send_msg(struct akwbs_result_io *msg, int sock)
{
  ssize_t bytes_sent = 0;
  
  bytes_sent = send(sock, msg, sizeof(*msg), 0);
  
  if (bytes_sent == AKWBS_ERROR)
    return AKWBS_ERROR;
  
  return AKWBS_SUCCESS;
}


/*!
 * Receive the result of the I/O performed as a message through a socketpair.
 *
 * \param msg message to be sent.
 * \param sock sock descriptor representing the read side of the socketpair.
 *
 * \return AKWBS_SUCCESS on success while receiving the message.
 *         AKWBS_ERROR on error while receiving the message.
 */
int akwbs_result_io_recv_msg(struct akwbs_result_io *msg, int sock)
{
  ssize_t bytes_received = 0;
  
  bytes_received = recv(sock, msg, sizeof(*msg), 0);
  
  if (bytes_received == AKWBS_ERROR)
    return AKWBS_ERROR;
  
  return AKWBS_SUCCESS;
}
