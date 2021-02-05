# JsonPerfMon

This is a fork of the original and admirable 
[jsonperfmon](https://github.com/pduveau/jsonperfmon).

why make a fork? The significant difference is that this version doesn't use standard output,
and is not designed for tight integration with rsyslog. Instead, it writes direct to syslog through
the normal channels, and can be processed via rsyslog that way. This makes it easy to set up as an
independent service daemon under, e.g., systemd. Given that recent versions of rsyslog no longer
include the `improg` module that the original version of jsonperfmon was designed for, this is
easier for our custom integration.

Yes, the whole json thing makes less sense under these circumstances, but it's still handy when
a downstream aggregation system is used, to provide a performance monitor across a cluster of
servers.

For this reason, most of the command line options are unchanged, but the `-r` option of the original
jsonperfmon has been removed, as it was very specifically intended for use with this module 
and tight integration with rsyslog.

## Linux / AIX real time system monitoring

**JsonPerfMon** collects system information from a Linux/AIX system and print a json structures at a regular period to a syslog stream.

### Features
**optimized C code** It only needs a few milliseconds per second to collect all the datas.

**lightweight** It only needs a few megabytes of memory (~4 Mb) to manage datas. Memory is allocated during the first collect and reused during all the process life.

**Per second data collection** Every group of data - cpu_total, cpus, memory, disks, nfs, adapters & processes - can be collected in a single json or in an indivual one at a period of 2^n (power) seconds.

### Usage / options
`jsonperfmon [-A <n>] [-t <n>] [-u <n>] [-m <n>] [-s <n>] [-n <n>] [-i <n>] [-p <n>] [-r] [-R]`

> `-A` set the period for All groups, always overwritten by individual group setting
>
> `-t` set the period for the cpu total
>
> `-u` set the period for the individual cpus 
>
> `-m` set the period for the memory
>
> `-s` set the period for the storage (disks & mounts)
>
> `-n` set the period for the nfs v3/v4
>
> `-i` set the period for the IO adapaters network and fiber channel
>
> `-p` set the period for the processes top 10 for high cpu and top 5 high memory
>
> `-R` insert an empty line between jsons for human readable purpose
>
> `<n>` `=0` disable the concerned group(s), `<0` produce the group every `2^(n-1)` seconds in the main json structure, `>0` same as `<0` except the json produced is dedicated to the group. 

### JSON attributes
> `_s` => per second
> `_us` => in µ-second
> `_mb` => in mega-bytes
> `_pct` => in percent

#### JSON structure on AIX
```jsonc
{
	"cpu_total": {							// Group total cpu (-t)
		"active": 4,
		"configured": 4,
		"processorMHZ": 4228,
		"run_queue_s": 0,
		"context_switch_s": 3572,
		"syscall_s": 22760,
		"logic": {
			"user_pct": 2.4,
			"sys_pct": 4.6,
			"wait_pct": 0.0,
			"idle_pct": 93.0
		},
		"physique": {
			"user_pct": 37.2,
			"sys_pct": 36.1,
			"wait_pct": 0.0,
			"idle_pct": 26.7
		},
		"load_average": {
			"T0": 5.6,
			"T5": 6.4,
			"T15": 6.6
		}
	},
	"cpus": { 								// Group cpus (-u)
		"0": {
			"user_pct": 3.0,
			"sys_pct": 15.2,
			"wait_pct": 0.0,
			"idle_pct": 81.8
		},
		"1": {
			"user_pct": 12.1,
			"sys_pct": 15.2,
			"wait_pct": 0.0,
			"idle_pct": 72.7
		},
		"2": {
			"user_pct": 0.0,
			"sys_pct": 0.0,
			"wait_pct": 0.0,
			"idle_pct": 100.0
		},
		"3": {
			"user_pct": 0.0,
			"sys_pct": 0.0,
			"wait_pct": 0.0,
			"idle_pct": 100.0
		}
	},
	"memory": {								// Group memory (-m)
		"virt_total": 92416,
		"real_total": 73728,
		"real_free": 28485,
		"virt_active_pg": 9712324,
		"pgins_s": 0,
		"pgouts_s": 0,
		"pgspins_s": 0,
		"pgspouts_s": 0,
		"numperm": 6715,
		"real_system": 7026,
		"real_user": 36486,
		"paging": {
			"reserved": 4784128,
			"total": 2,
			"used_pct": 18688,
			"faults_s": 26
		},
		"page_4k": {
			"rtotal": 18808831,
			"rfree": 7292270,
			"rused": 11516561
		}
	},
	"disks": {								// Group storage (-d)
		"hdisk0": {
			"busy_pct": 0,
			"read": {
				"blocks_s": 0,
				"timeouts_s": 0,
				"failed_s": 0,
				"time_min_us": 100,
				"time_max_us": 376159,
				"time_avg_us": 0
			},
			"write": {
				"blocks_s": 0,
				"timeouts_s": 0,
				"failed_s": 0,
				"time_min_us": 142,
				"time_max_us": 166931,
				"time_avg_us": 0
			},
			"queue": {
				"q_full_s": 0,
				"time_min_us": 0,
				"time_max_us": 1993627,
				"time_avg_us": 0,
				"write_len_avg": 0,
				"read_len_avg": 0,
				"wq_depth": 0
			}
		}
	},
	"fs": {									// Group storage
		"dev_hd4": {
			"mount": "/",
			"type": "LUN",
			"size_mb": 2048,
			"free_pct": 95
		},
		"dev_hd2": {
			"mount": "/usr",
			"type": "LUN",
			"size_mb": 4096,
			"free_pct": 28
		},
		...
		"exploit": {
			"mount": "/exploit/nfs",
			"type": "NFS",
			"size_mb": 20480,
			"free_pct": 77
		}
	},
	"nfsv3": {								// Group nfs (-n)
		"calls_s": 4,
		"access_pct": 0.0,
		"read_pct": 0.0,
		"write_pct": 0.0,
		"lookup_pct": 0.0,
		"attrGetSet_pct": 0.0
	},
	"nfsv4": {
		"calls_s": 0
	},
	"intfs": {								// Group io-adapters (-a)
		"en3": {
			"in": {
				"packets_s": 4,
				"errors": 0,
				"bytes_s": 896
			},
			"out": {
				"packets_s": 6,
				"errors": 0,
				"bytes_s": 908
			},
			"collisions": 0,
			"drops": 0
		},
		"en4": {
			"in": {
				"packets_s": 6,
				"errors": 0,
				"bytes_s": 364
			},
			"out": {
				"packets_s": 5,
				"errors": 0,
				"bytes_s": 6590
			},
			"collisions": 0,
			"drops": 0
		}
	},
	"fcadapters": {							// Group io-adapters
		"fcs0": {
			"max_xfer": 0,
			"rx_kb_s": 0,
			"tx_kb_s": 0,
			"err_frm_s": 0,
			"err_frm_tot": 0,
			"lost_frm_s": 0,
			"lost_frm_tot": 0,
			"link_fail_s": 0,
			"link_fail_tot": 0
		}
	},
	"processes": { 							// Group processes (-p)
		"cpu": {
			"0": {
				"pid": 33948658,
				"process": "perfjson",
				"cpu_pct": 0.3,
				"mem_mb": 1
			},
			"1": {
				"pid": 46990290,
				"process": "sshd",
				"cpu_pct": 0.1,
				"mem_mb": 4
			},
			// ... until "9"
		},
		"mem": {
			"0": {
				"pid": 52494940,
				"process": "rmcd",
				"mem_mb": 22
			},
			"1": {
				"pid": 54985398,
				"process": "IBM.ConfigRMd",
				"mem_mb": 16
			},
			// ... until "4"
		}
	},
	"server": "dev-aix-d1c",
	"timestamp": 1549737252
}
```

#### JSON structure on Linux
```jsonc
{
	"cpu_total": {							         // Group total cpu (-t)
		"active": 2,
		"configured": 2,
		"processorMHZ": 2497,
		"run_queue_s": 0,
		"context_switch_s": 24,
		"physique": {
			"user_pct": 0.0,
			"sys_pct": 0.0,
			"wait_pct": 0.0,
			"idle_pct": 100.0
		},
		"load_average": {
			"T0": 0.0,
			"T5": 0.0,
			"T15": 0.0
		}
	},
	"cpus": { 								// Group cpus (-u)
		"0": {
			"user_pct": 0.0,
			"sys_pct": 0.0,
			"wait_pct": 0.0,
			"idle_pct": 100.0
		},
		...
	},
	"memory": {								// Group memory (-m)
		"virt_total": 726,
		"real_total": 5845,
		"real_free": 4008,
		"virt_active_pg": 838856,
		"pgins_s": 0,
		"pgouts_s": 0,
		"pgspins_s": 0,
		"pgspouts_s": 0,
		"hugepage": {
			"size_kb": 2048,
			"total": 0,
			"free": 0
		},
		"paging": {
			"total": 4194300,
			"used_pct": 1,
			"faults_s": 30
		}
	},
	"pagingspaces": {					                 // Group memory
		"/dev/dm-1": {
			"type": "LV",
			"size_mb": 4095,
			"used_pct": 0.3
		}
	},
	"disks": {								// Group disks (-d)
		"sda": {
			"busy_pct": 0,
			"read": {
				"blocks_s": 0,
				"time_avg_us": 0
			},
			"write": {
				"blocks_s": 0,
				"time_avg_us": 0
			},
			"queue": {
				"time_avg_us": 0,
				"write_len_avg": 0,
				"read_len_avg": 0,
				"wq_depth": 0
			}
		},
		"dm-0": {
			"busy_pct": 0,
			"read": {
				"blocks_s": 0,
				"time_avg_us": 0
			},
			"write": {
				"blocks_s": 0,
				"time_avg_us": 0
			},
			"queue": {
				"time_avg_us": 0,
				"write_len_avg": 0,
				"read_len_avg": 0,
				"wq_depth": 0
			}
		}
	},
	"fs": {
			// ... see AIX
	},
	"intfs": {								// Group io-dapters (-a)
		"eth0:": {
			"in": {
				"packets_s": 3,
				"errors": 0,
				"bytes_s": 238
			},
			"out": {
				"packets_s": 3,
				"errors": 0,
				"bytes_s": 4258
			},
			"collisions": 0,
			"drops": 0
		}
	},
	"fcadapters": {						               // Group io-adapters
		"fcs0": {
			"rx_kb_s": 0,
			"tx_kb_s": 0,
			"err_frm_s": 0,
			"err_frm_tot": 0,
			"lost_frm_s": 0,
			"lost_frm_tot": 0,
			"link_fail_s": 0,
			"link_fail_tot": 0
		}
	},
	"processes": { 							// Group processes (-p)
			// ... see AIX
	},
	"server": "dev-lnx-d10",
	"timestamp": 1549740204
}
```
