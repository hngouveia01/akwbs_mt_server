#ifndef _AKWBS_MT_FILE_THREE_H_
#define _AKWBS_MT_FILE_THREE_H_

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/*!
 * Structure representing opened files.
 */
struct akwbs_file_stat
{
  ino_t inode_number;        /*!< Inode number of this opened file.                     */
  int file_descriptor;       /*!< File descriptor of this opened file.                  */
  unsigned int number_of_references;  /*!< Number of connections using this descriptor.          */
};

/* PROTOTYPES */
int akwbs_compare_file_stat(const void *pa, const void *pb);


#endif /* END OF FILE_TREE.H */
