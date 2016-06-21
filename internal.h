/*!
 * \file   internal.h
 * \brief  Internal shared structures used by AKWBS.
 * \author Henrique Nascimento Gouveia <h.gouveia@icloud.com>
 */

#ifndef _AKWBS_FINAL_S_INTERNAL_H_
#define _AKWBS_FINAL_S_INTERNAL_H_

#include <stdint.h>


#define AKWBS_SUCCESS     0  /*!< Number representing success.                          */

#define AKWBS_ERROR      -1  /*!< Number representing error.                            */

#define AKWBS_YES         1  /*!< Number representing yes.                              */

#define AKWBS_NO          0  /*!< Number representing no.                               */

#define AKWBS_READ_INDEX  0  /*!< Index indicating the read side.                       */

#define AKWBS_WRITE_INDEX 1  /*!< Index indicating the write side.                      */


/*!
 * This structure represents the server's configuration.
 */
struct akwbs_server_conf
{
  char          *root_path;        /*!< Root path to this directory.                    */
  uint16_t      port;              /*!< Server's port.                                  */
  unsigned long send_rate;         /*!< Rate of send transmission, in bytes per second. */
};


/*!
 * Insert an element at the head of a DLL. Assumes that head, tail and
 * element are structs with prev and next fields.
 *
 * \param head pointer to the head of the DLL
 * \param tail pointer to the tail of the DLL
 * \param element element to insert
 */
#define DLL_insert(head,tail,element) \
do                                    \
{                                     \
  (element)->next = (head);           \
  (element)->prev = NULL;             \
                                      \
if ((tail) == NULL)                   \
(tail) = element;                     \
else                                  \
(head)->prev = element;               \
                                      \
(head) = (element);                   \
                                      \
} while (0)


/*!
 * Remove an element from a DLL. Assumes
 * that head, tail and element are structs
 * with prev and next fields.
 *
 * \param head pointer to the head of the DLL
 * \param tail pointer to the tail of the DLL
 * \param element element to remove
 */
#define DLL_remove(head,tail,element)         \
do                                            \
{                                             \
  if ((element)->prev == NULL)                \
    (head) = (element)->next;                 \
  else                                        \
    (element)->prev->next = (element)->next;  \
                                              \
  if ((element)->next == NULL)                \
    (tail) = (element)->prev;                 \
  else                                        \
    (element)->next->prev = (element)->prev;  \
                                              \
  (element)->next = NULL;                     \
  (element)->prev = NULL;                     \
                                              \
} while (0)

#endif
