/*!
 * \file   connection.h
 * \brief  Public interface for managing connections.
 * \author Henrique Nascimento Gouveia <h.gouveia@icloud.com>
 */


#ifndef _AKWBS_MT_CONNECTION_H_
#define _AKWBS_MT_CONNECTION_H_


#include "ringbuffer.h"
#include "io.h"
#include "requestio.h"
#include "daemon.h"


/*!
 * HTTP Status Code
 */
#define AKWBS_HTTP_200 "HTTP/1.0 200 OK\r\n\r\n"
#define AKWBS_HTTP_201 "HTTP/1.0 201 CREATED\r\n\r\n"
#define AKWBS_HTTP_202 "HTTP/1.0 202 ACCEPTED\r\n\r\n"
#define AKWBS_HTTP_400 "HTTP/1.0 400 BAD REQUEST\r\n\r\n"
#define AKWBS_HTTP_411 "HTTP/1.0 411 LENGTH REQUIRED\r\n\r\n"
#define AKWBS_HTTP_413 "HTTP/1.0 413 REQUEST ENTITY TOO LARGE\r\n\r\n"
#define AKWBS_HTTP_414 "HTTP/1.0 414 REQUESTED-URI TOO LONG\r\n\r\n"
#define AKWBS_HTTP_404 "HTTP/1.0 404 NOT FOUND\r\n\r\n"
#define AKWBS_HTTP_505 "HTTP/1.0 505 HTTP VERSION NOT SUPPORTED\r\n\r\n"


#define AKWBS_SIZE_HEADER_TOO_BIG 8000 /*!< Beyond this limit, the requested header is
                                        *   considered as too big, and an error message is
                                        *   sent back to the client. Being more specific,
                                        *   an HTTP 400 BAD REQUEST error.
                                        */


#define AKWBS_TIMEOUT_SECONDS 120     /*!< After this limit, the connection will be
                                       *   dropped.
                                       */


/*!
 * States in a state machine for a connection.
 *
 * Transitions are any-state to CLOSED, any state to state+1.
 * CLOSED is the terminal state and
 * INIT the initial state.
 */
enum akwbs_connection_state
{
  /*!
   * 1: Connection just started (no headers received).
   * Waiting for the line with the request type, URL and version.
   */
  AKWBS_CONNECTION_INIT = 0,

  /*!
   * 2: We have received some part of the request header, but not everyhting.
   */
  AKWBS_CONNECTION_HEADERS_RECEIVING = AKWBS_CONNECTION_INIT + 1,

  /*!
   * 3: We have received all the request header. Now we must process it.
   */
  AKWBS_CONNECTION_HEADERS_RECEIVED = AKWBS_CONNECTION_HEADERS_RECEIVING + 1,

  /*!
   * 4: The headers received was proccessed.
   */
  AKWBS_CONNECTION_HEADERS_PROCESSED = AKWBS_CONNECTION_HEADERS_RECEIVED + 1,

  /*!
   * 5: We are currently on transmission of data.
   */
  AKWBS_CONNECTION_ON_TRANSMISSION = AKWBS_CONNECTION_HEADERS_PROCESSED + 1,

  /*!
   * 6: We have closed this connection.
   */
  AKWBS_CONNECTION_CLOSED = AKWBS_CONNECTION_ON_TRANSMISSION + 1,

  /*!
   * 7: This connection is marked for cleanup.
   */
  AKWBS_CONNECTION_CLEANUP = AKWBS_CONNECTION_CLOSED + 1
};


/*!
 * States in a state machine for storing information
 * about final elements of the requested header (aka
 * CRLFCRLF). The transitions are from any-state to
 * INITIAL. The states should follow the natural sequence
 * of the enum, in case some unexpected shows, the machine
 * state is reseted back to the INITIAL state.
 */
enum akwbs_header_state
{
  /*!
   * Primary and initial state. None relevant
   * information has been found.
   */
  AKWBS_HEADER_INITIAL = 0,

  /*!
   * First carriage return "\r" has been found. We should expect
   * a linefeed later on.
   */
  AKWBS_HEADER_FIRST_CARRIAGE_RETURN,

  /*!
   * First linefeed "\n" has been found. This indicates that we
   * reached the end of a header line. We should expect another
   * carriage return "\r" (LAST_CARRIAGE_RETURN) if we are at
   * the end of the request header, or we may found other character,
   * and this indicates that another potentially new header line is
   * going to be read.
   */
  AKWBS_HEADER_FIRST_LINEFEED,

  /*!
   * This one indicates that we are at the end of the header. We have
   * the last carriage return "\r", but we still need one more linefeed
   * to consider that as an end of the header.
   */
  AKWBS_HEADER_LAST_CARRIAGE_RETURN,

  /*!
   * This is it. We have reached the end of the request header. Beyond this,
   * we MUST find data only on PUT requests. In case of GET method request,
   * do not even bother checking ahead.
   */
  AKWBS_HEADER_LAST_LINEFEED
};


/*!
 * Structure representing a connection with client through a socket.
 */
struct akwbs_connection
{
  struct ring_buffer buffer;          /*!< Buffer for this connection.                  */

  struct akwbs_connection *next;     /*!< Pointer to the next connection.               */

  struct akwbs_connection *prev;     /*!< Pointer to the previous connection.           */

  int file_descriptor;               /*!< File descriptor of requested resource.        */

  int client_socket;                 /*!< Client socket descriptor.                     */

  int has_request_pending;           /*!< A previous request could not be sent.         */

  int is_waiting_result;             /*!< Waiting for a result.                         */

  int has_opening_fd_pending;        /*!< Resource could not be opened in last attempt. */

  char *file_name;                   /*!< Resource name.                                */

  off_t file_total_offset;           /*!< Total offset for the reuested file.           */

  off_t file_cur_offset;             /*!<  Current offset in the requested file.        */

  struct akwbs_request_io_msg
    pending_io_msg;                  /*!< Pending I/O message.                          */

  enum akwbs_connection_state
    connection_state;                /*!< State of this connection.                     */

  enum akwbs_header_state
    header_state;                    /*!< State related to the request header.          */

  struct akwbs_daemon *daemon_ref;   /*!< Reference to the daemon handling this conn.   */

  enum akwbs_io_type io_type;        /*!< Type of I/O that must be performed.           */

  struct timeval last_time_io;       /*!< Last time we performed some transmission.     */

  struct timeval last_activity;      /*!< Last time that this connection was used.      */

  size_t bytes_sent_last_io;         /*!< Bytes on last I/O operation.                  */

  char *end_of_first_header_line;    /*!< Pointer to the end of first line on header.   */

  char *end_of_header;               /*!< Pointer to the end of the header.             */
};


/*
 * Public interface.
 */
int akwbs_handle_connection(struct akwbs_connection *connection);
int akwbs_create_new_connection(struct akwbs_connection **connection);

#endif /* END OF CONNECTION.H */
