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
#include "conf.h"
#include "srvutils.h"
#include "utils.h"

/*
 * add_config_item	- add an item to a linked list
 */
int
add_config_item(configlist_t **current, const char *name, const char *value)
{
	configlist_t *new;

        new = (configlist_t *)Malloc(sizeof(configlist_t));
	memset(new, 0, sizeof(configlist_t));

	new->name = name;
	new->value = value;
	new->next = *current;
	*current = new;
	return 1;
}

/*
 * dconf	- Return the configvalue if name is found from the config,
 *		  default otherwise.
 */
const char *
dconf(configlist_t *config, const char *name, const char *def)
{
	while (config) {
		if (strcmp(config->name, name) == 0)
			return config->value;
		config = config->next;
	}	
	return def;
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
 * read_config	- parse a configfile
 */
configlist_t *
read_config(const char *filename)
{
	configlist_t *config;
	int fd;
	char buffer[MAXLINELEN];
	char line[MAXLINELEN];
/* 	size_t llen, nlen, vlen; */
/* 	char *ptr; */
	int rlstatus, ret, i, count = 0;
	char *name[1];
	char *value[1];
	const char *valids[] = {
		VALID_NAMES,
		NULL
	};

	/* init */
	config = NULL;

	/* open configfile for reading */
	fd = open(filename, O_RDONLY);

	if (fd < 0)
		return NULL;

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
				ret = add_config_item(&config, strdup(*name), strdup(*value));
				if (ret < 0) {
					perror("add_config_item");
					exit(1);
				}
			} else {
				fprintf(stderr, "Unknown configuration parameter: %s\n", *name);
				exit(1);
			}
		}
	} while (rlstatus == DATA);

	/* check if a real error occurred */
	if (rlstatus == ERROR) {
		perror("readline");
		exit(1);
	}
	
	return config;
}
