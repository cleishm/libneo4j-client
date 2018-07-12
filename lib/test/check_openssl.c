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
#include "../src/openssl_iostream.h"
#include "../src/iostream.h"
#include "../src/util.h"
#include "memiostream.h"
#include <check.h>
#include <errno.h>


static ring_buffer_t *rcv_rb;
static ring_buffer_t *snd_rb;
static neo4j_iostream_t *sink;
static neo4j_config_t *config;


static void setup(void)
{
    rcv_rb = rb_alloc(32);
    ck_assert(rcv_rb != NULL);
    snd_rb = rb_alloc(32);
    ck_assert(snd_rb != NULL);

    sink = neo4j_memiostream(rcv_rb, snd_rb);
    config = neo4j_new_config();
}


static void teardown(void)
{
    neo4j_config_free(config);
    if (sink != NULL)
    {
        neo4j_ios_close(sink);
    }
    rb_free(snd_rb);
    rb_free(rcv_rb);
}


START_TEST (server_refuses_handshake)
{
    neo4j_iostream_t *ios = neo4j_openssl_iostream(sink, "", 7687, config, 0);
    ck_assert(ios == NULL);
    ck_assert_int_eq(errno, NEO4J_NO_SERVER_TLS_SUPPORT);
}
END_TEST


TCase* openssl_tcase(void)
{
    TCase *tc = tcase_create("openssl");
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, server_refuses_handshake);
    return tc;
}
