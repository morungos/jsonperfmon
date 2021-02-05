/* jsonperf.c
 *
 * Implementation of the program core.
 *
 * File begun on 2015-09-06 by Philippe Duveau as a Rsyslog input module.
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <inttypes.h>
#include <sys/syslog.h>
#include <sys/statvfs.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#ifdef _AIX
# include <sys/systemcfg.h>
# include <sys/mntctl.h>
# include <sys/vmount.h>
# include <sys/limits.h>
# include <sys/dr.h>
# define   Hyp_Name Hyp_Name2
# include <libperfstat.h>
# include <procinfo.h>
# include <alloca.h>
#endif

#ifdef linux
# include <getopt.h>
# include <mntent.h>
# include "perflinux.h"
# include "proclinux.h"
#endif

#include "jsonperf.h"
#include "glib_compat.h"

#define STRUCT_ID_T STRUCT_PREFIX(id_t)

# define INIT_FREQ(self, x, y) \
  if (self->freq_data[x].type) init_ ## y(self);

#if defined(_AIX)
/* hardware ticks per microsecond */
#define HWTICS2USECS(x)    (((x)/_system_configuration.Xfrac)*_system_configuration.Xint)/1000
#endif

/* Defines Json */
#define SECOPEN(x) "\"" #x "\":{"
#define SECCLOSE   "}"

#define FMTSTR(x)  "\"" #x "\":\"%s\""
#define FMTNSTR(x) "\"" #x "\":\"%.*s\""
#define FMTI(x)    "\"" #x "\":%i"
#define FMTULL(x)  "\"" #x "\":%llu"
#define FMTILL(x)  "\"" #x  "\":%lli"
#define FMTDBL1(x) "\"" #x "\":%.1f"
#define FMTSEP     ","

/* Defines common */
#define DELTATYPE(x, y, t) ((x < y) ? (t)(UINTMAX_MAX - y + x) : (t)(x - y))

#define DELTAULL(x, y) DELTATYPE(x, y, TYPE_ULL)

#define DELTAMMBRULL(curr, prev, member) DELTATYPE(curr->member, prev->member, TYPE_ULL)
#define DELTAMMBRDBL(curr, prev, member)  DELTATYPE(curr->member, prev->member, double)

#define NONZERO(x) ((x)?(x):1)

#define CALLPROTO(v, m)                \
static int call_ ## m(modPerf_stats_t *v)

#define INITPROTO(s,x)                          \
static int init_ ## x(modPerf_stats_t *s __attribute__((unused)))

#define FREEPROTO(s,x)                          \
void free_ ## x(modPerf_stats_t *s)

/* Define components */
#define CALLCOMPBEGIN2(v, m, first, curr, nb_comp)                                 \
CALLPROTO(v, m) {                                                                  \
  STRUCT_PREFIX(m ## _t) *tab, *curr;                                              \
  STRUCT_PREFIX(id_t) id = {  first };                                             \
  int nb_comp = STRUCT_PREFIX(m)(NULL, NULL, sizeof(STRUCT_PREFIX(m ## _t)), 0);   \
  if ( nb_comp < 1 )                                                               \
    return -1;                                                                     \
  if ( nb_comp == 0 )                                                              \
    return 0;                                                                      \
  tab = (STRUCT_PREFIX(m ## _t) *)alloca(sizeof(STRUCT_PREFIX(m ## _t))*nb_comp);  \
  nb_comp = STRUCT_PREFIX(m) (&id, tab, sizeof(STRUCT_PREFIX(m ## _t)), nb_comp);

#define CALLCOMPBEGIN2FREQ(v, m, first, curr, nb_comp)   CALLCOMPBEGIN2(v, m, first, curr, nb_comp) \
  int group_frequency = v->freq_data[GROUP_ ## m].shift;

#define CALLCOMPBEGIN(v, m, first, curr)   CALLCOMPBEGIN2(v, m, first, curr, nb_comp)

#define CALLCOMPBEGINFREQ(v, m, first, curr) CALLCOMPBEGIN2(v, m, first, curr, nb_comp) \
  int group_frequency = v->freq_data[GROUP_ ## m].shift;


#define RETURN_ON_NB_NULL(x) if (!x) return 0
#define RETURN_ON_NBCOMP_NULL RETURN_ON_NB_NULL(nb_comp)

#define FOREACHCOMPBEGIN(i)       \
  for (i=0; i<nb_comp; i++) {     \
      curr = tab+i;

#define FOREACHCOMPBEGIN2(i,x)   \
  for (i=0; i<x; i++) {          \
      curr = tab+i;

#define FOREACHCOMPEND }

#define CALLCOMPEND return 0; }

/* defines TOTAL */

#define CALLTOTALBEGIN(v, m)  \
CALLPROTO(v, m) {

#define CALLTOTALBEGINFREQ(v, m)                         \
CALLPROTO(v, m) {                                        \
  int group_frequency = v->freq_data[GROUP_ ## m].shift;


#define CALLTOTALEND return 0; }

#define SWAPTOTAL(s,t,x,y)           \
    STRUCT_PREFIX(t ## _t) *x, *y;   \
                                     \
    x = s.previous_snapshot;         \
    y = s.current_snapshot;          \
    s.previous_snapshot = y;         \
    s.current_snapshot = x;

#define CALLTOTAL(t,x,id)                                           \
if (STRUCT_PREFIX(t)(id, x, sizeof(STRUCT_PREFIX(t ## _t)), 1) < 1) \
  return -1;


#define INITTOTALBEGIN(s,x,y,curr)                                          \
INITPROTO(s,x)                                                              \
{                                                                           \
  STRUCT_PREFIX(y ## _t) *curr;                                             \
                                                                            \
  curr = s->x.current_snapshot = &s->x.data[0];                             \
  s->x.previous_snapshot = &s->x.data[1];                                   \
                                                                            \
  if (STRUCT_PREFIX(y)(NULL, curr, sizeof(STRUCT_PREFIX(y ## _t)), 1) < 1 ) \
    return -1;

#define INITTOTALBEGIN2(s,x,y,curr,name)                                    \
INITPROTO(s,x)                                                              \
{                                                                           \
  STRUCT_PREFIX(y ## _t) *curr;                                             \
  STRUCT_PREFIX(id_t) id = { name };                                        \
                                                                            \
  curr = s->x.current_snapshot = &s->x.data[0];                             \
  s->x.previous_snapshot = &s->x.data[1];                                   \
                                                                            \
  if (STRUCT_PREFIX(y)(&id, curr, sizeof(STRUCT_PREFIX(y ## _t)), 1) < 1 )  \
    return -1;

#define INITTOTALEND return 0; }

struct _process_t {
  TYPE_ULL pid;
  TYPE_ULL cpu_ms;
  TYPE_ULL mem;
  TYPE_ULL cpu_pml;
  char *proc;
  uchar_t odd;
  process_t *next;
};

Group_source_t Group_source[] = {
    { "_CPU_TOTAL", 10 },
    { "_CPUS",       5 },
    { "_MEMORY",     7 },
    { "_DISKS",      6 },
    { "_NFS",        4 },
    { "_ADAPTERS",   9 },
    { "_PROCESSES", 10 }
};

/******************************************************************************************************************
 * cpu_total
 *****************************************************************************************************************/

INITTOTALBEGIN(our_stats, cpu_total, cpu_total, curr)
our_stats->cpu_total.processorMHZ = curr->processorHZ / 1000000;
our_stats->n100cpus = 100*curr->ncpus;
INITTOTALEND

CALLTOTALBEGIN(our_stats, cpu_total)
  TYPE_ULL  ptotal;

  SWAPTOTAL(our_stats->cpu_total, cpu_total, curr, prev);

  CALLTOTAL(cpu_total, curr, NULL);

  /* print general processor information */
#if defined(_AIX)
  static u_longlong_t ldavg_unit = (1<<SBITS);
  u_longlong_t  total = DELTAMMBRULL(curr,prev,user) + DELTAMMBRULL(curr,prev,sys) + DELTAMMBRULL(curr,prev,idle) + DELTAMMBRULL(curr,prev,wait);
  total = NONZERO(total);
#endif
  ptotal  = DELTAMMBRULL(curr,prev,puser) + DELTAMMBRULL(curr,prev,psys) + DELTAMMBRULL(curr,prev,pidle) + DELTAMMBRULL(curr,prev,pwait);
  ptotal = NONZERO(ptotal);

  g_string_append_printf(our_stats->out,
           SECOPEN(cpu_total)
               FMTI(active) FMTSEP
#if defined(_AIX)
               FMTI(configured) FMTSEP
#endif
               FMTULL(processorMHZ) FMTSEP
               FMTULL(run_queue_s) FMTSEP
               FMTULL(context_switch_s) FMTSEP
#if defined(_AIX)
               FMTULL(syscall_s) FMTSEP
               SECOPEN(logic)
                 FMTDBL1(user_pct) FMTSEP
                 FMTDBL1(sys_pct) FMTSEP
                 FMTDBL1(wait_pct) FMTSEP
                 FMTDBL1(idle_pct)
               SECCLOSE FMTSEP
#endif
               SECOPEN(physique)
                 FMTDBL1(user_pct) FMTSEP
                 FMTDBL1(sys_pct) FMTSEP
                 FMTDBL1(wait_pct) FMTSEP
                 FMTDBL1(idle_pct)
               SECCLOSE FMTSEP
               SECOPEN(load_average)
                 FMTDBL1(T0) FMTSEP
                 FMTDBL1(T5) FMTSEP
                 FMTDBL1(T15)
               SECCLOSE
             SECCLOSE
             FMTSEP
                         ,
                         curr->ncpus,
#if defined(_AIX)
                         curr->ncpus_cfg,
#endif
                         our_stats->cpu_total.processorMHZ,
                         DELTAMMBRULL(curr,prev,runque),
                         DELTAMMBRULL(curr,prev,pswitch),
#if defined(_AIX)
                         DELTAMMBRULL(curr,prev,syscall),

                         100*DELTAMMBRDBL(curr,prev,user) / total,
                         100*DELTAMMBRDBL(curr,prev,sys) / total,
                         100*DELTAMMBRDBL(curr,prev,wait) / total,
                         100*DELTAMMBRDBL(curr,prev,idle) / total,
#endif

                         100*DELTAMMBRDBL(curr,prev,puser) / ptotal,
                         100*DELTAMMBRDBL(curr,prev,psys) / ptotal,
                         100*DELTAMMBRDBL(curr,prev,pwait) / ptotal,
                         100*DELTAMMBRDBL(curr,prev,pidle) / ptotal,

#if defined(_AIX)
                         ((double)curr->loadavg[0])/ldavg_unit, /* uptime */
                         ((double)curr->loadavg[1])/ldavg_unit, /* uptime */
                         ((double)curr->loadavg[2])/ldavg_unit /* uptime */
#else
                         curr->loadavg_dbl[0], /* uptime */
                         curr->loadavg_dbl[1], /* uptime */
                         curr->loadavg_dbl[2] /* uptime */
#endif

              );

CALLTOTALEND

/******************************************************************************************************************
 * cpu
 *****************************************************************************************************************/

INITPROTO(our_stats, cpu)
{
  STRUCT_PREFIX(id_t) id = { FIRST_CPU };
  our_stats->cpu.nb = 0;

  int nb_cpus = STRUCT_PREFIX(cpu) (NULL, NULL, sizeof(STRUCT_PREFIX(cpu_t)), 0);
  if ( nb_cpus < 1 )
      return -1;
  our_stats->cpu.previous = (STRUCT_PREFIX(cpu_t)*)realloc(our_stats->cpu.previous, sizeof(STRUCT_PREFIX(cpu_t))*nb_cpus);

  nb_cpus = STRUCT_PREFIX(cpu) (&id, our_stats->cpu.previous, sizeof(STRUCT_PREFIX(cpu_t)), nb_cpus);

  our_stats->cpu.nb = nb_cpus;

  return 0;
}

CALLCOMPBEGIN2(our_stats, cpu, FIRST_CPU, curr, nb_cpus)
  TYPE_ULL  total;
  int j;
  STRUCT_PREFIX(cpu_t) *prev;

  g_string_append(our_stats->out, SECOPEN(cpus));

  if (our_stats->cpu.nb < nb_cpus)
    our_stats->cpu.previous = (STRUCT_PREFIX(cpu_t)*)realloc(our_stats->cpu.previous, sizeof(STRUCT_PREFIX(cpu_t))*nb_cpus);

  FOREACHCOMPBEGIN2(j,nb_cpus)

    prev = our_stats->cpu.previous+j;
    if (j == our_stats->cpu.nb)
      {
        memset(prev, 0, sizeof(STRUCT_PREFIX(cpu_t)));
        our_stats->cpu.nb++;
      }

    total = DELTAMMBRULL(curr,prev,user) + DELTAMMBRULL(curr,prev,sys) + DELTAMMBRULL(curr,prev,idle) + DELTAMMBRULL(curr,prev,wait);
    total = NONZERO(total); /* FREQ */
    g_string_append_printf(our_stats->out,
             "%s"
             SECOPEN(%d)
               FMTDBL1(user_pct) FMTSEP
               FMTDBL1(sys_pct) FMTSEP
               FMTDBL1(wait_pct) FMTSEP
               FMTDBL1(idle_pct)
             SECCLOSE
             ,
               (j) ? FMTSEP : "",
               j,
               100*DELTAMMBRDBL(curr,prev,user)/total,
               100*DELTAMMBRDBL(curr,prev,sys)/total,
               100*DELTAMMBRDBL(curr,prev,wait)/total,
               100*DELTAMMBRDBL(curr,prev,idle)/total
          );
    memcpy(prev, curr, sizeof (STRUCT_PREFIX(cpu_t)));
  FOREACHCOMPEND
  g_string_append(our_stats->out, SECCLOSE FMTSEP);
CALLCOMPEND

/******************************************************************************************************************
 * memory_total
 *****************************************************************************************************************/

INITTOTALBEGIN(our_stats, memory_total, memory_total, curr)
INITTOTALEND

#if defined(_AIX)
#define MEM_DECAL >> 8
#else
#define MEM_DECAL >> 10
#endif

CALLTOTALBEGINFREQ(our_stats, memory_total)
  SWAPTOTAL(our_stats->memory_total, memory_total, curr, prev)

  CALLTOTAL(memory_total, curr, NULL);

  g_string_append_printf(our_stats->out,
         SECOPEN(memory)
               FMTULL(virt_total) FMTSEP
               FMTULL(real_total) FMTSEP
               FMTULL(real_free) FMTSEP

               FMTULL(virt_active_pg) FMTSEP

               FMTULL(pgins_s) FMTSEP
               FMTULL(pgouts_s) FMTSEP
               FMTULL(pgspins_s) FMTSEP
               FMTULL(pgspouts_s) FMTSEP

#if defined(_AIX)
               FMTULL(numperm) FMTSEP
               FMTULL(real_system) FMTSEP
               FMTULL(real_user) FMTSEP
#else
               SECOPEN(hugepage)
                 FMTULL(size_kb) FMTSEP
                 FMTULL(total) FMTSEP
                 FMTULL(free)
               SECCLOSE FMTSEP
#endif

               SECOPEN(paging)
#if defined(_AIX)
                 FMTULL(reserved) FMTSEP
#endif
                 FMTULL(total) FMTSEP
                 FMTULL(used_pct) FMTSEP
                 FMTULL(faults_s)
               SECCLOSE
             ,
               (curr->virt_total MEM_DECAL),
               (curr->real_total MEM_DECAL),
               (curr->real_free MEM_DECAL),

               curr->virt_active,

               DELTAMMBRULL(curr,prev, pgins) >> group_frequency,
               DELTAMMBRULL(curr,prev, pgouts) >> group_frequency,
               DELTAMMBRULL(curr,prev, pgspins) >> group_frequency,
               DELTAMMBRULL(curr,prev, pgspouts) >> group_frequency,

#if defined(_AIX)
               (curr->numperm MEM_DECAL),
               (curr->real_system MEM_DECAL),
               (curr->real_user MEM_DECAL),
#else
               curr->huge_size,
               curr->huge_total,
               curr->huge_free,
#endif
               curr->pgsp_total,
               (100-((curr->pgsp_free*100)/NONZERO(curr->pgsp_total))),
#if defined(_AIX)
               curr->pgsp_rsvd,
#endif
               DELTAMMBRULL(curr,prev, pgexct) >> group_frequency

             );
#if defined(_AIX)
  perfstat_memory_page_t mem_page[4];
  perfstat_psize_t pagesize = { FIRST_PSIZE };
  int i, avail_psizes = perfstat_memory_page(&pagesize, mem_page, sizeof(perfstat_memory_page_t), 4);
  for (i = 0; i<avail_psizes;i++) {
      u_longlong_t ps = mem_page[i].psize;
      char *secopen = (ps & (PAGE_4K|PAGE_64K)) ? ((ps&PAGE_4K) ? "page_4k" : "page_64k") : ((ps&PAGE_16M) ? "page_16M" : "page_16G");
    g_string_append_printf(our_stats->out,
             FMTSEP SECOPEN(%s)
                FMTULL(rtotal) FMTSEP
                FMTULL(rfree) FMTSEP
                FMTULL(rused)
             SECCLOSE,
             secopen,
             mem_page[i].real_total,
             mem_page[i].real_free,
             mem_page[i].real_inuse
             );
  }
#endif
  g_string_append_printf(our_stats->out,
             SECCLOSE
             FMTSEP);
CALLTOTALEND

/******************************************************************************************************************
 * pagingspace
 *****************************************************************************************************************/

INITPROTO(our_stats, pagingspace)
{
  return 0;
}

CALLCOMPBEGIN(our_stats, pagingspace, FIRST_PAGINGSPACE, curr)

  RETURN_ON_NBCOMP_NULL;

  g_string_append(our_stats->out, SECOPEN(pagingspaces));
  int i;

  FOREACHCOMPBEGIN(i)
  g_string_append_printf(our_stats->out,
             "%s"
             SECOPEN(%s)
               FMTSTR(type) FMTSEP
               FMTILL(size_mb) FMTSEP
               FMTDBL1(used_pct)
              SECCLOSE
             ,
               (i) ? FMTSEP : "",
               curr->name,
               (curr->type == LV_PAGING)? "LV":"NFS",
               curr->mb_size,
               (double)(curr->mb_used)*100/(curr->mb_size)
              );
  FOREACHCOMPEND
  g_string_append(our_stats->out, SECCLOSE FMTSEP);
CALLCOMPEND

/******************************************************************************************************************
 * disks
 *****************************************************************************************************************/

INITPROTO(our_stats, disk)
{
  STRUCT_PREFIX(id_t) id = { FIRST_DISK };
  our_stats->disk.nb = 0;

  int nb_disks = STRUCT_PREFIX(disk)(((STRUCT_PREFIX(id_t) *)0), ((STRUCT_PREFIX(disk_t) *)0), sizeof(STRUCT_PREFIX(disk_t)), 0);
  if ( nb_disks < 1 )
      return -1;

  our_stats->disk.previous = (STRUCT_PREFIX(disk_t)*)realloc(our_stats->disk.previous, sizeof(STRUCT_PREFIX(disk_t))*nb_disks);

  nb_disks = STRUCT_PREFIX(disk) (&id, (STRUCT_PREFIX(disk_t)*)(our_stats->disk.previous), sizeof(STRUCT_PREFIX(disk_t)), nb_disks);

  our_stats->disk.nb = nb_disks;

  return 0;
}

CALLCOMPBEGIN2FREQ(our_stats, disk, FIRST_PAGINGSPACE, curr, nb_disks)
  int idx,j;
  STRUCT_PREFIX(disk_t) *prev;

  g_string_append(our_stats->out, SECOPEN(disks));

  if (our_stats->disk.nb < nb_disks)
     our_stats->disk.previous = (STRUCT_PREFIX(disk_t)*)realloc(our_stats->disk.previous, sizeof(STRUCT_PREFIX(disk_t))*nb_disks);

  FOREACHCOMPBEGIN2(j,nb_disks)
    for (idx = 0; idx < our_stats->disk.nb &&
            strncmp(curr->name, our_stats->disk.previous[idx].name, sizeof(curr->name));
        idx++)
      ;

    prev = our_stats->disk.previous+idx;
    if (idx == our_stats->disk.nb)
    {
      memset(prev, 0, sizeof(STRUCT_PREFIX(disk_t)));
      our_stats->disk.nb++;
    }

    g_string_append_printf(our_stats->out,
             "%s"
             SECOPEN(%s)
               FMTULL(busy_pct) FMTSEP
               SECOPEN(read)
                 FMTULL(blocks_s) FMTSEP
#if defined(_AIX)
                 FMTULL(timeouts_s) FMTSEP
                 FMTULL(failed_s) FMTSEP
                 FMTULL(time_min_us) FMTSEP
                 FMTULL(time_max_us) FMTSEP
#endif
                 FMTULL(time_avg_us)
               SECCLOSE FMTSEP
               SECOPEN(write)
                 FMTULL(blocks_s) FMTSEP
#if defined(_AIX)
                 FMTULL(timeouts_s) FMTSEP
                 FMTULL(failed_s) FMTSEP
                 FMTULL(time_min_us) FMTSEP
                 FMTULL(time_max_us) FMTSEP
#endif
                 FMTULL(time_avg_us)
               SECCLOSE FMTSEP
               SECOPEN(queue)
#if defined(_AIX)
                 FMTULL(q_full_s) FMTSEP
                 FMTULL(time_min_us) FMTSEP
                 FMTULL(time_max_us) FMTSEP
#endif
                 FMTULL(time_avg_us) FMTSEP
                 FMTULL(write_len_avg) FMTSEP
                 FMTULL(read_len_avg) FMTSEP
                 FMTULL(wq_depth)
               SECCLOSE
             SECCLOSE
             ,
               (j) ? FMTSEP : "",
               curr->name,
               DELTAMMBRULL(curr,prev,time) >> group_frequency,

               DELTAMMBRULL(curr,prev,rblks) >> group_frequency,
#if defined(_AIX)
               DELTAMMBRULL(curr,prev,rtimeout) >> group_frequency,
               DELTAMMBRULL(curr,prev,rfailed) >> group_frequency,
               HWTICS2USECS(curr->min_rserv),
               HWTICS2USECS(curr->max_rserv),
               HWTICS2USECS(DELTAMMBRULL(curr,prev,rserv))/NONZERO(DELTAMMBRULL(curr,prev,xrate)),
#else
               DELTAMMBRULL(curr,prev,rserv)/NONZERO(DELTAMMBRULL(curr,prev,rfers)),
#endif

               DELTAMMBRULL(curr,prev,wblks) >> group_frequency,
#if defined(_AIX)
               DELTAMMBRULL(curr,prev,wtimeout) >> group_frequency,
               DELTAMMBRULL(curr,prev,wfailed) >> group_frequency,
               HWTICS2USECS(curr->min_wserv),
               HWTICS2USECS(curr->max_wserv),
               HWTICS2USECS(DELTAMMBRULL(curr,prev,wserv))/NONZERO(DELTAMMBRULL(curr,prev,xfers)-DELTAMMBRULL(curr,prev,xrate)),
#else
               DELTAMMBRULL(curr,prev,wserv)/NONZERO(DELTAMMBRULL(curr,prev,wfers)),
#endif

#if defined(_AIX)
               DELTAMMBRULL(curr,prev,q_full) >> group_frequency,
               HWTICS2USECS(curr->wq_min_time),
               HWTICS2USECS(curr->wq_max_time),
               HWTICS2USECS(DELTAMMBRULL(curr,prev,wq_time))/NONZERO(DELTAMMBRULL(curr,prev,xfers)),
               (DELTAMMBRULL(curr,prev,wq_sampled) >> group_frequency)/our_stats->n100cpus,
               (DELTAMMBRULL(curr,prev,q_sampled) >> group_frequency)/our_stats->n100cpus,
#else
               DELTAMMBRULL(curr,prev,wq_time)/NONZERO(DELTAMMBRULL(curr,prev,wfers)+DELTAMMBRULL(curr,prev,rfers)),
               (DELTAMMBRULL(curr,prev,wq_sampled) >> group_frequency),
               (DELTAMMBRULL(curr,prev,q_sampled) >> group_frequency),
#endif
               curr->wq_depth
          );
    memcpy(prev, curr, sizeof (*curr));
  FOREACHCOMPEND
  g_string_append(our_stats->out, SECCLOSE FMTSEP);
CALLCOMPEND

/******************************************************************************************************************
 * filesystems
 *****************************************************************************************************************/

INITPROTO(our_stats, filesystems)
{
  return 0;
}


#if defined(_AIX)
CALLTOTALBEGIN(our_stats, filesystems)
  int size;
  char * buf;
  struct vmount * vm;
  int num;
  int j;
  struct statvfs svfs;
  char *sep = "";

  num = mntctl(MCTL_QUERY, sizeof(size), (char *) &size);
  if (num < 0)
      return -1;

  /*
   * Double the needed size, so that even when the user mounts a
   * filesystem between the previous and the next call to mntctl
   * the buffer still is large enough.
   */
  size *= 2;


  buf = (char*)alloca(size);
  num = mntctl(MCTL_QUERY, size, buf);
  if ( num <= 0 )
    return -1;

  g_string_append(our_stats->out, SECOPEN(fs));

  for (vm = (struct vmount *)buf, j = 0; j < num; j++)
    {
      if (vm->vmt_flags & (VFS_DEVMOUNT|VFS_REMOTE))
        {
          u_longlong_t size_mb = 0, free_pct = 0;
          int l, len = (int)vmt2datasize(vm,VMT_OBJECT);
          char *p, *str = (char*)vmt2dataptr(vm,VMT_OBJECT);

          if (*str == '/') str++, len--;

          for (p=str, l=len; l>0; p++, l--)
            if (*p=='/') *p='_';

          if (!statvfs((char *)vmt2dataptr(vm,VMT_STUB),&svfs))
            {
              size_mb = (u_longlong_t)(svfs.f_blocks * svfs.f_frsize / 1024 /1024);
              free_pct = (svfs.f_bfree *100) / svfs.f_blocks;
            }
          g_string_append_printf(our_stats->out,
                   "%s"
                   SECOPEN(%.*s)
                     FMTNSTR(mount) FMTSEP
                     FMTSTR(type) FMTSEP
                     FMTULL(size_mb) FMTSEP
                     FMTULL(free_pct)
                   SECCLOSE
                     ,
                         sep,
                         len, str,
                         (int)vmt2datasize(vm,VMT_STUB), (char*)vmt2dataptr(vm,VMT_STUB),
                         (vm->vmt_flags & VFS_REMOTE) ? "NFS" : "LUN",
                         size_mb,
                         free_pct
                     );
          sep = FMTSEP;
        }

      /* goto the next vmount structure: */
      vm = (struct vmount *)((char *)vm + vm->vmt_length);
    }
  g_string_append(our_stats->out, SECCLOSE FMTSEP);
CALLCOMPEND
#endif

#ifdef linux
CALLTOTALBEGIN(our_stats, filesystems)
  struct mntent *ent;
  FILE *aFile;
  struct statvfs svfs;
  char *sep = "";

  if ((aFile = setmntent("/etc/mtab", "r")) == NULL)
    return -1;

  g_string_append(our_stats->out, SECOPEN(fs));

  while (NULL != (ent = getmntent(aFile)))
  {
    if (strncmp(ent->mnt_type, "ext", 3) && strncmp(ent->mnt_type, "nfs", 3) && strncmp(ent->mnt_type, "xfs", 3))
      continue;
    uint64_t size_mb = 0, free_pct = 0;
    char *p, *str = ent->mnt_fsname;
    size_t l, len = safe_strlen(ent->mnt_fsname);

     if (*str == '/') str++, len--;

     for (p=str, l=len; l>0; p++, l--)
       if (*p=='/') *p='_';

     if (!statvfs(ent->mnt_dir,&svfs))
       {
         size_mb = (uint64_t)(svfs.f_blocks * svfs.f_frsize / 1024 /1024);
         free_pct = (svfs.f_bfree *100) / svfs.f_blocks;
       }
     g_string_append_printf(our_stats->out,
              "%s"
              SECOPEN(%.*s)
                FMTSTR(mount) FMTSEP
                FMTSTR(type) FMTSEP
                FMTULL(size_mb) FMTSEP
                FMTULL(free_pct)
              SECCLOSE
                ,
                    sep,
                    len, str,
                    ent->mnt_dir,
                    (*ent->mnt_type == 'n') ? "NFS" : "LUN",
                    size_mb,
                    free_pct
                );
     sep = FMTSEP;
  }
  endmntent(aFile);
  g_string_append(our_stats->out, SECCLOSE FMTSEP);

CALLCOMPEND
#endif

/******************************************************************************************************************
 * nfs
 *****************************************************************************************************************/

INITTOTALBEGIN2(our_stats, nfsv3, protocol, curr, "nfv3")
INITTOTALEND

INITTOTALBEGIN2(our_stats, nfsv4, protocol, curr, "nfv4")
INITTOTALEND

int init_nfs(modPerf_stats_t *our_stats)
{
  init_nfsv3(our_stats);
  init_nfsv4(our_stats);
  return 0;
}

CALLTOTALBEGINFREQ(our_stats, nfsv3)
  TYPE_ULL total, divisor;
  STRUCT_PREFIX(id_t) id = { "nfsv3" };

  SWAPTOTAL(our_stats->nfsv3, protocol, curr, prev)

  CALLTOTAL(protocol, curr, &id);

  if (curr->u.nfsv3.client.calls == 0 && prev->u.nfsv3.client.calls == 0) {
    g_string_append_printf(our_stats->out,
               SECOPEN(nfsv3)
                   FMTULL(calls_s)
               SECCLOSE
               , 0);
    return -1;
  }

  total = DELTAMMBRULL(curr,prev,u.nfsv3.client.calls);
  divisor = NONZERO(total);

  g_string_append_printf(our_stats->out,
             SECOPEN(nfsv3)
                 FMTULL(calls_s) FMTSEP
                 FMTDBL1(access_pct) FMTSEP
                 FMTDBL1(read_pct) FMTSEP
                 FMTDBL1(write_pct) FMTSEP
                 FMTDBL1(lookup_pct) FMTSEP
                 FMTDBL1(attrGetSet_pct)
             SECCLOSE FMTSEP
             ,
               total >> group_frequency,
               100*DELTAMMBRDBL(curr,prev,u.nfsv3.client.access)/divisor,
               100*DELTAMMBRDBL(curr,prev,u.nfsv3.client.read)/divisor,
               100*DELTAMMBRDBL(curr,prev,u.nfsv3.client.write)/divisor,
               100*DELTAMMBRDBL(curr,prev,u.nfsv3.client.lookup)/divisor,
               100*(DELTAMMBRDBL(curr,prev,u.nfsv3.client.getattr)+DELTAMMBRDBL(curr,prev,u.nfsv3.client.setattr))/divisor
             );

CALLTOTALEND

CALLTOTALBEGINFREQ(our_stats, nfsv4)
  TYPE_ULL total, divisor;
  STRUCT_PREFIX(id_t) id = { "nfsv4" };

  SWAPTOTAL(our_stats->nfsv4, protocol, curr, prev)

  CALLTOTAL(protocol, curr, &id);

  if (curr->u.nfsv4.client.operations == 0 && prev->u.nfsv4.client.operations == 0) {
    g_string_append_printf(our_stats->out,
               SECOPEN(nfsv4)
                   FMTULL(calls_s)
               SECCLOSE FMTSEP
               , 0);
    return -1;
  }

  total = DELTAMMBRULL(curr,prev,u.nfsv4.client.operations);
  divisor = NONZERO(total);

  g_string_append_printf(our_stats->out,
             SECOPEN(nfsv4)
                 FMTULL(calls_s) FMTSEP
                 FMTDBL1(access_pct) FMTSEP
                 FMTDBL1(read_pct) FMTSEP
                 FMTDBL1(write_pct) FMTSEP
                 FMTDBL1(lookup_pct) FMTSEP
                 FMTDBL1(attr_get_get_pct) FMTSEP
                 FMTDBL1(lock_unlock_pct)
             SECCLOSE FMTSEP
             ,
               total >> group_frequency,
               100*DELTAMMBRDBL(curr,prev,u.nfsv4.client.access)/divisor,
               100*DELTAMMBRDBL(curr,prev,u.nfsv4.client.read)/divisor,
               100*DELTAMMBRDBL(curr,prev,u.nfsv4.client.write)/divisor,
               100*DELTAMMBRDBL(curr,prev,u.nfsv4.client.lookup)/divisor,
               100*(DELTAMMBRDBL(curr,prev,u.nfsv4.client.getattr)+DELTAMMBRDBL(curr,prev,u.nfsv4.client.setattr))/divisor,
               100*(DELTAMMBRDBL(curr,prev,u.nfsv4.client.lock)+DELTAMMBRDBL(curr,prev,u.nfsv4.client.unlock))/divisor
             );
CALLTOTALEND

CALLTOTALBEGIN(our_stats, nfs)
  int v3 = call_nfsv3(our_stats);
  int v4 = call_nfsv4(our_stats);
  return (v3 && v4);
}

/******************************************************************************************************************
 * netinterface
 *****************************************************************************************************************/

INITPROTO(our_stats, netinterface)
{
  our_stats->netinterface.nb = 0;
  return 0;
}

CALLCOMPBEGIN2FREQ(our_stats, netinterface, FIRST_NETINTERFACE, curr, nb_nets)
  int idx, j;
  STRUCT_PREFIX(netinterface_t) *prev;
  char * sep = "";

  g_string_append(our_stats->out,
           SECOPEN(intfs));

  if (our_stats->netinterface.nb < nb_nets)
    our_stats->netinterface.previous = (STRUCT_PREFIX(netinterface_t)*)realloc(our_stats->netinterface.previous, sizeof(STRUCT_PREFIX(netinterface_t))*nb_nets);

  FOREACHCOMPBEGIN2(j,nb_nets)
    if (!strncmp(curr->name, "lo", 2) && safe_strlen(curr->name)==3)
      continue;

    for (idx = 0; idx < our_stats->netinterface.nb &&
            strncmp(curr->name, our_stats->netinterface.previous[idx].name, sizeof(curr->name));
        idx++)
      ;

    prev = our_stats->netinterface.previous+idx;
    if (idx == our_stats->netinterface.nb)
      {
        memset(prev, 0, sizeof(STRUCT_PREFIX(netinterface_t)));
        our_stats->netinterface.nb++;
      }

    g_string_append_printf(our_stats->out,
            "%s"
            SECOPEN(%s)
              SECOPEN(in)
                FMTULL(packets_s) FMTSEP
                FMTULL(errors) FMTSEP
                FMTULL(bytes_s)
              SECCLOSE FMTSEP
              SECOPEN(out)
                FMTULL(packets_s) FMTSEP
                FMTULL(errors) FMTSEP
                FMTULL(bytes_s)
              SECCLOSE FMTSEP
              FMTULL(collisions) FMTSEP
              FMTULL(drops)
            SECCLOSE
             ,
               sep,
               curr->name,
               DELTAMMBRULL(curr,prev,ipackets) >> group_frequency,
               curr->ierrors,
               DELTAMMBRULL(curr,prev,ibytes) >> group_frequency,

               DELTAMMBRULL(curr,prev,opackets) >> group_frequency,
               curr->oerrors,
               DELTAMMBRULL(curr,prev,obytes) >> group_frequency,

               curr->collisions,
               curr->xmitdrops+curr->if_iqdrops
          );
    memcpy(prev, curr, sizeof (*curr));
    sep = FMTSEP;
  FOREACHCOMPEND
  g_string_append(our_stats->out,
           SECCLOSE FMTSEP);
CALLCOMPEND

/******************************************************************************************************************
 * fcstat
 *****************************************************************************************************************/

INITPROTO(our_stats, fcstat)
{
  STRUCT_PREFIX(id_t) id = { FIRST_DISK };
  our_stats->fcstat.nb = 0;

  int nb_fcadapter = STRUCT_PREFIX(fcstat)(((STRUCT_PREFIX(id_t) *)0), ((STRUCT_PREFIX(fcstat_t) *)0), sizeof(STRUCT_PREFIX(fcstat_t)), 0);
  if ( nb_fcadapter < 1 )
      return -1;

  our_stats->fcstat.previous= (STRUCT_PREFIX(fcstat_t)*)realloc(our_stats->fcstat.previous,sizeof(STRUCT_PREFIX(fcstat_t))*nb_fcadapter);

  nb_fcadapter = STRUCT_PREFIX(fcstat)(&id, our_stats->fcstat.previous, sizeof(STRUCT_PREFIX(fcstat_t)), nb_fcadapter);

  our_stats->fcstat.nb = nb_fcadapter;

  return 0;
}

CALLCOMPBEGIN2FREQ(our_stats, fcstat, FIRST_PAGINGSPACE, curr, nb_fcstat)
  int idx,j;
  STRUCT_PREFIX(fcstat_t) *prev;

  RETURN_ON_NB_NULL(nb_fcstat);

  if (our_stats->fcstat.nb < nb_fcstat)
    our_stats->fcstat.previous= (STRUCT_PREFIX(fcstat_t)*)realloc(our_stats->fcstat.previous,sizeof(STRUCT_PREFIX(fcstat_t))*nb_fcstat);

  g_string_append(our_stats->out, SECOPEN(fcadapters));

  FOREACHCOMPBEGIN2(j, nb_fcstat)
    for (idx = 0; idx < our_stats->fcstat.nb &&
            strncmp(curr->name, our_stats->fcstat.previous[idx].name, sizeof(curr->name));
        idx++)
      ;

    prev = our_stats->fcstat.previous+idx;
    if (idx == our_stats->fcstat.nb)
      {
        memset(prev, 0, sizeof(STRUCT_PREFIX(fcstat_t)));
        our_stats->fcstat.nb++;
      }

    g_string_append_printf(our_stats->out,
             "%s"
             SECOPEN(%s)
#if defined(_AIX)
               FMTULL(max_xfer) FMTSEP
#endif
               FMTULL(rx_kb_s) FMTSEP
               FMTULL(tx_kb_s) FMTSEP
               FMTULL(err_frm_s) FMTSEP
               FMTULL(err_frm_tot) FMTSEP
               FMTULL(lost_frm_s) FMTSEP
               FMTULL(lost_frm_tot) FMTSEP
               FMTULL(link_fail_s) FMTSEP
               FMTULL(link_fail_tot)
            SECCLOSE
             ,
               (j) ? FMTSEP : "",
               curr->name,
#if defined(_AIX)
               curr->EffMaxTransfer,
#endif
               DELTAMMBRULL(curr,prev,RxWords) >> group_frequency,
               DELTAMMBRULL(curr,prev,TxWords) >> group_frequency,
               DELTAMMBRULL(curr,prev,ErrorFrames) >> group_frequency,
               curr->ErrorFrames,
               DELTAMMBRULL(curr,prev,DumpedFrames) >> group_frequency,
               curr->DumpedFrames,
               DELTAMMBRULL(curr,prev,LinkFailureCount) >> group_frequency,
               curr->LinkFailureCount
          );
    memcpy(prev, curr, sizeof (*curr));
  FOREACHCOMPEND
  g_string_append(our_stats->out,SECCLOSE FMTSEP);
CALLCOMPEND

/******************************************************************************************************************
 * processes
 *****************************************************************************************************************/
typedef int (*compare_t)(process_t *a, process_t *b);
#define NB_PROC_CPU 10
#define NB_PROC_MEM 5

static inline int compare_cpu(process_t *a, process_t *b)
{
  return (a->cpu_pml < b->cpu_pml);
}

static inline int compare_mem(process_t *a, process_t *b)
{
  return (a->mem < b->mem);
}

void classTopTen(process_t *cur, process_t **top_ten, int *nb_top_ten, int max_nb, compare_t comp_f)
{
  int i;
  for (i = *nb_top_ten-1; i>=0 && (*comp_f)(top_ten[i], cur); i--)
    {
      top_ten[i+1] = top_ten[i];
    }
  top_ten[i+1] = cur;
  if (*nb_top_ten < max_nb)
      (*nb_top_ten)++;
}

#if defined(_AIX)
#define TIMED(member) (((u_longlong_t)(member.tv_sec))*1000 + ((u_longlong_t)member.tv_usec)/1000000)
#else
#define TIMED(member) ((uint64_t)(member))
#endif


void store_procentry_init(process_t *cur, struct procentry64 *entry, uchar_t odd)
{
  cur->pid = (TYPE_ULL)entry->pi_pid;
  cur->mem = ((TYPE_ULL)entry->pi_size) MEM_DECAL;
  cur->cpu_ms = TIMED(entry->pi_ru.ru_utime) + TIMED(entry->pi_ru.ru_stime);
  cur->proc = strdup(entry->pi_comm);
  cur->cpu_pml = 0;
  cur->odd = odd;
}

void store_procentry_new(process_t *cur, struct procentry64 *entry, uchar_t odd, process_t **top_ten_cpu, int *nb_top_ten_cpu, process_t **top_ten_mem, int *nb_top_ten_mem)
{
  store_procentry_init(cur, entry, odd);
  cur->cpu_pml = cur->cpu_ms;
  classTopTen(cur, top_ten_cpu, nb_top_ten_cpu, NB_PROC_CPU, compare_cpu);
  classTopTen(cur, top_ten_mem, nb_top_ten_mem, NB_PROC_MEM, compare_mem);
}

void store_procentry_delta(process_t *cur, struct procentry64 *entry, uchar_t odd, process_t **top_ten_cpu, int *nb_top_ten_cpu, process_t **top_ten_mem, int *nb_top_ten_mem)
{
  TYPE_ULL t = TIMED(entry->pi_ru.ru_utime) + TIMED(entry->pi_ru.ru_stime);
  cur->cpu_pml = DELTAULL(t, cur->cpu_ms);
  cur->cpu_ms = t;
  cur->mem = ((TYPE_ULL)(entry->pi_size)) MEM_DECAL;
  cur->odd = odd;
  classTopTen(cur, top_ten_cpu, nb_top_ten_cpu, NB_PROC_CPU, compare_cpu);
  classTopTen(cur, top_ten_mem, nb_top_ten_mem, NB_PROC_MEM, compare_mem);
}

INITPROTO(our_stats, processes)
{
  pid_t firstproc = (pid_t)0;
  int nb_processes;
  int i;
  process_t *cur_proc, *tmp_proc, **p_prev_proc;
  struct procentry64 *procentrys;


  nb_processes = getprocs64(NULL, sizeof(struct procentry64), NULL, 0, &firstproc, 999999);
  procentrys = (struct procentry64 *)alloca( nb_processes * sizeof(struct procentry64) );

  firstproc = (pid_t)0; /* you have to reset this every time */
  nb_processes = getprocs64(procentrys, sizeof(struct procentry64), NULL, 0, &firstproc, nb_processes);

  for(i=0;i<nb_processes;i++)
    {
      if (procentrys[i].pi_state != SZOMB && (procentrys[i].pi_flags & SKPROC)==0)
        {
          tmp_proc = (process_t*)malloc(sizeof(process_t));
          if (!tmp_proc)
              return -1;

          store_procentry_init(tmp_proc, procentrys+i, 0);

          for (cur_proc = our_stats->processes.str_procs_first, p_prev_proc=&our_stats->processes.str_procs_first; cur_proc && cur_proc->pid < procentrys[i].pi_pid; p_prev_proc = &(cur_proc->next), cur_proc=cur_proc->next)
            ;

          tmp_proc->next = cur_proc;
          (*p_prev_proc) = tmp_proc;
        }
    }
  return 0;
}

CALLTOTALBEGINFREQ(our_stats, processes)
  process_t *top_ten_cpu[NB_PROC_CPU+1];
  int nb_top_ten_cpu = 0;
  process_t *top_ten_mem[NB_PROC_MEM+1];
  int nb_top_ten_mem = 0;
  pid_t firstproc = (pid_t)0;
  int nb_processes;
  int i;
  uchar_t ts = 1 - our_stats->processes.odd;
  process_t *cur_proc, *tmp_proc, **p_prev_proc;
  struct procentry64 *procentrys;

  memset(top_ten_cpu, 0, sizeof(top_ten_cpu));
  memset(top_ten_mem, 0, sizeof(top_ten_mem));

  our_stats->processes.odd = ts;

  nb_processes = getprocs64(NULL, sizeof(struct procentry64), NULL, 0, &firstproc, 999999);
  procentrys = (struct procentry64 *)alloca(nb_processes * sizeof(struct procentry64) );

  firstproc = (pid_t)0; /* you have to reset this every time */
  nb_processes = getprocs64(procentrys, sizeof(struct procentry64), NULL, 0, &firstproc, nb_processes);

  for(i=0; i<nb_processes; i++)
    {
      if (procentrys[i].pi_state != SZOMB && (procentrys[i].pi_flags & SKPROC)==0)
        {
          for (cur_proc = our_stats->processes.str_procs_first, p_prev_proc=&our_stats->processes.str_procs_first; cur_proc && cur_proc->pid < procentrys[i].pi_pid; p_prev_proc = &(cur_proc->next), cur_proc=cur_proc->next)
            ;

          if (cur_proc && cur_proc->pid == procentrys[i].pi_pid)
            {
              store_procentry_delta(cur_proc, procentrys+i, ts, top_ten_cpu, &nb_top_ten_cpu, top_ten_mem, &nb_top_ten_mem);
            }
          else
            {
              tmp_proc = (process_t*)malloc(sizeof(process_t));
              if (tmp_proc)
                {
                  store_procentry_new(tmp_proc, procentrys+i, ts, top_ten_cpu, &nb_top_ten_cpu, top_ten_mem, &nb_top_ten_mem);
                  tmp_proc->next = cur_proc;
                  (*p_prev_proc) = tmp_proc;
                }
            }
        }
    }

  for (cur_proc = our_stats->processes.str_procs_first, p_prev_proc=&our_stats->processes.str_procs_first; cur_proc; )
    {
      if (cur_proc->odd != ts)
        {
          *p_prev_proc = cur_proc->next;
          free(cur_proc->proc);
          free(cur_proc);
        }
      else
        {
          p_prev_proc = &(cur_proc->next);
        }
      cur_proc = *p_prev_proc;
   }

  g_string_append(our_stats->out, SECOPEN(processes) SECOPEN(cpu));
  for (i=0; i<nb_top_ten_cpu;i++)
    {
      g_string_append_printf(our_stats->out,
               "%s"
               SECOPEN(%d)
                 FMTULL(pid) FMTSEP
                 FMTSTR(process) FMTSEP
                 FMTDBL1(cpu_pct) FMTSEP
                 FMTULL(mem_mb)
               SECCLOSE
               ,
                 (i) ? FMTSEP : "",
                 i,
                 top_ten_cpu[i]->pid,
                 top_ten_cpu[i]->proc,
                 ((double)(top_ten_cpu[i]->cpu_pml >> group_frequency))/10.0,
                 top_ten_cpu[i]->mem
            );
    }
  g_string_append(our_stats->out, SECCLOSE FMTSEP SECOPEN(mem));
  for (i=0; i<nb_top_ten_mem;i++)
    {
      g_string_append_printf(our_stats->out,
               "%s"
               SECOPEN(%d)
                 FMTULL(pid) FMTSEP
                 FMTSTR(process) FMTSEP
                 FMTULL(mem_mb)
               SECCLOSE
               ,
                 (i) ? FMTSEP : "",
                 i,
                 top_ten_mem[i]->pid,
                 top_ten_mem[i]->proc,
                 top_ten_mem[i]->mem
            );
    }
  g_string_append(our_stats->out, SECCLOSE SECCLOSE FMTSEP);

CALLTOTALEND

/******************************************************************************************************************
 * global
 *****************************************************************************************************************/
void stats_allocate(modPerf_stats_t *self)
{
  int i;

  self->out = g_string_sized_new(1024);

  if (self->freq_data[CPU_TOTAL_GROUP].type)
    init_cpu_total(self);
  if (self->freq_data[CPUS_GROUP].type)
    init_cpu(self);
  if (self->freq_data[MEMORY_GROUP].type)
    {
      init_memory_total(self);
      init_pagingspace(self);
    }
  if (self->freq_data[DISKS_GROUP].type)
    {
      init_disk(self);
      init_filesystems(self);
      init_nfs(self);
    }
  if (self->freq_data[ADAPTERS_GROUP].type)
    {
      init_netinterface(self);
      init_fcstat(self);
    }
  if (self->freq_data[PROCESSES_GROUP].type)
    init_processes(self);
}

void stats_initialize(modPerf_stats_t *self, char *hostname, int lhostname)
{
  int i;

  self->cpu.previous = NULL;
  self->disk.previous = NULL;
  self->netinterface.previous = NULL;
  self->fcstat.previous = NULL;
  self->processes.str_procs_first = NULL;

  for (i=0; i< GROUP_MAX; i++)
  {
    self->freq_data[i].setted = 0;
    self->freq_data[i].type = 0;
    self->freq_data[i].mask = UINT32_MAX;
    self->freq_data[i].shift = 31;
  }

  self->mask_freq = UINT32_MAX;
  self->mask_freq_std = UINT32_MAX;
  self->lhostname = (((size_t)lhostname)>sizeof(self->hostname)) ? sizeof(self->hostname) : (size_t)lhostname;
  memcpy(self->hostname, hostname, self->lhostname);
}

void stats_free(modPerf_stats_t *self)
{
  process_t *cur_proc;
  if (self->freq_data[CPUS_GROUP].type)
    free(self->cpu.previous);
  if (self->freq_data[DISKS_GROUP].type)
    free(self->disk.previous);
  if (self->freq_data[ADAPTERS_GROUP].type)
  {
    free(self->netinterface.previous);
    free(self->fcstat.previous);
  }
  if (self->freq_data[PROCESSES_GROUP].type)
  {
    for (cur_proc = self->processes.str_procs_first; cur_proc; )
      {
        process_t *tmp = cur_proc;
        cur_proc = cur_proc->next;
        free(tmp->proc);
        free(tmp);
      }
  }

#ifdef perfstat_clean_all_exists
  perfstat_clean_all();
#endif

  if (self->out)
    g_string_free(self->out, 1);
}

void set_global_freq(modPerf_stats_t *self, int type, unsigned int shift)
{
  int i;
  TYPE_ULL mask = (TYPE_ULL)((0x0000000000000001<<shift)-1);
  unsigned int freq_min = UINT_MAX, freq_min_std = UINT_MAX;
  if (shift > 31)
    {
      mask = UINT32_MAX;
      shift = 31;
    }

  for (i=0; i< GROUP_MAX; i++)
  {
    if (!self->freq_data[i].setted)
    {
      self->freq_data[i].type = type;
      self->freq_data[i].mask = mask;
      self->freq_data[i].shift = shift;
    }

    freq_min = (freq_min > self->freq_data[i].mask) ? (unsigned int)self->freq_data[i].mask : freq_min;
    freq_min_std = (self->freq_data[i].type > 0 && freq_min_std > self->freq_data[i].mask) ? (unsigned int)self->freq_data[i].mask : freq_min_std;
  }

  self->mask_freq = freq_min;
  self->mask_freq_std = freq_min_std;
}

void set_group_freq(modPerf_stats_t *self, GROUP_e group, int type, unsigned int shift)
{
  int i;
  if (self->freq_data[group].setted)
    return;

  self->freq_data[group].setted = 1;

  self->freq_data[group].type  = type;
  if (shift > 31)
  {
    self->freq_data[group].mask = UINTMAX_MAX;
    self->freq_data[group].shift = 31;
  }
  else
  {
    self->freq_data[group].shift = shift;
    self->freq_data[group].mask  = (1ULL<<shift)-1ULL;
  }

  unsigned int freq_min = UINT_MAX, freq_min_std = UINT_MAX;
  for (i=0; i<GROUP_MAX; i++)
  {
    freq_min = (freq_min > self->freq_data[i].mask) ? (unsigned int)self->freq_data[i].mask : freq_min;
    freq_min_std = (self->freq_data[i].type > 0 && freq_min_std > self->freq_data[i].mask) ? (unsigned int)self->freq_data[i].mask : freq_min_std;
  }

  self->mask_freq = freq_min;
  self->mask_freq_std = freq_min_std;
}

int standard(modPerf_stats_t *self, uint64_t tv_sec, char *sep)
{
  int toprint = 0;
  g_string_assign(self->out, "{");
  if (self->freq_data[CPU_TOTAL_GROUP].type > 0 && (tv_sec & self->freq_data[CPU_TOTAL_GROUP].mask) == 0)
  {
    call_cpu_total(self);
    toprint = 1;
  }
  if (self->freq_data[CPUS_GROUP].type > 0 && (tv_sec & self->freq_data[CPUS_GROUP].mask) == 0)
  {
    call_cpu(self);
    toprint = 1;
  }
  if (self->freq_data[MEMORY_GROUP].type > 0 && (tv_sec & self->freq_data[MEMORY_GROUP].mask) == 0)
  {
    call_memory_total(self);
    call_pagingspace(self);
    toprint = 1;
  }
  if (self->freq_data[DISKS_GROUP].type > 0 && (tv_sec & self->freq_data[DISKS_GROUP].mask) == 0)
  {
    call_disk(self);
    call_filesystems(self);
    toprint = 1;
  }
  if (self->freq_data[NFS_GROUP].type > 0 && (tv_sec & self->freq_data[NFS_GROUP].mask) == 0)
  {
    call_nfs(self);
    toprint = 1;
  }
  if (self->freq_data[ADAPTERS_GROUP].type > 0 && (tv_sec & self->freq_data[ADAPTERS_GROUP].mask) == 0)
  {
    call_netinterface(self);
    call_fcstat(self);
    toprint = 1;
  }
  if (self->freq_data[PROCESSES_GROUP].type > 0 && (tv_sec & self->freq_data[PROCESSES_GROUP].mask) == 0)
  {
    call_processes(self);
    toprint = 1;
  }
  g_string_append_printf(self->out, "\"server\":\"%s\",\"timestamp\":%ld}%s\n", self->hostname, tv_sec, sep);
  return toprint;
}

int group(modPerf_stats_t *self, GROUP_e group, uint64_t tv_sec, char *sep)
{
  g_string_assign(self->out, "{");
  switch (group)
  {
    case CPU_TOTAL_GROUP:
      call_cpu_total(self);
      break;
    case CPUS_GROUP:
      call_cpu(self);
      break;
    case MEMORY_GROUP:
      call_memory_total(self);
      call_pagingspace(self);
      break;
    case DISKS_GROUP:
      call_disk(self);
      call_filesystems(self);
      break;
    case NFS_GROUP:
      if (call_nfs(self))
        return -1;
      break;
    case ADAPTERS_GROUP:
      call_netinterface(self);
      call_fcstat(self);
      break;
    case PROCESSES_GROUP:
      call_processes(self);
      break;
    default:
      break;
  }
  g_string_append_printf(self->out, "\"server\":\"%s\",\"timestamp\":%ld}%s\n", self->hostname, tv_sec, sep);
  return 0;
}

/******************************************************************************************************************
 * main
 *****************************************************************************************************************/
int go_on = 1;

#include <signal.h>
#include <sys/time.h>

#ifndef PACKAGE_NAME
#define PACKAGE_NAME "jsonperfmon"
#endif

static void end_pgm (int sig)
{
   go_on = 0;
}

static void usage() {
  printf("Usage : " PACKAGE_NAME " [-A <n>] [-t <n>] [-u <n>] [-m <n>] [-s <n>] [-n <n>] [-i <n>] [-p <n>] [-r] [-R]\n\n"
      " <n>   The absolute value is the period computed as 2^(n-1) seconds.\n"
      "       If <0 this group is printed separately. If >0 the output is embedded in the main json\n"
      "       structure. If global group is negative, all groups (not explicitly defined)\n"
      "       are printed separately. If zero then all the concerned groups are not printed.\n"
      "\nGroups: at least one group has to defined\n"
      " -A    Global means all groups\n"
      " -t    Total CPU group\n"
      " -u    All individual cpus\n"
      " -m    Memory group\n"
      " -s    Storage group disk, mounts\n"
      " -n    Nfs group\n"
      " -i    IO Adaptors net/FC\n"
      " -p    Top 10 high cpu processes and top 5 high memory processes\n"
      "\nOptions:\n"
      " -R    More human Readable output\n\n");
}

int main (int argc, char *argv[])
{
  modPerf_stats_t *self = malloc(sizeof(modPerf_stats_t));
  char hostname[500], line[100];

  signal(SIGINT, end_pgm);
  signal(SIGTERM, end_pgm);

  openlog("jsonperfmon", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);

  /* Get hostname => fqdn or short name */
  gethostname(hostname,500);
  /* if fqdn let's truncate to first dot */
  char *pos = strchr(hostname,'.');
  if (pos) *pos = '\0';

  /* Initialize the structure content */
  stats_initialize(self, hostname, strlen(hostname));

  int opt, i;
  char *sep = "";
  int nothing_to_do = 1;
  char *groupsopt = "tumsnip";

  /* Manage command line options */
  while ((opt = getopt(argc, argv, "A:t:u:m:s:n:i:p:Rh?")) != -1)
  {
    int grp, val = (optarg!=NULL) ? atoi(optarg) : 0;
    int typ = (val<0)?-1:((val>0)?1:0);
    val = abs(val) - 1;
    switch (opt) {
    case 'A':
      set_global_freq(self, typ, (unsigned int)val);
      nothing_to_do = 0;
      break;
    case 't':
      /* fall through */
    case 'u':
      /* fall through */
    case 'm':
      /* fall through */
    case 's':
      /* fall through */
    case 'n':
      /* fall through */
    case 'i':
      /* fall through */
    case 'p':
      /* with cases above, groupsopt will contain opt */
      for (grp = 0; grp < GROUP_MAX && opt != groupsopt[grp]; grp++)
        ;
      set_group_freq(self, (GROUP_e)grp, typ, (unsigned int)val);
      nothing_to_do = 0;
      break;
    case 'R':
      sep = "\n";
      break;
    case 'h':
    case '?':
    default:
      usage();
      return 0;
    }
  }

  if (nothing_to_do){
    usage();
    return 0;
  }

  stats_allocate(self);

  while (go_on)
  {
    struct timeval tim;
	struct timespec tsns, tsrm;
    gettimeofday(&tim, NULL);
    /* let sleep until next seconds border .000000
     * with this mechanism the period is the exact second
     */
	tsns.tv_sec = 0;
	tsns.tv_nsec = (1000000 - tim.tv_usec)*1000;
    nanosleep(&tsns, &tsrm);
    /* now we are at (tim.tv_sec++).000000 */
    tim.tv_sec++;

    if (((TYPE_ULL)tim.tv_sec & self->mask_freq_std) == 0)
    {
      /* This is the single json */
      if (standard(self, tim.tv_sec, sep))
      {
        /* With at least a group in the json */
        syslog(LOG_INFO, "%.*s", (int)self->out->len, self->out->str);
      }
    }
    for (i=0; go_on && i<GROUP_MAX; i++)
    {
      if (self->freq_data[i].type < 0 && (((TYPE_ULL)tim.tv_sec) & self->freq_data[i].mask) == 0)
      {
        /* This is the group level json */
        if (!group(self, (GROUP_e)i, tim.tv_sec, sep))
        {
          /* This group produce a json */
          syslog(LOG_INFO, "%.*s", (int)self->out->len, self->out->str);
        }
      }
    }
  }

  stats_free(self);
  free(self);
  closelog();

  return 0;
}

