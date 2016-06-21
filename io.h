/*!
 * \file io.h
 * \brief Public interface for I/O operations on files.
 * \author Henrique Nascimento Gouveia <h.gouveia@icloud.com>
 */

#ifndef _AKWBS_FINAL_S_IO_H_
#define _AKWBS_FINAL_S_IO_H_

#include <unistd.h>
#include <stdlib.h>


/*!
 * Types of I/O.
 */
enum akwbs_io_type
{
  AKWBS_IO_UNKNOWN_TYPE    = 0,    /*!< I/O type not recognized.                        */

  AKWBS_IO_GET_TYPE,               /*!< Reading from file.                              */

  AKWBS_IO_PUT_TYPE                /*!< Writing to file.                                */
};

/*
 * Public Interface.
 */
int akwbs_do_io(int fd, void *address, ssize_t *bytes, off_t *offset, int io_type);

#endif
