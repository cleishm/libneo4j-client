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

START_TEST (test_neo4j_config_set_supported_versions)
{
  neo4j_config_t *config = neo4j_new_config();
  ck_assert(config->supported_versions != NULL);
  ck_assert_str_eq
    (neo4j_config_get_supported_versions(config),"5.6-5.2 4.0 4.4-4.1 3.0 ");
  ck_assert_int_eq
    (neo4j_config_set_supported_versions(config, "5.4"),0);
  ck_assert_int_eq
    (neo4j_config_set_supported_versions(config, "5.6-5.1,4.3-4,3,2"),0);
  ck_assert_str_eq
    (neo4j_config_get_supported_versions(config), "5.6-5.1 4.3-4.0 3.0 2.0 ");
  ck_assert_int_eq
    (config->supported_versions[0].major, 5);
  ck_assert_int_eq
    (config->supported_versions[0].minor, 6);
  ck_assert_int_eq
    (config->supported_versions[0].and_lower, 5);
  ck_assert_int_eq
    (config->supported_versions[1].major, 4);
  ck_assert_int_eq
    (config->supported_versions[1].minor, 3);
  ck_assert_int_eq
    (config->supported_versions[1].and_lower, 3);
  ck_assert_int_eq
    (config->supported_versions[2].major, 3);
  ck_assert_int_eq
    (config->supported_versions[2].minor, 0);
  ck_assert_int_eq
    (config->supported_versions[2].and_lower, 0);
  ck_assert_int_eq
    (config->supported_versions[3].major, 2);
  ck_assert_int_eq
    (config->supported_versions[3].minor, 0);
  ck_assert_int_eq
    (config->supported_versions[3].and_lower, 0);

		   
  ck_assert_int_eq
    (neo4j_config_set_supported_versions(config, "5.6,6.3-6.1,4"),0);
  ck_assert_str_eq
    (neo4j_config_get_supported_versions(config), "5.6 6.3-6.1 4.0 ");
  ck_assert_int_eq
    (neo4j_config_set_supported_versions(config, "5.4,4.3-4,3,crap"),-1);
  ck_assert_str_eq
    (neo4j_config_get_supported_versions(config), "5.6-5.2 4.0 4.4-4.1 3.0 ");
  

  

}
END_TEST

TCase* config_tcase(void)
{
    TCase *tc = tcase_create("config");
    tcase_add_test(tc, test_neo4j_config_create_and_release);
    tcase_add_test(tc, test_neo4j_config_set_supported_versions);
    return tc;
}
