/* jsonperf.h
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

#include <sys/types.h>
#include <inttypes.h>

#include "glib_compat.h"
#ifdef _AIX
#include <libperfstat.h>
# define STRUCT_PREFIX(x) perfstat_ ## x
# define TYPE_ULL u_longlong_t
# define TYPE_LL longlong_t
#else
# include "perflinux.h"
# define STRUCT_PREFIX(x) perfunix_ ## x
# define TYPE_ULL uint64_t
#endif

#ifndef __MOD_PERF_H
#define __MOD_PERF_H

enum GROUP_e {
  CPU_TOTAL_GROUP = 0,
  CPUS_GROUP = 1,
  MEMORY_GROUP = 2,
  DISKS_GROUP = 3,
  NFS_GROUP = 4,
  ADAPTERS_GROUP = 5,
  PROCESSES_GROUP = 6,
  GROUP_MAX = 7
};
typedef enum GROUP_e GROUP_e;

struct Group_source_s {
  char *str;
  int len;
};

typedef struct _process_t process_t;

typedef struct Group_source_s Group_source_t;

#ifndef _UCHAR_T
#define _UCHAR_T
typedef unsigned char uchar_t;
#endif /*_UCHAR_T */

/* Main structure, it contains for must groups an array of 2 storages
 * one contains the previous collect one the current then methods can
 * subtract between the two collects. Pointers on current and previous
 * alternate as the current storage becomes the previous of the next
 * collect
 */
struct modPerf_stats_s {
  size_t  lhostname;
  char hostname[256];

  GString *out;

  int n100cpus;
  TYPE_ULL mask_freq;
  TYPE_ULL mask_freq_std;

  struct {
    STRUCT_PREFIX(cpu_total_t) data[2];
    STRUCT_PREFIX(cpu_total_t) *current_snapshot;
    STRUCT_PREFIX(cpu_total_t) *previous_snapshot;
    TYPE_ULL processorMHZ;
#   define GROUP_cpu_total CPU_TOTAL_GROUP
  } cpu_total;

  struct {
    STRUCT_PREFIX(cpu_t) *previous;
    int nb;
#   define GROUP_cpu CPUS_GROUP
  } cpu;

  struct  {
    STRUCT_PREFIX(memory_total_t) data[2];
    STRUCT_PREFIX(memory_total_t) *current_snapshot;
    STRUCT_PREFIX(memory_total_t) *previous_snapshot;
#   define GROUP_memory_total MEMORY_GROUP
  } memory_total;

  struct {
    STRUCT_PREFIX(disk_t) *previous;
    int nb;
#   define GROUP_disk DISKS_GROUP
  } disk;

  struct {
    STRUCT_PREFIX(fcstat_t) *previous;
    int nb;
#   define GROUP_fcstat ADAPTERS_GROUP
  } fcstat;

  struct {
    STRUCT_PREFIX(netinterface_t) *previous;
    int nb;
#   define GROUP_netinterface ADAPTERS_GROUP
  } netinterface;

#   define GROUP_nfsv3 NFS_GROUP
#   define GROUP_nfsv4 NFS_GROUP
  struct {
    STRUCT_PREFIX(protocol_t) data[2];
    STRUCT_PREFIX(protocol_t) *current_snapshot;
    STRUCT_PREFIX(protocol_t) *previous_snapshot;
  } nfsv3;

  struct {
    STRUCT_PREFIX(protocol_t) data[2];
    STRUCT_PREFIX(protocol_t) *current_snapshot;
    STRUCT_PREFIX(protocol_t) *previous_snapshot;
  } nfsv4;

 struct {
    process_t *str_procs_first;
    uchar_t odd;
#   define GROUP_processes PROCESSES_GROUP
  } processes;

#   define GROUP_filesystems DISKS_GROUP
#   define GROUP_pagingspace MEMORY_GROUP

  struct {
    int type;
    unsigned int shift;
    TYPE_ULL mask;
    int setted;
  } freq_data[GROUP_MAX];

};

typedef struct modPerf_stats_s modPerf_stats_t;

#endif
