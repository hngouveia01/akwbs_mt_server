/*!
 * \file ringbuffer.h
 * \brief Public interface for ringbuffer.c
 * \author Henrique Nascimento Gouveia <henrique.gouveia@aker.com.br>
 */

#ifndef _AKWBS_MT_RINGBUFFER_H_
#define _AKWBS_MT_RINGBUFFER_H_


#define RING_BUFFER_SUCCESS                  0    /*!< Return value for success.        */

#define RING_BUFFER_ERROR                   -1    /*!< Return value for error.          */

#define RING_BUFFER_ERROR_INVALID_PARAMETER -2    /*!< Invalid parameter.               */

#define RING_BUFFER_ERROR_NO_SPARE_ROOM     -3    /*!< The buffer do NOT have space to
                                                   *   store all the data.
                                                   */

#define RING_BUFFER_IS_FULL                 -4    /*!< Buffer is full.                  */

#define RING_BUFFER_PATH                    \
  "/tmp/ring-buffer-XXXXXX"                       /*!< Path of the buffer.              */


/*!
 * Structure representing a buffer and its accounting.
 */
struct ring_buffer
{
  void   *address;              /*!< Buffer's address.                                  */
  size_t count_bytes;           /*!< Bytes on buffer.                                   */
  size_t write_offset_bytes;    /*!< Offset where new data must be written.             */
  size_t read_offset_bytes;     /*!< Offset where new data must be read.                */
};

/*
 * Public interface.
 */
int ring_buffer_create (struct ring_buffer *buffer, size_t order);
int ring_buffer_free (struct ring_buffer *buffer);
void *ring_buffer_write_address(struct ring_buffer *buffer);
void ring_buffer_write_advance(struct ring_buffer *buffer, size_t count_bytes);
void *ring_buffer_read_address(struct ring_buffer *buffer);
void ring_buffer_read_advance(struct ring_buffer *buffer, size_t count_bytes);
unsigned long ring_buffer_count_bytes(struct ring_buffer *buffer);
unsigned long ring_buffer_count_free_bytes(struct ring_buffer *buffer);
void ring_buffer_clear(struct ring_buffer *buffer);


#endif
