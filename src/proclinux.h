
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

int	getprocs64 (void *procsinfo, int sizproc, void *fdsinfo, int sizfd,
   	           pid_t *index, int count);

#endif	/* _H_PROCLINUX */
