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

#include <stdarg.h>
#include <syslog.h>

#include "common.h"
#include "srvutils.h"
#include "utils.h"

/* global context */
gross_ctx_t *ctx;

/* prototypes of internals */
int log_put(const char *msg);
char *acct_fmt(int type, const char *msg);
int acct_put(int type, const char *msg);
size_t date_fmt(char *msg, size_t len);

int
logstr(int level, const char *fmt, ...) {
	char logfmt[MSGSZ];
	char mbuf[MSGSZ];
	va_list vap;

	if (level > ctx->config.loglevel) {
		return 0;
	}

	/* prepend thread id */
	snprintf(logfmt, MSGSZ, "#%x: %s", (uint32_t)pthread_self(), fmt);

	va_start(vap, fmt);
	vsnprintf(mbuf, MSGSZ, logfmt, vap);
	va_end(vap);

	if (ctx->config.flags & FLG_NODAEMON)
		return log_put(mbuf); 

	if (level > GLOG_DEBUG) level = GLOG_DEBUG;

	level ^= LOG_TYPE;

	syslog(level, "%s", mbuf);
	
	return 0;
}

/*
 * remove acctstr as redundant for now
 * accounting must be redesigned 
 */
#if 0
int
acctstr(int level, const char *fmt, ...) {
	char logfmt[MSGSZ];
	char mbuf[MSGSZ];
	char *final;
	va_list vap;

	if (level & ctx->config.acctmask == 0)
		return 0;

	va_start(vap, fmt);
	vsnprintf(mbuf, MSGSZ, fmt, vap);
	va_end(vap);

	if (ctx->config.flags & FLG_NODAEMON)
		return acct_put(level, mbuf);
	
	final = acct_fmt(level, mbuf);
	syslog(LOG_INFO, "%s", final);
	free(final);

	return 0;
}
#endif

void
daemon_shutdown(int return_code, const char *fmt, ...)
{
        char logfmt[MSGSZ];
        char out[MSGSZ];
        va_list vap;

        /* prepend the reason string */
        snprintf(logfmt, MSGSZ, "Grossd shutdown with exit code %d: %s", return_code, fmt);

        va_start(vap, fmt);
        vsnprintf(out, MSGSZ, logfmt, vap);
        va_end(vap);

        printf("%s\n", out);
        exit(return_code);
}

void
daemon_perror(const char *reason)
{
	char *combo;
	char *errstr;
	int errnum;
	size_t combolen;

	errnum = errno;
	errstr = strerror(errnum);
	assert(errstr);

	combolen = strlen(reason) + 1 + strlen(errstr) + 1;
	combo = malloc(combolen);

	assert(combo); /* no Malloc() here because of possible recursion loop */
	snprintf(combo, combolen, "%s %s\n", reason, errstr);

	daemon_shutdown(errno, combo);
}

int
connected(peer_t* peer)
{
	return peer->connected;
}

void *
new_address(void* val1, size_t val2)
{
	return (void *) (((size_t)val1) + val2);
}

/*
 * walk_mmap_info	- Walks through the state information datastore and
 * changes pointers according the offset. Offset is calculated based on the
 * current address and the saved address in the mmapped state file.
 */
int
walk_mmap_info(void)
{
        int i;
        size_t offset = (((size_t) &(ctx->mmap_info->brq[1])) - ((size_t)ctx->mmap_info->brq->group));

        logstr(GLOG_DEBUG, "fixing bloom ring queue memory pointers, offset=%x", offset);

#define CHANGE_ADDRESS(X,Y) { X = new_address(X,Y); }
        CHANGE_ADDRESS(ctx->mmap_info->brq->group, offset);
        CHANGE_ADDRESS(ctx->mmap_info->brq->aggregate, offset);
        CHANGE_ADDRESS(ctx->mmap_info->brq->aggregate->filter, offset);
        CHANGE_ADDRESS(ctx->mmap_info->brq->group->filter_group, offset);

        for ( i=0 ; i<ctx->mmap_info->brq->group->group_size ; i++ ) {
                CHANGE_ADDRESS(ctx->mmap_info->brq->group->filter_group[i], offset);
                CHANGE_ADDRESS(ctx->mmap_info->brq->group->filter_group[i]->filter, offset);
        }

        return TRUE;
}

bloom_ring_queue_t *
build_bloom_ring(unsigned int num, bitindex_t num_bits)
{
        bloom_ring_queue_t *brq;
        char *ptr;
        int i, ret;
        int fd, lumpsize;
        struct stat statbuf;
        char *magic = "mmbrq2\n";
        int use_mmap = FALSE;
	FILE *statefile;

        assert(num_bits > 3);

	/*
	 * lumpsize is the size of the needed contiguous memory block
	 * for the state information. We want to allocate just one 
	 * mmap()'ed file for all the state info
	 */
        lumpsize = sizeof(bloom_ring_queue_t) +         /* filter group metadata */
                sizeof(bloom_filter_group_t) +          /* filter group data */
                num * sizeof(bloom_filter_t *) +        /* pointers to filters */
                (num + 1) * sizeof(bloom_filter_t) +    /* filter metadata */
                (num + 1) * ( 1 << num_bits ) / BITS_PER_CHAR; /* filter data */

        if (ctx->config.statefile) {
                use_mmap = TRUE;
        }

        if (use_mmap) {
                /* prepare for mmapping */
                lumpsize += sizeof(mmapped_brq_t);

                ret = stat(ctx->config.statefile, &statbuf);
                if (ret == 0 && (ctx->config.flags & FLG_CREATE_STATEFILE)) {
			/* if statefile exists, but creation requested */
			daemon_shutdown(1, "statefile already exists");
		} else if (ret == 0 && (statbuf.st_size != lumpsize)) {
			/* if statefile exists, but is wrong size */
			printf("statefile size (%d) differs from the calculated size (%d)\n",
				((int)statbuf.st_size), lumpsize);
			daemon_shutdown(1, "statefile size differs from the calculated size");
                } else if (ret != 0 && (ctx->config.flags & FLG_CREATE_STATEFILE)) {
			/* statefile does not exist and creation requested */
			statefile = fopen(ctx->config.statefile, "w");
			if (statefile == NULL) {
				daemon_perror("stat(): statefile creation failed");
			}
			for (i = 0; i < lumpsize; i++) {
				if (fputc(0, statefile)) daemon_perror("fputc()");
			}
			fclose(statefile);
		} else if (ret != 0) {
			/* statefile does not exist or is not accessible */
			daemon_perror("stat(): statefile opening failed");
                }

		fd = open(ctx->config.statefile, O_RDWR);

                ptr = (char *)mmap((void*)0, lumpsize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
                assert(ptr);
                ctx->mmap_info = (mmapped_brq_t *)ptr;

                ctx->last_rotate = &(ctx->mmap_info->last_rotate);

                if (strncmp(ctx->mmap_info->magic, magic, strlen(magic)) == 0) {
                        logstr(GLOG_DEBUG, "Found the correct state file magic string.");
                        ctx->mmap_info->brq = (bloom_ring_queue_t *) &(ctx->mmap_info[1]);
                        walk_mmap_info();
                        return ctx->mmap_info->brq;
                }
                logstr(GLOG_DEBUG, "Unable to find the state file magic string. Initializing.");
                strncpy(ctx->mmap_info->magic, magic, 8);
                ctx->mmap_info->last_rotate = time(NULL);

                ptr += sizeof(mmapped_brq_t); /* skip over the mmap_info */
        } else {
                ptr = Malloc(lumpsize);
        }

        /* filter group metadata */
        brq = (bloom_ring_queue_t *)ptr;

#ifdef G_MMAP_DEBUG
	printf("brq: %p\n", ptr);
	printf("end: %p\n", (ptr + lumpsize));
#endif

        if (use_mmap)
                ctx->mmap_info->brq = brq;

        brq->current_index = 0;

        /* filter group data */
        ptr += sizeof(bloom_ring_queue_t);
        brq->group = (bloom_filter_group_t *)ptr;       /* see malloc */
        brq->group->group_size = num;

        /* pointers of pointers to filters */
        ptr += sizeof(bloom_filter_group_t);
        brq->group->filter_group = (bloom_filter_t **)ptr;

        /* filter metadata */
        ptr += num * sizeof(bloom_filter_t *); 
        brq->aggregate = (bloom_filter_t *)ptr;
        brq->aggregate->bitsize = 1 << num_bits;
        brq->aggregate->mask = ((bitindex_t) - 1) >> (BITARRAY_BASE_SIZE - num_bits);
        brq->aggregate->size = brq->aggregate->bitsize / BITARRAY_BASE_SIZE;

        for (i = 0; i < brq->group->group_size; i++) {
                brq->group->filter_group[i] = (bloom_filter_t *)(ptr + sizeof(bloom_filter_t) * (i + 1));
                brq->group->filter_group[i]->bitsize = 1 << num_bits;
                brq->group->filter_group[i]->mask =
                        ((bitindex_t) -1) >> (BITARRAY_BASE_SIZE - num_bits);
                brq->group->filter_group[i]->size =
                        brq->group->filter_group[i]->bitsize / BITARRAY_BASE_SIZE;
        }

        /* filter data */
        ptr += (num + 1) * sizeof(bloom_filter_t);
        brq->aggregate->filter = (bitarray_base_t *)ptr;
#ifdef G_MMAP_DEBUG
        printf("brq->aggregate->filter: %x\n", brq->aggregate->filter);
#endif
        for (i = 0; i < brq->group->group_size; i++) {
#ifdef G_MMAP_DEBUG
          printf("jump: %d\n", (i+1) * brq->aggregate->size );
#endif
                brq->group->filter_group[i]->filter =
			(bitarray_base_t *)(ptr + (i + 1) *
			(sizeof(bitarray_base_t) * brq->group->filter_group[i]->size));
#ifdef G_MMAP_DEBUG
                printf("%x\n", brq->group->filter_group[i]->filter);
#endif
        }

        /* zero out the filters */
        zero_bloom_filter(brq->aggregate);
        for (i = 0; i < brq->group->group_size; i++)
                zero_bloom_filter(brq->group->filter_group[i]);

        /* sync to make sure everything is working fine if using mmap */
        if (use_mmap) {
                ret = msync((void *)ctx->mmap_info, lumpsize, MS_SYNC);
                if (ret < 0) {
                        perror("msync");
                        daemon_shutdown(1, "msync woes");
                }
        }

#ifdef G_MMAP_DEBUG
        printf("brq: %x\nbrq->group: %x\n", brq, brq->group);
        printf("lumpsize: %x\n", lumpsize);
        for ( i=0 ; i<brq->group->group_size ; i++) {
          printf("Filter pointer %d: %x\n", i, brq->group->filter_group[i]);
        }
        printf("brq->aggregate: %x\n", brq->aggregate);

        for ( i=0 ; i<brq->group->group_size ; i++ ) {
          printf("Filter address %d: %x\n", i, brq->group->filter_group[i]->filter);
        }

        printf("brq->aggregate->filter: %x\n", brq->aggregate->filter);
#endif

        return brq;
}

/*
 * daemonize	- daemonize the process
 */
void
daemonize(void)
{
	int i;
	pid_t pid;

	if ((pid = fork()) != 0)
		exit(0);		/* parent terminates */
	
	/* 1st child continues */
	setsid();			/* become session leader */
	
	if ((pid = fork()) != 0)
		exit(0);		/* 1st child terminates */

	/* 2nd child continues */
	for (i = 0; i < MAXFD; i++)
		close(i);
}

/*
 * Malloc       - Wrapper for malloc(). Bails out if not successful.
 */
void *
Malloc(size_t size)
{
        void *chunk;

        chunk = malloc(size);

        if (! chunk)
                daemon_perror("malloc");

        return chunk;
}

/*
 * Pthread_create	- Wrapper, bails out if not successful.
 */
void *
Pthread_create(thread_info_t *tinfo, void *(*routine)(void *), void *arg)
{
	pthread_t *tid;
        pthread_attr_t tattr;
	int ret;

	tid = (pthread_t *)Malloc(sizeof(pthread_t));

	ret = pthread_attr_init(&tattr);
	if (ret)
		daemon_perror("pthread_attr_init");
	ret = pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);
	if (ret)
		daemon_perror("pthread_attr_setdetachstate");

	ret = pthread_create(tid, &tattr, routine, arg);
	if (ret)
		daemon_perror("pthread_create");

	if (tinfo)
		tinfo->thread = tid;
	else
		free(tid);

	pthread_attr_destroy(&tattr);

	return (void*)tid;
}

int
log_put(const char *msg)
{
        char *final;

        final = Malloc(MSGSZ);
        snprintf(final, MSGSZ-1, "%s", msg);
        date_fmt(final, MSGSZ);
        printf("%s", final);
        free(final);
        return 0;
}

char *acct_fmt(int type, const char *msg)
{
        char *final;

        final = Malloc(MSGSZ);

        switch (type & ACCT_FULL) {
                case ACCT_GREY:
                        snprintf(final, MSGSZ-1, "grey %s", msg);
                        break;
                case ACCT_MATCH:
                        snprintf(final, MSGSZ-1, "match %s", msg);
                        break;
                case ACCT_TRUST:
                        snprintf(final, MSGSZ-1, "trust %s", msg);
                        break;
                case ACCT_DNS_TMOUT:
                        snprintf(final, MSGSZ-1, "dns-timeout %s", msg);
                        break;
                case ACCT_DNS_MATCH:
                        snprintf(final, MSGSZ-1, "dns-match %s", msg);
                        break;
                case ACCT_DNS_SKIP:
                        snprintf(final, MSGSZ-1, "dns-skip %s", msg);
                        break;
		/* FIX: this should be accounted only, if debugging */
                case ACCT_DNS_QUERY:
                        snprintf(final, MSGSZ-1, "dns-query %s", msg);
                        break;
                default:
                        return NULL;
                        break;
        }

        return final;
}
int
acct_put(int type, const char *msg)
{
        char *final;

	final = acct_fmt(type, msg);

	assert(final);

        date_fmt(final, MSGSZ);
        printf("%s", final);
        free(final);
        return 0;
}

size_t
date_fmt(char *msg, size_t len) {
        time_t tt;
        char *timestr;
        char *buf;
        size_t ret;

        buf = Malloc(MSGSZ);

        tt = time(NULL);
        timestr = ctime(&tt);
        chomp(timestr);

        snprintf(buf, MSGSZ-1, "%s %s\n", timestr, msg);
        strncpy(msg, buf, len - 1);
        msg[len-1] = '\0';

        free(buf);
        return ret;
}
