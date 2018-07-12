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
#include "../src/client_config.h"
#include <check.h>
#include <errno.h>


START_TEST (test_neo4j_config_create_and_release)
{
    neo4j_config_t *config = neo4j_new_config();
    ck_assert(config != NULL);
    neo4j_config_free(config);
}
END_TEST


TCase* config_tcase(void)
{
    TCase *tc = tcase_create("config");
    tcase_add_test(tc, test_neo4j_config_create_and_release);
    return tc;
}
