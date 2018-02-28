#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <getopt.h>
#include <string.h>
#include <linux/types.h>
#include <limits.h>

#include "wrapfs.h"
#define ARRAY_SIZE(arr) (sizeof(arr)/sizeof((arr)[0]))

const char *progname;

static int hide_file(char **args, int argc);
static int unhide_file(char **args, int argc);
static int block_file(char **args, int argc);
static int unblock_file(char **args, int argc);
static int list_all(char **args, int argc);
static int help(char **args, int argc);

struct cmd_opts {
	const char *cmd;
	int (*func) (char **args, int argc);
	const char *usage;
} cmds[] = {
	{"hide",	hide_file,	"hide     <path>"},
	{"unhide",	unhide_file,	"unhide   <path>"},
	{"block",	block_file,	"block    <path>"},
	{"unblock",	unblock_file,	"unblock  <path> <inode_number> <mntpt>"},
	{"list",	list_all,	"list"},
	{"help",	help,		"help"},
};

void usage()
{
	int i;

	printf("  %s [options]\n", progname);
	for (i=0; i<ARRAY_SIZE(cmds); i++)
		printf("\t\t%s\n", cmds[i].usage);
}

static int get_inode_number(const char *path, unsigned long *ino)
{
	struct stat stbuf;
	int err;

	err = stat(path, &stbuf);
	if (err) {
		printf("stat failed on %s: %s\n", path, strerror(errno));
		return err;
	}
	*ino = stbuf.st_ino;
	return err;
}

static int do_ioctl(const char *dev, long cmd,
		    struct wrapfs_ioctl *wr_ioctl)
{
	int fd, err;

	fd = open(dev, O_RDONLY);
	if (fd < 0) {
		printf("open failed(%s): %s\n", strerror(errno), dev);
		return fd;
	}

	err = ioctl(fd, cmd, wr_ioctl);
	if (err)
		printf("ioctl failed(%s): %s\n", strerror(errno), dev);

	close(fd);
	return err;
}

/* trim leading '/' */
void trim(char *fname)
{
	int sz = strlen(fname);

	if (fname[sz - 1] == '/')
		fname[sz - 1] = '\0';
}

static int hide_file(char **args, int argc)
{
	struct wrapfs_ioctl wr_ioctl = {0};
	int err, cmd;
	char *dev;

	if (argc < 1) {
		printf("Not enough agruments\n");
		usage();
		return -EINVAL;
	}

	strcpy(wr_ioctl.path, args[0]);
	trim(wr_ioctl.path);

	err = get_inode_number(wr_ioctl.path, &wr_ioctl.ino);
	if (err < 0)
		return err;

	cmd = WRAPFS_IOC_HIDE;
	dev = wr_ioctl.path;

	err = do_ioctl(dev, cmd, &wr_ioctl);
	if (!err)
		printf("%s hidden\n", wr_ioctl.path);
	return err;
}

static int unhide_file(char **args, int argc)
{
	struct wrapfs_ioctl wr_ioctl = {0};
	int err, cmd;
	char *dev;

	if (argc < 1) {
		printf("Not enough agruments\n");
		usage();
		return -EINVAL;
	}

	strcpy(wr_ioctl.path, args[0]);
	trim(wr_ioctl.path);

	err = get_inode_number(wr_ioctl.path, &wr_ioctl.ino);
	if (err < 0)
		return err;

	cmd = WRAPFS_IOC_UNHIDE;
	dev = wr_ioctl.path;

	err = do_ioctl(dev, cmd, &wr_ioctl);
	if (!err)
		printf("unhide %s\n", wr_ioctl.path);
	return err;
}

static int block_file(char **args, int argc)
{
	struct wrapfs_ioctl wr_ioctl = {0};
	int err, cmd;
	char *dev;

	if (argc < 1) {
		printf("Not enough agruments\n");
		usage();
		return -EINVAL;
	}

	strcpy(wr_ioctl.path, args[0]);
	trim(wr_ioctl.path);

	err = get_inode_number(wr_ioctl.path, &wr_ioctl.ino);
	if (err < 0)
		return err;

	cmd = WRAPFS_IOC_BLOCK;
	dev = wr_ioctl.path;

	err = do_ioctl(dev, cmd, &wr_ioctl);
	if (!err)
		printf("blocked %s\n", wr_ioctl.path);
	return err;
}

static int unblock_file(char **args, int argc)
{
	struct wrapfs_ioctl wr_ioctl = {0};
	char *endp;
	int err, cmd;
	char *dev;

	if (argc < 3) {
		printf("Not enough agruments\n");
		usage();
		return -EINVAL;
	}

	strcpy(wr_ioctl.path, args[0]);
	trim(wr_ioctl.path);
	wr_ioctl.ino = strtoul(args[1], &endp, 10);

	cmd = WRAPFS_IOC_UNBLOCK;
	dev = args[2];

	err = do_ioctl(dev, cmd, &wr_ioctl);
	if (!err)
		printf("unblocked %s\n", wr_ioctl.path);
	return err;
}

const char *flags_to_str(unsigned int flags)
{
	if (flags == WRAPFS_BLOCK)
		return "blocked";
	else if (flags == WRAPFS_HIDE)
		return "hidden";
	else if (flags & (WRAPFS_BLOCK|WRAPFS_HIDE))
		return "blocked,hidden";
	return "";
}

static int list_all(char **args, int argc)
{
	struct wrapfs_ioctl wr_ioctl[128] = {0};
	int fd, count = 0, i;

	if (argc) {
		printf("Not enough agruments\n");
		usage();
		return -EINVAL;
	}

	fd = open(WRAPFS_CDEV, O_RDWR);
	if (fd < 0) {
		printf("open failed: %s\n", strerror(-errno));
		return fd;
	}

	printf("%-16s%-11s%s\n", "STATE", "INODE_NUM", "FILE");
	do {
		count = read(fd, &wr_ioctl, sizeof(wr_ioctl));
		if (count > 0) {
			for (i=0; i<count; i++)
				printf("%-16s%-11lu%s\n",
				       flags_to_str(wr_ioctl[i].flags),
				       wr_ioctl[i].ino, wr_ioctl[i].path);
		}
	} while (count > 0);
	close(fd);
	return 0;
}

static int help(char **args, int argc)
{
	usage();
	return 0;
}

int main(int argc, char **argv)
{
	int min_args = 2, i;

	progname = argv[0];
	if (argc < 2) {
		printf("Invalid arguments\n");
		usage();
		return -EINVAL;
	}

	for (i=0; i<ARRAY_SIZE(cmds); i++) {
		if (!strcmp(cmds[i].cmd, argv[1]))
			return cmds[i].func(&argv[2], argc - 2);
	}

	printf("unknown option\n");
	usage();
	return -EINVAL;
}
