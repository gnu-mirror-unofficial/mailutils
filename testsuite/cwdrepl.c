/*

NAME

   cwdrepl - replace occurrences of CWD with .

SYNOPSIS

   COMMAND | cwdrepl [DIR REPL]...

DESCRIPTION

   Some testcases operate programs that produce full file names as part
   of their output.  To make this output independent of the actual file
   location, this tool replaces every occurrence of the current working
   directory with a dot.  Both logical (as given by the PWD environment
   variable) and physical (as returned by getcwd(3)) locations are replaced.

   The same effect could have been achieved by using "pwd -P", "pwd -L"
   and sed, but this would pose portability problems.

   Additionally, any number of DIR REPL pairs can be supplied in the command
   line.  Each pair instructs the tool to replace every occurrence of DIR
   with REPL on output.  Note that these pairs take precedence over the
   default ones, so running "cwdrepl $PWD 'PWD'" will replace occurrences
   of the logical current working directory name with the string PWD, instead
   of the default dot.

LICENSE
   Copyright (C) 2017-2020 Sergey Poznyakoff

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 3 of the License, or (at your
   option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program. If not, see <http://www.gnu.org/licenses/>. 
   
HISTORY

   2017-06-07  The program appeared in the GNU mailutils testsuite (see
               commit 453cd17f7a4be5ceaa8411a8a3ebd9fddd88df8e).
   2019-11-29  Make sure the longest possible match is replaced.
               (ibid., commit 5ccef4cfd1eb3252430f04fa8418268a93ff8b08).
   2020-03-13  Port to standard libc for use in other projects.
   
*/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>

struct dirtrans {
    char *dir;
    size_t dirlen;
    char const *trans;
    ssize_t translen;
    struct dirtrans *next;
};

struct dirtrans *dir_head, *dir_tail;
size_t dir_count;

static int
dirtranscmp(struct dirtrans const *trans1, struct dirtrans const *trans2)
{
    size_t l1 = strlen (trans1->dir);
    size_t l2 = strlen (trans2->dir);
    if (l1 < l2)
	return 1;
    else if (l1 > l2)
	return -1;
    return strcmp(trans2->dir, trans1->dir);
}

static struct dirtrans *
dirtrans_alloc(char const *dir, char const *trans)
{
    size_t dirlen = strlen(dir);
    size_t translen = strlen(trans);
    struct dirtrans *dt = malloc(sizeof *dt);
    
    assert(dt!=NULL);
	
    while (dirlen > 0 && dir[dirlen-1] == '/')
	dirlen--;

    dt->dir = malloc(dirlen + 1);
    assert(dt->dir != NULL);
    memcpy(dt->dir, dir, dirlen);
    dt->dir[dirlen] = 0;
    dt->dirlen = dirlen;
    dt->trans = trans;
    dt->translen = translen;
    dt->next = NULL;

    return dt;
}

static void
dirtrans_free(struct dirtrans *dt)
{
    if (dt) {
	free(dt->dir);
	free(dt);
    }
}

static void
newdir(char const *dir, char const *trans)
{
    struct dirtrans *l, *m, *dt;
    size_t i, n, count;

    dt = dirtrans_alloc(dir, trans);
    
    l = dir_head;
    if (l) {
	if (dirtranscmp(l, dt) > 0) {
	    l = NULL;
	} else if (dirtranscmp(dir_tail, dt) < 0) {
	    l = dir_tail;
	} else {
	    count = dir_count;
	    while (count) {
		int c;
		
		n = count / 2;
		for (m = l, i = 0; i < n; m = m->next, i++)
		    ;
				
		c = dirtranscmp(m, dt);
		if (c == 0) {
		    dirtrans_free(dt);
		    return;
		} else if (n == 0) {
		    break;
		} else if (c < 0) {
		    l = m;
		    count -= n;
		} else {
		    count = n;
		}
	    }
	}
    }

    if (!l) {
	if (dir_head == NULL)
	    dir_head = dir_tail = dt;
	else {
	    dt->next = dir_head;
	    dir_head = dt;
	}
    } else {
	while (l->next && dirtranscmp(l->next, dt) < 0)
	    l = l->next;
	
	dt->next = l->next;
	l->next = dt;
	if (!dt->next)
	    dir_tail = dt;
    }
    dir_count++;
}

static char *
bufrealloc(char *buf, size_t *psize)
{
    size_t size = *psize;
    
    if (size == 0) {
	size = 64;
	buf = malloc(64);
    } else {
	if ((size_t) -1 / 3 * 2 <= size) {
	    errno = ENOMEM;
	    return NULL;
	} 
	size += (size + 1) / 2;
	buf = realloc(buf, size);
    }
    *psize = size;
    return buf;
}

static char *
getcwd_alloc(void)
{
    size_t size = 0;
    char *buf = NULL;
    char *p;
    
    do {
	buf = bufrealloc(buf, &size);
	if (!buf) {
	    perror("getcwd_alloc");
	    abort();
	}
    } while ((p = getcwd(buf, size)) == NULL && errno == ERANGE);
    return buf;
}

ssize_t
input(char **pbuf, size_t *psize)
{
    char *buf = *pbuf;
    size_t size = *psize;
    ssize_t off = 0;

    do {
	if (size == 0 || off + 1 == size) {
	    buf = bufrealloc(buf, &size);
	    if (!buf) {
		perror("input");
		abort();
	    }
	    
	}
	if (!fgets(buf + off, size - off, stdin)) {
	    if (off == 0)
		return -1;
	    break;
	}
	off += strlen(buf + off);
    } while (buf[off - 1] != '\n');

    buf[--off] = 0;
    *pbuf = buf;
    *psize = size;
    
    return off;
}

static inline int
isbnd(int c)
{
    return iscntrl(c) || ispunct(c) || isspace(c);
}

int
main(int argc, char **argv)
{
    size_t i;
    char *buf = NULL;
    size_t size = 0;
    ssize_t n;
    
    assert(argc % 2 != 0);
    for (i = 1; i < argc; i += 2)
	newdir(argv[i], argv[i + 1]);
    
    newdir(getenv("PWD"), ".");
    newdir (getcwd_alloc(), ".");
    
    while ((n = input(&buf, &size)) >= 0) {
	struct dirtrans *dt;
	
	for (dt = dir_head; dt; dt = dt->next) {
	    size_t start = 0;
	    char *p;
	    
	    while ((p = strstr(buf + start, dt->dir))) {
		if (isbnd(p[dt->dirlen])) {
		    size_t off = p - buf;
		    size_t rest = n - start;
		    ssize_t d = (ssize_t) dt->translen - dt->dirlen;
		    
		    if (d > 0) {
			if (n + d + 1 > size) {
			    size = n + d + 1;
			    buf = realloc(buf, size);
			    assert(buf != NULL);
			    p = buf + off;
			}
		    }
		    
		    memmove(p + dt->translen, p + dt->dirlen,
			    rest - dt->dirlen + 1);
		    memcpy(p, dt->trans, dt->translen);
		  
		    n += d;
		    start = off + dt->translen;
		} else
		    start++;
	    }
	}
	fwrite(buf, n, 1, stdout);
	putchar('\n');
    }
    return 0;
}

/* Local Variables: */
/* mode: c */
/* c-basic-offset: 4 */
/* End: */
