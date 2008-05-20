/* $Id$ */

/*
 * Copyright (c) 2007, 2008
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

#include "common.h"
#include "proto_sjsms.h"
#include "srvutils.h"
#include "worker.h"
#include "utils.h"

#include <libmilter/mfapi.h>

struct private_ctx_s
{
	char *client_address;
	char *sender;
	char *helo_name;
};

#define MILTER_PRIVATE	((struct private_ctx_s *) smfi_getpriv(milter_ctx))

/* internal functions */
sfsistat mlfi_connect(SMFICTX * milter_ctx, char *hostname, _SOCK_ADDR * hostaddr);
sfsistat mlfi_helo(SMFICTX * milter_ctx, char *helohost);
sfsistat mlfi_envfrom(SMFICTX * milter_ctx, char **argv);
sfsistat mlfi_envrcpt(SMFICTX * milter_ctx, char **argv);
sfsistat mlfi_close(SMFICTX * milter_ctx);
static void *milter_server(void *arg);

struct smfiDesc grossfilter = {
	"Gross",		/* filter name */
	SMFI_VERSION,		/* version code -- do not change */
	0,			/* flags */
	mlfi_connect,		/* connection info filter */
	mlfi_helo,		/* SMTP HELO command filter */
	mlfi_envfrom,		/* envelope sender filter */
	mlfi_envrcpt,		/* envelope recipient filter */
	NULL,			/* header filter */
	NULL,			/* end of header */
	NULL,			/* body block filter */
	NULL,			/* end of message */
	NULL,			/* message aborted */
	mlfi_close		/* connection cleanup */
};

sfsistat
mlfi_connect(SMFICTX * milter_ctx, char *hostname, _SOCK_ADDR * hostaddr)
{
	struct private_ctx_s *priv;
	struct sockaddr_in *client_saddr;
	char caddr[INET_ADDRSTRLEN + 1];

	logstr(GLOG_INSANE, "milter: connect");

	client_saddr = (struct sockaddr_in *)hostaddr;
	if (client_saddr->sin_family != AF_INET) {
		/* not supported */
		logstr(GLOG_INFO, "milter: unsupported protocol %d", client_saddr->sin_family);
		return SMFIS_ACCEPT;
	}

	if (NULL == inet_ntop(AF_INET, &client_saddr->sin_addr, caddr, INET_ADDRSTRLEN)) {
		logstr(GLOG_ERROR, "inet_top failed: %s", strerror(errno));
		return SMFIS_ACCEPT;
	}

	priv = Malloc(sizeof(*priv));
	bzero(priv, sizeof(*priv));
	priv->client_address = strdup(caddr);
	smfi_setpriv(milter_ctx, priv);

	return SMFIS_CONTINUE;
}

sfsistat
mlfi_helo(SMFICTX * milter_ctx, char *helohost)
{
	struct private_ctx_s *priv = MILTER_PRIVATE;

	logstr(GLOG_INSANE, "milter: helo");

	priv->helo_name = strdup(helohost);

	return SMFIS_CONTINUE;
}

sfsistat
mlfi_envfrom(SMFICTX * milter_ctx, char **argv)
{
	struct private_ctx_s *priv = MILTER_PRIVATE;

	priv->sender = strdup(argv[0]);

	logstr(GLOG_INSANE, "milter: envfrom: %s", priv->sender);

	return SMFIS_CONTINUE;
}

sfsistat
mlfi_envrcpt(SMFICTX * milter_ctx, char **argv)
{
	struct private_ctx_s *priv = MILTER_PRIVATE;
	grey_tuple_t *tuple;
	final_status_t *status;
	int retvalue = SMFIS_CONTINUE;
	int ret;

	logstr(GLOG_INSANE, "milter: envrcpt");

	tuple = request_new();
	status = init_status("milter");

	tuple->sender = strdup(priv->sender);
	tuple->recipient = strdup(argv[0]);
	tuple->client_address = strdup(priv->client_address);
	if (priv->helo_name)
		tuple->helo_name = strdup(priv->helo_name);

	ret = test_tuple(status, tuple, NULL);
	if (ret == 0) {
		switch (status->status) {
		case STATUS_GREY:
			smfi_setreply(milter_ctx, "452", "4.4.3",
			    status->reason ? status->reason : "Please try again later");
			retvalue = SMFIS_TEMPFAIL;
			break;
		case STATUS_BLOCK:
			smfi_setreply(milter_ctx, "550", "5.7.1",
			    status->reason ? status->reason : "Rejected by policy");
			retvalue = SMFIS_REJECT;
			break;
		default:
			retvalue = SMFIS_CONTINUE;
			break;
		}
	} else {
		logstr(GLOG_DEBUG, "test_tuple() returned <0");
		retvalue = SMFIS_CONTINUE;
	}

	finalize(status);
	request_unlink(tuple);
	return retvalue;
}

sfsistat
mlfi_close(SMFICTX * milter_ctx)
{
	struct private_ctx_s *priv = MILTER_PRIVATE;

	logstr(GLOG_INSANE, "milter: close");

	if (priv) {
		if (priv->sender)
			Free(priv->sender);
		if (priv->client_address)
			Free(priv->client_address);
		if (priv->helo_name)
			Free(priv->helo_name);
		Free(priv);
		smfi_setpriv(milter_ctx, NULL);
	}

	return SMFIS_CONTINUE;
}

static void *
milter_server(void *arg)
{
	int ret;

	ret = smfi_setconn(ctx->config.milter.listen);
	if (MI_FAILURE == ret)
		daemon_shutdown(EXIT_FATAL, "smfi_setconn failed");

	if (strncasecmp(ctx->config.milter.listen, "unix:", 5) == 0)
		unlink(ctx->config.milter.listen + 5);
	if (strncasecmp(ctx->config.milter.listen, "local:", 5) == 0)
		unlink(ctx->config.milter.listen + 6);

	ret = smfi_register(grossfilter);
	if (MI_FAILURE == ret)
		daemon_shutdown(EXIT_FATAL, "smfi_register failed");

	logstr(GLOG_DEBUG, "milter thread calling smfi_main()");
	smfi_main();

	/* NOTREACHED */
	pthread_exit(NULL);
}

static void *
milter_watcher(void *arg)
{
	int ret;

	ret = pthread_join(*ctx->process_parts.milter_server.thread, NULL);
	daemon_shutdown(EXIT_NOERROR, "milter exited");

	pthread_exit(NULL);
}

void
milter_server_init(void)
{
	logstr(GLOG_INFO, "starting milter policy server");

	/* the milter thread */
	create_thread(&ctx->process_parts.milter_server, 0, &milter_server, NULL);
	/* watcher thread */
	create_thread(NULL, DETACH, &milter_watcher, NULL);
}
