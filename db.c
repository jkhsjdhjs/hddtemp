/*
 * Copyright (C) 2002  Emmanuel VARAGNAT <coredump@free.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <regex.h>
#include <string.h>
#include <errno.h>
#include "db.h"
#include "i18n.h"

#define MAX_LINE_LEN           1024

struct harddrive_entry   *supported_drives = NULL;
struct harddrive_entry   **last_entry = &supported_drives;

struct harddrive_entry *is_a_supported_drive(const char* model) {
  struct harddrive_entry *p;

  if(model == NULL)
    return NULL;

  for(p = supported_drives; p; p = p->next) {
    regex_t     preg;
    regmatch_t  pmatch;

    if(regcomp(&preg, p->regexp, REG_EXTENDED))
      exit(-2);
    if(regexec(&preg, model, 1, &pmatch, 0) == 0)
      return p;
  }

  return NULL;
}


static const char *extract_string(char **string) {
  char *str;

  if(**string != '"')
    return NULL;
  else
    (*string)++;

  str = *string;

  for(; *str; str++) {
    switch(*str) {
      case '"':
	 return str; 
         break;
      case '\\':
         str++;
         break;
    }
  }

  return NULL;
}


static int parse_db_line(char *line) {
  const char             *regexp, 
                         *description;
  struct harddrive_entry *new_entry;
  unsigned short         value;
  char                   unit;
  char                   *p;

  line += strspn(line, " \t");
  if(*line == '#' || *line == '\0')
    return 0;

  /* extract regexp */
  p = (char*) extract_string(&line); 
  if(p == NULL || *p == '\0')
    return 1;

  *p = '\0';
  regexp = line;
  line = p + 1;

  /* extract value */
  line += strspn(line, " \t");
  if(! *line)
    return 1;

  p = line;
  value = 0;
  while(*p >= '0' && *p <= '9') {
    value = 10*value + *p-'0';
    p++;
  }
  if(*p == '\0' || ( *p != ' ' && *p != '\t') )
    return 1;
  line = p;

  /* extract temperature units */
  line += strspn(line, " \t");
  if(! *line)
    return 1;

  if((*line != 'C' && *line != 'F') || ( *(line+1) != ' ' && *(line+1) != '\t' ))
    return 1;

  unit = *(line++);

  /* extract description */
  line += strspn(line, " \t");
  if(! *line)
    return 1;

  p = (char*) extract_string(&line);
  if(p == NULL)
    return 1;

  description = line; 
  *p = '\0';

  /* add new entry to the list */
  new_entry = (struct harddrive_entry*) malloc(sizeof(struct harddrive_entry));
  if(new_entry == NULL) {
    perror("malloc");
    exit(-1);
  }
  
  new_entry->regexp       = strdup(regexp);
  new_entry->description  = strdup(description);
  new_entry->attribute_id = value;
  new_entry->unit         = unit;
  new_entry->next         = NULL;
  *last_entry = new_entry;
  last_entry = &(new_entry->next);

  return 0;
}

void load_database(const char* filename) {
  char  buff[MAX_LINE_LEN];
  char  *p, *s, *e, *ee;
  int   fd;
  int   n, rest, numline;

  errno = 0;
  fd = open(filename, O_RDONLY);
  if(fd == -1) {
    fprintf(stderr, _("hddtemp: can't open %1$s: %2$s\n"), filename, strerror(errno));
    exit(2);
  }

  numline = 0;
  rest = MAX_LINE_LEN;
  p = buff;
  while((n = read(fd, p, rest)) > 0) {
    s = buff;
    ee = p + n;
    while((e = memchr(s, '\n', ee-s))) {
      *e = '\0';
      numline++;
      if(parse_db_line(s)) {
	fprintf(stderr, _("ERROR: syntax error at line %1$d in %2$s\n"), numline, filename);
        exit(2);
      }
      s = e+1;
    }

    if(s == buff) {
      fprintf(stderr, _("  ERROR: a line exceed %1$d characters in %2$s.\n"), MAX_LINE_LEN, filename);
      close(fd);
      exit(2);
    }

    memmove(buff, s, ee-s); /* because memory can overlap */

    rest = MAX_LINE_LEN - (ee-s);
    p = buff + (ee-s);
  }

  close(fd);
}

