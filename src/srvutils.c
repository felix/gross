/* $Id$ */

/*
 * Copyright (c) 2006, 2007, 2008
 *               Eino Tuominen <eino@utu.fi>
 *               Antti Siira <antti@utu.fi>
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
size_t date_fmt(char *msg, size_t len);

int
logstr(int level, const char *fmt, ...)
{
	char logfmt[MSGSZ] = { '\0' };
	char mbuf[MSGSZ] = { '\0' };
	va_list vap;

	if (level > ctx->config.loglevel) {
		return 0;
	}

	/* prepend thread id */
	snprintf(logfmt, MSGSZ, "#%x: %s", (uint32_t) pthread_self(), fmt);

	va_start(vap, fmt);
	vsnprintf(mbuf, MSGSZ, logfmt, vap);
	va_end(vap);

	if (false == ctx->syslog_open)
		return log_put(mbuf);

	if (level > GLOG_DEBUG)
		level = GLOG_DEBUG;

	level ^= LOG_TYPE;

	syslog(level, "%s", mbuf);

	return 0;
}

int
statstr(int level, const char *fmt, ...)
{
	char mbuf[MSGSZ] = { 0x00 };
	va_list vap;

	if ((level & ctx->config.statlevel) == STATS_NONE) {
		return 0;
	}

	if (GLOG_NOTICE > ctx->config.loglevel) {
		return 0;
	}

	va_start(vap, fmt);
	vsnprintf(mbuf, MSGSZ, fmt, vap);
	va_end(vap);

	if (ctx->config.flags & FLG_NODAEMON)
		return log_put(mbuf);

	level = GLOG_NOTICE;

	level ^= LOG_TYPE;

	syslog(level, "%s", mbuf);

	return 0;
}

void
daemon_shutdown(int return_code, const char *fmt, ...)
{
	char logfmt[MSGSZ];
	char out[MSGSZ];
	va_list vap;

	if (fmt) {
		/* prepend the reason string */
		snprintf(logfmt, MSGSZ, "Grossd shutdown with exit code %d: %s", return_code, fmt);

		va_start(vap, fmt);
		vsnprintf(out, MSGSZ, logfmt, vap);
		va_end(vap);

		fprintf(stderr, "%s\n", out);

		if (ctx->syslog_open)
			logstr(GLOG_ERROR, "%s", out);

	}

	if (EXIT_NOERROR == return_code && (ctx->config.flags & FLG_CREATE_PIDFILE) && ctx->config.pidfile)
		unlink(ctx->config.pidfile);
	exit(return_code);
}

void
daemon_fatal(const char *reason)
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

	assert(combo);		/* no Malloc() here because of possible recursion loop */
	snprintf(combo, combolen, "%s %s\n", reason, errstr);

	daemon_shutdown(EXIT_FATAL, combo);
}

int
connected(peer_t *peer)
{
	return peer->connected;
}

void *
new_address(void *val1, size_t val2)
{
	return (void *)(((size_t) val1) + val2);
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
	size_t offset = (((size_t) & (ctx->mmap_info->brq[1])) - ((size_t) ctx->mmap_info->brq->group));

	logstr(GLOG_DEBUG, "fixing bloom ring queue memory pointers, offset=%x", offset);

#define CHANGE_ADDRESS(X,Y) { X = new_address(X,Y); }
	CHANGE_ADDRESS(ctx->mmap_info->brq->group, offset);
	CHANGE_ADDRESS(ctx->mmap_info->brq->aggregate, offset);
	CHANGE_ADDRESS(ctx->mmap_info->brq->aggregate->filter, offset);
	CHANGE_ADDRESS(ctx->mmap_info->brq->group->filter_group, offset);

	for (i = 0; i < ctx->mmap_info->brq->group->group_size; i++) {
		CHANGE_ADDRESS(ctx->mmap_info->brq->group->filter_group[i], offset);
		CHANGE_ADDRESS(ctx->mmap_info->brq->group->filter_group[i]->filter, offset);
	}

	return TRUE;
}

/*
 * create_statefile     - return only when creation succeeds */
void
create_statefile(void)
{
	int ret;
	int lumpsize;
	int i;
	struct stat statbuf;
	FILE *statefile;
	unsigned int num = ctx->config.num_bufs;
	bitindex_t num_bits = ctx->config.filter_size;

	/* calculate the statefile size */
	lumpsize = sizeof(bloom_ring_queue_t) +	/* filter group metadata */
	    sizeof(bloom_filter_group_t) +	/* filter group data */
	    num * sizeof(bloom_filter_t *) +	/* pointers to filters */
	    (num + 1) * sizeof(bloom_filter_t) +	/* filter metadata */
	    (num + 1) * (1 << num_bits) / BITS_PER_CHAR +	/* filter data */
	    sizeof(mmapped_brq_t);	/* mmap_info */

	ret = stat(ctx->config.statefile, &statbuf);
	if (ret == 0) {
		daemon_shutdown(EXIT_FATAL, "statefile already exists");
	} else if (ENOENT == errno) {
		/* statefile does not exist */
		statefile = fopen(ctx->config.statefile, "w");
		if (statefile == NULL) {
			daemon_fatal("stat(): statefile creation failed");
		}
		for (i = 0; i < lumpsize; i++)
			if (fputc(0, statefile))
				daemon_fatal("fputc()");
		fclose(statefile);
		return;
	} else {
		daemon_fatal("statefile opening failed: stat:");
	}
}

bloom_ring_queue_t *
build_bloom_ring(unsigned int num, bitindex_t num_bits)
{
	bloom_ring_queue_t *brq;
	char *ptr;
	int i, ret;
	int lumpsize;
	struct stat statbuf;
	char *magic = "mmbrq2\n";
	int use_mmap = FALSE;

	assert(num_bits > 3);

	/*
	 * lumpsize is the size of the needed contiguous memory block
	 * for the state information. We want to allocate just one 
	 * mmap()'ed file for all the state info
	 */
	lumpsize = sizeof(bloom_ring_queue_t) +	/* filter group metadata */
	    sizeof(bloom_filter_group_t) +	/* filter group data */
	    num * sizeof(bloom_filter_t *) +	/* pointers to filters */
	    (num + 1) * sizeof(bloom_filter_t) +	/* filter metadata */
	    (num + 1) * (1 << num_bits) / BITS_PER_CHAR;	/* filter data */

	if (ctx->config.statefile) {
		use_mmap = TRUE;
	}

	if (use_mmap) {
                if (NULL != ctx->statefile_info)
                        daemon_shutdown(EXIT_FATAL, "statefile already open");

		/* prepare for mmapping */
		lumpsize += sizeof(mmapped_brq_t);

		ret = stat(ctx->config.statefile, &statbuf);
		if (ret < 0) {
			/* statefile does not exist or is not accessible */
			daemon_fatal("stat(): statefile opening failed");
		} else if (statbuf.st_size != lumpsize) {
			/* statefile exists, but is wrong size */
			printf("statefile size (%d) differs from the calculated size (%d)\n",
			    ((int)statbuf.st_size), lumpsize);
			daemon_shutdown(EXIT_FATAL, "statefile size differs from the calculated size");
		}

		ctx->statefile_info = Malloc(sizeof(statefile_info_t));
                ctx->statefile_info->fd = open(ctx->config.statefile, O_RDWR);
		if (ctx->statefile_info->fd < 0)
			daemon_fatal("open() statefile:");

		ptr = (char *)mmap((void *)0, lumpsize, PROT_READ | PROT_WRITE, MAP_SHARED, ctx->statefile_info->fd, 0);
		assert(ptr);
		ctx->mmap_info = (mmapped_brq_t *)ptr;

		ctx->last_rotate = &(ctx->mmap_info->last_rotate);

		if (strncmp(ctx->mmap_info->magic, magic, strlen(magic)) == 0) {
			logstr(GLOG_DEBUG, "Found the correct state file magic string.");
			ctx->mmap_info->brq = (bloom_ring_queue_t *)&(ctx->mmap_info[1]);
			walk_mmap_info();
			return ctx->mmap_info->brq;
		}
		logstr(GLOG_DEBUG, "Unable to find the state file magic string. Initializing.");
		strncpy(ctx->mmap_info->magic, magic, 8);
		ctx->mmap_info->last_rotate = time(NULL);

		ptr += sizeof(mmapped_brq_t);	/* skip over the mmap_info */
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
	brq->group = (bloom_filter_group_t *)ptr;	/* see malloc */
	brq->group->group_size = num;

	/* pointers of pointers to filters */
	ptr += sizeof(bloom_filter_group_t);
	brq->group->filter_group = (bloom_filter_t **)ptr;

	/* filter metadata */
	ptr += num * sizeof(bloom_filter_t *);
	brq->aggregate = (bloom_filter_t *)ptr;
	brq->aggregate->bitsize = 1 << num_bits;
	brq->aggregate->mask = ((bitindex_t)-1) >> (BITARRAY_BASE_SIZE - num_bits);
	brq->aggregate->size = brq->aggregate->bitsize / BITARRAY_BASE_SIZE;

	for (i = 0; i < brq->group->group_size; i++) {
		brq->group->filter_group[i] = (bloom_filter_t *)(ptr + sizeof(bloom_filter_t) * (i + 1));
		brq->group->filter_group[i]->bitsize = 1 << num_bits;
		brq->group->filter_group[i]->mask = ((bitindex_t)-1) >> (BITARRAY_BASE_SIZE - num_bits);
		brq->group->filter_group[i]->size = brq->group->filter_group[i]->bitsize / BITARRAY_BASE_SIZE;
	}

	/* filter data */
	ptr += (num + 1) * sizeof(bloom_filter_t);
	brq->aggregate->filter = (bitarray_base_t *)ptr;
#ifdef G_MMAP_DEBUG
	printf("brq->aggregate->filter: %x\n", brq->aggregate->filter);
#endif
	for (i = 0; i < brq->group->group_size; i++) {
#ifdef G_MMAP_DEBUG
		printf("jump: %d\n", (i + 1) * brq->aggregate->size);
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
			daemon_fatal("msync");
		}
	}
#ifdef G_MMAP_DEBUG
	printf("brq: %x\nbrq->group: %x\n", brq, brq->group);
	printf("lumpsize: %x\n", lumpsize);
	for (i = 0; i < brq->group->group_size; i++) {
		printf("Filter pointer %d: %x\n", i, brq->group->filter_group[i]);
	}
	printf("brq->aggregate: %x\n", brq->aggregate);

	for (i = 0; i < brq->group->group_size; i++) {
		printf("Filter address %d: %x\n", i, brq->group->filter_group[i]->filter);
	}

	printf("brq->aggregate->filter: %x\n", brq->aggregate->filter);
#endif

	return brq;
}

void
release_bloom_ring_queue(bloom_ring_queue_t *brq)
{
	if (ctx->statefile_info && brq == ctx->mmap_info->brq) {
		/* requested release of mmapped brq */
		munmap((void *)ctx->mmap_info->brq, ctx->mmap_info->lumpsize);
		close(ctx->statefile_info->fd);
		Free(ctx->statefile_info);
		ctx->statefile_info = NULL;
		ctx->filter = NULL;
		ctx->mmap_info = NULL;
	} else {
		Free(brq);
	}
}

/*
 * create_pidfile	- write the process id into the pidfile 
 */
void
create_pidfile(void)
{
	FILE *pf;
	int ret;

	assert(ctx->config.pidfile);
	logstr(GLOG_INFO, "creating pidfile %s", ctx->config.pidfile);
	pf = fopen(ctx->config.pidfile, "w");
	if (pf != NULL) {
		ret = fprintf(pf, "%d", getpid());
		if (ret < 0)
			daemon_fatal("writing pidfile");
	} else {
		daemon_fatal("opening pidfile: fdopen");
	}
	fclose(pf);
}

/*
 * check_pidfile	- returns if pidfile does not exist
 */
void
check_pidfile(void)
{
	int ret;
	struct stat statinfo;

	ret = stat(ctx->config.pidfile, &statinfo);
	if (ret < 0)
		if (ENOENT != errno)
			daemon_fatal("stat");
		else
			return;
	else
		daemon_shutdown(EXIT_PIDFILE_EXISTS, "pidfile already exists");
}

/*
 * daemonize	- daemonize the process
 */
void
daemonize(void)
{
	int i;
	pid_t pid;

	log_close();
	if ((pid = fork()) > 0) {
		log_open();
		exit(EXIT_NOERROR);	/* parent terminates */
	} else if (pid < 0) {
		log_open();
		daemon_fatal("fork(): "); /* error */
	}

	/* 1st child continues */
	/* become session leader */
	if ((pid = setsid()) < 0) {
		log_open();
		daemon_fatal("setsid(): ");
	}

	if ((pid = fork()) > 0) {
		log_open();
		exit(EXIT_NOERROR);	/* 1st child terminates */
	} else if (pid < 0) {
		log_open();
		daemon_fatal("fork: "); /* error */
	}

	/* 2nd child continues */
	close(0);
	open("/dev/null", O_RDONLY, 0);
	close(1);
	open("/dev/null", O_WRONLY, 0);
	close(2);
	open("/dev/null", O_WRONLY, 0);
	for (i = 3; i < MAXFD; i++)
		close(i);
	log_open();
}

/*
 * Malloc       - Wrapper for malloc(). Bails out if not successful.
 */
void *
Malloc(size_t size)
{
	void *chunk;

	assert(size);
	chunk = malloc(size);

	if (!chunk)
		daemon_fatal("malloc");

	return chunk;
}

/*
 * create_thread	- Wrapper, bails out if not successful.
 */
void *
create_thread(thread_info_t *tinfo, int detach, void *(*routine) (void *), void *arg)
{
	pthread_t *tid;
	pthread_attr_t tattr;
	int ret;

	tid = (pthread_t *) Malloc(sizeof(pthread_t));

	ret = pthread_attr_init(&tattr);
	if (ret)
		daemon_fatal("pthread_attr_init");
	if (DETACH == detach)
		ret = pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);
	if (ret)
		daemon_fatal("pthread_attr_setdetachstate");

	ret = pthread_create(tid, &tattr, routine, arg);
	if (ret)
		daemon_fatal("pthread_create");

	if (tinfo)
		tinfo->thread = tid;
	else
		Free(tid);

	pthread_attr_destroy(&tattr);

	return (void *)tid;
}

int
log_put(const char *msg)
{
	char *final;

	final = Malloc(MSGSZ);
	snprintf(final, MSGSZ - 1, "%s", msg);
	date_fmt(final, MSGSZ);
	printf("%s", final);
	Free(final);
	fflush(stdout);
	return 0;
}

size_t
date_fmt(char *msg, size_t len)
{
	time_t tt;
	char timestr[DATESTRLEN];
	char buf[MSGSZ];

	tt = time(NULL);
	ctime_r(&tt, timestr);
	chomp(timestr);

	snprintf(buf, MSGSZ - 1, "%s %s\n", timestr, msg);
	strncpy(msg, buf, len - 1);
	msg[len - 1] = '\0';

	return strlen(msg);
}

void
register_check(thread_pool_t *pool, bool definitive)
{
	int i;
	check_t *check;

	check = Malloc(sizeof(*check));
	check->pool = pool;
	check->definitive = definitive;

	for (i = 0; i < MAXCHECKS; i++)
		if (NULL == ctx->checklist[i]) {
			ctx->checklist[i] = check;
			break;
		}
	if (i == MAXCHECKS)
		logstr(GLOG_ERROR, "unable to register pool %s", pool->name);
}

char *
ipstr(struct sockaddr_in *saddr)
{
	char ipstr[INET_ADDRSTRLEN];

	if (inet_ntop(AF_INET, &saddr->sin_addr, ipstr, INET_ADDRSTRLEN) == NULL) {
		strncpy(ipstr, "UNKNOWN\0", INET_ADDRSTRLEN);
	}
	return strdup(ipstr);
}

/*
 * log_open	- open the configured log facility (currently only syslog)
 */
int
log_open(void)
{
        if ((ctx->config.flags & (FLG_NODAEMON | FLG_SYSLOG)) == FLG_SYSLOG) {
		if (ctx->syslog_open)
			return -1;
                openlog("grossd", LOG_ODELAY, ctx->config.syslogfacility);
                ctx->syslog_open = true;
        }
	return 0;
}
	
/*
 * log_close	- close the configured log facility (currenlty only syslog)
 */
int
log_close(void)
{
        if ((ctx->config.flags & (FLG_NODAEMON | FLG_SYSLOG)) == FLG_SYSLOG) {
		if (! ctx->syslog_open)
			return -1;
		closelog();
                ctx->syslog_open = false;
        }
	return 0;
}
