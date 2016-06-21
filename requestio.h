/*!
 * \file requestio.h
 * \brief Public interface for sending and receiving request I/O messages.
 * \author Henrique Nascimento Gouveia <henrique.gouveia@aker.com.br>
 */


#ifndef _AKWBS_MT_REQUEST_IO_QUEUE_H_
#define _AKWBS_MT_REQUEST_IO_QUEUE_H_

#include <signal.h>
#include <stdlib.h>

#include "io.h"


#define AKWBS_REQUEST_IO_FIFO_PATH "/tmp/akwbs_mt" /*!< Path where the FIFO resides.    */

/*!
 * This structure represents an I/O request message.
 */
struct akwbs_request_io_msg
{
  int                sd;          /*!< Connection's socket descriptor.                  */
  int                fd;          /*!< File descriptor of requested file.               */
  void               *address;    /*!< Buffer address.                                  */
  ssize_t            bytes;       /*!< Bytes in or available in this buffer.            */
  off_t              offset;      /*!< Start performing I/O on this offset.             */
  enum akwbs_io_type type;        /*!< Type of I/O that must be performed.              */
};


/*
 * Public Interface.
 */
int akwbs_request_io_create_queue(void);
int akwbs_request_io_open_for_write(int *write_fd);
int akwbs_request_io_open_for_read(int *read_fd);
int akwbs_request_io_recv_msg(struct akwbs_request_io_msg *msg, int read_fd);
int akwbs_request_io_send_msg(struct akwbs_request_io_msg *msg, int write_fd);


#endif /* END OF requestio.h */