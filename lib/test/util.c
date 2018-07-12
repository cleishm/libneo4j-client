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
#include "util.h"
#include <assert.h>
#include <errno.h>
#include <fts.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>


#define CHECK_TMPDIR_TEMPLATE "/check-XXXXXX"

ssize_t tmpfilename(char *buf, size_t n, const char *name);


int create_tmpdir(char *buf, size_t n)
{
    const char *dir = getenv("TMPDIR");
    if (dir == NULL)
    {
#ifdef P_tmpdir
        dir = P_tmpdir;
#else
        dir = "/tmp";
#endif
    }
    size_t c = strlen(dir);
    size_t tlen = strlen(CHECK_TMPDIR_TEMPLATE);
    if (n < c || (n - c) < (tlen + 1))
    {
        errno = ERANGE;
        return -1;
    }
    strcpy(buf, dir);
    strcpy(buf + c, CHECK_TMPDIR_TEMPLATE);
    return (mkdtemp(buf) == NULL)? -1 : 0;
}


FILE *check_tmpfile(char *buf, size_t n, const char *template)
{
    ssize_t len = tmpfilename(buf, n, template);
    if (len < 0)
    {
        return NULL;
    }

    int fd = mkstemp(buf);
    if (fd < 0)
    {
        return NULL;
    }
    return fdopen(fd, "w+");
}


int check_tmpdir(char *buf, size_t n, const char *template)
{
    ssize_t len = tmpfilename(buf, n, template);
    if (len < 0)
    {
        return -1;
    }

    return (mkdtemp(buf) == NULL)? -1 : 0;
}


ssize_t tmpfilename(char *buf, size_t n, const char *name)
{
    const char *dir = getenv("CHECK_TMPDIR");
    if (dir == NULL)
    {
        dir = getenv("TMPDIR");
    }
    if (dir == NULL || dir[0] == '\0')
    {
#ifdef P_tmpdir
        dir = P_tmpdir;
#else
        dir = "/tmp";
#endif
    }

    size_t dirlen = strlen(dir);
    size_t nlen = strlen(name);
    if ((dirlen + nlen + 2) > n)
    {
        errno = ERANGE;
        return -1;
    }

    memcpy(buf, dir, dirlen);
    buf[dirlen] = '/';
    memcpy(buf + dirlen + 1, name, nlen);
    buf[dirlen + nlen + 1] = '\0';
    return dirlen + nlen;
}


int rm_rf(char *path)
{
    char * const argv[] = { path, NULL };
    FTS *fts = fts_open(argv, FTS_PHYSICAL | FTS_NOSTAT, NULL);
    if (fts == NULL)
    {
        fprintf(stderr, "%s: %s\n", path, strerror(errno));
        return -1;
    }

    int result = 0;

    FTSENT *p;
    while ((p = fts_read(fts)) != NULL)
    {
        switch (p->fts_info)
        {
        case FTS_DNR:
            if (p->fts_errno == ENOENT)
            {
                continue;
            }
            // fall through
        case FTS_ERR:
            fprintf(stderr, "%s: %s\n", p->fts_path, strerror(p->fts_errno));
            result = -1;
            continue;
        case FTS_D:
            continue;
        }

        switch (p->fts_info)
        {
        case FTS_DP:
            if (!rmdir(p->fts_accpath))
            {
                continue;
            }
            break;
        default:
            if (!unlink(p->fts_accpath))
            {
                continue;
            }
            break;
        }
        if (errno != ENOENT)
        {
            fprintf(stderr, "%s: %s\n", p->fts_path, strerror(p->fts_errno));
            result = -1;
        }
    }

    if (errno != 0)
    {
        perror("fts_read");
        result = -1;
    }
    fts_close(fts);
    return result;
}
