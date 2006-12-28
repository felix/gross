/*
 * Copyright (c) 2006 Eino Tuominen <eino@utu.fi>
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
 */

#ifndef UTILS_H
#define UTILS_H

enum readlineret_t { ERROR = -1, EMPTY = 0, DATA = 1};

#define SI_KILO 1000
#define SI_MEGA (SI_KILO * SI_KILO)
#define SI_GIGA (SI_KILO * SI_MEGA)

#ifndef HAVE_CLOCK_GETTIME
/* OS X does not have clock_gettime, so we will fake one */
typedef enum {
	CLOCK_KLUDGE,
} clockid_t;

int clock_gettime(clockid_t clk_id, struct timespec *tp);
#endif /* ! HAVE_CLOCK_GETTIME */

int readline(int fd, void *vptr, size_t maxlen);
int getline(int fd, char *line, size_t maxlen);
ssize_t readn(int fd, void *vptr, size_t n);
ssize_t writen(int fd, const void *vptr, size_t n);
ssize_t writeline(int fd, const char *line);
ssize_t writet(int fd, const char *line, const char *terminator);
ssize_t respond(int fd, const char *response);
int trim(char **buffer);
int chomp(char *buffer);
int ms_diff(struct timespec *t1, struct timespec *t2);
int ts_sum(struct timespec *sum, const struct timespec *t1, const struct timespec *t2);
int ts_diff(struct timespec *diff, const struct timespec *t1, const struct timespec *t2);
void mstotimespec(int mseconds, struct timespec *ts);
void tvtots(const struct timeval *tv, struct timespec *ts);
void tstotv(const struct timespec *ts, struct timeval *tv);
#endif
