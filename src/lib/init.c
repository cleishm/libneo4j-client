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
#ifdef HAVE_OPENSSL
#include "openssl.h"
#endif
#include "thread.h"
#include <errno.h>


static void do_init(void);
static void do_cleanup(void);

static int init_errno;
static int cleanup_errno;


int neo4j_client_init(void)
{
    static neo4j_once_t once = NEO4J_ONCE_INIT;
    neo4j_thread_once(&once, do_init);
    if (init_errno != 0)
    {
        errno = init_errno;
        return -1;
    }
    return 0;
}


int neo4j_client_cleanup(void)
{
    static neo4j_once_t once = NEO4J_ONCE_INIT;
    neo4j_thread_once(&once, do_cleanup);
    if (cleanup_errno != 0)
    {
        errno = cleanup_errno;
        return -1;
    }
    return 0;
}


void do_init(void)
{
    init_errno = 0;
#ifdef HAVE_OPENSSL
    if (neo4j_openssl_init())
    {
        init_errno = errno;
    }
#endif
}


void do_cleanup(void)
{
    cleanup_errno = 0;
#ifdef HAVE_OPENSSL
    if (neo4j_openssl_cleanup())
    {
        cleanup_errno = errno;
    }
#endif
}


#if defined(__GNUC__) && !defined(NEO4J_NO_AUTOMATIC_GLOBALS)
static void init_constructor(void) __attribute__((constructor));
static void init_destructor(void) __attribute__((destructor));

static void init_constructor(void)
{
    neo4j_client_init();
}

static void init_destructor(void)
{
    neo4j_client_cleanup();
}
#endif
