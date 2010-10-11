#define _ATFILE_SOURCE
#include <errno.h>
#include <inttypes.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <linux/fanotify.h>

#include "fanotify-syscalllib.h"

#define FANOTIFY_ARGUMENTS "cfhmp"

int mark_object(int fan_fd, const char *path, int fd, uint64_t mask, unsigned int flags)
{
	return fanotify_mark(fan_fd, flags, mask, fd, path);
}

int set_ignored_mask(int fan_fd, int fd, uint64_t mask)
{
	unsigned int flags = (FAN_MARK_ADD | FAN_MARK_IGNORED_MASK);

	return mark_object(fan_fd, NULL, fd, mask, flags);
}

int handle_perm(int fan_fd, struct fanotify_event_metadata *metadata)
{
	struct fanotify_response response_struct;
	int ret;

	response_struct.fd = metadata->fd;
	response_struct.response = FAN_ALLOW;

	ret = write(fan_fd, &response_struct, sizeof(response_struct));
	if (ret < 0)
		return ret;

	return 0;
}

void synopsis(const char *progname, int status)
{
	FILE *file = status ? stderr : stdout;

	fprintf(file, "USAGE: %s [-" FANOTIFY_ARGUMENTS "] "
		"[-o {open,close,access,modify,open_perm,access_perm}] "
		"file ...\n"
		"-c: learn about events on children of a directory (not decendants)\n"
		"-f: set premptive ignores (go faster)\n"
		"-h: this help screen\n"
		"-m: place mark on the whole mount point, not just the inode\n"
		"-p: check permissions, not just notification\n",
		progname);
	exit(status);
}

int main(int argc, char *argv[])
{
	int opt, ret;
	int fan_fd;
	uint64_t fan_mask = FAN_OPEN | FAN_CLOSE | FAN_ACCESS | FAN_MODIFY;
	unsigned int mark_flags = FAN_MARK_ADD;
	bool opt_child, opt_on_mount, opt_add_perms, opt_fast;
	ssize_t len;
	char buf[4096];
	fd_set rfds;


	opt_child = opt_on_mount = opt_add_perms = opt_fast = false;

	while ((opt = getopt(argc, argv, "o:"FANOTIFY_ARGUMENTS)) != -1) {
		switch(opt) {
			case 'o': {
				char *str, *tok;

				fan_mask = 0;
				str = optarg;
				while ((tok = strtok(str, ",")) != NULL) {
					str = NULL;
					if (strcmp(tok, "open") == 0)
						fan_mask |= FAN_OPEN;
					else if (strcmp(tok, "close") == 0)
						fan_mask |= FAN_CLOSE;
					else if (strcmp(tok, "access") == 0)
						fan_mask |= FAN_ACCESS;
					else if (strcmp(tok, "modify") == 0)
						fan_mask |= FAN_MODIFY;
					else if (strcmp(tok, "open_perm") == 0)
						fan_mask |= FAN_OPEN_PERM;
					else if (strcmp(tok, "access_perm") == 0)
						fan_mask |= FAN_ACCESS_PERM;
					else
						synopsis(argv[0], 1);
				}
				break;
			}
			case 'c':
				opt_child = true;
				break;
			case 'f':
				opt_fast = true;
				break;
			case 'm':
				opt_on_mount = true;
				break;
			case 'p':
				opt_add_perms = true;
				break;
			case 'h':
				synopsis(argv[0], 0);
			default:  /* '?' */
				synopsis(argv[0], 1);
		}
	}
	if (optind == argc)
		synopsis(argv[0], 1);

	if (opt_child)
		fan_mask |= FAN_EVENT_ON_CHILD;

	if (opt_on_mount)
		mark_flags |= FAN_MARK_MOUNT;

	if (opt_add_perms)
		fan_mask |= FAN_ALL_PERM_EVENTS;

	fan_fd = fanotify_init(0, O_RDONLY | O_LARGEFILE);
	if (fan_fd < 0)
		goto fail;

	for (; optind < argc; optind++)
		if (mark_object(fan_fd, argv[optind], AT_FDCWD, fan_mask, mark_flags) != 0)
			goto fail;

	FD_ZERO(&rfds);
	FD_SET(fan_fd, &rfds);

	if (select(fan_fd+1, &rfds, NULL, NULL, NULL) < 0)
		goto fail;

	while ((len = read(fan_fd, buf, sizeof(buf))) > 0) {
		struct fanotify_event_metadata *metadata;

		metadata = (void *)buf;
		while(FAN_EVENT_OK(metadata, len)) {
			if (opt_fast) {
				ret = set_ignored_mask(fan_fd, metadata->fd,
					       	       FAN_ALL_EVENTS | FAN_ALL_PERM_EVENTS);
				if (ret)
					goto fail;
			}

			if (metadata->fd >= 0) {
				char path[PATH_MAX];
				int path_len;

				sprintf(path, "/proc/self/fd/%d", metadata->fd);
				path_len = readlink(path, path, sizeof(path)-1);
				if (path_len < 0)
					goto fail;
				path[path_len] = '\0';
				printf("%s:", path);
			} else
				printf("?:");

			printf(" pid=%ld", (long)metadata->pid);

			if (metadata->mask & FAN_ACCESS)
				printf(" access");
			if (metadata->mask & FAN_OPEN)
				printf(" open");
			if (metadata->mask & FAN_MODIFY)
				printf(" modify");
			if (metadata->mask & FAN_CLOSE) {
				if (metadata->mask & FAN_CLOSE_WRITE)
					printf(" close(writable)");
				else
					printf(" close");
			}
			if (metadata->mask & FAN_OPEN_PERM)
				printf(" open_perm");
			if (metadata->mask & FAN_ACCESS_PERM)
				printf(" access_perm");
			if (metadata->mask & FAN_ALL_PERM_EVENTS) {
				if (handle_perm(fan_fd, metadata))
					goto fail;
				if (!opt_fast && set_ignored_mask(fan_fd, metadata->fd,
						       		  metadata->mask))
					goto fail;
			}

			printf("\n");

			if (metadata->fd >= 0 && close(metadata->fd) != 0)
				goto fail;
			metadata = FAN_EVENT_NEXT(metadata, len);
		}
		if (select(fan_fd+1, &rfds, NULL, NULL, NULL) < 0)
			goto fail;
	}
	if (len < 0)
		goto fail;
	return 0;

fail:
	fprintf(stderr, "%s\n", strerror(errno));
	return 1;
}
