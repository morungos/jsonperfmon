/* proclinux.c
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

#define PROCDIR "/proc"
#define FSDIRSEP "/"
#include <features.h>
#include <unistd.h>

#include <stdio.h>
#include <fcntl.h>

#include <sys/types.h>
#include <dirent.h>
#include <ctype.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>

#include "proclinux.h"

int getprocs64 (void *procsinfo, int sizproc __attribute__((unused)), void *fdsinfo __attribute__((unused)),
                int sizfd __attribute__((unused)), pid_t *idx __attribute__((unused)), int count)
{
	DIR *proc = opendir(PROCDIR);
	char path[100], *subpath, *filepath;
	struct dirent *dent;
	int nb = 0;
	procentry64_t *data = (procentry64_t*) procsinfo;
	static long int jiffies = 0;

	if (!jiffies) jiffies = sysconf(_SC_CLK_TCK);

	if (!proc) return -1;

	if (!procsinfo) {
	while ((dent = readdir(proc)) != NULL) {
	if(*dent->d_name < '1' || *dent->d_name > '9')
	continue;
	nb++;
	}
	closedir(proc);
	return nb;
	}

	strcpy(path, PROCDIR FSDIRSEP );

	subpath = path + sizeof(PROCDIR);

	while ((dent = readdir(proc)) != NULL && nb < count) {
		if(*dent->d_name < '1' || *dent->d_name > '9')
			continue;
		unsigned long utime = 0, stime = 0, vsize = 0, lflag;
		long rss;
		strcpy(subpath, dent->d_name);
		filepath = subpath + strlen(dent->d_name);
		strcpy(filepath, FSDIRSEP "stat");
		int hf = open(path, O_RDONLY, 0666);
		if (hf > -1) {
			char line[512];
			ssize_t s = read(hf, line, sizeof(line));
			if (s < 1 || s == sizeof(line))
			return -1;

			sscanf(line,"%" SCNu32 " %*s %c %*d %*d "
			    "%*d %*d %*d %lu %*u "
			    "%*u %*u %*u %lu %lu "
			    "%*d %*d %*d %*d %*d "
			    "%*d %*u %lu %ld",
			    &(data[nb].pi_pid) /*pid*/, &(data[nb].pi_state) /*state*/,  /*ppid*/ /*pgrp*/
			    /*session*/ /*tty_nr*/ /*tpgid*/ &lflag /*flags*/, /*minflt*/
			    /*cminflt*/ /*majflt*/ /*cmajflt*/ &utime /*utime*/, &stime /*stime*/,
			    /*cutime*/ /*cstime*/ /*priority*/ /*nice*/ /*num_threads*/
			    /*itrealvalue*/ /*starttime*/ &vsize /*vsize*/, &rss /*rss*/);
			close(hf);
			if (!(lflag & 0x80000000)) {
				ssize_t lenname = 0;
				strcpy(filepath, FSDIRSEP "status");
				hf = open(path, O_RDONLY, 0666);
				if (hf > -1) {
					s = lseek(hf, 6, SEEK_SET);
					if (s == 6) {
						s = read(hf, data[nb].pi_comm, MAX_PATH);
						for (;lenname < s && data[nb].pi_comm[lenname] != '\n'; 
								lenname++)
							;
						data[nb].pi_comm[lenname] = '\0';
					}
				close(hf);
				}
				else
					*data[nb].pi_comm = '\0';

				data[nb].pi_size = vsize >> 10;
				data[nb].pi_ru.ru_stime = stime * 1000 / jiffies;
				data[nb].pi_ru.ru_utime = utime * 1000 / jiffies;
				nb++;
			}
		}
	}
	closedir(proc);
	return nb;
}
