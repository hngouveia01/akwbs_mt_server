/*!
 * \file    ringbuffer.c
 *
 * \brief   Circular buffer mmap'ed, an optimized POSIX implementation.
 *
 * \details This circular buffer is optimized by mapping the underlying buffer to two
 *          contiguous regions of virtual memory. Naturally, the underlying buffer's
 *          length must then equal some multiple of the system 's page size. Reading from
 *          and writing to the circular buffer may then be carried out with greater effic
 *          iency by means of direct memory access (DMA); those accesses which fall beyond
 *          the end of the first virtual-memory region will automatically wrap aroud to
 *          the beginnig of the underlying buffer. When the read offset is advanced into
 *          the second virtual-memory region, both offsets - read and write - are decrem
 *          ented by the length of the underlying buffer.
 *
 * \author  Henrique Nascimento Gouveia <henrique.gouveia@aker.com.br>
 */


#define _GNU_SOURCE

#include <sys/mman.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <strings.h>
#include <sys/param.h>

#include "ringbuffer.h"


/*!
 * Create the ring buffer following the given order.
 *
 * \param  buffer return-param with the address of the buffer.
 * \param  order  page shift to correctly-align value for all data types.
 *
 * \return RING_BUFFER_SUCCESS on success creating the ring buffer.
 *         RING_BUFFER_ERROR   on error while creating the ring buffer.
 *
 *\warning Order should be at least 12 for Linux. See the macro PGSHIFT for reference of
 *         a valid value.
 */
int ring_buffer_create(struct ring_buffer *buffer, size_t order)
{
  int  status = -1;
  int  file_descriptor = -1;
  void *address = NULL;
  char path[] = RING_BUFFER_PATH;
  
  file_descriptor = mkstemp(path);
  if (file_descriptor < 0)
    goto free_and_fail;
  
  status = unlink(path);
  if (status)
    goto free_and_fail;
  
  buffer->count_bytes = 1UL << order;
  buffer->write_offset_bytes = 0;
  buffer->read_offset_bytes = 0;
  
  status = ftruncate(file_descriptor, (off_t)buffer->count_bytes);
  if (status)
    goto free_and_fail;
  
  buffer->address = mmap(NULL, buffer->count_bytes << 1, PROT_NONE,
                         MAP_ANON | MAP_PRIVATE, -1, (off_t)0);
  
  if (buffer->address == MAP_FAILED)
    goto free_and_fail;
  
  address =
  mmap(buffer->address, buffer->count_bytes, PROT_READ | PROT_WRITE,
       MAP_FIXED | MAP_SHARED, file_descriptor, (off_t)0);
  
  if (address != buffer->address)
    goto free_and_fail;
  
  address = mmap(((char *)buffer->address) + buffer->count_bytes,
                 buffer->count_bytes, PROT_READ | PROT_WRITE,
                 MAP_FIXED | MAP_SHARED, file_descriptor, (off_t)0);
  
  if (address != ((char *)buffer->address) + buffer->count_bytes)
    goto free_and_fail;
  
  status = close(file_descriptor);
  if (status)
    goto free_and_fail;
  
  return RING_BUFFER_SUCCESS;
  
free_and_fail:
  close(file_descriptor);
  munmap(address, buffer->count_bytes);
  munmap(buffer->address, buffer->count_bytes);
  return RING_BUFFER_ERROR;
}

/*!
 * Free the memory mapped for the ring buffer.
 *
 * \param buffer Pointer to the ring buffer.
 *
 * \return RING_BUFFER_SUCCESS on success freeing the buffer.
 *         RING_BUFFER_ERROR   on error while freeing the buffer.
 */
int ring_buffer_free(struct ring_buffer *buffer)
{
  if (munmap(buffer->address, buffer->count_bytes << 1) == RING_BUFFER_ERROR)
    return RING_BUFFER_ERROR;
  
  return RING_BUFFER_SUCCESS;
}

/*!
 * Get the address of the memory area allowed to write to.
 *
 * \param buffer Address of the structure ring_buffer..
 *
 * \return Address area of the ring buffer allowed to perform writing.
 */
void *ring_buffer_write_address(struct ring_buffer *buffer)
{
  return ((char *)buffer->address) + buffer->write_offset_bytes;
}

/*!
 * Advance the write offset bytes by count_bytes bytes given.
 *
 * \param buffer      Address of the structure ring_buffer.
 * \param count_bytes Number of bytes to perform the offset.
 */
void ring_buffer_write_advance(struct ring_buffer *buffer, size_t count_bytes)
{
  buffer->write_offset_bytes += count_bytes;
}

/*!
 * Get the address of the memory area allowed to read from.
 *
 * \param buffer Address of the structure ring_buffer.
 *
 * \return Address area of the ring buffer allowed to perform reading.
 */
void *ring_buffer_read_address(struct ring_buffer *buffer)
{
  return ((char *)buffer->address) + buffer->read_offset_bytes;
}

/*!
 * Advance the read offset bytes by count_bytes bytes given.
 *
 * \param buffer      Address of the structure ring_buffer.
 * \param count_bytes Number of bytes to perform the offset.
 */
void ring_buffer_read_advance(struct ring_buffer *buffer, size_t count_bytes)
{
  buffer->read_offset_bytes += count_bytes;
  
  if (buffer->read_offset_bytes >= buffer->count_bytes)
  {
    buffer->read_offset_bytes -= buffer->count_bytes;
    buffer->write_offset_bytes -= buffer->count_bytes;
  }
}

/*!
 * Get the number of bytes inside the buffer.
 *
 * \param buffer Address of the structure ring_buffer.
 *
 * \return Number of bytes inside the given buffer.
 */
unsigned long ring_buffer_count_bytes(struct ring_buffer *buffer)
{
  return buffer->write_offset_bytes - buffer->read_offset_bytes;
}

/*!
 * Get the number of free bytes inside the buffer.
 *
 * \param buffer Address of the structure ring_buffer.
 *
 * \return Number of free bytes inside the given buffer.
 */
unsigned long ring_buffer_count_free_bytes(struct ring_buffer *buffer)
{
  return buffer->count_bytes - ring_buffer_count_bytes(buffer);
}

/*!
 * Clear (set to zero) both - read and write - offset bytes.
 *
 * \param buffer address of the structure ring_buffer.
 */
void ring_buffer_clear(struct ring_buffer *buffer)
{
  buffer->write_offset_bytes = 0;
  buffer->read_offset_bytes  = 0;
}
