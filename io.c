/*!
 * \file io.c
 * \brief Functions related to performing I/O.
 * \author Henrique Nascimento Gouveia <h.gouveia@icloud.com>
 */

#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

#include "io.h"

/*!
 * Read bytes from the given file from the given offset.
 *
 * \param fd      file descriptor.
 * \param address address to where store the data read.
 * \param bytes   return-param initially with the desired number of bytes to read,
 *                being set to the actual number of bytes read afterwards.
 * \param offset  return-param initially containing the actual offset before the I/O,
 *                and, afterwards, being set to represent the actual offset on file.
 *
 * \return -1 on error while reading the file. 0 on success reading file.
 */
static int do_io_read(int fd, void *address, ssize_t *bytes, off_t *offset)
{
  ssize_t bytes_read = 0;


  bytes_read = pread(fd, address, *bytes, *offset);

  switch (bytes_read)
  {
    case -1:
      switch (errno)
    {
      case EAGAIN:
        /* This is OK. */
        break;
      defaul:
        return -1;
    }
      break;
    default:
      *bytes = bytes_read;
      *offset += bytes_read;
  }

  return 0;
}

/*!
 * Write bytes into the given file from the given offset.
 *
 * \param fd      file descriptor.
 * \param address address to get data to write into the file.
 * \param bytes   return-param initially with the desired number of bytes to be written,
 *                being set to the actual number of bytes written afterwards.
 * \param offset  return-param initially containing the actual offset before the I/O,
 *                and, afterwards, being set to represent the actual offset on file.
 *
 * \return -1 on error while writing to file. 0 on success writing to file.
 */
static int do_io_write(int fd, void *address, ssize_t *bytes, off_t *offset)
{
  ssize_t bytes_written = 0;


  bytes_written = pwrite(fd, address, *bytes, *offset);

  switch (bytes_written)
  {
    case -1:
      switch (errno)
    {
      case EAGAIN:
        /* This is OK. */
        break;
      default:
        puts(strerror(errno));
        return -1;
    }
      break;
    default:
      *bytes = bytes_written;
      *offset += bytes_written;
  }

  return 0;
}



/*!
 * Perform I/O to the given file.
 *
 * \param fd      file descriptor.
 * \param address address to the buffer.
 * \param bytes   return-param initially with the desired number of bytes used in this
 *                operation, being set to the actual number of bytes used afterwards.
 * \param offset  return-param initially containing the actual offset before the I/O,
 *                and, afterwards, being set to represent the actual offset on file.
 *
 * \return -1 on error while performing I/O to file. 0 on success.
 */
int akwbs_do_io(int fd, void *address, ssize_t *bytes, off_t *offset, int io_type)
{
  int ret = 0;


  if ((address == NULL)
      || (bytes == NULL)
      || (offset == NULL))
    return -1;

  if (fd == -1)
    return -1;

  if (*bytes < 0)
    return -1;

  if (*bytes > BUFSIZ)
    *bytes = BUFSIZ;

  if (*offset < 0)
    return -1;


  switch (io_type)
  {
    case AKWBS_IO_GET_TYPE:
      ret = do_io_read(fd, address, bytes, offset);
      break;
    case AKWBS_IO_PUT_TYPE:
      ret = do_io_write(fd, address, bytes, offset);
      break;
    default:
      return -1;
  }

  return ret;
}
