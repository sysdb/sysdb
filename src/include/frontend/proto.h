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
 * Any strings in the message body may not include a zero byte.
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
	 * Generic status and log messages.
	 */

	/*
	 * SDB_CONNECTION_OK:
	 * Indicates that a command was successful. The message body will usually
	 * be empty but may contain a string providing unformatted information
	 * providing more details.
	 *
	 * 0               32              64
	 * +---------------+---------------+
	 * | OK            | len(msg)      |
	 * +---------------+---------------+
	 * | optional status message ...   |
	 */
	SDB_CONNECTION_OK = 0,

	/*
	 * SDB_CONNECTION_ERROR:
	 * Indicates that a command has failed. The message body will contain a
	 * string describing the error.
	 *
	 * 0               32              64
	 * +---------------+---------------+
	 * | ERROR         | len(msg)      |
	 * +---------------+---------------+
	 * | error message ...             |
	 */
	SDB_CONNECTION_ERROR,

	/*
	 * SDB_CONNECTION_LOG:
	 * Indicates an asynchronous log message. The message body will contain
	 * the log priority (see utils/error.h) and message. Log messages may be
	 * sent to the client any time.
	 *
	 * 0               32              64
	 * +---------------+---------------+
	 * | LOG           | length        |
	 * +---------------+---------------+
	 * | log priority  | log message   |
	 * +---------------+               |
	 * | ...                           |
	 */
	SDB_CONNECTION_LOG,

	/*
	 * Query-result specific messages.
	 */

	/*
	 * SDB_CONNECTION_DATA:
	 * Indicates that a data query was successful. The message body will
	 * contain the type of the data and the result encoded as a JSON string.
	 * The type is the same as the command code of the respective command (see
	 * below) and is stored as an unsigned 32bit integer in network
	 * byte-order. The result may be empty (but the type is still included) if
	 * the query did not return any result. The type and the result message
	 * are empty on empty commands.
	 *
	 * 0               32              64
	 * +---------------+---------------+
	 * | DATA          | length        |
	 * +---------------+---------------+
	 * | result type   | result ...    |
	 * +---------------+               |
	 * | ...                           |
	 */
	SDB_CONNECTION_DATA = 100,
} sdb_conn_status_t;

/* accepted commands / state of the connection */
typedef enum {
	/*
	 * SDB_CONNECTION_IDLE:
	 * This is an internal state used for idle connections.
	 */
	SDB_CONNECTION_IDLE = 0,

	/*
	 * SDB_CONNECTION_PING:
	 * Check if the current connection is still alive. The server will reply
	 * with SDB_CONNECTION_OK and an empty message body if it was able to
	 * handle the command.
	 *
	 * 0               32              64
	 * +---------------+---------------+
	 * | PING          | 0             |
	 * +---------------+---------------+
	 */
	SDB_CONNECTION_PING,

	/*
	 * SDB_CONNECTION_STARTUP:
	 * Setup of a client connection. The message body shall include the
	 * username of the user contacting the server. The server may then send
	 * further requests to the client for authentication (not implemented
	 * yet). Once the setup and authentication was successful, the server
	 * replies with SDB_CONNECTION_OK. Further information may be requested
	 * from the server using special messages specific to the authentication
	 * method. The server does not send any asynchronous messages before
	 * startup is complete.
	 *
	 * 0               32              64
	 * +---------------+---------------+
	 * | STARTUP       | len(username) |
	 * +---------------+---------------+
	 * | username ...                  |
	 */
	SDB_CONNECTION_STARTUP,

	/*
	 * Querying the server. On success, the server replies with
	 * SDB_CONNECTION_DATA.
	 *
	 * The command codes listed here are used, both, for sending a query to
	 * the server and to indicate the response type from a query in a DATA
	 * message.
	 */

	/*
	 * SDB_CONNECTION_QUERY:
	 * Execute a query in the server. The message body shall include a single
	 * query command as a text string. Multiple commands are ignored by the
	 * server entirely to protect against injection attacks.
	 *
	 * 0               32              64
	 * +---------------+---------------+
	 * | QUERY         | len(query)    |
	 * +---------------+---------------+
	 * | query string ...              |
	 */
	SDB_CONNECTION_QUERY,

	/*
	 * SDB_CONNECTION_FETCH:
	 * Execute the 'FETCH' command in the server. The message body shall
	 * include the type and the identifier of the object to be retrieved.
	 * Hosts are identified by their name. Other types are not supported yet.
	 * The type is encoded as a 32bit integer in network byte-order.
	 *
	 * 0               32              64
	 * +---------------+---------------+
	 * | FETCH         | length        |
	 * +---------------+---------------+
	 * | object type   | identifier    |
	 * +---------------+               |
	 * | ...                           |
	 */
	SDB_CONNECTION_FETCH,

	/*
	 * SDB_CONNECTION_LIST:
	 * Execute the 'LIST' command in the server. The message body may include
	 * the type of the objects to be listed, encoded as a 32bit integer in
	 * network byte-order. The response includes all hosts and the respective
	 * child objects. By default, all hosts are listed.
	 *
	 * 0               32              64
	 * +---------------+---------------+
	 * | LIST          | length        |
	 * +---------------+---------------+
	 * | [object type] |
	 * +---------------+
	 */
	SDB_CONNECTION_LIST,

	/*
	 * SDB_CONNECTION_LOOKUP:
	 * Execute the 'LOOKUP' command in the server. The message body shall
	 * include the type of the objects to look up and a string representing
	 * the conditional expression of the 'MATCHING' clause. The type is
	 * encoded as a 32bit integer in network byte-order.
	 *
	 * 0               32              64
	 * +---------------+---------------+
	 * | LOOKUP        | length        |
	 * +---------------+---------------+
	 * | object type   | matching      |
	 * +---------------+               |
	 * | clause ...                    |
	 */
	SDB_CONNECTION_LOOKUP,

	/*
	 * SDB_CONNECTION_TIMESERIES:
	 * Execute the 'TIMESERIES' command in the server. This command is not yet
	 * supported on the wire. Use SDB_CONNECTION_QUERY instead.
	 */
	SDB_CONNECTION_TIMESERIES,

	/*
	 * SDB_CONNECTION_STORE:
	 * Execute the 'STORE' command in the server. The message body shall
	 * include the type of the object to be stored, the timestamp of the last
	 * update, and a list of fields describing the object depending on the
	 * object type. Object types are encoded as 32bit integers in network
	 * byte-order where attribute types are bitwise ORed with the appropriate
	 * parent object type. Timestamps are encoded as 64bit integers in network
	 * byte-order. Fields are null-terminated strings.
	 *
	 * 0               32              64
	 * +---------------+---------------+
	 * | STORE         | length        |
	 * +---------------+---------------+
	 * | object type   | last_update.. |
	 * +---------------+---------------+
	 * | ...           | fields        |
	 * +---------------+               |
	 * | ...                           |
	 *
	 * Fields:
	 *
	 *      HOST: name
	 *   SERVICE: hostname, name
	 *    METRIC: hostname, name, [store type, store id]
	 * ATTRIBUTE: [hostname], parent object name, key, <value>
	 *
	 * Values are encoded as their type (32bit integer in network byte-order),
	 * and their content as implemented by sdb_proto_marshal_data.
	 */
	SDB_CONNECTION_STORE = 50,

	/* Only used internally: */
	SDB_CONNECTION_STORE_HOST,
	SDB_CONNECTION_STORE_SERVICE,
	SDB_CONNECTION_STORE_METRIC,
	SDB_CONNECTION_STORE_ATTRIBUTE,

	/*
	 * Command subcomponents.
	 */

	/*
	 * SDB_CONNECTION_MATCHER:
	 * A parsed matcher. Only used internally.
	 */
	SDB_CONNECTION_MATCHER = 100,

	/*
	 * SDB_CONNECTION_EXPR:
	 * A parsed expression. Only used internally.
	 */
	SDB_CONNECTION_EXPR,
} sdb_conn_state_t;

#define SDB_CONN_MSGTYPE_TO_STRING(t) \
	(((t) == SDB_CONNECTION_IDLE) ? "IDLE" \
		: ((t) == SDB_CONNECTION_PING) ? "PING" \
		: ((t) == SDB_CONNECTION_STARTUP) ? "STARTUP" \
		: ((t) == SDB_CONNECTION_QUERY) ? "QUERY" \
		: ((t) == SDB_CONNECTION_FETCH) ? "FETCH" \
		: ((t) == SDB_CONNECTION_LIST) ? "LIST" \
		: ((t) == SDB_CONNECTION_LOOKUP) ? "LOOKUP" \
		: ((t) == SDB_CONNECTION_TIMESERIES) ? "TIMESERIES" \
		: ((t) == SDB_CONNECTION_STORE) ? "STORE" \
		: "UNKNOWN")

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ! SDB_FRONTEND_PROTO_H */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

