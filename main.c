/*!
 * \file main.c
 * \brief HTTP multithreaded server. It utilizes a pool of threads that are used only
 *        to GET bytes from a file and PUT bytes into a file.
 * \author Henrique Nascimento Gouveia <henrique.gouveia@aker.com.br>
 */


#include <stdio.h>
#include <limits.h>
#include <string.h>

#include "daemon.h"
#include "internal.h"
#include "connection.h"

#define AKWBS_INDEX_ARGV_PROGRAM_NAME 0          /*!< Index argv to program name.       */

#define AKWBS_INDEX_ARGV_ROOT_PATH    1          /*!< Index argv to root path.          */

#define AKWBS_INDEX_ARGV_PORT         2          /*!< Index argv of port to bind to.    */

#define AKWBS_INDEX_ARGV_SPEED_LIMIT  3          /*!< Index argv to bytes/sec.          */

#define AKWBS_INDEX_ARGC_EXPECTED     4          /*!< Expected number of args in argv.  */

/*!
 * Check if the params are valid.
 *
 * \param argv arguments passed through the terminal.
 *
 * \return AKWBS_SUCCESS on success.
 *         AKWBS_ERROR on invalid parameter.
 */
static int akwbs_check_params(const int argc, char *argv[])
{
  if (argc != AKWBS_INDEX_ARGC_EXPECTED)
    return AKWBS_ERROR;

  if (strlen(argv[AKWBS_INDEX_ARGV_ROOT_PATH]) >= PATH_MAX)
    return AKWBS_ERROR;

  if ((atol(argv[AKWBS_INDEX_ARGV_PORT]) == 0)
      || (atol(argv[AKWBS_INDEX_ARGV_PORT]) >= LONG_MAX))
    return AKWBS_ERROR;

  if (atol(argv[AKWBS_INDEX_ARGV_SPEED_LIMIT]) == 0)
    return AKWBS_ERROR;

  return AKWBS_SUCCESS;
}

int main(int argc, char * argv[])
{
  if (akwbs_check_params(argc, argv) == AKWBS_ERROR)
    return EXIT_FAILURE;


  if (akwbs_start_daemon(atol(argv[AKWBS_INDEX_ARGV_PORT]),
                         argv[AKWBS_INDEX_ARGV_ROOT_PATH],
                         atol(argv[AKWBS_INDEX_ARGV_SPEED_LIMIT])) == AKWBS_ERROR)
    return EXIT_FAILURE;

  pthread_exit(NULL);
}

