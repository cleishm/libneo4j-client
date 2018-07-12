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
#include "../src/uri.h"
#include <check.h>
#include <errno.h>


START_TEST (test_parse_full_uri)
{
    struct uri *uri = parse_uri("http://waitbutwhy.com:80/?s=procrastinate#why", NULL);
    ck_assert(uri != NULL);
    ck_assert_str_eq(uri->scheme, "http");
    ck_assert(uri->userinfo == NULL);
    ck_assert_str_eq(uri->hostname, "waitbutwhy.com");
    ck_assert_int_eq(uri->port, 80);
    ck_assert_str_eq(uri->path, "/");
    ck_assert_str_eq(uri->query, "s=procrastinate");
    ck_assert_str_eq(uri->fragment, "why");
    free_uri(uri);
}
END_TEST


START_TEST (test_parse_full_uri_with_userinfo)
{
    struct uri *uri = parse_uri("http://cleishm@waitbutwhy.com:80/?s=procrastinate#why", NULL);
    ck_assert(uri != NULL);
    ck_assert_str_eq(uri->scheme, "http");
    ck_assert_str_eq(uri->userinfo, "cleishm");
    ck_assert_str_eq(uri->hostname, "waitbutwhy.com");
    ck_assert_int_eq(uri->port, 80);
    ck_assert_str_eq(uri->path, "/");
    ck_assert_str_eq(uri->query, "s=procrastinate");
    ck_assert_str_eq(uri->fragment, "why");
    free_uri(uri);
}
END_TEST


START_TEST (test_parse_file_uri)
{
    struct uri *uri = parse_uri("file:///usr/lib/docs/", NULL);
    ck_assert(uri != NULL);
    ck_assert_str_eq(uri->scheme, "file");
    ck_assert(uri->userinfo == NULL);
    ck_assert(uri->hostname == NULL);
    ck_assert_int_eq(uri->port, -1);
    ck_assert_str_eq(uri->path, "/usr/lib/docs/");
    ck_assert(uri->query == NULL);
    ck_assert(uri->fragment == NULL);
    free_uri(uri);
}
END_TEST


START_TEST (test_parse_uri_with_ipv6_host)
{
    struct uri *uri = parse_uri("http://[2001:200:dff:fff1:216:3eff:feb1:44d7%43]:80/", NULL);
    ck_assert(uri != NULL);
    ck_assert_str_eq(uri->scheme, "http");
    ck_assert_str_eq(uri->hostname, "2001:200:dff:fff1:216:3eff:feb1:44d7%43");
    ck_assert_int_eq(uri->port, 80);
    ck_assert_str_eq(uri->path, "/");
    ck_assert(uri->query == NULL);
    ck_assert(uri->fragment == NULL);
    free_uri(uri);

    uri = parse_uri("http://[2001:200:dff:fff1:216:3eff:feb1:44d7]/", NULL);
    ck_assert(uri != NULL);
    ck_assert_str_eq(uri->scheme, "http");
    ck_assert_str_eq(uri->hostname, "2001:200:dff:fff1:216:3eff:feb1:44d7");
    ck_assert_int_eq(uri->port, -1);
    ck_assert_str_eq(uri->path, "/");
    ck_assert(uri->query == NULL);
    ck_assert(uri->fragment == NULL);
    free_uri(uri);
}
END_TEST


START_TEST (test_parse_uri_without_path)
{
    struct uri *uri = parse_uri("https://feelthebern.org:443", NULL);
    ck_assert(uri != NULL);
    ck_assert_str_eq(uri->scheme, "https");
    ck_assert(uri->userinfo == NULL);
    ck_assert_str_eq(uri->hostname, "feelthebern.org");
    ck_assert_int_eq(uri->port, 443);
    ck_assert_str_eq(uri->path, "");
    ck_assert(uri->query == NULL);
    ck_assert(uri->fragment == NULL);
    free_uri(uri);
}
END_TEST


START_TEST (test_parse_uri_without_port)
{
    struct uri *uri = parse_uri("http://waitbutwhy.com/2013/10/why-procrastinators-procrastinate.html", NULL);
    ck_assert(uri != NULL);
    ck_assert_str_eq(uri->scheme, "http");
    ck_assert(uri->userinfo == NULL);
    ck_assert_str_eq(uri->hostname, "waitbutwhy.com");
    ck_assert_int_eq(uri->port, -1);
    ck_assert_str_eq(uri->path, "/2013/10/why-procrastinators-procrastinate.html");
    ck_assert(uri->query == NULL);
    ck_assert(uri->fragment == NULL);
    free_uri(uri);

    uri = parse_uri("http://waitbutwhy.com:/2013/10/why-procrastinators-procrastinate.html", NULL);
    ck_assert(uri != NULL);
    ck_assert_str_eq(uri->scheme, "http");
    ck_assert(uri->userinfo == NULL);
    ck_assert_str_eq(uri->hostname, "waitbutwhy.com");
    ck_assert_int_eq(uri->port, -1);
    ck_assert_str_eq(uri->path, "/2013/10/why-procrastinators-procrastinate.html");
    ck_assert(uri->query == NULL);
    ck_assert(uri->fragment == NULL);
    free_uri(uri);
}
END_TEST


START_TEST (test_parse_uri_without_port_or_path)
{
    struct uri *uri = parse_uri("http://berniesanders.com", NULL);
    ck_assert(uri != NULL);
    ck_assert_str_eq(uri->scheme, "http");
    ck_assert(uri->userinfo == NULL);
    ck_assert_str_eq(uri->hostname, "berniesanders.com");
    ck_assert_int_eq(uri->port, -1);
    ck_assert_str_eq(uri->path, "");
    ck_assert(uri->query == NULL);
    ck_assert(uri->fragment == NULL);
    free_uri(uri);
}
END_TEST


START_TEST (test_parse_uri_without_path_and_with_query)
{
    struct uri *uri = parse_uri("http://slowtravelberlin.com?q=bestbars", NULL);
    ck_assert(uri != NULL);
    ck_assert_str_eq(uri->scheme, "http");
    ck_assert(uri->userinfo == NULL);
    ck_assert_str_eq(uri->hostname, "slowtravelberlin.com");
    ck_assert_int_eq(uri->port, -1);
    ck_assert_str_eq(uri->path, "");
    ck_assert_str_eq(uri->query, "q=bestbars");
    ck_assert(uri->fragment == NULL);
    free_uri(uri);
}
END_TEST


START_TEST (test_parse_uri_without_query_and_with_fragment)
{
    struct uri *uri = parse_uri("http://slowtravelberlin.com/#bestbars", NULL);
    ck_assert(uri != NULL);
    ck_assert_str_eq(uri->scheme, "http");
    ck_assert(uri->userinfo == NULL);
    ck_assert_str_eq(uri->hostname, "slowtravelberlin.com");
    ck_assert_int_eq(uri->port, -1);
    ck_assert_str_eq(uri->path, "/");
    ck_assert(uri->query == NULL);
    ck_assert_str_eq(uri->fragment, "bestbars");
    free_uri(uri);
}
END_TEST


START_TEST (test_parse_uri_with_null_uri)
{
    const char *endptr = "test";
    struct uri *uri = parse_uri(NULL, &endptr);
    ck_assert(uri == NULL);
    ck_assert_int_eq(errno, EINVAL);
    ck_assert(endptr == NULL);
}
END_TEST


START_TEST (test_parse_uri_with_empty_uri)
{
    const char *endptr = "test";
    struct uri *uri = parse_uri("", &endptr);
    ck_assert(uri == NULL);
    ck_assert_int_eq(errno, EINVAL);
    ck_assert(*endptr == '\0');
}
END_TEST


START_TEST (test_parse_uri_with_no_scheme)
{
    const char *endptr = "test";
    struct uri *uri = parse_uri("//slowtravelberlin.com:80/", &endptr);
    ck_assert(uri == NULL);
    ck_assert_int_eq(errno, EINVAL);
    ck_assert(*endptr == '/');

    uri = parse_uri("://slowtravelberlin.com:80/", &endptr);
    ck_assert(uri == NULL);
    ck_assert_int_eq(errno, EINVAL);
    ck_assert(*endptr == ':');

    uri = parse_uri("bernie", NULL);
    ck_assert(uri == NULL);
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST


START_TEST (test_parse_uri_with_no_slash)
{
    const char *endptr = "test";
    struct uri *uri = parse_uri("http:/docs/", &endptr);
    ck_assert(uri == NULL);
    ck_assert_int_eq(errno, EINVAL);
    ck_assert(*endptr == 'd');

    uri = parse_uri("http:docs/", &endptr);
    ck_assert(uri == NULL);
    ck_assert_int_eq(errno, EINVAL);
    ck_assert(*endptr == 'd');
}
END_TEST


START_TEST (test_parse_uri_with_no_host)
{
    struct uri *uri = parse_uri("http://:80/docs/", NULL);
    ck_assert(uri != NULL);
    ck_assert_str_eq(uri->scheme, "http");
    ck_assert(uri->userinfo == NULL);
    ck_assert(uri->hostname == NULL);
    ck_assert_int_eq(uri->port, 80);
    ck_assert_str_eq(uri->path, "/docs/");
    ck_assert(uri->query == NULL);
    ck_assert(uri->fragment == NULL);
    free_uri(uri);
}
END_TEST


START_TEST (test_parse_uri_with_invalid_host)
{
    const char *endptr = "test";
    struct uri *uri = parse_uri("http://bernie$sanders.com:80/support/", &endptr);
    ck_assert(uri == NULL);
    ck_assert_int_eq(errno, EINVAL);
    ck_assert(*endptr == '$');
}
END_TEST


START_TEST (test_parse_uri_with_invalid_ipv6_host)
{
    const char *endptr = "test";
    struct uri *uri = parse_uri("http://[2001:xx::]/", &endptr);
    ck_assert(uri == NULL);
    ck_assert_int_eq(errno, EINVAL);
    ck_assert(*endptr == 'x');
}
END_TEST


START_TEST (test_parse_uri_with_invalid_port)
{
    const char *endptr = "test";
    struct uri *uri = parse_uri("http://slowtravelberlin.com:boring/", &endptr);
    ck_assert(uri == NULL);
    ck_assert_int_eq(errno, EINVAL);
    ck_assert(*endptr == 'b');
}
END_TEST


START_TEST (test_parse_uri_with_invalid_path)
{
    const char *endptr = "test";
    struct uri *uri = parse_uri("http://berniesanders.com/big business", &endptr);
    ck_assert(uri == NULL);
    ck_assert_int_eq(errno, EINVAL);
    ck_assert(*endptr == ' ');
}
END_TEST


TCase* uri_tcase(void)
{
    TCase *tc = tcase_create("uri");
    tcase_add_test(tc, test_parse_full_uri);
    tcase_add_test(tc, test_parse_full_uri_with_userinfo);
    tcase_add_test(tc, test_parse_file_uri);
    tcase_add_test(tc, test_parse_uri_with_ipv6_host);
    tcase_add_test(tc, test_parse_uri_without_path);
    tcase_add_test(tc, test_parse_uri_without_port);
    tcase_add_test(tc, test_parse_uri_without_port_or_path);
    tcase_add_test(tc, test_parse_uri_without_path_and_with_query);
    tcase_add_test(tc, test_parse_uri_without_query_and_with_fragment);
    tcase_add_test(tc, test_parse_uri_with_null_uri);
    tcase_add_test(tc, test_parse_uri_with_empty_uri);
    tcase_add_test(tc, test_parse_uri_with_no_scheme);
    tcase_add_test(tc, test_parse_uri_with_no_slash);
    tcase_add_test(tc, test_parse_uri_with_no_host);
    tcase_add_test(tc, test_parse_uri_with_invalid_host);
    tcase_add_test(tc, test_parse_uri_with_invalid_ipv6_host);
    tcase_add_test(tc, test_parse_uri_with_invalid_port);
    tcase_add_test(tc, test_parse_uri_with_invalid_path);
    return tc;
}
