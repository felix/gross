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
#include "check_dnsbl.h"
#include "srvutils.h"
#include "utils.h"
#include "worker.h"

/* the cleanup routine */
int
cleanup_dnsblc(void *state)
{
	ares_channel channel;
	
	channel = (ares_channel)state;
	ares_destroy(channel);
	return 0;
}

int
add_dnsbl(dnsbl_t **current, const char *name, int weight)
{
	dnsbl_t *new;

	logstr(GLOG_INFO, "adding dnsbl: %s", name);
	
	new = Malloc(sizeof(dnsbl_t));
	memset(new, 0, sizeof(dnsbl_t));

	/*
	 * this is not threadsafe, but we do not need
	 * an exact result, we can afford errors here 
	 */
	new->tolerancecounter = ERRORTOLERANCE;

	new->name = strdup(name);
	new->next = *current;
	*current = new;
	return 1;
}

int
query_clearance(dnsbl_t *dnsbl)
{
	int retvalue;

	if (dnsbl->tolerancecounter > 0) {
		retvalue = TRUE;
	} else {
		retvalue = FALSE;
	}
	return retvalue;
}

int
tolerate_dnsbl(dnsbl_t *dnsbl)
{
	/* increment counter if needed */
	if (dnsbl->tolerancecounter < ERRORTOLERANCE) {
		logstr(GLOG_INFO, "incrementing tolerance counter for dnsbl %s", dnsbl->name);
		dnsbl->tolerancecounter++;
	}
	return 0;
}

int
increment_dnsbl_tolerance_counters(dnsbl_t *dnsbl)
{
	while (dnsbl) {
		tolerate_dnsbl(dnsbl);
		dnsbl = dnsbl->next;
	}

	return 0;
}

static void
addrinfo_callback(void *arg, int status, struct hostent *host)
{
	callback_arg_t *cba;

	cba = (callback_arg_t *)arg;

	if (status == ARES_SUCCESS) {
		*cba->matches = 1;
		stat_dnsbl_match(cba->dnsbl->name);
		logstr(GLOG_DEBUG, "dns-match: %s for %s",
			cba->dnsbl->name, cba->client_address);
		acctstr(ACCT_DNS_MATCH, "%s for %s", cba->dnsbl->name, cba->client_address);
	}

	if (*cba->timeout) {
		logstr(GLOG_DEBUG, "dns-timeout: %s for %s",
			cba->dnsbl->name, cba->client_address);
		acctstr(ACCT_DNS_TMOUT, "%s for %s", cba->dnsbl->name, cba->client_address);
		/* decrement tolerancecounter */
		cba->dnsbl->tolerancecounter--;
	}

	Free(cba);
}


/*
 * reverse_inet_addr	- reverse ipaddress string for dnsbl query
 *                        e.g. 1.2.3.4 -> 4.3.2.1
 */
int
reverse_inet_addr(char *ipstr)
{
	unsigned int ipa, tmp;
	int i;
	int ret;
	struct in_addr inaddr;
	const char *ptr;
        char tmpstr[INET_ADDRSTRLEN];
	size_t iplen;

	if ((iplen = strlen(ipstr)) > INET_ADDRSTRLEN) {
		fprintf(stderr, "invalid ipaddress: %s\n", ipstr);
		return -1;
	}

	ret = inet_pton(AF_INET, ipstr, &inaddr);
        switch (ret) {
        case -1:
                perror("reverse_inet_addr inet_pton");
                return -1;
                break;
        case 0:
                logstr(GLOG_ERROR, "not a valid ip address: %s", ipstr);
                return -1;
                break;
        }

        /* case default */
	ipa = inaddr.s_addr;

	tmp = 0;

	for (i = 0; i < 4; i++) {
		tmp = tmp << 8;
		tmp |= ipa & 0xff;
		ipa = ipa >> 8;
	}

        /* this tmpstr hack here is because at least FreeBSD
         * seems to handle buffer lengths differently from
         * Linux and Solaris. Specifically, with
         * inet_ntop(AF_INET, &tmp, ipstr, iplen) one gets
         * a truncated address in ipstr in FreeBSD.
         */
        ptr = inet_ntop(AF_INET, &tmp, tmpstr, INET_ADDRSTRLEN);
	if (! ptr) {
		perror("inet_ntop");
		return -1;
	}
	
        assert(strlen(tmpstr) == iplen);
        strncpy(ipstr, tmpstr, iplen);

	return 0;
}

int
dnsblc(thread_ctx_t *thread_ctx, edict_t *edict)
{
	ares_channel channel;
	int nfds, count, ret;
	int match_found = 0;
	int timeout = 0;
	fd_set readers, writers;
	struct timeval tv;
	struct timespec ts, start, now, timeleft;
	char buffer[MAXQUERYSTRLEN];
	char *query;
	char *ipstr;
	dnsbl_t *dnsbl;
	callback_arg_t *callback_arg;
	int timeused;
	const char *client_address;
	chkresult_t *result;
	grey_tuple_t *request;

	logstr(GLOG_DEBUG, "dnsblc called");

	request = (grey_tuple_t *)edict->job;
	client_address = request->client_address;
	assert(client_address);

	result = (chkresult_t *)Malloc(sizeof(chkresult_t));
	memset(result, 0, sizeof(*result));

	ipstr = strdup(client_address);

	if (strlen(ipstr) > INET_ADDRSTRLEN - 1) {
		logstr(GLOG_ERROR, "invalid ipaddress: %s", ipstr);
		Free(ipstr);
		goto FINISH;
	}

	ret = reverse_inet_addr(ipstr);
	if (ret < 0) {
		Free(ipstr);
		goto FINISH;
	}
	
	/* initialize if we are not yet initialized */
	if (NULL == thread_ctx->state) {
		channel = Malloc(sizeof(channel));
		if (ares_init(&channel) != ARES_SUCCESS) {
			perror("ares_init");
			Free(ipstr);
			goto FINISH;
		}
		thread_ctx->state = channel;
		thread_ctx->cleanup = &cleanup_dnsblc;
	} else {
		channel = (ares_channel)thread_ctx->state;
	}

	dnsbl = ctx->dnsbl;

	/* initiate dnsbl queries */
	while (dnsbl) {
		assert(dnsbl->name);
		snprintf(buffer, MAXQUERYSTRLEN, "%s.%s", ipstr, dnsbl->name);
		query = strdup(buffer);
		if (query_clearance(dnsbl) == TRUE) {
			logstr(GLOG_INSANE, "initiating dnsbl query: %s", query);
			/* we should only count the queries, not log them */
			/* acctstr(ACCT_DNS_QUERY, "%s for %s (%s)", dnsbl->name, client_address, query); */

			callback_arg = Malloc(sizeof(callback_arg_t));
			callback_arg->dnsbl = dnsbl;
			callback_arg->matches = &match_found;
			callback_arg->timeout = &timeout;
			callback_arg->client_address = client_address;
			ares_gethostbyname(channel, query, PF_INET, &addrinfo_callback, callback_arg);
		} else {
			logstr(GLOG_DEBUG, "Skipping dnsbl %s due to timeouts.", dnsbl->name);
			acctstr(ACCT_DNS_SKIP, "%s for %s (%s)", dnsbl->name, client_address, query);
		}
		Free(query);
		dnsbl = dnsbl->next;
	}

	clock_gettime(CLOCK_TYPE, &start);

	while (! timeout) {
		do {
			clock_gettime(CLOCK_TYPE, &now);
			timeused = ms_diff(&now, &start);
			if (timeused > edict->timelimit)
				break;

			mstotimespec(edict->timelimit - timeused, &timeleft);

			FD_ZERO(&readers);
			FD_ZERO(&writers);
			nfds = ares_fds(channel, &readers, &writers);
			if (nfds == 0)
				break;
			ares_timeout(channel, NULL, &tv);
			tvtots(&tv, &ts);
			
			if (ms_diff(&timeleft, &ts) < 0)
				memcpy(&ts, &timeleft, sizeof(timeleft));

			tstotv(&ts, &tv);
			

			count = select(nfds, &readers, &writers, NULL, &tv);
			ares_process(channel, &readers, &writers);
		} while (!match_found);

		if (match_found || nfds == 0)
			break;
	
		logstr(GLOG_INSANE, "debug before %d", match_found);
		if (timeused > edict->timelimit) {
			/* the final timeout value */
			timeout = 1;
		}
	}

	Free(ipstr);

FINISH:
	if (match_found > 0)
		result->judgment = J_SUSPICIOUS;
	else
		result->judgment = J_UNDEFINED;
	send_result(edict, result);
	
	logstr(GLOG_DEBUG, "dnsblc returning");
	request_unlink(request);

	return 0;
}

void
dnsbl_init()
{
	thread_pool_t *pool;

	/* initialize the thread pool */
        logstr(GLOG_INFO, "initializing dnsbl checker thread pool");
	pool = create_thread_pool("dnsbl", &dnsblc);
        if (pool == NULL)
                daemon_perror("create_thread_pool");

	register_check(pool, false);
}
