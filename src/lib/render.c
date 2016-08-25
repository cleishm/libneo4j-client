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
#include "render.h"
#include <assert.h>
#if HAVE_LANGINFO_CODESET
#include <langinfo.h>
#endif


static const char *ascii_corners[] = { "+", "+", "+" };
static const char *ascii_rule = "-";
static const char *ascii_bar = "|";

#if HAVE_LANGINFO_CODESET
static const char *u8_top_corners[] = { u8"\u250C", u8"\u252C", u8"\u2510" };
static const char *u8_mid_corners[] = { u8"\u251C", u8"\u253C", u8"\u2524" };
static const char *u8_bot_corners[] = { u8"\u2514", u8"\u2534", u8"\u2518" };
static const char *u8_rule = u8"\u2500";
static const char *u8_bar = u8"\u2502";
#endif


uint_fast32_t normalize_render_flags(uint_fast32_t flags)
{
#if HAVE_LANGINFO_CODESET
    if ((flags & NEO4J_RENDER_ASCII) ||
        (strcmp(nl_langinfo(CODESET), "UTF-8") != 0))
    {
        flags |= NEO4J_RENDER_ASCII_ART;
    }
#else
    flags |= NEO4J_RENDER_ASCII_ART;
#endif
    return flags;
}


int render_line(FILE *stream, unsigned int ncolumns,
        unsigned int *widths, line_position_t position,
        bool undersize, uint_fast32_t flags)
{
    const char *rule = ascii_rule;
    const char **corners = ascii_corners;
#if HAVE_LANGINFO_CODESET
    if (!(flags & NEO4J_RENDER_ASCII_ART))
    {
        rule = u8_rule;
        switch (position)
        {
        case LINE_TOP:
            corners = u8_top_corners;
            break;
        case LINE_BOTTOM:
            corners = u8_bot_corners;
            break;
        default:
            corners = u8_mid_corners;
            break;
        }
    }
#endif
    for (unsigned int i = 0, corner = 0; i < ncolumns; ++i)
    {
        if (widths[i] == 0)
        {
            continue;
        }
        if (fputs(corners[corner], stream) == EOF)
        {
            return -1;
        }
        corner = 1;
        for (unsigned int w = widths[i]; w > 0; --w)
        {
            if (fputs(rule, stream) == EOF)
            {
                return -1;
            }
        }
    }
    if (fputs(corners[undersize? 1 : 2], stream) == EOF)
    {
        return -1;
    }
    if (undersize && fputs(rule, stream) == EOF)
    {
        return -1;
    }
    if (fputc('\n', stream) == EOF)
    {
        return -1;
    }
    return 0;
}


int render_row(FILE *stream, unsigned int ncolumns,
        unsigned int *widths, bool undersize, uint_fast32_t flags,
        render_row_callback_t callback, void *cdata)
{
    const char *bar = ascii_bar;
#if HAVE_LANGINFO_CODESET
    if (!(flags & NEO4J_RENDER_ASCII_ART))
    {
        bar = u8_bar;
    }
#endif
    for (unsigned int i = 0; i < ncolumns; ++i)
    {
        if (widths[i] == 0)
        {
            continue;
        }
        if (fputs(bar, stream) == EOF || fputc(' ', stream) == EOF)
        {
            return -1;
        }
        assert(widths[i] >= 2);
        int w = (callback != NULL)?
                callback(cdata, stream, i, widths[i] - 2) : 0;
        if (w < 0)
        {
            return -1;
        }
        for (; (unsigned int)w < widths[i] - 2; ++w)
        {
            if (fputc(' ', stream) == EOF)
            {
                return -1;
            }
        }
        if (fputc(((unsigned int)w > widths[i] - 2)? '=' : ' ', stream) == EOF)
        {
            return -1;
        }
    }
    if (fputs(bar, stream) == EOF)
    {
        return -1;
    }
    if (undersize && fputc('=', stream) == EOF)
    {
        return -1;
    }
    if (fputc('\n', stream) == EOF)
    {
        return -1;
    }
    return 0;
}
