/*!
 * \file request_io_queue.c
 * \brief Functions related to managing I/O requests on queue.
 * \author Henrique Nascimento Gouveia <henrique.gouveia@aker.com.br>
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

#include "requestio.h"
#include "internal.h"


/*!
 * Create the FIFO related to I/O requests.
 *
 * \return AKWBS_SUCCESS on success.
 *         AKWBS_ERROR on error.
 */
int akwbs_request_io_create_queue(void)
{
  if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
    return AKWBS_ERROR;

  umask(0);

  unlink(AKWBS_REQUEST_IO_FIFO_PATH);
  if (mkfifo(AKWBS_REQUEST_IO_FIFO_PATH, S_IWRITE | S_IREAD) == AKWBS_ERROR)
    return AKWBS_ERROR;

  return AKWBS_SUCCESS;
}

/*!
 * Open the FIFO for writing.
 *
 * \param write_fd file descriptor of the write side of the FIFO.
 *
 * \return AKWBS_SUCCESS on succes.
 *         AKWBS_ERROR on error.
 */
int akwbs_request_io_open_for_write(int *write_fd)
{
  int fd = AKWBS_ERROR;


  fd = open(AKWBS_REQUEST_IO_FIFO_PATH, O_WRONLY | O_NONBLOCK);

  if (fd == AKWBS_ERROR)
    return AKWBS_ERROR;

  *write_fd = fd;

  return AKWBS_SUCCESS;
}

/*!
 * Open the FIFO for reading.
 *
 * \param read_fd file descriptor of the read side of the FIFO.
 *
 * \return AKWBS_SUCCESS on success while opening the FIFO.
 *         AKWBS_ERROR on error while opening the FIFO.
 */
int akwbs_request_io_open_for_read(int *read_fd)
{
  int fd = AKWBS_ERROR;

  fd = open(AKWBS_REQUEST_IO_FIFO_PATH, O_RDONLY | O_NONBLOCK);

  if (fd == AKWBS_ERROR)
    return AKWBS_ERROR;

  *read_fd = fd;

  return AKWBS_SUCCESS;
}

/*!
 * Receive request from the request I/O queue.
 *
 * \param msg param-return pointer to the location where the message will be stored.
 * \param read_fd file descriptor of read side of the FIFO.
 *
 * \return AKWBS_SUCCESS on success.
 *         AKWBS_ERROR on error.
 */
int akwbs_request_io_recv_msg(struct akwbs_request_io_msg *msg, int read_fd)
{
  ssize_t bytes = 0;

  bytes = read(read_fd, &(*msg), sizeof(struct akwbs_request_io_msg));

  if (bytes == AKWBS_ERROR)
    return AKWBS_ERROR;

  if (bytes != sizeof(struct akwbs_request_io_msg))
    return AKWBS_ERROR;

  return AKWBS_SUCCESS;
}

/*!
 * Send request to the request I/O queue.
 *
 * \param msg message to be sent.
 * \param write_fd file descriptor of write side of the FIFO.
 *
 * \return AKWBS_SUCCESS on success.
 *         AKWBS_ERROR on error.
 */
int akwbs_request_io_send_msg(struct akwbs_request_io_msg *msg, int write_fd)
{
  ssize_t bytes = 0;

  bytes = write(write_fd, msg, sizeof(struct akwbs_request_io_msg));

  if (bytes == AKWBS_ERROR)
    return AKWBS_ERROR;

  if (bytes != sizeof(struct akwbs_request_io_msg))
    return AKWBS_ERROR;

  return AKWBS_SUCCESS;
}
