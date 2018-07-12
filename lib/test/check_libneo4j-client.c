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
#include "../src/neo4j-client.h"
#include "util.h"
#include <check.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

Suite *libneo4j_client_suite(void);


int main(void)
{
    unsigned int seed = random();
    printf("Initialising check using random seed: %d\n", seed);
    srandom(seed);

    char tmpdir_buf[1024];
    tmpdir_buf[0] = '\0';
    char *tdir;
    if ((tdir = getenv("CHECK_TMPDIR")) == NULL)
    {
        if (create_tmpdir(tmpdir_buf, sizeof(tmpdir_buf)))
        {
            perror("Failed to create temporary directory");
            exit(EXIT_FAILURE);
        }
        if (setenv("CHECK_TMPDIR", tmpdir_buf, 1))
        {
            perror("Failed to set CHECK_TEMPDIR");
            exit(EXIT_FAILURE);
        }
        tdir = tmpdir_buf;
    }
    if (neo4j_mkdir_p(tdir))
    {
        fprintf(stderr, "Failed to create '%s': %s\n", tdir, strerror(errno));
        exit(EXIT_FAILURE);
    }
    printf("CHECK_TEMPDIR=\"%s\"%s\n", tdir,
            (tmpdir_buf[0] == '\0')? "" : " (autocleaned)");

    neo4j_client_init();

    int number_failed;
    Suite *s = libneo4j_client_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "results.xml");
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    neo4j_client_cleanup();

    if (tmpdir_buf[0] != '\0')
    {
        if (rm_rf(tmpdir_buf))
        {
            exit(EXIT_FAILURE);
        }
    }
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
