/*
 * Copyright (c) 2000, 2002 Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F39502-99-1-0512.
 */
#ifndef NEO4J_READPASSPHRASE_H
#define NEO4J_READPASSPHRASE_H

#include <unistd.h>

#ifdef HAVE_READPASSPHRASE

#  ifdef HAVE_READPASSPHRASE_H
#    include <readpassphrase.h>
#  elif HAVE_BSD_READPASSPHRASE_H
#    include <bsd/readpassphrase.h>
#  endif

#else

#define RPP_ECHO_OFF    0x00        /* Turn off echo (default). */
#define RPP_ECHO_ON     0x01        /* Leave echo on. */
#define RPP_REQUIRE_TTY 0x02        /* Fail if there is no tty. */
#if 0
#define RPP_FORCELOWER  0x04        /* Force input to lower case. */
#define RPP_FORCEUPPER  0x08        /* Force input to upper case. */
#define RPP_SEVENBIT    0x10        /* Strip the high bit from input. */
#endif
#define RPP_STDIN       0x20        /* Read from stdin, not /dev/tty */

char *readpassphrase(const char *, char *, size_t, int);

#define HAVE_READPASSPHRASE

#endif

#endif/*NEO4J_READPASSPHRASE_H*/
