/*
 * Copyright (c) 1998-2017 Erez Zadok
 * Copyright (c) 2009	   Shrikar Archak
 * Copyright (c) 2003-2017 Stony Brook University
 * Copyright (c) 2003-2017 The Research Foundation of SUNY
 * Copyright (c) 2018	   Swapnil Ingle <1985swapnil@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _WRAPFS_H_
#define _WRAPFS_H_

#include <linux/dcache.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/mount.h>
#include <linux/namei.h>
#include <linux/seq_file.h>
#include <linux/statfs.h>
#include <linux/fs_stack.h>
#include <linux/magic.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/xattr.h>
#include <linux/exportfs.h>
#include <linux/hashtable.h>
#include <linux/list.h>

#define WRAPFS_SUPER_MAGIC      0xb550ca10

/* the file system name */
#define WRAPFS_NAME "wrapfs"

/* wrapfs root inode number */
#define WRAPFS_ROOT_INO     1

#define MAXNAMELEN	128
/* useful for tracking code reachability */
#define UDBG printk(KERN_DEFAULT "DBG:%s:%s:%d\n", __FILE__, __func__, __LINE__)

/* wrapfs super-block data in memory */
struct wrapfs_sb_info {
	struct super_block *lower_sb;
	DECLARE_HASHTABLE(hlist, 4);
	spinlock_t hlock;
};

struct wrapfs_ioctl {
	unsigned long ino;
	char path[MAXNAMELEN];
	unsigned int flags;
};

struct wrapfs_list_ioctl {
	struct wrapfs_ioctl *list;
	unsigned long size;
};

struct wrapfs_hnode {
	struct hlist_node hnode;
	char *path;
	unsigned long inode;
	unsigned int flags;
};

/* miscellaneous ioctls */
#define WRAPFS_IOC_HIDE			_IO('h', 1)
#define WRAPFS_IOC_UNHIDE		_IO('h', 2)
#define WRAPFS_IOC_BLOCK		_IO('h', 3)
#define WRAPFS_IOC_UNBLOCK		_IO('h', 4)
#define WRAPFS_IOC_GET_LIST_SIZE	_IO('h', 5)
#define WRAPFS_IOC_GET_LIST		_IO('h', 6)

/* flags */
#define WRAPFS_HIDE	(1 << 0)
#define WRAPFS_BLOCK	(1 << 1)

int wrapfs_hide_file(struct wrapfs_sb_info *sbinfo, const char *path, unsigned
		     long ino);
int wrapfs_unhide_file(struct wrapfs_sb_info *sbinfo, const char *path, unsigned
		     long ino);
int wrapfs_block_file(struct dentry *dentry, const char *path,
		      unsigned long ino);
int wrapfs_unblock_file(struct wrapfs_sb_info *sbinfo, const char *path, unsigned
		     long ino);
int wrapfs_is_hidden(struct wrapfs_sb_info *sbinfo, const char *path,
		     unsigned long ino);
void wrapfs_remove_hnode(struct wrapfs_sb_info *sbinfo, const char *path,
			 unsigned long ino);
int wrapfs_is_blocked(struct wrapfs_sb_info *sbinfo, const char *path,
		      unsigned long inode);
void wrapfs_hide_list_deinit(struct wrapfs_sb_info *sbinfo);
unsigned long wrapfs_get_list_size(struct wrapfs_sb_info *sbinfo);
int wrapfs_get_list(struct wrapfs_sb_info *sbinfo, void __user *buf);

/* operations vectors defined in specific files */
extern const struct file_operations wrapfs_main_fops;
extern const struct file_operations wrapfs_dir_fops;
extern const struct inode_operations wrapfs_main_iops;
extern const struct inode_operations wrapfs_dir_iops;
extern const struct inode_operations wrapfs_symlink_iops;
extern const struct super_operations wrapfs_sops;
extern const struct dentry_operations wrapfs_dops;
extern const struct address_space_operations wrapfs_aops, wrapfs_dummy_aops;
extern const struct vm_operations_struct wrapfs_vm_ops;
extern const struct export_operations wrapfs_export_ops;
extern const struct xattr_handler *wrapfs_xattr_handlers[];

extern int wrapfs_init_inode_cache(void);
extern void wrapfs_destroy_inode_cache(void);
extern int wrapfs_init_dentry_cache(void);
extern void wrapfs_destroy_dentry_cache(void);
extern int new_dentry_private_data(struct dentry *dentry);
extern void free_dentry_private_data(struct dentry *dentry);
extern struct dentry *wrapfs_lookup(struct inode *dir, struct dentry *dentry,
				    unsigned int flags);
extern struct inode *wrapfs_iget(struct super_block *sb,
				 struct inode *lower_inode);
extern int wrapfs_interpose(struct dentry *dentry, struct super_block *sb,
			    struct path *lower_path);

/* file private data */
struct wrapfs_file_info {
	struct file *lower_file;
	const struct vm_operations_struct *lower_vm_ops;
};

/* wrapfs inode data in memory */
struct wrapfs_inode_info {
	struct inode *lower_inode;
	struct inode vfs_inode;
};

/* wrapfs dentry data in memory */
struct wrapfs_dentry_info {
	spinlock_t lock;	/* protects lower_path */
	struct path lower_path;
};

/*
 * inode to private data
 *
 * Since we use containers and the struct inode is _inside_ the
 * wrapfs_inode_info structure, WRAPFS_I will always (given a non-NULL
 * inode pointer), return a valid non-NULL pointer.
 */
static inline struct wrapfs_inode_info *WRAPFS_I(const struct inode *inode)
{
	return container_of(inode, struct wrapfs_inode_info, vfs_inode);
}

/* dentry to private data */
#define WRAPFS_D(dent) ((struct wrapfs_dentry_info *)(dent)->d_fsdata)

/* superblock to private data */
#define WRAPFS_SB(super) ((struct wrapfs_sb_info *)(super)->s_fs_info)

/* file to private Data */
#define WRAPFS_F(file) ((struct wrapfs_file_info *)((file)->private_data))

/* file to lower file */
static inline struct file *wrapfs_lower_file(const struct file *f)
{
	return WRAPFS_F(f)->lower_file;
}

static inline void wrapfs_set_lower_file(struct file *f, struct file *val)
{
	WRAPFS_F(f)->lower_file = val;
}

/* inode to lower inode. */
static inline struct inode *wrapfs_lower_inode(const struct inode *i)
{
	return WRAPFS_I(i)->lower_inode;
}

static inline void wrapfs_set_lower_inode(struct inode *i, struct inode *val)
{
	WRAPFS_I(i)->lower_inode = val;
}

/* superblock to lower superblock */
static inline struct super_block *wrapfs_lower_super(
	const struct super_block *sb)
{
	return WRAPFS_SB(sb)->lower_sb;
}

static inline void wrapfs_set_lower_super(struct super_block *sb,
					  struct super_block *val)
{
	WRAPFS_SB(sb)->lower_sb = val;
}

/* path based (dentry/mnt) macros */
static inline void pathcpy(struct path *dst, const struct path *src)
{
	dst->dentry = src->dentry;
	dst->mnt = src->mnt;
}
/* Returns struct path.  Caller must path_put it. */
static inline void wrapfs_get_lower_path(const struct dentry *dent,
					 struct path *lower_path)
{
	spin_lock(&WRAPFS_D(dent)->lock);
	pathcpy(lower_path, &WRAPFS_D(dent)->lower_path);
	path_get(lower_path);
	spin_unlock(&WRAPFS_D(dent)->lock);
	return;
}
static inline void wrapfs_put_lower_path(const struct dentry *dent,
					 struct path *lower_path)
{
	path_put(lower_path);
	return;
}
static inline void wrapfs_set_lower_path(const struct dentry *dent,
					 struct path *lower_path)
{
	spin_lock(&WRAPFS_D(dent)->lock);
	pathcpy(&WRAPFS_D(dent)->lower_path, lower_path);
	spin_unlock(&WRAPFS_D(dent)->lock);
	return;
}
static inline void wrapfs_reset_lower_path(const struct dentry *dent)
{
	spin_lock(&WRAPFS_D(dent)->lock);
	WRAPFS_D(dent)->lower_path.dentry = NULL;
	WRAPFS_D(dent)->lower_path.mnt = NULL;
	spin_unlock(&WRAPFS_D(dent)->lock);
	return;
}
static inline void wrapfs_put_reset_lower_path(const struct dentry *dent)
{
	struct path lower_path;
	spin_lock(&WRAPFS_D(dent)->lock);
	pathcpy(&lower_path, &WRAPFS_D(dent)->lower_path);
	WRAPFS_D(dent)->lower_path.dentry = NULL;
	WRAPFS_D(dent)->lower_path.mnt = NULL;
	spin_unlock(&WRAPFS_D(dent)->lock);
	path_put(&lower_path);
	return;
}

/* locking helpers */
static inline struct dentry *lock_parent(struct dentry *dentry)
{
	struct dentry *dir = dget_parent(dentry);
	inode_lock_nested(d_inode(dir), I_MUTEX_PARENT);
	return dir;
}

static inline void unlock_dir(struct dentry *dir)
{
	inode_unlock(d_inode(dir));
	dput(dir);
}
#endif	/* not _WRAPFS_H_ */
