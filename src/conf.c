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
#include "conf.h"
#include "srvutils.h"
#include "utils.h"

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
 * add_config_item	- add an item to a linked list
 */
int
add_config_item(configlist_t **current, const char *name, const char *value, bool is_default)
{
	configlist_t *new;

        new = (configlist_t *)Malloc(sizeof(configlist_t));
	memset(new, 0, sizeof(configlist_t));

	new->name = name;
	new->value = value;
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
record_config_item(configlist_t **config, const char *name, const char *value)
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
		add_config_item(config, name, value, false);
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
			add_config_item(config, name, value, false);
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
 * namevalue	- search a name = value pair from the buffer
 */
int
namevalue(char *buffer, char **name, char **value)
{
	char *ptr;
	
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
	ptr = strchr(buffer, '=');
	if (ptr) {
		/* delimiter found, possible match */
		*name = buffer;
		*ptr = '\0';
		*value = ptr + 1;

		if (!trim(name))
			return -1;
		if (!trim(value))
			return -1;
		return 1;
	}
	if(trim(&buffer)) {
		/* couldn't parse buffer */
		return -1;
	} else {
		/* an empy line or a line containing only whitespace */
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
		add_config_item(&config, defaults[i], defaults[i+1], true);
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

		ret = namevalue(buffer, name, value);
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
				/*
				 * we must strdup() name and value because those are just
				 * part of the working buffer which will be overwritten on
				 * every cycle of this loop
				 */
				record_config_item(&config, strdup(*name), strdup(*value));
				if (ret < 0)
					daemon_perror("record_config_item");
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
		daemon_perror("readline");
	
	return config;
}
