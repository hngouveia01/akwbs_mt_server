//
//  http.c
//  akwbs_final_s
//
//  Created by Henrique Gouveia on 11/1/13.
//  Copyright (c) 2013 Henrique Nascimento Gouveia. All rights reserved.
//

#include <stdio.h>
#include <stdint.h>
#include <ctype.h>
#include <string.h>

#include "connection.h"
#include "internal.h"
#include "http.h"


/*!
 * Parse the first line of the HTTP HEADER.
 *
 * \example Example of a simple HTTP request: "METHOD SP URI SP VERSION CRLFCRLF".<p>
 *          METHOD  = GET | PUT                      <p>
 *          SP      = whitespace ' '                 <p>
 *          URI     = absolute path to file.         <p>
 *          VERSION = HTTP/1.0                       <p>
 *          CR      = '\r'                           <p>
 *          LF      = '\n'                           <p>
 *
 * \param connection the connection (updated)
 * \param line the first line
 *
 * \return AKWBS_REACHED_HEADER_END if we have reached the header's end
 *         AKWBS_ERROR if the header is malformed.
 */
static int parse_initial_message_line(struct akwbs_connection *connection)
{
  char *uri                      = NULL;
  char *line                     = NULL;
  char *first_space_after_uri    = NULL;
  char *first_space_after_method = NULL;


  *(connection->end_of_first_header_line -= 1) = '\0';

  line = ring_buffer_read_address(&connection->buffer);

  if (! isspace(*(line + STRLEN_ANY_ACCEPTED_METHOD)))
    return AKWBS_ERROR;

  first_space_after_method = line + STRLEN_ANY_ACCEPTED_METHOD;

  *first_space_after_method = '\0';


  if (strncmp(line, "GET", STRLEN_ANY_ACCEPTED_METHOD) == AKWBS_SUCCESS)
    connection->io_type = AKWBS_IO_GET_TYPE;
  else if (strncmp(line, "PUT", STRLEN_ANY_ACCEPTED_METHOD) == AKWBS_SUCCESS)
    connection->io_type = AKWBS_IO_PUT_TYPE;
  else
  {
    connection->io_type = AKWBS_IO_UNKNOWN_TYPE;
    goto free_and_fail;
  }

  if (*(++first_space_after_method) != '/')
    goto free_and_fail;

  first_space_after_uri = first_space_after_method;

  while (*first_space_after_uri != ' ')
    first_space_after_uri++;

  *first_space_after_uri = '\0';

  uri = strdup(first_space_after_method);

  if (uri == NULL)
    goto free_and_fail;

  connection->file_name = uri;

  return AKWBS_SUCCESS;

free_and_fail:
  free(uri);
  return AKWBS_ERROR;
}


/*!
 * Get the content length of a PUT request header.
 *
 * \param connection connection holding the requested header in buffer.
 *
 * \return AKWBS_SUCCESS on success getting the content length value.
 *         AKWBS_ERRROR on error while trying to get the content length off the requested
 *         header.
 */
static int get_content_length(struct akwbs_connection *connection)
{
  int i = 0;
  size_t bytes_in_buffer = ring_buffer_count_bytes(&connection->buffer);
  char *content_length = NULL;

  char *read_address = ring_buffer_read_address(&connection->buffer);

  while (content_length == NULL)
  {
    for (; i <= bytes_in_buffer; read_address++, i++)
      if (*read_address == ':')
      {
        *read_address = '\0';
        break;
      }

    content_length = strstr(ring_buffer_read_address(&connection->buffer) + i
                             - strlen("Content-Length"),
                             "Content-Length");
    if (content_length == NULL)
      continue;
    else
    {
      char *begining_number_content_length = ++read_address;

      while (connection->file_total_offset == 0)
      {
        unsigned long j = bytes_in_buffer;

        for (; j <= bytes_in_buffer; read_address++, j++)
          if (*read_address == '\r')
          {
            *read_address = '\0';
            connection->file_total_offset = atol(++begining_number_content_length);
            return AKWBS_SUCCESS;
          }

        if (j == bytes_in_buffer)
          return AKWBS_ERROR;
      }
    }
  }

  if ((i == bytes_in_buffer)
      && (content_length == NULL))
    return AKWBS_ERROR;

  return AKWBS_SUCCESS;
}


/*!
 * Do header processing to collect requested informations.
 *
 * \param connection connection containing the header.
 * \param daemon_p pointer to the daemon structure containing all file descriptors sets.
 *
 * \return AKWBS_SUCCESS on success.
 *         AKWBS_ERROR on error.
 */
static int do_processing(struct akwbs_connection *connection)
{
  size_t end_of_header = 0;


  if (parse_initial_message_line(connection) == AKWBS_ERROR)
    return AKWBS_ERROR;

  if (connection->io_type == AKWBS_IO_PUT_TYPE)
    if (get_content_length(connection) == AKWBS_ERROR)
      return AKWBS_ERROR;

  end_of_header = (size_t)((connection->end_of_header)
                           - (char *)ring_buffer_read_address(&connection->buffer));

  ring_buffer_read_advance(&connection->buffer, end_of_header);

  switch (connection->io_type)
  {
    case AKWBS_IO_GET_TYPE:
      FD_CLR(connection->client_socket, &connection->daemon_ref->master_read_set);
      FD_SET(connection->client_socket, &connection->daemon_ref->master_write_set);
      break;
    case AKWBS_IO_PUT_TYPE:
      FD_CLR(connection->client_socket, &connection->daemon_ref->master_write_set);
      FD_SET(connection->client_socket, &connection->daemon_ref->master_read_set);
      break;
    default:
      close(connection->client_socket);
      connection->connection_state = AKWBS_CONNECTION_CLOSED;
      return AKWBS_ERROR;
  }

  connection->connection_state = AKWBS_CONNECTION_HEADERS_PROCESSED;

  return AKWBS_SUCCESS;
}

int akwbs_process_header(struct akwbs_connection *connection)
{
  if (do_processing(connection) == AKWBS_ERROR)
    return AKWBS_ERROR;
  return AKWBS_SUCCESS;
}
