//
//  http.h
//  akwbs_final_s
//
//  Created by Henrique Gouveia on 11/1/13.
//  Copyright (c) 2013 Henrique Nascimento Gouveia. All rights reserved.
//

#ifndef _AKWBS_HTTP_H_
#define _AKWBS_HTTP_H_

#define STRLEN_ANY_ACCEPTED_METHOD 3 /*!< strlen value for GET or PUT.                  */


/* Public Interface. */

int akwbs_process_header(struct akwbs_connection *connection);

#endif
