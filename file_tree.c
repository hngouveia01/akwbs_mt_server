/*!
 * \file file_tree.c
 * \brief Function that implements a comparison to walkthrough the tree of opened files.
 * \author Henrique Nascimento Gouveia <h.gouveia@icloud.com>
 */

#include <stdlib.h>
#include <search.h>

#include "file_tree.h"


int akwbs_compare_file_stat(const void *pa, const void *pb)
{
  if (* (ino_t *) pa < *(ino_t *) pb)
    return -1;

  if (* (ino_t *) pa > *(ino_t *) pb)
    return 1;

  return 0;
}
