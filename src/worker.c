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

#include "common.h"
#include "srvutils.h"
#include "syncmgr.h"

#ifdef DNSBL
#include "check_dnsbl.h"
#endif

#include "msgqueue.h"
#include "worker.h"
#include "utils.h"

/* these are implemented in worker_*.c */
void postfix_server_init();
void sjsms_server_init();
void milter_server_init();

/* internals */
void update_counters(int status);
char *grey_mask(char *ipstr);

/*
 * destructor for client_info_t
 */
void
free_client_info(client_info_t *arg)
{
	if (arg->caddr)
		Free(arg->caddr);
	if (arg->ipstr)
		Free(arg->ipstr);
	if (arg->message)
		Free(arg->message);
	Free(arg);
}

/*
 * destructor for grey_tuple_t
 */
void
request_unlink(grey_tuple_t *request)
{
	int ret;

	ret = pthread_mutex_lock(&request->reference.mx);
	assert(request);
	assert(0 == ret);
	assert(request->reference.count > 0);

	request->reference.count = request->reference.count - 1;
	if (request->reference.count == 0) {
		/* last reference */
		if (request->sender)
			Free(request->sender);
		if (request->recipient)
			Free(request->recipient);
		if (request->client_address)
			Free(request->client_address);
		pthread_mutex_unlock(&request->reference.mx);
		Free(request);
	} else {
		pthread_mutex_unlock(&request->reference.mx);
	}
}

grey_tuple_t *
request_new()
{
	grey_tuple_t *request;

	request = Malloc(sizeof(grey_tuple_t));
	bzero(request, sizeof(grey_tuple_t));

	pthread_mutex_init(&request->reference.mx, NULL);
	request->reference.count = 1;

	return request;
}

char *
grey_mask(char *ipstr)
{
	int ret;
	unsigned int ip, net, mask;
	const char *ptr = NULL;
	char masked[INET_ADDRSTRLEN] = { '\0' };
	struct in_addr inaddr;

	/*
	 * apply checkmask to the ip 
	 */
	if (strlen(ipstr) > INET_ADDRSTRLEN) {
		logstr(GLOG_NOTICE, "invalid ipaddress: %s", ipstr);
		return NULL;
	}

	ret = inet_pton(AF_INET, ipstr, &inaddr);
	switch (ret) {
	case -1:
		logstr(GLOG_ERROR, "test_tuple: inet_pton: %s", strerror(errno));
		return NULL;
		break;
	case 0:
		logstr(GLOG_ERROR, "not a valid ip address: %s", ipstr);
		return NULL;
		break;
	}

	/* case default */
	ip = inaddr.s_addr;

	/* this is 0xffffffff ^ (2 ** (32 - mask - 1) - 1) */
	mask = 0xffffffff ^ ((1 << (32 - ctx->config.grey_mask)) - 1);

	/* ip is in network order */
	net = ip & htonl(mask);

	ptr = inet_ntop(AF_INET, &net, masked, INET_ADDRSTRLEN);
	if (!ptr) {
		logstr(GLOG_ERROR, "test_tuple: inet_ntop: %s", strerror(errno));
		return NULL;
	}
	return strdup(masked);
}

void
update_counters(int status)
{
	/* Update counters */
	switch (status) {
	case STATUS_BLOCK:
		logstr(GLOG_INSANE, "updating block counters", status);
		INCF_STATS(block);
		INCF_STATS(all_block);
		break;
	case STATUS_MATCH:
		logstr(GLOG_INSANE, "updating match counters", status);
		INCF_STATS(match);
		INCF_STATS(all_match);
		break;
	case STATUS_GREY:
		logstr(GLOG_INSANE, "updating grey counters", status);
		INCF_STATS(greylist);
		INCF_STATS(all_greylist);
		break;
	case STATUS_TRUST:
		logstr(GLOG_INSANE, "updating trust counters", status);
		INCF_STATS(trust);
		INCF_STATS(all_trust);
		break;
	}
}

int
test_tuple(final_status_t *final, grey_tuple_t *request, tmout_action_t *ta)
{
	char maskedtuple[MSGSZ];
	char *chkipstr = NULL;
	sha_256_t digest;
	update_message_t update;
	int ret;
	int retvalue = STATUS_UNKNOWN;
	oper_sync_t os;
	edict_t *edict = NULL;
	poolresult_message_t message;
	chkresult_t *result = NULL;
	struct timespec start, now;
	mseconds_t timeused;
	tmout_action_t *tap = NULL;
	tmout_action_t *ta_default_reserved = NULL;
	int i;
	int checks_running;
	int definitives_running;
	int checkcount;
	int susp_weight = 0;
	int block_threshold;
	int grey_threshold;
	bool free_ta = false;
	judgment_t judgment;
	bool definitive;
	char *reasonstr = NULL;
	querylog_entry_t *querylog_entry;

	/* record the processing start time */
	clock_gettime(CLOCK_TYPE, &start);

	/* default value */
	final->status = STATUS_FAIL;

	block_threshold = ctx->config.block_threshold;
	grey_threshold = ctx->config.grey_threshold;

	/* apply grey_mask for client_address */
	chkipstr = grey_mask(request->client_address);
	if (NULL == chkipstr) {
		logstr(GLOG_ERROR, "applying grey_mask failed: %s", request->client_address);
		return -1;
	}

	/* greylist */
	snprintf(maskedtuple, MSGSZ, "%s %s %s", chkipstr, request->sender, request->recipient);
	digest = sha256_string(maskedtuple);

	querylog_entry = &final->querylog_entry;

	querylog_entry->sender = request->sender;
	querylog_entry->recipient = request->recipient;
	querylog_entry->helo = request->helo_name;
	querylog_entry->client_ip = request->client_address;

	logstr(GLOG_INSANE, "checking ip=%s, net=%s", request->client_address, chkipstr);

	Free(chkipstr);

	/* how many checks to run */
	i = 0;
	while (ctx->checklist[i])
		++i;
	checkcount = i;

	/* check status */
	if (is_in_ring_queue(ctx->filter, digest)) {
		retvalue = STATUS_MATCH;
	} else if (0 == checkcount) {
		/* traditional greylister */
		retvalue = STATUS_GREY;
	} else {
		/* build default entry, if timeout not given */
		if (!ta) {
			free_ta = true;
			ta = Malloc(sizeof(tmout_action_t));
			ta->timeout = ctx->config.query_timelimit;
			ta->action = NULL;
			ta->next = NULL;
			ta_default_reserved = ta;
		}

		/* Write the edict */
		edict = edict_get(false);
		edict->job = (void *)request;
		tap = ta;
		while (tap) {
			edict->timelimit += tap->timeout;
			tap = tap->next;
		}

		/* submit jobs for checks */
		i = 0;
		definitives_running = 0;
		while (ctx->checklist[i]) {
			request->reference.count++;
			submit_job(ctx->checklist[i]->pool, edict);
			if (ctx->checklist[i]->definitive)
				definitives_running++;
			i++;
		}
		checks_running = i;

		/* judgment is the final combined result of all checks */
		judgment = J_UNDEFINED;

		/*
		 * definitive boolean is used to test if we can short cut 
		 * from waiting all the cheks complete. We must wait all
		 * the definitive checks to complete, that is all tests which
		 * can return a STATUS_TRUST or STATUS_BLOCK response.
		 */
		definitive = false;

		/* 
		 * wait until a definitive result arrives, every check has
		 * returned or timeout is reached.
		 */
		while (definitive == false && checks_running > 0 && ta) {
			clock_gettime(CLOCK_TYPE, &now);
			timeused = ms_diff(&now, &start);
			/* make sure timeleft != 0 as it would cause get_msg_timed to block */
			if (timeused < ta->timeout) {
				ret = get_msg_timed(edict->resultmq, &message,
				    sizeof(message.result), 0, ta->timeout - timeused);
				if (ret > 0) {
					/* We've got a response */
					result = (chkresult_t *)message.result;
					if (NULL == result) {
						/*
						 * FIXME: we do not know if the failed check was definitive
						 * so we end up waiting until all checks return
						 */
						logstr(GLOG_DEBUG,
						    "NULL check result received (pool exhausted)");
						checks_running--;
						/*
						 * Because the request never reached its destination
						 * we have to unlink it here
						 */
						request_unlink(request);
					} else {
						logstr(GLOG_INSANE,
						    "Received a check result, judgment = %d, weight = %d",
						    result->judgment, result->weight);
						/* was this a final result from the check? */
						if (!result->wait)
							checks_running--;
						/* update the judgment */
						judgment = MAX(judgment, result->judgment);
						susp_weight += result->weight;

						/* update querylog entry */
						if (result->judgment != J_UNDEFINED)
							record_match(querylog_entry, result);

						/* was this a definitive result? */
						if (result->definitive)
							definitives_running--;
						if (result->reason) {
							reasonstr = strdup(result->reason);
							Free(result->reason);
						}
						Free(result);
					}
					/*
					 * Do we have a definitive result so far?
					 * That is,
					 * 1.  we have a whitelist match, or
					 * 2a. all the definitive checks have returned, and
					 * 2b. susp_weight > grey_threshold
					 */
					if (judgment == J_PASS
					    || (0 == definitives_running
						&& block_threshold != 0 && susp_weight >= block_threshold))
						definitive = true;
				}
			} else if (ta->action) {
				ta->action(ta->arg, timeused);
				ta = ta->next;
			} else {
				ta = ta->next;
			}
		}

		/* we don't want more results */
		edict->obsolete = true;

		/* Let's sum up the results */
		switch (judgment) {
		case J_PASS:
			retvalue = STATUS_TRUST;
			break;
		case J_BLOCK:
			retvalue = STATUS_BLOCK;
			break;
		case J_SUSPICIOUS:
			if (block_threshold > 0 && susp_weight >= block_threshold) {
				retvalue = STATUS_BLOCK;
				reasonstr = strdup(ctx->config.block_reason);
			} else if (grey_threshold > 0 && susp_weight >= grey_threshold) {
				retvalue = STATUS_GREY;
			} else {
				retvalue = STATUS_TRUST;
			}
			break;
		case J_UNDEFINED:
			retvalue = STATUS_TRUST;
			break;
		default:
			/* this should never happen */
			retvalue = STATUS_TRUST;
			break;
		}

		edict_unlink(edict);
	}

	/* update the querylog entry */
	querylog_entry->action = retvalue;

	/* we cannot free(ta) if we got it as parameter */
	if (free_ta)
		Free(ta_default_reserved);

	if (((retvalue == STATUS_GREY) || (retvalue == STATUS_MATCH))
	    || (ctx->config.flags & FLG_UPDATE_ALWAYS)) {
		/* update the filter */
		update.mtype = UPDATE;
		memcpy(update.mtext, &digest, sizeof(sha_256_t));
		ret = put_msg(ctx->update_q, &update, sizeof(sha_256_t), 0);
		if (ret < 0)
			gerror("update put_msg");

		/* update peer */
		if (connected(&(ctx->config.peer))) {
			os.digest = digest;
			logstr(GLOG_INSANE, "Sending oper sync");
			send_oper_sync(&(ctx->config.peer), &os);
		}
	}

	/* check if DRYRUN is enabled */
	if (ctx->config.flags & FLG_DRYRUN)
		retvalue = STATUS_TRUST;

	update_counters(retvalue);

	final->status = retvalue;
	if (reasonstr)
		final->reason = reasonstr;
	return 0;
}

int
process_parameter(grey_tuple_t *tuple, const char *str)
{
	char *match;

	/* matching switch */
	do {
		match = try_match("sender=", str);
		if (match) {
			tuple->sender = match;
			logstr(GLOG_DEBUG, "sender=%s", match);
			continue;
		}
		match = try_match("recipient=", str);
		if (match) {
			tuple->recipient = match;
			logstr(GLOG_DEBUG, "recipient=%s", match);
			continue;
		}
		match = try_match("client_address=", str);
		if (match) {
			tuple->client_address = match;
			logstr(GLOG_DEBUG, "client_address=%s", match);
			continue;
		}
		match = try_match("helo_name=", str);
		if (match) {
			tuple->helo_name = match;
			logstr(GLOG_DEBUG, "helo_name=%s", match);
			continue;
		}
		/* no match */
		return -1;
	} while (0);

	/* match */
	return 0;
}

int
check_request(grey_tuple_t *tuple)
{
	if (tuple->sender && tuple->recipient && tuple->client_address) {
		return 0;
	} else {
		return -1;
	}
}

char *
try_match(const char *matcher, const char *matchee)
{
	if (strncmp(matcher, matchee, strlen(matcher)) == 0)
		/* found a match, return part after the match */
		return strdup(matchee + strlen(matcher));
	else
		return NULL;
}

final_status_t *
init_status(const char *proto)
{
	final_status_t *status;

	status = Malloc(sizeof(final_status_t));
	memset(status, 0, sizeof(final_status_t));

	status->querylog_entry.proto = proto;
	clock_gettime(CLOCK_TYPE, &status->starttime);

	return status;
}

/*
 * record_match         - add checkresult info to the query log entry
 */
void
record_match(querylog_entry_t *q, chkresult_t *r)
{
	check_match_t *m, *n;

	m = Malloc(sizeof(check_match_t));
	memset(m, 0, sizeof(check_match_t));
	if (r->checkname)
		/*             m->name = strdup(r->checkname); */
		m->name = r->checkname;
	else
		m->name = strdup("<anonymous>");
	m->weight = r->weight;
	m->next = NULL;

	q->totalweight += m->weight;

	/*
	 * insert entry to the end of the linked list
	 * to preserve order
	 */
	n = q->match;
	if (n) {
		while (n->next)
			n = n->next;
		n->next = m;
	} else {
		q->match = m;
	}
}

void
update_delay_stats(querylog_entry_t *q)
{
	switch (q->action) {
	case STATUS_BLOCK:
		block_delay_update((double)q->delay);
		break;
	case STATUS_MATCH:
		match_delay_update((double)q->delay);
		break;
	case STATUS_GREY:
		greylist_delay_update((double)q->delay);
		break;
	case STATUS_TRUST:
		trust_delay_update((double)q->delay);
		break;
	default:
		/* FIX: count errors */
		;
	}
}

void
finalize(final_status_t *status)
{
	struct timespec now;
	check_match_t *m, *n;
	querylog_entry_t *q;

	q = &status->querylog_entry;

	clock_gettime(CLOCK_TYPE, &now);
	q->delay = ms_diff(&now, &status->starttime);

	update_delay_stats(q);

	querylogwrite(q);

	n = q->match;
	m = n;
	while (n) {
		n = m->next;
		Free(m);
		m = n;
	}

	if (status->reason)
		Free(status->reason)
		    Free(status);
}

void
querylogwrite(querylog_entry_t *q)
{
	char line[MAXLINELEN];
	char buffer[MAXLINELEN];
	char *actionstr;
	check_match_t *m;

	switch (q->action) {
	case STATUS_GREY:
		actionstr = "greylist";
		break;
	case STATUS_MATCH:
		actionstr = "match";
		break;
	case STATUS_TRUST:
		actionstr = "trust";
		break;
	case STATUS_UNKNOWN:
		actionstr = "unknown";
		break;
	case STATUS_FAIL:
		actionstr = "fail";
		break;
	case STATUS_BLOCK:
		actionstr = "block";
		break;
	default:
		daemon_shutdown(EXIT_FATAL, "querylogwrite: unknown statuscode %d", q->action);
		break;
	}

	if (NULL == q->proto)
		q->proto = "N/A";
	if (NULL == q->client_ip)
		q->client_ip = "N/A";
	if (NULL == q->sender)
		q->sender = "N/A";
	if (NULL == q->recipient)
		q->recipient = "N/A";
	if (NULL == q->helo)
		q->helo = "N/A";

<<<<<<< .working
	snprintf(line, MAXLINELEN - 1, "a=%s d=%d w=%d c=%s s=%s r=%s h=%s", actionstr, q->delay, q->totalweight,  q->client_ip, q->sender, q->recipient, q->helo);
=======
	snprintf(line, MAXLINELEN - 1, "a=%s d=%d w=%d c=%s s=%s r=%s", actionstr, q->delay, q->totalweight,
	    q->client_ip, q->sender, q->recipient);

	if (q->helo) {
		snprintf(buffer, MAXLINELEN - 1, " h=%s", q->helo);
		strncat(line, buffer, MAXLINELEN - 1);
	}
>>>>>>> .merge-right.r314

	m = q->match;
	while (m) {
		snprintf(buffer, MAXLINELEN - 1, " m=%s", m->name);
		strncat(line, buffer, MAXLINELEN - 1);
		if (m->weight) {
			snprintf(buffer, MAXLINELEN - 1, "%+d", m->weight);
			strncat(line, buffer, MAXLINELEN - 1);
		}
		m = m->next;
	}

	logstr(GLOG_INFO, line);
}

void
worker_init()
{
	if (ctx->config.protocols == 0)
		logstr(GLOG_NOTICE, "No protocols configured");
	if (ctx->config.protocols & PROTO_POSTFIX)
		postfix_server_init();
	if (ctx->config.protocols & PROTO_SJSMS)
		sjsms_server_init();
#ifdef MILTER
	if (ctx->config.protocols & PROTO_MILTER)
		milter_server_init();
#endif
}
