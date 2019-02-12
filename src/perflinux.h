/* perflinux.h
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

#if !defined(PERFLINUX_H) && !defined(_AIX)
#define PERFLINUX_H

#include <sys/types.h>
#include <inttypes.h>

#define LV_PAGING 1

#define ID_LENGTH 64

/* compliance with libperfstat */
#define FIRST_CPU  ""
#define FIRST_NETINTERFACE  ""
#define FIRST_DISK  ""
#define FIRST_FCADAPTER    ""
#define FIRST_PAGINGSPACE  ""
#define FIRST_PROTOCOL  ""

/* ================================================================================== */
typedef struct { /* perfstat_id_t : structure element identifier */
    char name[ID_LENGTH]; /* name of the identifier */
} perfunix_id_t;

typedef struct { /* perfunix_cpu_t : cpu information */
    uint64_t user;     /* ticks spent in user mode */
    uint64_t sys;      /* ticks spent in system mode */
    uint64_t idle;     /* ticks spent idle */
    uint64_t wait;     /* ticks spent waiting for I/O */
} perfunix_cpu_t;

typedef struct { /* perfunix_cpu_total_t : global cpu information */
    int ncpus;            /* number of processors */
    uint64_t processorHZ; /* processor speed in Hz */
    uint64_t pswitch;     /* number of process switches (change in currently running process) */
    double loadavg_dbl[3];/* load average : differ from AIX */
    uint64_t runque;      /* length of the run queue (processes ready) */
    uint64_t puser;       /* processor tics in user mode */
    uint64_t psys;        /* processor tics in system mode */
    uint64_t pidle;       /* processor tics idle */
    uint64_t pwait;       /* processor tics waiting for I/O */
} perfunix_cpu_total_t;

typedef struct { /* perfunix_memory_total_t : Virtual memory utilization */
    uint64_t virt_total;    /* total virtual memory (in KB pages) */
    uint64_t real_total;    /* total real memory (in KB pages) */
    uint64_t real_free;     /* free real memory (in KB pages) */
    uint64_t huge_total;    /* total virtual memory (in KB pages) */
    uint64_t huge_free;     /* total virtual memory (in KB pages) */
    uint64_t huge_size;     /* total virtual memory (in KB pages) */
    uint64_t pgexct;        /* number of page faults */
    uint64_t pgins;         /* number of pages paged in */
    uint64_t pgouts;        /* number of pages paged out */
    uint64_t pgspins;       /* number of pages paged in */
    uint64_t pgspouts;      /* number of pages paged out */
    uint64_t pgsp_total;    /* total paging space (in KB pages) */
    uint64_t pgsp_free;     /* free paging space (in KB pages) */
    uint64_t virt_active;   /* Active virtual pages. Virtual pages are considered active if they have been accessed */
} perfunix_memory_total_t;

typedef struct { /* perfunix_pagingspace_t : Paging space data for a specific logical volume */
    char name[ID_LENGTH];    /* Paging space name */
    char type; /* type of paging device (LV_PAGING or NFS_PAGING) *
            * Possible values are:                            *
            *     LV_PAGING      logical volume               *
            *     NFS_PAGING     NFS file                     */
    uint64_t mb_size;    /* size in megabytes  */
    uint64_t mb_used;    /* portion used in megabytes  */
} perfunix_pagingspace_t;

typedef struct { /* perfunix_disk_t : disk information */
    char name[ID_LENGTH];        /* name of the disk */
    uint64_t time;              /* amount of time disk is active */
    uint64_t rblks;             /* number of blocks read from disk */
    uint64_t rserv;             /* read or receive service time */
    uint64_t rfers;             /* number of transfers from disk */
    uint64_t wfers;             /* number of transfers to disk */
    uint64_t wblks;             /* number of blocks written to disk */
    uint64_t wserv;             /* write or send service time */
    uint64_t wq_time;           /* accumulated wait queueing time */
    uint64_t wq_sampled;        /* accumulated sampled dk_wq_depth */
    uint64_t q_sampled;         /* accumulated sampled dk_q_depth */

    uint64_t wq_depth;          /* instantaneous wait queue depth
                                     * (number of requests waiting to be sent to disk) */
} perfunix_disk_t;

typedef struct { /* perfunix_netinterface_t : Description of the network interface */
    char name[ID_LENGTH];   /* name of the interface */
    uint64_t ipackets;    /* number of packets received on interface */
    uint64_t ibytes;      /* number of bytes received on interface */
    uint64_t ierrors;     /* number of input errors on interface */
    uint64_t opackets;    /* number of packets sent on interface */
    uint64_t obytes;      /* number of bytes sent on interface */
    uint64_t oerrors;     /* number of output errors on interface */
    uint64_t collisions;  /* number of collisions on csma interface */
    uint64_t xmitdrops;   /* number of packets not transmitted */
    uint64_t if_iqdrops;  /* Dropped on input, this interface */
} perfunix_netinterface_t;

typedef struct { /* perfunix_fcstat_t : Fiber Channel Statistics */
    char name[ID_LENGTH];           /* name of the adapter */
    /* Tranfer Statistics */
    uint64_t TxWords;                   /* Fiber Channel Kbytes transmitted */
    uint64_t RxWords;                   /* Fiber Channel Kbytes Received */
    uint64_t ErrorFrames;               /* Number of frames received with the CRC Error */
    uint64_t DumpedFrames;              /* Number of lost frames */
    uint64_t LinkFailureCount;          /* Count of Link failures */
}perfunix_fcstat_t;

typedef struct { /* perfunix_protocol_t : utilization of protocols */
  union{
    uint64_t calls;       /* NFS client calls */
    struct{
      struct{
        uint64_t calls;       /* NFS V3 client calls */
        uint64_t null;        /* NFS V3 client nulls */
        uint64_t getattr;     /* NFS V3 client getattr requests */
        uint64_t setattr;     /* NFS V3 client setattr requests */
        uint64_t lookup;      /* NFS V3 client file name lookup requests */
        uint64_t access;      /* NFS V3 client access requests */
        uint64_t read;        /* NFS V3 client read requests */
        uint64_t write;       /* NFS V3 client write requests */
      } client; /* nfsv3 client */
    } nfsv3;
    struct {
      struct {
        uint64_t operations;   /* NFS V4 client operations */
        uint64_t null;         /* NFS V4 client nulls */
        uint64_t read;         /* NFS V4 client read operations */
        uint64_t write;        /* NFS V4 client write operations */
        uint64_t setattr;      /* NFS V4 client setattr operations */
        uint64_t lock;         /* NFS V4 client lock operations */
        uint64_t unlock;       /* NFS V4 client unlock operations */
        uint64_t access;       /* NFS V4 client access operations */
        uint64_t getattr;      /* NFS V4 client getattr operations */
        uint64_t lookup;       /* NFS V4 client lookup operations */
      } client; /* nfsv4 client*/
    } nfsv4;
  } u;
} perfunix_protocol_t;


extern int perfunix_cpu_total(perfunix_id_t *name ,
                              perfunix_cpu_total_t* userbuff,
                              int sizeof_userbuff,
                              int desired_number);

extern int perfunix_cpu(perfunix_id_t *name ,
                        perfunix_cpu_t* userbuff,
                        int sizeof_userbuff,
                        int desired_number);

extern int perfunix_memory_total(perfunix_id_t *name0,
                                 perfunix_memory_total_t* userbuff,
                                 int sizeof_userbuff,
                                 int desired_number);

extern int perfunix_pagingspace(perfunix_id_t *name,
                                perfunix_pagingspace_t* userbuff,
                                int sizeof_userbuff,               
                                int desired_number);               

extern int perfunix_disk(perfunix_id_t *name,
                               perfunix_disk_t* userbuff,
                               int sizeof_userbuff,
                               int desired_number);

extern int perfunix_protocol(perfunix_id_t *name,
                             perfunix_protocol_t* userbuff,
                             int sizeof_userbuff,
                             int desired_number);

extern int perfunix_netinterface(perfunix_id_t *name,
                                 perfunix_netinterface_t* userbuff,
                                 int sizeof_userbuff,
                                 int desired_number);

extern int perfunix_fcstat(perfunix_id_t *name,
                                 perfunix_fcstat_t* userbuff,
                                 int sizeof_userbuff,
                                 int desired_number);

#define perfunix_clean_all_exists 1
void perfunix_clean_all();

#endif /*ifdef PERFLINUX_H*/

