/*
 * Copyright (c) 2006,2007
 *                    Eino Tuominen <eino@utu.fi>
 *                    Antti Siira <antti@utu.fi>
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

#include "common.h"
#include "utils.h"

/*
 * readline	- implementation by W. Richard Stevens, 
 * modified to not include line terminator (\n or \r\n)
 */
int
readline(int fd, void *vptr, size_t maxlen)
{
	ssize_t n, rc;
	char	c, *ptr;

	ptr = vptr;
	for (n = 1; n < maxlen; n++) {
	    again:
		if ((rc = read(fd, &c, 1)) == 1) {
			if (c == '\n')
				break;	/* we don't want newline */
			if (! (c == '\r'))  /* we don't need these either */
				*ptr++ = c;
		} else if (rc == 0) {
			if (n == 1)
				return EMPTY; /* EOF, no data read */
			else
				break; /* EOF, some data read */
		} else {
			if (errno == EINTR)
				goto again;
			return ERROR;	/* error, errno set by read() */
		}
	}

	*ptr = 0; /* null terminate like fgets() */
	return DATA;
}

/*
 * readn        - read n bytes from a descriptor, implementation by
 * W. Richard Stevens
 */
ssize_t
readn(int fd, void *vptr, size_t n)
{
  size_t nleft;
  ssize_t nread;
  char *ptr;

  ptr = vptr;
  nleft = n;
  while(nleft > 0) {
    if ( (nread = read(fd, ptr, nleft)) < 0 ) {
      if (EINTR == errno)
	nread = 0;
      else
	return (-1);
    } else if (nread == 0)
      break;
    
    nleft -= nread;
    ptr += nread;
  }

  return (n-nleft);
}

/* 
 * writen	- write n bytes to a descriptor, implemetation by
 * W. Richard Stevens
 */ 
ssize_t
writen(int fd, const void *vptr, size_t n)
{
	size_t nleft;
	ssize_t nwritten;
	const char *ptr;

	ptr = vptr;
	nleft = n;
	while (nleft > 0) {
		if ((nwritten = write(fd, ptr, nleft)) <= 0) {
			if (errno == EINTR)
				nwritten = 0;	/* and call write() again */
			else
				return -1;	/* error */
		}
		nleft -= nwritten;
		ptr += nwritten;
	}
	return n;
}

/* 
 * writet	- write a terminated string to a descriptor
 */
ssize_t
writet(int fd, const char *line, const char *terminator)
{
	char *str;
	size_t linelen;
	size_t len;

	linelen = strlen(line) + strlen(terminator) + 1;
	str = malloc(linelen);
	if (! str) {
		gerror("malloc");
		return -1;
	}
	snprintf(str, linelen, "%s%s", line, terminator);
	len = writen(fd, str, linelen - 1);
	Free(str);
	return len;
}

/* 
 * writeline	- write a line to a descriptor, terminate with \r\n
 */
ssize_t
writeline(int fd, const char *line)
{
	const char terminator[] = "\r\n";
	return writet(fd, line, terminator);
}

/* 
 * respond	- write a line to a descriptor, terminate with \n\n
 */
ssize_t
respond(int fd, const char *response)
{
	const char terminator[] = "\n\n";
	return writet(fd, response, terminator);
}

/*
 * chomp \r\n
 */
int
chomp(char *buffer)
{
        int len;

        len = strlen(buffer);
	if (len < 2) {
		return -1;
	}
        if (buffer[len-1] == '\n') {
                buffer[len-1] = '\0';
        }
	if (buffer[len-2] == '\r') {
		buffer[len-2] = '\0';
	}
	return 0;
}

/*
 * trim whitespace, return strlen of the result
 */
int
trim(char **buf)
{
	char *end;

	/* the beginning */
	*buf += strspn(*buf, " \t");

	/* the end */
	end = *buf + strlen(*buf) - 1;
	while (*end == ' ' || *end == '\t')
		*end-- = '\0';

	return strlen(*buf);
}

/*
 * ms_diff      - returns time difference of t1 and t2 in milliseconds
 *                returns positive integer if t1 is greater than t2
 */
int
ms_diff(struct timespec *t1, struct timespec *t2)
{
        return (t1->tv_sec - t2->tv_sec) * SI_KILO +
               (t1->tv_nsec - t2->tv_nsec) / SI_MEGA;
}

/*
 * ts_sum	- calculate sum of t1 and t2 and save result in sum
 */
int
ts_sum(struct timespec *sum, const struct timespec *t1, const struct timespec *t2)
{
        sum->tv_sec = t1->tv_sec + t2->tv_sec;
        sum->tv_nsec = (t1->tv_nsec + t2->tv_nsec) % (SI_GIGA);
        if ((t1->tv_nsec + t2->tv_nsec) / (SI_GIGA) > 0)
                sum->tv_sec++;
        return 0;
}

/*
 * tsdiff	- calculate time difference of t1 and t2 and save the result in diff
 * 		  t1 MUST be greater the t2, otherwise -1
 */
int
ts_diff(struct timespec *diff, const struct timespec *t1, const struct timespec *t2)
{
        diff->tv_sec = t1->tv_sec - t2->tv_sec;
        diff->tv_nsec = t1->tv_nsec - t2->tv_nsec;
        if (diff->tv_nsec < 0) {
                diff->tv_nsec += SI_GIGA;
                diff->tv_sec--;
        }

        if (diff->tv_sec < 0) {
                return -1;
        }

        return 0;
}

/*
 * mstotimespec	- convert milliseconds to struct timespec
 */
void
mstotimespec(int mseconds, struct timespec *ts)
{
	ts->tv_sec = mseconds / SI_KILO;
	ts->tv_nsec = abs(mseconds % SI_KILO) * SI_MEGA;
}

void
tvtots(const struct timeval *tv, struct timespec *ts)
{
        ts->tv_sec = tv->tv_sec;
        ts->tv_nsec = tv->tv_usec * SI_KILO;
}

void
tstotv(const struct timespec *ts, struct timeval *tv)
{
        tv->tv_sec = ts->tv_sec;
        tv->tv_usec = ts->tv_nsec / SI_KILO;
}

#ifndef HAVE_CLOCK_GETTIME
int
clock_gettime(clockid_t clk_id, struct timespec *ts)
{
        struct timeval tv;
 
        if (clk_id == CLOCK_KLUDGE) {
                if (gettimeofday(&tv, NULL) != 0)
                        return -1;
                tvtots(&tv, ts);
                return 0;
        } else {
                errno = EINVAL;
                return -1; 
        }
}
#endif /* ! HAVE_CLOCK_GETTIME */
