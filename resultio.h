/*!
 * \file resultio.h
 * \brief Public interface for managing result I/O messages.
 * \author Henrique Nascimento Gouveia <henrique.gouveia@aker.com.br>
 */

#ifndef _AKWBS_MT_RESULTIO_H
#define _AKWBS_MT_RESULTIO_H

/*!
 * This structure represents a result  message of requested I/O operation.
 */
struct akwbs_result_io
{
  int    connection_fd;         /*!< Client socket.                                     */
  size_t bytes_read;            /*!< Bytes read from the queue.                         */
};


/*
 * Prototypes.
 */
int akwbs_result_io_init_msg(struct akwbs_result_io *msg);
int akwbs_result_io_send_msg(struct akwbs_result_io *msg, int sock);
int akwbs_result_io_recv_msg(struct akwbs_result_io *msg, int sock);

#endif
