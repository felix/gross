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

#include <netdb.h>
#include <signal.h>
#include <syslog.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "common.h"
#include "conf.h"
#include "srvutils.h"
#include "msgqueue.h"

#ifdef DNSBL
#include "dnsblc.h"
#endif /* DNSBL */

/* maximum simultaneus tcp worker threads */
#define MAXWORKERS 1

#define MAXCONNQ 5

#define SECONDS_IN_HOUR ((time_t)60*60)
#define MAX_PEER_NAME_LEN 1024

#define CONF(item)	gconf(config, item)

/* function prototypes */
void bloommgr_init();
void syncmgr_init();
void worker_init();
void srvstatus_init();

gross_ctx_t *
initialize_context()
{
	gross_ctx_t *ctx;
/* 	sem_t *sp; */
/* 	int ret; */
	
	ctx = Malloc(sizeof(gross_ctx_t));

	/* Clear flags  */
	ctx->config.flags = 0;

	/* Clear protocols */
	ctx->config.protocols = 0;

	/* Clear checks */
	ctx->config.checks = 0;

	/* initial loglevel and facility, they will be set in configure_grossd() */
	ctx->config.loglevel = 0;
	ctx->config.syslogfacility = 0;

	ctx->filter = NULL;
	
	memset(&ctx->config.gross_host, 0, sizeof(ctx->config.gross_host));
	memset(&ctx->config.sync_host, 0, sizeof(ctx->config.sync_host));
	memset(&ctx->config.peer.peer_addr, 0, sizeof(ctx->config.peer.peer_addr));
	memset(&ctx->config.status_host, 0, sizeof(ctx->config.status_host));

	ctx->config.peer.peerfd_out = -1;
	ctx->config.peer.peerfd_in = -1;
	
	ctx->last_rotate = Malloc(sizeof(time_t));

#ifdef DNSBL
	ctx->dnsbl = NULL;
#endif /* DNSBL */

	return ctx;
}

void
configure_grossd(configlist_t *config)
{
	int ret;
	configlist_t *cp;
	const char *updatestr;
	struct hostent *host = NULL;

	cp = config;
	if (ctx->config.flags & (FLG_NODAEMON))
		while (cp) {
			logstr(GLOG_INSANE, "config: %s = %s", cp->name, cp->value);
			cp = cp->next;
		}

#ifdef USE_SEM_OPEN
	ret = sem_unlink("sem_sync");
	if (ret == -1 && errno == EACCES) 
		daemon_perror("sem_unlink");
	ctx->sync_guard = sem_open("sem_sync", O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 1);
	if (ctx->sync_guard == (sem_t *)SEM_FAILED)
		daemon_perror("sem_open");
#else
	ctx->sync_guard = Malloc(sizeof(sem_t));
	ret = sem_init(ctx->sync_guard, 0, 1); /* Process local (0), initial count 1. */
	if (ret != 0)
		daemon_perror("sem_init");
#endif /* USE_SEM_OPEN */	    

	pthread_mutex_init(&ctx->bloom_guard, NULL);
	
	pthread_mutex_init(&ctx->config.peer.peer_in_mutex, NULL);

	ctx->config.gross_host.sin_family = AF_INET;
	host = gethostbyname(CONF("host"));
	if (!host) daemon_perror("'host' configuration option invalid:");
	inet_pton(AF_INET, inet_ntoa( *(struct in_addr*)host->h_addr_list[0]), &(ctx->config.gross_host.sin_addr));
	logstr(GLOG_DEBUG, "Listening host address %s", inet_ntoa( *(struct in_addr*)host->h_addr_list[0]));

	ctx->config.sync_host.sin_family = AF_INET;
	host = gethostbyname( CONF("sync_listen") ? CONF("sync_listen") : CONF("host") );
	inet_pton(AF_INET,inet_ntoa( *(struct in_addr*)host->h_addr_list[0]),
		  &(ctx->config.sync_host.sin_addr));
	logstr(GLOG_DEBUG, "Sync listen address %s", inet_ntoa( *(struct in_addr*)host->h_addr_list[0]));

	ctx->config.sync_host.sin_port =
		htons(atoi(CONF("sync_port")));
	ctx->config.gross_host.sin_port =
		htons(atoi(CONF("port")));
	ctx->config.max_connq = 50;
	ctx->config.max_threads = 10;
	ctx->config.peer.connected = 0;

	ctx->config.greylist_delay = atoi(CONF("grey_delay"));

	if (10 != ctx->config.greylist_delay)
	  logstr(GLOG_DEBUG, "Greylisting delay %d", ctx->config.greylist_delay);

	/* peer port is the same as the local sync_port */
	ctx->config.peer.peer_addr.sin_port = htons(atoi(CONF("sync_port")));

	if (CONF("sync_peer") == NULL) {
		logstr(GLOG_INFO, "No peer configured. Replication suppressed.");
		ctx->config.flags |= FLG_NOREPLICATE;	  
	} else {
		logstr(GLOG_INFO, "Peer %s configured. Replicating.", CONF("sync_peer"));
		ctx->config.peer.peer_addr.sin_family = AF_INET;
		host = gethostbyname( CONF("sync_peer") );
		inet_pton(AF_INET, inet_ntoa( *(struct in_addr*)host->h_addr_list[0]),
			&(ctx->config.peer.peer_addr.sin_addr));
		logstr(GLOG_DEBUG, "Sync peer address %s", inet_ntoa( *(struct in_addr*)host->h_addr_list[0]));
	}

	updatestr = CONF("update");
	if (strncmp(updatestr, "always", 7) == 0) {
		logstr(GLOG_INFO, "updatestyle: ALWAYS");
		ctx->config.flags |= FLG_UPDATE_ALWAYS;
	} else if ((updatestr == NULL) || (strncmp(updatestr, "grey", 5) == 0))
		logstr(GLOG_INFO, "updatestyle: GREY");
	else {
		daemon_shutdown(1, "Invalid updatestyle: %s", updatestr);
	}

	/* we must reset errno because strtol returns 0 if it fails */
	errno = 0;
	ctx->config.grey_mask = strtol(CONF("grey_mask"), (char **)NULL, 10);
	if (errno || ctx->config.grey_mask > 32 || ctx->config.grey_mask < 0)
		daemon_shutdown(1, "Invalid grey_mask: %s", CONF("grey_mask"));

	ctx->config.status_host.sin_family = AF_INET;
	host = gethostbyname( CONF("status_host") ? CONF("status_host") : CONF("host") );
	inet_pton(AF_INET, inet_ntoa( *(struct in_addr*)host->h_addr_list[0]),
		  &(ctx->config.status_host.sin_addr));

	ctx->config.status_host.sin_port =
		htons(atoi(CONF("status_port")));

	ctx->config.rotate_interval = atoi(CONF("rotate_interval"));
	ctx->config.filter_size = atoi(CONF("filter_bits"));
	ctx->config.num_bufs = atoi(CONF("number_buffers"));

	if (CONF("statefile"))
		ctx->config.statefile = strdup(CONF("statefile"));
	else
		ctx->config.statefile = NULL;

	if ((ctx->config.filter_size<5) || (ctx->config.filter_size>32)) {
	  daemon_shutdown(1, "filter_bits should be in range [4,32]");
	}

	if (!CONF("sjsms_response_grey"))
		daemon_shutdown(1, "No sjsms_response_grey set!");
	else
		ctx->config.sjsms.responsegrey = strdup(CONF("sjsms_response_grey"));
	if (!CONF("sjsms_response_trust"))
		daemon_shutdown(1, "No sjsms_response_trust set!");
	else
		ctx->config.sjsms.responsetrust = strdup(CONF("sjsms_response_trust"));
	if (!CONF("sjsms_response_match"))
		daemon_shutdown(1, "No sjsms_response_match set!");
	else
		ctx->config.sjsms.responsematch = strdup(CONF("sjsms_response_match"));

	if (CONF("stat_interval"))
	  ctx->config.stat_interval = atoi(CONF("stat_interval"));

	ctx->config.statlevel = STATS_NONE;
	cp = config;

	while (cp) {
	  if (strcmp(cp->name, "stat_type") != 0) {
	    cp = cp->next;
	    continue;
	  }

	  if (strncmp(cp->value, "full", 5) == 0) {
	    ctx->config.statlevel = STATS_FULL;
	    break;
	  }
	  
	  if (strncmp(cp->value, "none", 5) == 0) {
	    ctx->config.statlevel = STATS_NONE;
	    break;
	  }
	  
	  if (strncmp(cp->value, "status", 7) == 0) ctx->config.statlevel |= STATS_STATUS;
	  if (strncmp(cp->value, "since_startup", 14) == 0) ctx->config.statlevel |= STATS_STATUS_BEGIN;
	  if (strncmp(cp->value, "delay", 6) == 0) ctx->config.statlevel |= STATS_DELAY;
	  cp = cp->next;
	}

	ctx->config.acctmask = 0x003f;

	*(ctx->last_rotate) = time(NULL);

	init_stats();

#ifdef DNSBL
	ctx->dnsbl = NULL;

	cp = config;
	while (cp) {
		if (strcmp(cp->name, "dnsbl") == 0)
			add_dnsbl(&ctx->dnsbl, cp->value, 1);
		cp = cp->next;
	}
#endif /* DNSBL */

	/* protocols */
	cp = config;
	while(cp) {
		if (strcmp(cp->name, "protocol") == 0) {
			if (strcmp(cp->value, "sjsms") == 0) 
				ctx->config.protocols |= PROTO_SJSMS;
			else if (strcmp(cp->value, "postfix") == 0) 
				ctx->config.protocols |= PROTO_POSTFIX;
		}
		cp = cp->next;
	}

	/* checks */
	cp = config;
	while(cp) {
		if (strcmp(cp->name, "check") == 0) {
			if (strcmp(cp->value, "dnsbl") == 0) 
				ctx->config.checks |= CHECK_DNSBL;
		}
		cp = cp->next;
	}

	/* log_ parameters */
	cp = config;
	while (cp) {
		if (strcmp(cp->name, "log_method") == 0) {
			if (strcmp(cp->value, "syslog") == 0) 
				ctx->config.flags |= FLG_SYSLOG;
		} else if (strcmp(cp->name, "log_level") == 0 && ctx->config.loglevel == 0) {
			/* only set loglevel if it's still unset */
			if (strcmp(cp->value, "debug") == 0)
				ctx->config.loglevel = GLOG_DEBUG;
			else if (strcmp(cp->value, "info") == 0)
				ctx->config.loglevel = GLOG_INFO;
			else if (strcmp(cp->value, "notice") == 0)
				ctx->config.loglevel = GLOG_NOTICE;
			else if (strcmp(cp->value, "warning") == 0)
				ctx->config.loglevel = GLOG_WARNING;
			else if (strcmp(cp->value, "error") == 0)
				ctx->config.loglevel = GLOG_ERROR;
			else daemon_shutdown(1, "Unknown log_level: %s", cp->value);
		} else if (strcmp(cp->name, "syslog_facility") == 0) {
			if (strcmp(cp->value, "mail") == 0)
				ctx->config.syslogfacility = LOG_MAIL;
			else if (strcmp(cp->value, "local0") == 0)
                                ctx->config.syslogfacility = LOG_LOCAL0; 
			else if (strcmp(cp->value, "local1") == 0)
                                ctx->config.syslogfacility = LOG_LOCAL1; 
			else if (strcmp(cp->value, "local2") == 0)
                                ctx->config.syslogfacility = LOG_LOCAL2; 
			else if (strcmp(cp->value, "local3") == 0)
                                ctx->config.syslogfacility = LOG_LOCAL3; 
			else if (strcmp(cp->value, "local4") == 0)
                                ctx->config.syslogfacility = LOG_LOCAL4; 
			else if (strcmp(cp->value, "local5") == 0)
                                ctx->config.syslogfacility = LOG_LOCAL5; 
			else if (strcmp(cp->value, "local6") == 0)
                                ctx->config.syslogfacility = LOG_LOCAL6; 
			else if (strcmp(cp->value, "local7") == 0)
                                ctx->config.syslogfacility = LOG_LOCAL7; 
			else daemon_shutdown(1, "Unknown syslog_facility: %s", cp->value);
		}
		cp = cp->next;
	}
	/* these should be set by now, at least via default config */
	assert(ctx->config.loglevel);
	assert(ctx->config.syslogfacility);
}

/* 
 * mrproper	- tidy upon exit
 */
void
mrproper(int signo)
{
  static int cleanup_in_progress = 0;

  if (cleanup_in_progress)
    raise(signo);

  cleanup_in_progress = 1;
  signal(SIGTERM, SIG_DFL);
  signal(SIGINT, SIG_DFL);
  raise(signo);
}

void
usage(void)
{
	printf("Usage: grossd [-dCDnrV] [-f configfile]\n");
	printf("       -d	Run grossd as a foreground process.\n");
	printf("       -C	create statefile\n");
	printf("       -D	Enable debug logging.\n");
	printf("       -f	override default configfile\n");
	printf("       -n	dry run: always send TRUST\n");
	printf("       -r	disable replication\n");
	printf("       -V	version information\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	int ret;
	update_message_t rotatecmd;
	time_t toleration;
	configlist_t *config;
	char *configfile = CONFIGFILE;
	extern char *optarg;
	extern int optind, optopt;
	int c;
	struct timespec *delay;

	/* mind the signals */
	signal(SIGHUP, SIG_IGN);
	signal(SIGALRM, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGTERM, &mrproper);
	signal(SIGINT, &mrproper);

	ctx = initialize_context();

	if ( ! ctx )
		daemon_shutdown(1, "Couldn't initialize context");

	/* command line arguments */
	while ((c = getopt(argc, argv, ":drf:VCDn")) != -1) {
		switch (c) {
		case 'd':
			ctx->config.flags |= FLG_NODAEMON;
			break;
		case 'n':
			ctx->config.flags |= FLG_DRYRUN;
			break;
		case 'f':
			configfile = optarg;
			break;
		case ':':
			fprintf(stderr,
				"Option -%c requires an operand\n", optopt);
			usage();
			break;
		case 'r':
                        ctx->config.flags |= FLG_NOREPLICATE;
			break;
		case 'V':
                        printf("grossd - Greylisting of Suspicious Sources. Version %s.\n", VERSION);
			exit(0);
                        break;
		case 'C':
                        ctx->config.flags |= FLG_CREATE_STATEFILE;
			break;
		case 'D':
			if (ctx->config.loglevel == GLOG_DEBUG)
				ctx->config.loglevel = GLOG_INSANE;
			else
				ctx->config.loglevel = GLOG_DEBUG;
			break;
		case '?':
			fprintf(stderr,
				"Unrecognized option: -%c\n", optopt);
			usage();
			break;
		}
	}

	config = read_config(configfile);
	configure_grossd(config);
	
	if ((ctx->config.flags & (FLG_NODAEMON | FLG_SYSLOG)) == FLG_SYSLOG) {
		openlog("grossd", LOG_ODELAY, ctx->config.syslogfacility);
	}

	/* daemonize must be run before any pthread_create */
	if ((ctx->config.flags & FLG_NODAEMON) == 0) 
		daemonize();


	/* initialize the update queue */
	delay = Malloc(sizeof(struct timespec));
	delay->tv_sec = ctx->config.greylist_delay;
	delay->tv_nsec = 0;
	ctx->update_q = get_delay_queue(delay);
	if (ctx->update_q < 0)
		daemon_perror("get_delay_queue");

	/* start the bloom manager thread */
	bloommgr_init();

	if ( (ctx->config.flags & FLG_NOREPLICATE) == 0) {
		syncmgr_init();
	}

	WITH_SYNC_GUARD(logstr(GLOG_INFO, "Filters in sync. Starting..."););
	
	/*
	 * now that we are in synchronized state we can start listening
	 * for client requests
	 */

	/* start the check pools */
#ifdef DNSBL
	if (ctx->config.checks & CHECK_DNSBL)
		dnsblc_init();
#endif /* DNSBL */

	/* start the worker thread */
	worker_init();

	/* start the server status thread */
	srvstatus_init();

	/*
	 * run some periodic maintenance tasks
	 */
	toleration = time(NULL);
	for ( ; ; ) {
		if ((time(NULL) - *ctx->last_rotate) > ctx->config.rotate_interval) {
			/* time to rotate filters */
			rotatecmd.mtype = ROTATE;
			ret = instant_msg(ctx->update_q, &rotatecmd, 0, 0);
			if (ret < 0)
				perror("rotate put_msg");
		}

		if (time(NULL) > ctx->stats.begin + ctx->config.stat_interval) {
		  log_stats();
		}

#ifdef DNSBL
		if (time(NULL) >= toleration + 10) {
			toleration = time(NULL);
			increment_dnsbl_tolerance_counters(ctx->dnsbl);
		}
#endif /* DNSBL */

		/* not so busy loop */
		sleep(1);
	}
}
