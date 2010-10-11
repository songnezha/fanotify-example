#include <linux/types.h>
#include <unistd.h>
#include <sys/syscall.h>   /* For SYS_xxx definitions */

#include "fanotify-syscalllib.h"

#ifdef X86_64
#define __NR_fanotify_init	300
#define __NR_fanotify_mark	301
#else
#define __NR_fanotify_init	338
#define __NR_fanotify_mark	339
#endif

int fanotify_init(unsigned int flags, unsigned int event_f_flags)
{
	return syscall(__NR_fanotify_init, flags, event_f_flags);
}

int fanotify_mark(int fanotify_fd, unsigned int flags, __u64 mask,
		  int dfd, const char *pathname)
{
	return syscall(__NR_fanotify_mark, fanotify_fd, flags, mask,
		       dfd, pathname);
}
