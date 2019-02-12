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
		  read(hf, line, sizeof(line));

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
			    read(hf, data[nb].pi_comm, 6);
			    while (read(hf, data[nb].pi_comm + lenname, 1) == 1 &&
			        data[nb].pi_comm[lenname] != '\n' &&
			        (++lenname) < MAX_PATH)
			      ;
				  data[nb].pi_comm[lenname] = '\0';
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
