/*
 * Copyright (c) 2006 Eino Tuominen <eino@utu.fi>
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
#include "dnsblc.h"
#include "srvutils.h"
#include "utils.h"

int
add_dnsbl(dnsbl_t **current, const char *name, int weight)
{
	dnsbl_t *new;
	sem_t *sp;
	int ret;

	logstr(GLOG_INFO, "adding dnsbl: %s", name);
	
	new = Malloc(sizeof(dnsbl_t));
	memset(new, 0, sizeof(dnsbl_t));

#ifdef USE_SEM_OPEN
	/* make sure we get a private semaphore */
        ret = sem_unlink("sem_dnsbl");
        if (ret == -1 && errno == EACCES) {
                perror("sem_unlink");
		return -1;
	}
        sp = sem_open("sem_dnsbl", O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, (unsigned int) ERRORTOLERANCE);
        if (sp == (sem_t *)SEM_FAILED) {
                daemon_perror("sem_open");
		return -1;
	}
	/* we do not need the named semaphore, so it's fine to delete the name */
        ret = sem_unlink("sem_dnsbl");
#else
	sp = Malloc(sizeof(sem_t));

	ret = sem_init(sp, 0, (unsigned int) ERRORTOLERANCE);
	if (ret != 0) {
		perror("sem_init");
		return -1;
	}
#endif /* USE_SEM_OPEN */

	new->name = strdup(name);
	new->failurecount_sem = sp;
	new->next = *current;
	*current = new;
	return 1;
}

int
query_clearance(dnsbl_t *dnsbl)
{
/* 	int ret; */
	char *errorstr;
	
	if (sem_trywait(dnsbl->failurecount_sem) < 0) {
		if (errno == EAGAIN)
			return FALSE;
		errorstr = strerror(errno);
		logstr(GLOG_ERROR, "Error in query_clearance() -> sem_trywait(): %s", errorstr);
	}
	return TRUE;
}

int
tolerate_dnsbl(dnsbl_t *dnsbl)
{
	int sval;
	int ret;

	/* increment counter if needed */
	ret = sem_getvalue(dnsbl->failurecount_sem, &sval);
	if (ret < 0) {
		logstr(GLOG_CRIT, "sem_getvalue failed for dnsbl %s", dnsbl->name);
		/* increment anyway */
		sval = 0;
	} 
	if (sval < ERRORTOLERANCE) {
		logstr(GLOG_INFO, "incrementing tolerance counter for dnsbl %s", dnsbl->name);
		sem_post(dnsbl->failurecount_sem);
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
		logstr(GLOG_INFO, "dns-match: %s for %s",
			cba->dnsbl->name, cba->client_address);
		acctstr(ACCT_DNS_MATCH, "%s for %s", cba->dnsbl->name, cba->client_address);
	}

	if (*cba->timeout) {
		logstr(GLOG_INFO, "dns-timeout: %s for %s",
			cba->dnsbl->name, cba->client_address);
		acctstr(ACCT_DNS_TMOUT, "%s for %s", cba->dnsbl->name, cba->client_address);
	} else {
		sem_post(cba->dnsbl->failurecount_sem);
	}

	free(cba);
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
dnsblc(const char *client_address, tmout_action_t *ta)
{
	ares_channel channel;
	int nfds, count, ret;
	int match_found = 0, timeout = 0;
	fd_set readers, writers;
	struct timeval tv;
	struct timespec ts, start, now, timeleft;
	char buffer[MAXQUERYSTRLEN];
	char *query;
	char *ipstr;
	dnsbl_t *dnsbl;
	callback_arg_t *callback_arg;
	int timeused;

	logstr(GLOG_DEBUG, "dnsblc called");

	ipstr = strdup(client_address);

	if (strlen(ipstr) > INET_ADDRSTRLEN - 1) {
		logstr(GLOG_ERROR, "invalid ipaddress: %s", ipstr);
		free(ipstr);
		return -1;
	}

	ret = reverse_inet_addr(ipstr);
	if (ret < 0) {
		free(ipstr);
		return -1;
	}
	
	if (ares_init(&channel) != ARES_SUCCESS) {
		perror("ares_init");
		free(ipstr);
		return -1;
	}
	
	dnsbl = ctx->dnsbl;

	/* initiate dnsbl queries */
	while (dnsbl) {
		assert(dnsbl->name);
		snprintf(buffer, MAXQUERYSTRLEN, "%s.%s", ipstr, dnsbl->name);
		query = strdup(buffer);
		if (query_clearance(dnsbl) == TRUE) {
			logstr(GLOG_INSANE, "initiating dnsbl query: %s for %s (%s)", query);
			/* we should only count the queries, not log them */
			/* acctstr(ACCT_DNS_QUERY, "%s for %s (%s)", dnsbl->name, client_address, query); */

			callback_arg = Malloc(sizeof(callback_arg_t));
			callback_arg->dnsbl = dnsbl;
			callback_arg->matches = &match_found;
			callback_arg->timeout = &timeout;
			callback_arg->client_address = client_address;
			ares_gethostbyname(channel, query, PF_INET, &addrinfo_callback, callback_arg);
		} else {
			logstr(GLOG_INFO, "Skipping dnsbl %s due to timeouts.", dnsbl->name);
			acctstr(ACCT_DNS_SKIP, "%s for %s (%s)", dnsbl->name, client_address, query);
		}
		free(query);
		dnsbl = dnsbl->next;
	}

	/* build default entry, if timeout not given */
	if (! ta) {
		ta = Malloc(sizeof(tmout_action_t));
		ta->timeout = 10000;		/* 10 seconds */
		ta->action = NULL;
		ta->next = NULL;
	}

	clock_gettime(CLOCK_TYPE, &start);

	while (ta) {
		do {
			clock_gettime(CLOCK_TYPE, &now);
			timeused = ms_diff(&now, &start);
			if (timeused > ta->timeout)
				break;

			mstotimespec(ta->timeout - timeused, &timeleft);

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

		if (match_found)
			break;
	
		if (timeused > ta->timeout) {
			if (ta->action)
				ta->action(ta->arg, timeused);
			if (! ta->next)
				/* the final timeout value */
				timeout = 1;
		}

		ta = ta->next;
	}

	ares_destroy(channel);
	free(ipstr);

	logstr(GLOG_DEBUG, "dnsblc returning");
	return match_found;
}
