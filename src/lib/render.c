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


struct border_glifs
{
    const char *horizontal_line;
    const char *vertical_line;
    const char *top_corners[3];
    const char *middle_corners[3];
    const char *bottom_corners[3];
    const char *overflow;
};


static const struct border_glifs ascii_border_glifs =
    { .horizontal_line = "-",
      .vertical_line = "|",
      .top_corners = { "+", "+", "+" },
      .middle_corners = { "+", "+", "+" },
      .bottom_corners = { "+", "+", "+" },
      .overflow = "=" };

#if HAVE_LANGINFO_CODESET
static const struct border_glifs u8_border_glifs =
    { .horizontal_line = u8"\u2500",
      .vertical_line = u8"\u2502",
      .top_corners = { u8"\u250C", u8"\u252C", u8"\u2510" },
      .middle_corners = { u8"\u251C", u8"\u253C", u8"\u2524" },
      .bottom_corners = { u8"\u2514", u8"\u2534", u8"\u2518" },
      .overflow = u8"\u2026" };
#endif


uint_fast32_t normalize_render_flags(uint_fast32_t flags)
{
#if HAVE_LANGINFO_CODESET
    // check if the codeset will support extended border drawing,
    // and set NEO4J_RENDER_ASCII_ART if it will not.
    if (flags & NEO4J_RENDER_ASCII)
    {
        flags |= NEO4J_RENDER_ASCII_ART;
    }
    else
    {
        const char *codeset = nl_langinfo(CODESET);
        if (strcmp(codeset, "UTF-8") != 0)
        {
            flags |= NEO4J_RENDER_ASCII_ART;
        }
    }
#else
    flags |= NEO4J_RENDER_ASCII_ART;
#endif
    return flags;
}


static const struct border_glifs *glifs_for_encoding(uint_fast32_t flags)
{
#if HAVE_LANGINFO_CODESET
    if (!(flags & NEO4J_RENDER_ASCII_ART))
    {
        return &u8_border_glifs;
    }
#endif
    return &ascii_border_glifs;
}


int render_border_line(FILE *stream, border_line_t line_type,
        uint_fast32_t flags)
{
    const struct border_glifs *glifs = glifs_for_encoding(flags);
    const char *glif;
    switch (line_type)
    {
    case HORIZONTAL_LINE:
        glif = glifs->horizontal_line;
        break;
    case VERTICAL_LINE:
        glif = glifs->vertical_line;
        break;
    case TOP_LEFT_CORNER:
        glif = glifs->top_corners[0];
        break;
    case TOP_MIDDLE_CORNER:
        glif = glifs->top_corners[1];
        break;
    case TOP_RIGHT_CORNER:
        glif = glifs->top_corners[2];
        break;
    case MIDDLE_LEFT_CORNER:
        glif = glifs->middle_corners[0];
        break;
    case MIDDLE_MIDDLE_CORNER:
        glif = glifs->middle_corners[1];
        break;
    case MIDDLE_RIGHT_CORNER:
        glif = glifs->middle_corners[2];
        break;
    case BOTTOM_LEFT_CORNER:
        glif = glifs->bottom_corners[0];
        break;
    case BOTTOM_MIDDLE_CORNER:
        glif = glifs->bottom_corners[1];
        break;
    default:
        assert(line_type == BOTTOM_RIGHT_CORNER);
        glif = glifs->bottom_corners[2];
        break;
    }

    if (fputs(glif, stream) == EOF)
    {
        return -1;
    }
    return 0;
}


int render_hrule(FILE *stream, unsigned int ncolumns,
        unsigned int *widths, hline_position_t position,
        bool undersize, uint_fast32_t flags)
{
    const struct border_glifs *glifs = glifs_for_encoding(flags);
    const char * const *corners;
    switch (position)
    {
    case HLINE_TOP:
        corners = glifs->top_corners;
        break;
    case HLINE_BOTTOM:
        corners = glifs->bottom_corners;
        break;
    default:
        assert(position == HLINE_MIDDLE);
        corners = glifs->middle_corners;
        break;
    }
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
            if (fputs(glifs->horizontal_line, stream) == EOF)
            {
                return -1;
            }
        }
    }
    if (fputs(corners[undersize? 1 : 2], stream) == EOF)
    {
        return -1;
    }
    if (undersize && fputs(glifs->horizontal_line, stream) == EOF)
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
    const struct border_glifs *glifs = glifs_for_encoding(flags);
    for (unsigned int i = 0; i < ncolumns; ++i)
    {
        if (widths[i] == 0)
        {
            continue;
        }
        if (fputs(glifs->vertical_line, stream) == EOF ||
                fputc(' ', stream) == EOF)
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
        if (fputs(((unsigned int)w > widths[i] - 2)? glifs->overflow : " ", stream) == EOF)
        {
            return -1;
        }
    }
    if (fputs(glifs->vertical_line, stream) == EOF)
    {
        return -1;
    }
    if (undersize && fputs(glifs->overflow, stream) == EOF)
    {
        return -1;
    }
    if (fputc('\n', stream) == EOF)
    {
        return -1;
    }
    return 0;
}
