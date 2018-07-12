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
#include "../src/tofu.h"
#include "util.h"
#include <check.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>

static neo4j_config_t *config;
static char known_hosts[256];

static void setup(void)
{
    FILE *f = check_tmpfile(known_hosts, sizeof(known_hosts),
            "known_hosts_XXXXXX");
    ck_assert_ptr_ne(f, NULL);
    fputs("host.local:6546 aa7b6261e21d7b2950e044453543bce3840429e2\r\n", f);
    fputs("  host2.local:6546   aa7b6261e21d7b2950e044453543bce3840429e2\r\n", f);
    fputs("#host3.local:6546 aa7b6261e21d7b2950e044453543bce3840429e2\r\n", f);
    fclose(f);

    config = neo4j_new_config();
    ck_assert_int_eq(neo4j_config_set_known_hosts_file(config, known_hosts), 0);
}


static void teardown(void)
{
    neo4j_config_free(config);
}


struct callback_data
{
    char host[256];
    char fingerprint[60];
    neo4j_unverified_host_reason_t reason;
};


static void update_callback_data(struct callback_data *data,
        const char *host, const char *fingerprint,
        neo4j_unverified_host_reason_t reason)
{
    strncpy(data->host, host, sizeof(data->host));
    data->host[sizeof(data->host) - 1] = '\0';
    strncpy(data->fingerprint, fingerprint, sizeof(data->fingerprint));
    data->fingerprint[sizeof(data->fingerprint) - 1] = '\0';
    data->reason = reason;
}


static int reject_host(void *userdata, const char *host,
        const char *fingerprint, neo4j_unverified_host_reason_t reason)
{
    update_callback_data((struct callback_data *)userdata,
            host, fingerprint, reason);
    return NEO4J_HOST_VERIFICATION_REJECT;
}


static int accept_host(void *userdata, const char *host,
        const char *fingerprint, neo4j_unverified_host_reason_t reason)
{
    update_callback_data((struct callback_data *)userdata,
            host, fingerprint, reason);
    return NEO4J_HOST_VERIFICATION_ACCEPT_ONCE;
}


static int trust_host(void *userdata, const char *host,
        const char *fingerprint, neo4j_unverified_host_reason_t reason)
{
    update_callback_data((struct callback_data *)userdata,
            host, fingerprint, reason);
    return NEO4J_HOST_VERIFICATION_TRUST;
}


START_TEST (test_finds_trusted_host)
{
    int r = neo4j_check_known_hosts("host.local", 6546,
            "aa7b6261e21d7b2950e044453543bce3840429e2", config, 0);
    ck_assert_int_eq(r, 0);
}
END_TEST


START_TEST (test_finds_trusted_host_with_indent)
{
    int r = neo4j_check_known_hosts("host2.local", 6546,
            "aa7b6261e21d7b2950e044453543bce3840429e2", config, 0);
    ck_assert_int_eq(r, 0);
}
END_TEST


START_TEST (test_unfound_host_with_no_callback_registered)
{
    int r = neo4j_check_known_hosts("unknown.local", 6546,
            "aa7b6261e21d7b2950e044453543bce3840429e2", config, 0);
    ck_assert_int_eq(r, 1);
}
END_TEST


START_TEST (test_commented_host)
{
    int r = neo4j_check_known_hosts("host3.local", 6546,
            "aa7b6261e21d7b2950e044453543bce3840429e2", config, 0);
    ck_assert_int_eq(r, 1);
}
END_TEST


START_TEST (test_mismatch_host_with_no_callback_registered)
{
    int r = neo4j_check_known_hosts("host.local", 6546,
            "ffffff61e21d7b2950e044453543bce3840429e2", config, 0);
    ck_assert_int_eq(r, 1);
}
END_TEST


START_TEST (test_unfound_host_invokes_callback_and_rejects)
{
    struct callback_data data;
    neo4j_config_set_unverified_host_callback(config, reject_host, &data);
    int r = neo4j_check_known_hosts("unknown.local", 6546,
            "aa7b6261e21d7b2950e044453543bce3840429e2", config, 0);
    ck_assert_str_eq(data.host, "unknown.local:6546");
    ck_assert_str_eq(data.fingerprint,
            "aa7b6261e21d7b2950e044453543bce3840429e2");
    ck_assert_int_eq(data.reason, NEO4J_HOST_VERIFICATION_UNRECOGNIZED);
    ck_assert_int_eq(r, 2);
}
END_TEST


START_TEST (test_mismatch_host_invokes_callback_and_rejects)
{
    struct callback_data data;
    neo4j_config_set_unverified_host_callback(config, reject_host, &data);
    int r = neo4j_check_known_hosts("host.local", 6546,
            "ffffff61e21d7b2950e044453543bce3840429e2", config, 0);
    ck_assert_str_eq(data.host, "host.local:6546");
    ck_assert_str_eq(data.fingerprint,
            "ffffff61e21d7b2950e044453543bce3840429e2");
    ck_assert_int_eq(data.reason, NEO4J_HOST_VERIFICATION_MISMATCH);
    ck_assert_int_eq(r, 2);
}
END_TEST


START_TEST (test_unfound_host_invokes_callback_and_accepts_once)
{
    struct callback_data data;
    neo4j_config_set_unverified_host_callback(config, accept_host, &data);
    int r = neo4j_check_known_hosts("unknown.local", 6546,
            "aa7b6261e21d7b2950e044453543bce3840429e2", config, 0);
    ck_assert_str_eq(data.host, "unknown.local:6546");
    ck_assert_str_eq(data.fingerprint,
            "aa7b6261e21d7b2950e044453543bce3840429e2");
    ck_assert_int_eq(data.reason, NEO4J_HOST_VERIFICATION_UNRECOGNIZED);
    ck_assert_int_eq(r, 0);

    neo4j_config_set_unverified_host_callback(config, NULL, NULL);
    r = neo4j_check_known_hosts("unknown.local", 6546,
            "aa7b6261e21d7b2950e044453543bce3840429e2", config, 0);
    ck_assert_int_eq(r, 1);

    r = neo4j_check_known_hosts("host.local", 6546,
            "aa7b6261e21d7b2950e044453543bce3840429e2", config, 0);
    ck_assert_int_eq(r, 0);
}
END_TEST


START_TEST (test_mismatch_host_invokes_callback_and_accepts_once)
{
    struct callback_data data;
    neo4j_config_set_unverified_host_callback(config, accept_host, &data);
    int r = neo4j_check_known_hosts("host.local", 6546,
            "ffffff61e21d7b2950e044453543bce3840429e2", config, 0);
    ck_assert_str_eq(data.host, "host.local:6546");
    ck_assert_str_eq(data.fingerprint,
            "ffffff61e21d7b2950e044453543bce3840429e2");
    ck_assert_int_eq(data.reason, NEO4J_HOST_VERIFICATION_MISMATCH);
    ck_assert_int_eq(r, 0);

    neo4j_config_set_unverified_host_callback(config, NULL, NULL);
    r = neo4j_check_known_hosts("host.local", 6546,
            "ffffff61e21d7b2950e044453543bce3840429e2", config, 0);
    ck_assert_int_eq(r, 1);

    r = neo4j_check_known_hosts("host.local", 6546,
            "aa7b6261e21d7b2950e044453543bce3840429e2", config, 0);
    ck_assert_int_eq(r, 0);
}
END_TEST


START_TEST (test_unfound_host_invokes_callback_and_trusts)
{
    struct callback_data data;
    neo4j_config_set_unverified_host_callback(config, trust_host, &data);
    int r = neo4j_check_known_hosts("unknown.local", 6546,
            "aa7b6261e21d7b2950e044453543bce3840429e2", config, 0);
    ck_assert_str_eq(data.host, "unknown.local:6546");
    ck_assert_str_eq(data.fingerprint,
            "aa7b6261e21d7b2950e044453543bce3840429e2");
    ck_assert_int_eq(data.reason, NEO4J_HOST_VERIFICATION_UNRECOGNIZED);
    ck_assert_int_eq(r, 0);

    neo4j_config_set_unverified_host_callback(config, NULL, NULL);
    r = neo4j_check_known_hosts("unknown.local", 6546,
            "aa7b6261e21d7b2950e044453543bce3840429e2", config, 0);
    ck_assert_int_eq(r, 0);

    r = neo4j_check_known_hosts("host.local", 6546,
            "aa7b6261e21d7b2950e044453543bce3840429e2", config, 0);
    ck_assert_int_eq(r, 0);
}
END_TEST


START_TEST (test_mismatch_host_invokes_callback_and_trusts)
{
    struct callback_data data;
    neo4j_config_set_unverified_host_callback(config, trust_host, &data);
    int r = neo4j_check_known_hosts("host.local", 6546,
            "ffffff61e21d7b2950e044453543bce3840429e2", config, 0);
    ck_assert_str_eq(data.host, "host.local:6546");
    ck_assert_str_eq(data.fingerprint,
            "ffffff61e21d7b2950e044453543bce3840429e2");
    ck_assert_int_eq(data.reason, NEO4J_HOST_VERIFICATION_MISMATCH);
    ck_assert_int_eq(r, 0);

    neo4j_config_set_unverified_host_callback(config, NULL, NULL);
    r = neo4j_check_known_hosts("host.local", 6546,
            "ffffff61e21d7b2950e044453543bce3840429e2", config, 0);
    ck_assert_int_eq(r, 0);

    r = neo4j_check_known_hosts("host.local", 6546,
            "aa7b6261e21d7b2950e044453543bce3840429e2", config, 0);
    ck_assert_int_eq(r, 1);
}
END_TEST


START_TEST (test_trust_creates_known_hosts_file_and_directory)
{
    char dir[1024];
    int r = check_tmpdir(dir, sizeof(dir), ".neo4j_XXXXXX");
    ck_assert_int_eq(r, 0);

    char path[1024];
    const char *kh_path = "/sub/dir/kh";
    size_t dirlen = strlen(dir);
    ck_assert_int_lt(dirlen + strlen(kh_path), sizeof(path));
    memcpy(path, dir, dirlen);
    strncpy(path + dirlen, kh_path, sizeof(path) - dirlen);

    ck_assert_int_eq(neo4j_config_set_known_hosts_file(config, path), 0);

    struct callback_data data;
    neo4j_config_set_unverified_host_callback(config, trust_host, &data);
    r = neo4j_check_known_hosts("host.local", 6546,
            "aa7b6261e21d7b2950e044453543bce3840429e2", config, 0);
    ck_assert_str_eq(data.host, "host.local:6546");
    ck_assert_str_eq(data.fingerprint,
            "aa7b6261e21d7b2950e044453543bce3840429e2");
    ck_assert_int_eq(data.reason, NEO4J_HOST_VERIFICATION_UNRECOGNIZED);
    ck_assert_int_eq(r, 0);

    neo4j_config_set_unverified_host_callback(config, NULL, NULL);
    r = neo4j_check_known_hosts("host.local", 6546,
            "aa7b6261e21d7b2950e044453543bce3840429e2", config, 0);
    ck_assert_int_eq(r, 0);
}
END_TEST



TCase* tofu_tcase(void)
{
    TCase *tc = tcase_create("tofu");
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_finds_trusted_host);
    tcase_add_test(tc, test_finds_trusted_host_with_indent);
    tcase_add_test(tc, test_unfound_host_with_no_callback_registered);
    tcase_add_test(tc, test_commented_host);
    tcase_add_test(tc, test_mismatch_host_with_no_callback_registered);
    tcase_add_test(tc, test_unfound_host_invokes_callback_and_rejects);
    tcase_add_test(tc, test_mismatch_host_invokes_callback_and_rejects);
    tcase_add_test(tc, test_unfound_host_invokes_callback_and_accepts_once);
    tcase_add_test(tc, test_mismatch_host_invokes_callback_and_accepts_once);
    tcase_add_test(tc, test_unfound_host_invokes_callback_and_trusts);
    tcase_add_test(tc, test_mismatch_host_invokes_callback_and_trusts);
    tcase_add_test(tc, test_trust_creates_known_hosts_file_and_directory);
    return tc;
}
