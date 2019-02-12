/* perflinux.c
 *
 * Implementation of the linux performance collector.
 *
 * As the module was first written on AIX plate-form using libperfstat interface,
 * this file is an libperfstat like for linux. This allow to keep a very closed
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

#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <sys/types.h>

#include "perflinux.h"

#define PROCDIR "/proc"
#define FC_HOSTDIR "/sys/class/fc_host"
#define FSDIRSEP "/"

typedef struct {
  char RxWords[70], TxWords[70], ErrorFrames[70], DumpedFrames[70], LinkFailureCount[70];
  char name[50];
} fc_apadter_t;

struct {
  fc_apadter_t *sys_fc_host;
  int nb;
} fc_host = { NULL, -1 };

struct {
  uint64_t ts;
  uint64_t mhz;
  int nbcpu;
} perfunix_cpu_data = { 0, 0, -1};

void perfunix_clean_all()
{
  free(fc_host.sys_fc_host);
}

int perfunix_cpu_total(perfunix_id_t *name __attribute__((unused)), perfunix_cpu_total_t *userbuff,
                 int sizeof_userbuff, int desired_number __attribute__((unused)))
{
  FILE *f;
  char buf[512], *p;

  if (!userbuff || sizeof_userbuff < (int)sizeof(perfunix_cpu_total_t))
    return -1;

  if (perfunix_cpu_data.nbcpu == -1)
  {
    perfunix_cpu_data.nbcpu = 0;
    if ((f = fopen(PROCDIR FSDIRSEP "cpuinfo", "r")) != NULL)
    {
      while (fgets(buf,512,f))
      {
        if (buf[4] == 'M')
        {
            perfunix_cpu_data.nbcpu++;
            if (!perfunix_cpu_data.mhz)
                perfunix_cpu_data.mhz = strtoull(buf+11, NULL, 10);
        }
      }
      fclose(f);
    }
  }

  userbuff->ncpus = perfunix_cpu_data.nbcpu;

  uint64_t *ui64_buf = (uint64_t*)buf;
  uint32_t *ui32_buf = (uint32_t*)buf;
  if ((f = fopen(PROCDIR FSDIRSEP "stat", "r")) != NULL)
  {
    while (fgets(buf,512,f))
    {
      if(*ui32_buf == *((uint32_t*)"ctxt") )
        userbuff->pswitch = strtoull(buf+5, NULL, 10);     /* number of process switches (change in currently running process) */
      else if(*ui64_buf == *((uint64_t*)"procs_bl"))
        userbuff->runque = strtoull(buf+14, NULL, 10);     /* number of process switches (change in currently running process) */
      else if(*ui32_buf == *((uint32_t*)"cpu ") || *ui32_buf == *((uint32_t*)"cpu\t"))
      {
        userbuff->puser = strtoull(buf+4, &p, 10) + (p) ? strtoull(++p, &p, 10) : 0;
        userbuff->psys  = (p) ? strtoull(++p, &p, 10) : 0;
        userbuff->pidle = (p) ? strtoull(++p, &p, 10) : 0;
        userbuff->pwait = (p) ? strtoull(++p, &p, 10) : 0;
      }
    }
    fclose(f);
  }
  if ((f = fopen(PROCDIR FSDIRSEP "loadavg", "r")) != NULL)
  {
    size_t l;
    fgets(buf,512,f);
    for (l = 0, p=buf; l<3; l++, p++)
      userbuff->loadavg_dbl[l] = strtod(p, &p);
    fclose(f);
  }

  userbuff->processorHZ  = perfunix_cpu_data.mhz*1000000;
  userbuff->ncpus = perfunix_cpu_data.nbcpu;

  return 1;
}

int perfunix_cpu(perfunix_id_t *name __attribute__((unused)), perfunix_cpu_t* userbuff,
                 int sizeof_userbuff, int desired_number)
{
  FILE *f;
  char buf[512], *p;

  if (userbuff == NULL && desired_number == 0)
  {
		if (perfunix_cpu_data.nbcpu == -1)
		{
      perfunix_cpu_data.nbcpu = 0;
			if ((f = fopen(PROCDIR FSDIRSEP "cpuinfo", "r")) != NULL)
			{
				while (fgets(buf, 512, f))
				{
					if (buf[4] == 'M')
					{
						perfunix_cpu_data.nbcpu++;
						if (!perfunix_cpu_data.mhz)
							perfunix_cpu_data.mhz = strtoull(buf + 14, NULL, 10);
					}
				}
			}
		}
		return perfunix_cpu_data.nbcpu;
  }

  if (userbuff == NULL || sizeof_userbuff<(int)sizeof(perfunix_cpu_t))
    return -1;

  size_t s = 0;

  uint32_t *ui32_buf = (uint32_t*)buf;
  uint16_t *ui16_buf = (uint16_t*)buf;

  if ((f = fopen(PROCDIR FSDIRSEP "stat", "r")) != NULL)
  {
    while (fgets(buf,512,f) && s < (size_t)desired_number)
    {
      if(*ui32_buf == *((uint32_t*)"cpu ") || *ui32_buf == *((uint32_t*)"cpu\t"))
        continue;
      if(*ui16_buf == *((uint16_t*)"cp"))
      {
        if ((p = strpbrk(buf+4," \t")) != NULL)
        {
          userbuff[s].user = strtoull(++p, &p, 10);
          userbuff[s].user += (p) ? strtoull(++p, &p, 10) : 0;
          userbuff[s].sys  = (p) ? strtoull(++p, &p, 10) : 0;
          userbuff[s].idle = (p) ? strtoull(++p, &p, 10) : 0;
          userbuff[s++].wait = (p) ? strtoull(++p, &p, 10) : 0;
        }
      }
    }
    fclose(f);
  }
	return s;
}

int perfunix_memory_total(perfunix_id_t *name0 __attribute__((unused)), perfunix_memory_total_t* userbuff,
                         int sizeof_userbuff __attribute__((unused)), int desired_number __attribute__((unused)))
{
  FILE *f;
  char buf[512];

  if ((f = fopen(PROCDIR FSDIRSEP "meminfo", "r")) != NULL)
  {
    while (fgets(buf,512,f))
    {
      if (*buf == 'M')
      {
        if (buf[3] == 'T')
          userbuff->real_total = strtoull(buf+10, NULL, 10);
        if (buf[3] == 'F')
          userbuff->real_free = strtoull(buf+9, NULL, 10);
      }
      if (*buf == 'S' && buf[1] == 'w')
      {
        switch (buf[4])
        {
        case 'F':
          userbuff->pgsp_free = strtoull(buf+10, NULL, 10);
          break;
        case 'T':
          userbuff->pgsp_total = strtoull(buf+11, NULL, 10);
          break;
        default:
          ;
        }
      }
      if (*buf == 'A' && buf[1] == 'c' && buf[6] == ':')
        userbuff->virt_active = strtoull(buf+7, NULL, 10);
      if (*buf == 'I' && buf[8] == ':')
        userbuff->virt_total = strtoull(buf+9, NULL, 10);
      if (*buf == 'H')
      {
        switch (buf[10])
        {
        case 'F':
          userbuff->huge_free = strtoull(buf+16, NULL, 10);
          break;
        case 'T':
          userbuff->huge_total = strtoull(buf+17, NULL, 10);
          break;
        case 'z':
          userbuff->huge_size = strtoull(buf+14, NULL, 10);
          break;
        default:
          ;
        }
      }
    }
    fclose(f);
  }
  if ((f = fopen(PROCDIR FSDIRSEP "vmstat", "r")) != NULL)
  {
    while (fgets(buf,512,f))
    {
      if (*buf == 'p')
      {
        if (buf[1] == 's')
        {
          if (buf[4] == 'i')
            userbuff->pgins = strtoull(buf+7, NULL, 10);
          else
            userbuff->pgouts = strtoull(buf+8, NULL, 10);
        }
        else
        {
          if (buf[3] == 'g')
          {
            if (buf[4] == 'i')
              userbuff->pgspins = strtoull(buf+7, NULL, 10);
            if (buf[4] == 'o')
              userbuff->pgspouts = strtoull(buf+8, NULL, 10);
          }
          else
            if (buf[2] == 'f' && buf[3] == 'a')
              userbuff->pgexct = strtoull(buf+8, NULL, 10);

        }
      }
    }
    fclose(f);
  }
  return 1;
}

int perfunix_pagingspace(perfunix_id_t *name __attribute__((unused)),
                         perfunix_pagingspace_t* userbuff,
                         int sizeof_userbuff,
                         int desired_number __attribute__((unused)))
{
  static int nb_lines = -1;
  FILE *f;
  char buf[512];

  if (userbuff == NULL && desired_number == 0)
  { // initialize number of lines
    if (nb_lines == -1)
    {
      if ((f = fopen(PROCDIR FSDIRSEP "swaps", "r")) != NULL)
      {
        while (fgets(buf,512,f))
          nb_lines++;
        fclose(f);
      }
      else
        nb_lines = 0;
    }
    return nb_lines;
  }

  if (userbuff == NULL || sizeof_userbuff<(int)sizeof(perfunix_pagingspace_t))
    return -1;

  int ret = 0;
  if ((f = fopen(PROCDIR FSDIRSEP "swaps", "r")) != NULL)
  {
    nb_lines = 0;
    fgets(buf,512,f); // suppress first line
    while (fgets(buf,512,f))
    {
      nb_lines++;
      char *p = strpbrk(buf, " \t");
      if (p && ret < desired_number)
      {
        *p++ = '\0';
        strcpy(userbuff->name, buf);
        for (;p && (*p==' '|| *p=='\t');p++) ; // find next value
        for (;p && *p!=' ' && *p!='\t';p++) ; // find next space
        for (;p && (*p==' '|| *p=='\t');p++) ; // find next value
        userbuff->mb_size = (p) ? strtoll(p, &p, 10) >> 10 : 0;
        userbuff->mb_used = (p) ? strtoll(++p, &p, 10) >> 10 : 0;
        userbuff->type = LV_PAGING;
        ret++;
      }
    }
    fclose(f);
  }
  else
    return -1;
  return ret;
}

int perfunix_disk(perfunix_id_t *name __attribute__((unused)),
                               perfunix_disk_t* userbuff,
                               int sizeof_userbuff,
                               int desired_number)
{
  static int nb_lines = -1;
  FILE *f;
  char buf[512];
  uint32_t *ui32_buf = (uint32_t*)buf;

  if (userbuff == NULL && desired_number == 0)
  { // initialize number of lines
    if (nb_lines == -1)
    {
      nb_lines = 0;
      if ((f = fopen(PROCDIR FSDIRSEP "diskstats", "r")) != NULL)
      {
        while (fgets(buf,512,f)) {
          if ((*ui32_buf != *((uint32_t*)"   8") || buf[16] != ' ')  &&
               *ui32_buf != *((uint32_t*)" 253"))
            continue;

          nb_lines++;

        }
        fclose(f);
      }
    }
    return nb_lines;
  }

  if (userbuff == NULL || sizeof_userbuff<(int)sizeof(perfunix_disk_t))
    return -1;

  int ret = 0;
  if ((f = fopen(PROCDIR FSDIRSEP "diskstats", "r")) != NULL)
  {
    nb_lines = 0;
    while (fgets(buf,512,f))
    {
      if ((*ui32_buf != *((uint32_t*)"   8") || buf[16] != ' ')  &&
           *ui32_buf != *((uint32_t*)" 253"))
        continue;

      nb_lines++;
      char *p = strpbrk(buf+13, " \t");
      if (p && ret < desired_number)
      {
        *p++ = '\0';
        strcpy(userbuff[ret].name, buf+13);
        userbuff[ret].rfers = (p) ? strtoull(p, &p, 10) : 0; // read IO
        userbuff[ret].q_sampled = (p) ? strtoull(++p, &p, 10) : 0; // read merged
        userbuff[ret].rblks = (p) ? strtoull(++p, &p, 10) : 0; // read sectors
        userbuff[ret].rserv = (p) ? strtoull(++p, &p, 10) * 1000: 0; // read tim
        userbuff[ret].wfers = (p) ? strtoull(++p, &p, 10) : 0; // write IO
        userbuff[ret].wq_sampled = (p) ? strtoull(++p, &p, 10) : 0; // write merged
        userbuff[ret].wblks = (p) ? strtoull(++p, &p, 10) : 0; // write sectors
        userbuff[ret].wserv =(p) ? strtoull(++p, &p, 10) * 1000 : 0; // write time
        userbuff[ret].wq_depth = (p) ? strtoull(++p, &p, 10) : 0;// in_flight
        userbuff[ret].time = (p) ? strtoull(++p, &p, 10) : 0; // time active
        userbuff[ret].wq_time = (p) ? strtoull(++p, &p, 10) : 0; // wait time
        ret++;
      }
    }
    fclose(f);
  }
  else
    return -1;
  return ret;
}

int perfunix_protocol( perfunix_id_t *name,
                       perfunix_protocol_t* userbuff,
                       int sizeof_userbuff,
                       int desired_number __attribute__((unused)))
{
  FILE *f;
  char buf[512];

  if (userbuff == NULL || sizeof_userbuff<(int)(sizeof(perfunix_protocol_t)))
    return -1;

  char key = '\0';
  uint64_t *tab = NULL;
  uint32_t length = 0;
  if (strcmp(name->name, "nfsv3")){
    key = '3';
    tab = &(userbuff->u.nfsv3.client.null);
    length = sizeof(userbuff->u.nfsv3.client)/sizeof(userbuff->u.nfsv3.client.null)-1;
  }
  else if (strcmp(name->name, "nfsv4")){
    key = '4';
    tab = &(userbuff->u.nfsv4.client.null);
    length = sizeof(userbuff->u.nfsv4.client)/sizeof(userbuff->u.nfsv4.client.null)-1;
  }

  if ((f = fopen(PROCDIR FSDIRSEP "net" FSDIRSEP "rpc" FSDIRSEP "nfs", "r")) != NULL)
  {
    uint32_t s;
    while (fgets(buf,512,f))
    {
      if (buf[4] != key)
        continue;

      char *p = strpbrk(buf+6, " \t");

      for (s = 0; p && s < length; s++) {
          userbuff->u.calls += tab[s] = strtoull(++p, &p, 10);
      }
      break; // line found
    }
    fclose(f);
  }
  else
    return -1;
  return 1;
}

int perfunix_netinterface(perfunix_id_t *name __attribute__((unused)),
                                 perfunix_netinterface_t* userbuff,
                                 int sizeof_userbuff,
                                 int desired_number)
{
  static int nb_lines = -1;
  FILE *f;
  char buf[512];
  uint16_t *ui16_buf = (uint16_t*)(buf+4);

  if (userbuff == NULL && desired_number == 0)
  { // initialize number of lines
    if (nb_lines == -1)
    {
      nb_lines = 0;
      if ((f = fopen(PROCDIR FSDIRSEP "net" FSDIRSEP "dev", "r")) != NULL)
      {
        fgets(buf,512,f);
        fgets(buf,512,f);
        while (fgets(buf,512,f)) {
          if (*ui16_buf == *((uint16_t*)"lo"))
            continue;
          nb_lines++;
        }
        fclose(f);
      }
    }
    return nb_lines;
  }

  if (userbuff == NULL || sizeof_userbuff<(int)sizeof(perfunix_netinterface_t))
    return -1;

  int ret = 0;
  if ((f = fopen(PROCDIR FSDIRSEP "net" FSDIRSEP "dev", "r")) != NULL)
  {
    fgets(buf,512,f);
    fgets(buf,512,f);
    while (fgets(buf,512,f)) {
      if (*ui16_buf == *((uint16_t*)"lo"))
        continue;
      char *n, *p;
      for (n=buf;*n==' '|| *n=='\t';n++)
        ;
      p = strpbrk(n, " \t");
      if (p && ret < desired_number)
      {
        *p++ = '\0';
        strcpy(userbuff[ret].name, n);
        userbuff[ret].ibytes    = (p) ? strtoull(++p, &p, 10) : 0; // 1
        userbuff[ret].ipackets  = (p) ? strtoull(++p, &p, 10) : 0; // 2
        userbuff[ret].ierrors   = (p) ? strtoull(++p, &p, 10) : 0; // 3
        userbuff[ret].if_iqdrops= (p) ? strtoull(++p, &p, 10) : 0; // 4
        for (;p && (*p==' '|| *p=='\t');p++) ; // next value
        for (;p && *p!=' ' && *p!='\t';p++) ; // next sep 5
        for (;p && (*p==' '|| *p=='\t');p++) ; // next value
        for (;p && *p!=' ' && *p!='\t';p++) ; // next sep 6
        for (;p && (*p==' '|| *p=='\t');p++) ; // next value
        for (;p && *p!=' ' && *p!='\t';p++) ; // next sep 7
        for (;p && (*p==' '|| *p=='\t');p++) ; // next value
        for (;p && *p!=' ' && *p!='\t';p++) ; // next sep 8
        for (;p && (*p==' '|| *p=='\t');p++) ; // next value
        userbuff[ret].obytes    = (p) ? strtoull(p, &p, 10) : 0;
        userbuff[ret].opackets  = (p) ? strtoull(p, &p, 10) : 0;
        userbuff[ret].oerrors   = (p) ? strtoull(p, &p, 10) : 0;
        userbuff[ret].xmitdrops = (p) ? strtoull(p, &p, 10) : 0;
        for (;p && (*p==' '|| *n=='\t');p++) ; // next value
        for (;p && *p!=' ' && *p!='\t';p++) ; // next sep 13
        for (;p && (*p==' '|| *p=='\t');p++) ; // next value
        userbuff[ret++].collisions = (p) ? strtoull(p, NULL, 10) : 0;
      }
    }
    fclose(f);
  }

  return ret;
}

int perfunix_fcstat(perfunix_id_t *name __attribute__((unused)),
                                 perfunix_fcstat_t* userbuff,
                                 int sizeof_userbuff,
                                 int desired_number)
{
  if (fc_host.nb < 0) {
    struct dirent *dent;
    DIR *dir=opendir( FC_HOSTDIR );
    if (!dir) return -1;

    fc_host.nb = 0;

	  while ((dent = readdir(dir)) != NULL) {
		  if (*(dent->d_name) == '.') 
			  continue;
		  fc_host.nb++;
	  }

    if (fc_host.nb > 0) {
      int i = 0, l;
      char path[256], *sub_path,*d,*e,*s;

      fc_host.sys_fc_host = (fc_apadter_t*)malloc(sizeof(fc_apadter_t)*fc_host.nb);

      rewinddir(dir);

      strcpy(path, FC_HOSTDIR FSDIRSEP);

      while ((dent = readdir(dir)) != NULL){
        if (*(dent->d_name) == '.')
          continue;
        for (d=fc_host.sys_fc_host[i].name, e=d+sizeof(fc_host.sys_fc_host->name)-1, s=dent->d_name;
            d<e && *s; *d++=*s++)
          ;
        *d = '\0';
        l = d-fc_host.sys_fc_host[i].name;

        /*  sizeof(FC_HOSTDIR) = strlen(FC_HOSTDIR)+1 so it includes FSDIRSEP length */
        strcpy( path + sizeof(FC_HOSTDIR), fc_host.sys_fc_host[i].name );

        sub_path = path + sizeof(FC_HOSTDIR) + l;

        strcpy( sub_path, FSDIRSEP "statistics" FSDIRSEP "dumped_frames");
        strcpy(fc_host.sys_fc_host[i].DumpedFrames, path);

        /* again for sizeof  */
        sub_path += sizeof(FSDIRSEP "statistics");

        strcpy( sub_path,"error_frames");
        strcpy(fc_host.sys_fc_host[i].ErrorFrames, path);

        strcpy( sub_path,"link_failure_count");
        strcpy(fc_host.sys_fc_host[i].LinkFailureCount, path);

        strcpy( sub_path,"rx_words");
        strcpy(fc_host.sys_fc_host[i].RxWords, path);

        /* just switch rx_words to tx_words */
        *sub_path = 't';
        strcpy(fc_host.sys_fc_host[i].TxWords, path);

        i++;
      }
    }
    closedir(dir);
  }

  if (userbuff)
  {
    char line[150];
    int i, l = (fc_host.nb > desired_number) ? desired_number : fc_host.nb;

    if (sizeof_userbuff < (int)sizeof(perfunix_fcstat_t))
      return -1;

    for (i = 0; i < l; i++)
    {
      FILE *f;
      strcpy(userbuff[i].name, fc_host.sys_fc_host[i].name);

#define SETFCVALUE(x)                                     \
      userbuff[i].x = 0;                                  \
      if ((f=fopen(fc_host.sys_fc_host[i].x, "r"))!=NULL) \
      {                                                   \
        if (fgets(line, 150, f))                          \
          userbuff[i].x = strtoull(line+2, NULL, 16);     \
        fclose(f);                                        \
      }

      SETFCVALUE(TxWords)

      SETFCVALUE(RxWords)

      SETFCVALUE(LinkFailureCount)

      SETFCVALUE(ErrorFrames)

      SETFCVALUE(DumpedFrames)
     }
     return l;
  }

  return fc_host.nb;
}
