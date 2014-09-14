/*
 * SysDB - src/include/frontend/proto.h
 * Copyright (C) 2013 Sebastian 'tokkee' Harl <sh@tokkee.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SDB_FRONTEND_PROTO_H
#define SDB_FRONTEND_PROTO_H 1

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The SysDB frontend protocol is based on messages being passed between the
 * client and the server. Each message includes a header containing the
 * message type which is usually a status or command code, the length of the
 * message not including the header, and the message body. The content of the
 * message body depends on the message type. Both, the message type and length
 * are stored in an unsigned 32bit integer in network byte-order.
 *
 *                  1               3               4               6
 *  0               6               2               8               4
 * +-------------------------------+-------------------------------+
 * | message type                  | message length                |
 * +-------------------------------+-------------------------------+
 * | message body ...
 *
 */

/* status codes returned to a client */
typedef enum {
	/*
	 * CONNECTION_OK:
	 * Indicates that a command was successful. The message body will usually
	 * be empty but may contain a string providing unformatted information
	 * providing more details.
	 *
	 * 0               32              64
	 * +---------------+---------------+
	 * | message type  | length        |
	 * +---------------+---------------+
	 * | optional status message ...   |
	 */
	CONNECTION_OK = 0,

	/*
	 * CONNECTION_DATA:
	 * Indicates that a data query was successful. The message body will
	 * contain the type of the data and the result encoded as a JSON string.
	 * The type is the same as the command code of the respective command (see
	 * below) and is stored as an unsigned 32bit integer in network
	 * byte-order. The result may be empty (but the type is still included).
	 *
	 * 0               32              64
	 * +---------------+---------------+
	 * | message type  | length        |
	 * +---------------+---------------+
	 * | result type   | result ...    |
	 * +---------------+               |
	 * | ...                           |
	 */
	CONNECTION_DATA,

	/*
	 * CONNECTION_ERROR:
	 * Indicates that a command has failed. The message body will contain a
	 * string describing the error.
	 *
	 * 0               32              64
	 * +---------------+---------------+
	 * | message type  | length        |
	 * +---------------+---------------+
	 * | error message ...             |
	 */
	CONNECTION_ERROR,

	/*
	 * CONNECTION_LOG:
	 * Indicates an asynchronous log message. The message body will contain
	 * the message string providing informational or warning logs. Log
	 * messages may be sent to the client any time.
	 *
	 * 0               32              64
	 * +---------------+---------------+
	 * | message type  | length        |
	 * +---------------+---------------+
	 * | log message ...               |
	 */
	CONNECTION_LOG,
} sdb_conn_status_t;

/* accepted commands / state of the connection */
typedef enum {
	/*
	 * CONNECTION_IDLE:
	 * This is an internal state used for idle connections.
	 */
	CONNECTION_IDLE = 0,

	/*
	 * CONNECTION_PING:
	 * Check if the current connection is still alive. The server will reply
	 * with CONNECTION_OK if it was able to handle the command.
	 */
	CONNECTION_PING,

	/*
	 * CONNECTION_STARTUP:
	 * Setup of a client connection. The message body shall include the
	 * username of the user contacting the server. The server may then send
	 * further requests to the client for authentication (not implemented
	 * yet). Once the setup and authentication was successful, the server
	 * replies with CONNECTION_OK. Further information may be requested from
	 * the server using special messages specific to the authentication
	 * method. The server does not send any asynchronous messages before
	 * startup is complete.
	 */
	CONNECTION_STARTUP,

	/*
	 * Querying the server. On success, the server replies with
	 * CONNECTION_DATA.
	 *
	 * The command codes listed here are used, both, for sending a query to
	 * the server and to indicate the response type from a query in a DATA
	 * message.
	 */

	/*
	 * CONNECTION_QUERY:
	 * Execute a query in the server. The message body shall include a single
	 * query command as a text string. Multiple commands are ignored by the
	 * server entirely.
	 */
	CONNECTION_QUERY,

	/*
	 * CONNECTION_FETCH:
	 * Execute the 'FETCH' command in the server. The message body shall
	 * include the hostname of the host to be retrieved.
	 */
	CONNECTION_FETCH,

	/*
	 * CONNECTION_LIST:
	 * Execute the 'LIST' command in the server.
	 */
	CONNECTION_LIST,

	/*
	 * CONNECTION_LOOKUP:
	 * Execute the 'LOOKUP' command in the server. The message body shall
	 * include the conditional expression of the 'MATCHING' clause.
	 */
	CONNECTION_LOOKUP,

	/*
	 * CONNECTION_TIMESERIES:
	 * Execute the 'TIMESERIES' command in the server. This command is not yet
	 * supported on the wire. Use CONNECTION_QUERY instead.
	 */
	CONNECTION_TIMESERIES,

	/*
	 * Command subcomponents.
	 */

	/*
	 * CONNECTION_EXPR:
	 * A parsed expression. Only used internally.
	 */
	CONNECTION_EXPR,
} sdb_conn_state_t;

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ! SDB_FRONTEND_PROTO_H */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

