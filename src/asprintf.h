/*
Copyright (C) 2014 insane coder (http://insanecoding.blogspot.com/, http://asprintf.insanecoding.org/)

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#ifndef INSANE_ASPRINTF_H
#define INSANE_ASPRINTF_H

#ifndef __cplusplus
#include <stdarg.h>
#else
#include <cstdarg>
extern "C"
{
#endif

#define insane_free(ptr) { free(ptr); ptr = 0; }

    int vasprintf(char **strp, const char *fmt, va_list ap);
    int asprintf(char **strp, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif
