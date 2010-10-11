#ifndef __FANOTIFY_SYSCALL_LIB
#define __FANOTIFY_SYSCALL_LIB

#include <linux/types.h>

extern int fanotify_init(unsigned int flags, unsigned int event_f_flags);
extern int fanotify_mark(int fanotify_fd, unsigned int flags, __u64 mask,
			 int dfd, const char *pathname);
#endif
