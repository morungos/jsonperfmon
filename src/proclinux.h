/* proclinux.h
 *
 * Implementation of the linux performance collector.
 *
 * As the module was first written on AIX plate-form using getproc64 interface,
 * this file is an getproc64 like for linux. This allow to keep a very closed
 * core program code available for those two OS.
 *
 * File begun on 2015-09-06 by Philippe Duveau as a rsyslog input module.
 *
 * Now it has been extracted to an external tool which can be integrated as
 * an event provider of rsyslog improg (new input module)
 *
 * Copyright 2019 Philippe Duveau and Pari Mutuel Urbain.
 *
 * This file is part of jsonperfmon.
 *
 * Jsonperfmon is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Jsonperfmon is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Jsonperfmon.  If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 */


#if !defined(_H_PROCLINUX) && !defined(_AIX)
#define	_H_PROCLINUX
#include <inttypes.h>

#ifndef MAX_PATH
#define MAX_PATH 256
#endif
#ifndef _AIX
#define SKPROC 0
#define SZOMB 'Z'
#endif

typedef struct procentry64 procentry64_t;

struct procentry64
{
	uint32_t      pi_pid;		/* process ID */
	char          pi_state;	/* process state */
	unsigned long pi_flags;	/* process flags */
	long long     pi_size;	/* size of image (pages) */
	char          pi_comm[MAX_PATH+1]; /* (truncated) program name */
	struct {
		uint64_t ru_utime;
		uint64_t ru_stime;
	}pi_ru;		/* this process' rusage info */
};

int	getprocs64(void *procsinfo, int sizproc, void *fdsinfo, int sizfd,
   	           pid_t *index, int count);

#endif	/* _H_PROCLINUX */
