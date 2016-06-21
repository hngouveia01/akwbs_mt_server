/*!
 * \file thread_io.c
 * \brief Implementation of functions related to the I/O operations performed by all
 *        working threads.
 * \author Henrique Nascimento Gouveia <henrique.gouveia@aker.com.br>
 */


#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <sys/mman.h>

#include "internal.h"
#include "daemon.h"
#include "resultio.h"
#include "requestio.h"




/*!
 * Clean up handler called when a thread is cancelled.
 *
 * \param arg argument passed to this cleaner when it was called. Actually, this is a
 *        pointer to the daemon structure.
 */
static void thread_cleanup_routine(void *arg)
{
  struct akwbs_daemon *daemon_p = NULL;


  if (arg == NULL)
    return;

  daemon_p = (struct akwbs_daemon *)arg;

  pthread_mutex_unlock(&daemon_p->request_io_queue_mutex);


}

/*!
 * Set up the calling thread.
 *
 * \return AKWBS_SUCCESS on succes setting up this thread.
 *         AKWBS_ERROR on error while trying to set up this thread.
 *
 * \details The set up is do.ne by detaching the thread, enabling cancellation on deferred
 *          type.
 */
static int setup_io_thread(void)
{
  if (pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL) == AKWBS_ERROR)
    return AKWBS_ERROR;

  if (pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL) == AKWBS_ERROR)
    return AKWBS_ERROR;

  return AKWBS_SUCCESS;
}


/*!
 * Main routine of working threads.
 *
 * \param arg argument to this routine.
 */
void *akwbs_thread_io_routine(void *arg)
{
  struct akwbs_daemon *daemon_p = NULL;
  struct akwbs_request_io_msg msg;
  struct akwbs_result_io result_msg;


  if (arg == NULL)
    pthread_exit(NULL);

  daemon_p = (struct akwbs_daemon *)arg;

  if (setup_io_thread() == AKWBS_ERROR)
    pthread_exit(NULL);

  pthread_cleanup_push(thread_cleanup_routine, daemon_p)

  while (1)
  {
    ssize_t ret = 0;

    bzero(&msg, sizeof(struct akwbs_request_io_msg));
    bzero(&result_msg, sizeof(struct akwbs_result_io));

    if (pthread_mutex_lock(&daemon_p->request_io_queue_mutex) != AKWBS_SUCCESS)
      pthread_exit(NULL);

    while ((ret = read(daemon_p->request_io_queue[AKWBS_READ_INDEX],
                       &msg,
                       sizeof(struct akwbs_request_io_msg))) == AKWBS_ERROR)
    {
      if (errno == EAGAIN)
      {
        if (pthread_cond_wait(&daemon_p->request_io_queue_cond,
                              &daemon_p->request_io_queue_mutex) != AKWBS_SUCCESS)
        {
          pthread_mutex_unlock(&daemon_p->request_io_queue_mutex);
          pthread_exit(NULL);
        }
      }
      else
      {
        pthread_mutex_unlock(&daemon_p->request_io_queue_mutex);
        pthread_exit(NULL);
      }
    }

    pthread_mutex_unlock(&daemon_p->request_io_queue_mutex);

    akwbs_do_io(msg.fd, msg.address, &msg.bytes, &msg.offset, msg.type);

    result_msg.bytes_read    = msg.bytes;
    result_msg.connection_fd = msg.sd;

    posix_madvise(msg.address, msg.bytes, POSIX_MADV_SEQUENTIAL);

    akwbs_result_io_send_msg(&result_msg, daemon_p->result_io_queue[AKWBS_WRITE_INDEX]);

  }

  pthread_cleanup_pop(0);

  pthread_exit(NULL);

}
