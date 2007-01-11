/*
 * Copyright (c) 2006,2007 Eino Tuominen <eino@utu.fi>
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

#ifndef MSGQUEUE_H
#define MSGQUEUE_H

typedef struct msg_s {
	void *msgp;
	size_t msgsz;
	struct msg_s *next;
	struct timespec timestamp;
} msg_t;

typedef struct msgqueue_s {
	pthread_cond_t cv;
	pthread_mutex_t mx;
	msg_t *head;
	msg_t *tail;
	int msgcount;
	struct msgqueue_s *delaypair;
	const struct timespec *delay_ts;
	int *impose_delay; /* both the queues point to the same int */
	bool active;
	int id;
} msgqueue_t;

typedef struct {
	msgqueue_t *inq;
	msgqueue_t *outq;
} queue_info_t;

int get_queue(void);
int get_delay_queue(const struct timespec *ts);
int disable_delay(int msqid);
int enable_delay(int msqid);
int set_delay(int msqid, const struct timespec *ts);
int put_msg(int msqid, void *msgp, size_t msgsz, int msgflg);
int instant_msg(int msqid, void *msgp, size_t msgsz, int msgflg);
int release_queue(int msqid);
size_t get_msg(int msqid, void *msgp, size_t maxsize, int msgflag);
size_t get_msg_timed(int msqid, void *msgp, size_t maxsize, int msgflag, time_t timeout);
size_t in_queue_len(int msgid);
size_t out_queue_len(int msgid);


#endif /* MSGQUEUE_H */
