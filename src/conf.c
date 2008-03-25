/*
 * Copyright (c) 2006,2007,2008
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
#include "conf.h"
#include "srvutils.h"
#include "utils.h"

int a_delim_b(char *buffer, char delim, char **stra, char **strb);

/*
 * multivalue		- checks if name is a multivalue property
 */
int
multivalue(const char *name) {
	int i;
	const char *multivalues[] = {
		MULTIVALUES,
		NULL
	};

	i = 0;
	while (multivalues[i]) {
		if (strcmp(name, multivalues[i]) == 0) 
			return 1;
		i++;
	}
	return 0;
}

/*
 * maxparams		- returns maximum parameters for a config option
 */
uint32_t
maxparams(const char *name) {
	int i;
	const char *paramcounts[] = {
		PARAMS,
		NULL
	};

	i = 0;
	while (paramcounts[i]) {
		if (strcmp(name, paramcounts[i]) == 0) 
			return atoi(paramcounts[i+2]);
		i++;
	}
	return 0;
}

/*
 * minparams		- returns minimum parameters for a config option
 */
uint32_t
minparams(const char *name) {
	int i;
	const char *paramcounts[] = {
		PARAMS,
		NULL
	};

	i = 0;
	while (paramcounts[i]) {
		if (strcmp(name, paramcounts[i]) == 0) 
			return atoi(paramcounts[i+1]);
		i++;
	}
	return 0;
}

/*
 * add_config_item	- add an item to a linked list
 */
int
add_config_item(configlist_t **current, const char *name, const char *value, params_t *params, bool is_default)
{
	configlist_t *new;

        new = (configlist_t *)Malloc(sizeof(configlist_t));
	memset(new, 0, sizeof(configlist_t));

	new->name = name;
	new->value = value;
	new->params = params;
	new->is_default = is_default;
	new->next = *current;
	*current = new;
	return 1;
}

/*
 * record_config_item	- replace if name already exists in config and name not
 * a multivalue property, add otherwise
 */
int
record_config_item(configlist_t **config, const char *name, const char *value, params_t *params)
{
	configlist_t *cp, *prev, *delete;

	cp = *config;
	prev = NULL;

	if (multivalue(name)) {
		/* remove default values */
		while (cp) {
			if ((cp->is_default == true) && strcmp(cp->name, name) == 0) {
				delete = cp;
				if (prev) 
					prev->next = cp->next;
				cp = cp->next;
				Free(delete);
			} else {
				prev = cp;
				cp = cp->next;
			}
		}
		add_config_item(config, name, value, params, false);
	} else {
		while (cp) {
			if (strcmp(cp->name, name) == 0) {
				cp->value = value;
				break;
			}
			cp = cp->next;
		}
		if (cp == NULL) {
			/* name not found in configlist */
			add_config_item(config, name, value, params, false);
		}
	}
	return 1;
}

/*
 * gconf	- Return the configvalue if name is found from the config,
 *		  NULL otherwise.
 */
const char *
gconf(configlist_t *config, const char *name)
{
	while (config) {
		if (strcmp(config->name, name) == 0)
			return config->value;
		config = config->next;
	}	
	return NULL;
}

/*
 * namevalueparams	- search a name = value pair and check
 *			  if value has optional parameters
 */
int
namevalueparams(char *buffer, char **name, char **value, params_t **params)
{
	char *ptr;
	params_t *p;
	char *head;
	char *tail;
	int ret;
	
        /*
         * *name and *value will point inside the buffer
         * or NULL if no pair is found
         */
	*name = *value = NULL;

	/* first search the possible comment character */
	ptr = strchr(buffer, '#');
	if (ptr)
		*ptr = '\0';

	/*  search the delimiter (== '=')*/
	ret = a_delim_b(buffer, '=', name, value);
	if (1 == ret) {
		/* success */
		*params = NULL;
		tail = *value;
		/* loop through value to find any optional parameters */
		while (1 == a_delim_b(tail, ';', &head, &tail)) {
			p = *params;
			/* first node? */
			if (NULL == *params) {
				*params = p = Malloc(sizeof(p));
				memset(p, 0, sizeof(p));
			} else {
				/* find the end of the list */
				while (p->next)
					p = p->next;
				p->next = Malloc(sizeof(p->next));
				p = p->next;
				memset(p, 0, sizeof(p));
			}
 			/* p->value contains whole tail, so strdup must be done at the later stage */
			p->value = tail;
			p->next = NULL;
		}
		/* now that the params list is splitted with \0-characters we can strdup those values */
		p = *params;
		while (p) {
			p->value = strdup(p->value);
			p = p->next;
		}
		*name = strdup(*name);
		*value = strdup(*value);
		return 1;
	} else {
		return ret;
	}
}

/*
 * a_delim_b	- search stra delim strb pair from the buffer
 *		  and trim whitespace
 */
int
a_delim_b(char *buffer, char delim, char **stra, char **strb)
{
	char *ptr;
	
        /*
         * *stra and *strb will point inside the buffer
         * or NULL if no pair is found
         */
	*stra = *strb = NULL;

	/*  search the delimiter */
	ptr = strchr(buffer, delim);
	if (ptr) {
		/* delimiter found, possible match */
		*stra = buffer;
		*ptr = '\0';
		*strb = ptr + 1;

		if (!trim(stra))
			return -1;
		if (!trim(strb))
			return -1;
		assert(*stra);
		assert(*strb);
		return 1;
	} else {
		return 0;
	}
}

/*
 * default_config	- build the default configuration
 */
configlist_t *
default_config(void)
{
	configlist_t *config;
	int i;
	const char *defaults[] = {
		DEFAULT_CONFIG,
		NULL
	};
	
	/* init */
	config = NULL;

	i = 0;
	while (defaults[i]) {
		assert(defaults[i]);
		assert(defaults[i+1]);
		add_config_item(&config, defaults[i], defaults[i+1], NULL, true);
		i += 2;
	}
	return config;
}

/*
 * read_config	- parse a configfile
 */
configlist_t *
read_config(const char *filename)
{
	configlist_t *config;
	int fd;
	char buffer[MAXLINELEN];
	char line[MAXLINELEN];
	int rlstatus, ret, i, count = 0;
	char *name[1];
	char *value[1];
	params_t *params[1];
	params_t *paramptr;
	int paramcount;
	const char *valids[] = {
		VALID_NAMES,
		NULL
	};
	const char *deprecated[] = {
		DEPRECATED_NAMES,
		NULL
	};

	/* init */
	config = default_config();

	/* open configfile for reading */
	fd = open(filename, O_RDONLY);

	if (fd < 0)
		return config;

	/*
         * Process the config file.
	 */
	do {
		count++;
		rlstatus = readline(fd, buffer, MAXLINELEN);
		if (rlstatus != DATA)
			break;

		strncpy(line, buffer, MAXLINELEN);

		ret = namevalueparams(buffer, name, value, params);
		if (ret < 0) {
			fprintf(stderr, "Couldn't parse line %d: %s\n", count, line);
			exit(1);
		} else if (ret) {
			i = 0;
			while (valids[i]) {
				if (strcmp(*name, valids[i]) == 0)
					break;
				i++;
			}
			if (valids[i]) {
				paramptr = *params;
				for (paramcount = 0; paramptr; paramptr = paramptr->next) paramcount++;
				if (paramcount <= maxparams(*name) && paramcount >= minparams(*name)) {
					record_config_item(&config, *name, *value, *params);
					if (ret < 0)
						daemon_fatal("record_config_item");
				} else {
					daemon_shutdown(1, "Invalid parameter count for configuration parameter: %s", *name);
				}
			} else {
				i = 0;
				while (deprecated[i]) {
					if (strcmp(*name, deprecated[i]) == 0)
						break;
					i++;
				}
				if (deprecated[i])
					daemon_shutdown(1, "Deprecated configuration parameter: %s", *name);
				else
					daemon_shutdown(1, "Unknown configuration parameter: %s", *name);
			}
		}
	} while (rlstatus == DATA);

	/* check if a real error occurred */
	if (rlstatus == ERROR)
		daemon_fatal("readline");
	
	return config;
}
