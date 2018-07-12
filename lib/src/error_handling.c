/* vi:set ts=4 sw=4 expandtab:
 *
 * Copyright 2016, Chris Leishman (http://github.com/cleishm)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "../../config.h"
#include "neo4j-client.h"
#include "util.h"
#include <string.h>
#include <errno.h>


void neo4j_perror(FILE *stream, int errnum, const char *message)
{
    char buf[1024];
    fprintf(stream, "%s%s%s\n", (message != NULL)? message : "",
        (message != NULL)? ": " : "",
        neo4j_strerror(errnum, buf, sizeof(buf)));
}


const char *neo4j_strerror(int errnum, char *buf, size_t buflen)
{
    REQUIRE(buflen == 0 || buf != NULL, NULL);

    switch (errnum)
    {
    case NEO4J_UNEXPECTED_ERROR:
        return "Unexpected error";
    case NEO4J_INVALID_URI:
        return "Invalid URI";
    case NEO4J_UNKNOWN_URI_SCHEME:
        return "Unknown URI scheme";
    case NEO4J_UNKNOWN_HOST:
        return "Unknown host";
    case NEO4J_PROTOCOL_NEGOTIATION_FAILED:
        return "Could not agree on a protocol version";
    case NEO4J_INVALID_CREDENTIALS:
        return "Username or password is invalid";
    case NEO4J_CONNECTION_CLOSED:
        return "Connection closed";
    case NEO4J_SESSION_FAILED:
        return "Session has failed";
    case NEO4J_SESSION_ENDED:
        return "Session has ended";
    case NEO4J_UNCLOSED_RESULT_STREAM:
        return "Unclosed result stream";
    case NEO4J_STATEMENT_EVALUATION_FAILED:
        return "Statement evaluation failed";
    case NEO4J_STATEMENT_PREVIOUS_FAILURE:
        return "Statement ignored due to previously failed request";
    case NEO4J_TLS_NOT_SUPPORTED:
        return "Library has not been compiled with TLS support";
    case NEO4J_TLS_VERIFICATION_FAILED:
        return "Authenticity of the server cannot be established";
    case NEO4J_NO_SERVER_TLS_SUPPORT:
        return "Server does not support TLS";
    case NEO4J_SERVER_REQUIRES_SECURE_CONNECTION:
        return "Server requires a secure connection";
    case NEO4J_INVALID_MAP_KEY_TYPE:
        return "Map contains key of non-String type";
    case NEO4J_INVALID_LABEL_TYPE:
        return "Node/Relationship contains label of non-String type";
    case NEO4J_INVALID_PATH_NODE_TYPE:
        return "Path contains a node of non-Node type";
    case NEO4J_INVALID_PATH_RELATIONSHIP_TYPE:
        return "Path contains a relationship of non-Relationship type";
    case NEO4J_INVALID_PATH_SEQUENCE_LENGTH:
        return "Path contains an invalid sequence length";
    case NEO4J_INVALID_PATH_SEQUENCE_IDX_TYPE:
        return "Path contains a sequence index of non-Int type";
    case NEO4J_INVALID_PATH_SEQUENCE_IDX_RANGE:
        return "Path contains an out-of-range sequence index";
    case NEO4J_NO_PLAN_AVAILABLE:
        return "The server did not return a plan or profile";
    case NEO4J_AUTH_RATE_LIMIT:
        return "Too many authentication attempts - wait 5 seconds before trying again";
    case NEO4J_TLS_MALFORMED_CERTIFICATE:
        return "Server presented a malformed TLS certificate";
    case NEO4J_SESSION_RESET:
        return "Session has been reset";
    case NEO4J_SESSION_BUSY:
        return "Session cannot be accessed concurrently";
    default:
#ifdef STRERROR_R_CHAR_P
        return strerror_r(errnum, buf, buflen);
#else
        return (strerror_r(errnum, buf, buflen)) ? NULL : buf;
#endif
    }
}
