/* Copyright (c) 2009, 2010 The Regents of the University of California
 * Barret Rhoden <brho@cs.berkeley.edu>
 * See LICENSE for details.
 *
 * Default implementations and global values for the VFS. */

#include <vfs.h> // keep this first
#include <sys/queue.h>
#include <assert.h>
#include <stdio.h>
#include <atomic.h>
#include <slab.h>
#include <kmalloc.h>
#include <kfs.h>

struct sb_tailq super_blocks = TAILQ_HEAD_INITIALIZER(super_blocks);
spinlock_t super_blocks_lock = SPINLOCK_INITIALIZER;
struct fs_type_tailq file_systems = TAILQ_HEAD_INITIALIZER(file_systems);
struct namespace default_ns;
// TODO: temp dcache, holds all dentries ever for now
struct dentry_slist dcache = SLIST_HEAD_INITIALIZER(dcache);
spinlock_t dcache_lock = SPINLOCK_INITIALIZER;

struct kmem_cache *dentry_kcache; // not to be confused with the dcache
struct kmem_cache *inode_kcache;
struct kmem_cache *file_kcache;

/* Mounts fs from dev_name at mnt_pt in namespace ns.  There could be no mnt_pt,
 * such as with the root of (the default) namespace.  Not sure how it would work
 * with multiple namespaces on the same FS yet.  Note if you mount the same FS
 * multiple times, you only have one FS still (and one SB).  If we ever support
 * that... */
struct vfsmount *mount_fs(struct fs_type *fs, char *dev_name,
                          struct dentry *mnt_pt, int flags,
                          struct namespace *ns)
{
	struct super_block *sb;
	struct vfsmount *vmnt = kmalloc(sizeof(struct vfsmount), 0);

	/* Build the vfsmount, if there is no mnt_pt, mnt is the root vfsmount (for now).
	 * fields related to the actual FS, like the sb and the mnt_root are set in
	 * the fs-specific get_sb() call. */
	if (!mnt_pt) {
		vmnt->mnt_parent = NULL;
		vmnt->mnt_mountpoint = NULL;
	} else { /* common case, but won't be tested til we try to mount another FS */
		mnt_pt->d_mount_point = TRUE;
		mnt_pt->d_mounted_fs = vmnt;
		atomic_inc(&vmnt->mnt_refcnt); /* held by mnt_pt */
		vmnt->mnt_parent = mnt_pt->d_sb->s_mount;
		vmnt->mnt_mountpoint = mnt_pt;
	}
	TAILQ_INIT(&vmnt->mnt_child_mounts);
	vmnt->mnt_flags = flags;
	vmnt->mnt_devname = dev_name;
	vmnt->mnt_namespace = ns;
	atomic_inc(&ns->refcnt); /* held by vmnt */
	atomic_set(&vmnt->mnt_refcnt, 1); /* for the ref in the NS tailq below */

	/* Read in / create the SB */
	sb = fs->get_sb(fs, flags, dev_name, vmnt);
	if (!sb)
		panic("You're FS sucks");

	/* TODO: consider moving this into get_sb or something, in case the SB
	 * already exists (mounting again) (if we support that) */
	spin_lock(&super_blocks_lock);
	TAILQ_INSERT_TAIL(&super_blocks, sb, s_list); /* storing a ref here... */
	spin_unlock(&super_blocks_lock);

	/* Update holding NS */
	spin_lock(&ns->lock);
	TAILQ_INSERT_TAIL(&ns->vfsmounts, vmnt, mnt_list);
	spin_unlock(&ns->lock);
	/* note to self: so, right after this point, the NS points to the root FS
	 * mount (we return the mnt, which gets assigned), the root mnt has a dentry
	 * for /, backed by an inode, with a SB prepped and in memory. */
	return vmnt;
}

void vfs_init(void)
{
	struct fs_type *fs;

	dentry_kcache = kmem_cache_create("dentry", sizeof(struct dentry),
	                                  __alignof__(struct dentry), 0, 0, 0);
	inode_kcache = kmem_cache_create("inode", sizeof(struct inode),
	                                 __alignof__(struct inode), 0, 0, 0);
	file_kcache = kmem_cache_create("file", sizeof(struct file),
	                                __alignof__(struct file), 0, 0, 0);

	atomic_set(&default_ns.refcnt, 1); // default NS never dies, +1 to exist
	spinlock_init(&default_ns.lock);
	default_ns.root = NULL;
	TAILQ_INIT(&default_ns.vfsmounts);

	/* build list of all FS's in the system.  put yours here.  if this is ever
	 * done on the fly, we'll need to lock. */
	TAILQ_INSERT_TAIL(&file_systems, &kfs_fs_type, list);
	TAILQ_FOREACH(fs, &file_systems, list)
		printk("Supports the %s Filesystem\n", fs->name);

	/* mounting KFS at the root (/), pending root= parameters */
	// TODO: linux creates a temp root_fs, then mounts the real root onto that
	default_ns.root = mount_fs(&kfs_fs_type, "RAM", NULL, 0, &default_ns);

	printk("vfs_init() completed\n");
	/*
	put structs and friends in struct proc, and init in proc init
	*/
	// LOOKUP: follow_mount, follow_link, etc
	// pains in the ass for having .. or . in the middle of the path
}

/* Builds / populates the qstr of a dentry based on its d_iname.  If there is an
 * l_name, (long), it will use that instead of the inline name.  This will
 * probably change a bit. */
void qstr_builder(struct dentry *dentry, char *l_name)
{
	dentry->d_name.name = l_name ? l_name : dentry->d_iname;
	// TODO: pending what we actually do in d_hash
	//dentry->d_name.hash = dentry->d_op->d_hash(dentry, &dentry->d_name); 
	dentry->d_name.hash = 0xcafebabe;
	dentry->d_name.len = strnlen(dentry->d_name.name, MAX_FILENAME_SZ);
}

/* Helper to alloc and initialize a generic superblock.  This handles all the
 * VFS related things, like lists.  Each FS will need to handle its own things
 * in it's *_get_sb(), usually involving reading off the disc. */
struct super_block *get_sb(void)
{
	struct super_block *sb = kmalloc(sizeof(struct super_block), 0);
	sb->s_dirty = FALSE;
	spinlock_init(&sb->s_lock);
	atomic_set(&sb->s_refcnt, 1); // for the ref passed out
	TAILQ_INIT(&sb->s_inodes);
	TAILQ_INIT(&sb->s_dirty_i);
	TAILQ_INIT(&sb->s_io_wb);
	SLIST_INIT(&sb->s_anon_d);
	TAILQ_INIT(&sb->s_files);
	sb->s_fs_info = 0; // can override somewhere else
	return sb;
}

/* Final stages of initializing a super block, including creating and linking
 * the root dentry, root inode, vmnt, and sb.  The d_op and root_ino are
 * FS-specific, but otherwise it's FS-independent, tricky, and not worth having
 * around multiple times.
 *
 * Not the world's best interface, so it's subject to change, esp since we're
 * passing (now 3) FS-specific things. */
void init_sb(struct super_block *sb, struct vfsmount *vmnt,
             struct dentry_operations *d_op, unsigned long root_ino,
             void *d_fs_info)
{
	/* Build and init the first dentry / inode.  The dentry ref is stored later
	 * by vfsmount's mnt_root.  The parent is dealt with later. */
	struct dentry *d_root = get_dentry(sb, 0,  "/");	/* probably right */

	/* a lot of here on down is normally done in lookup() */
	d_root->d_op = d_op;
	d_root->d_fs_info = d_fs_info;
	struct inode *inode = sb->s_op->alloc_inode(sb);
	if (!inode)
		panic("This FS sucks!");
	d_root->d_inode = inode;
	TAILQ_INSERT_TAIL(&inode->i_dentry, d_root, d_alias);
	atomic_inc(&d_root->d_refcnt);			/* held by the inode */
	inode->i_ino = root_ino;
	/* TODO: add the inode to the appropriate list (off i_list) */
	/* TODO: do we need to read in the inode?  can we do this on demand? */
	/* if this FS is already mounted, we'll need to do something different. */
	sb->s_op->read_inode(inode);
	/* Link the dentry and SB to the VFS mount */
	vmnt->mnt_root = d_root;				/* refcnt'd above */
	vmnt->mnt_sb = sb;
	/* If there is no mount point, there is no parent.  This is true only for
	 * the rootfs. */
	if (vmnt->mnt_mountpoint) {
		d_root->d_parent = vmnt->mnt_mountpoint;	/* dentry of the root */
		atomic_inc(&vmnt->mnt_mountpoint->d_refcnt);/* held by d_root */
	}
	/* insert the dentry into the dentry cache.  when's the earliest we can?
	 * when's the earliest we should?  what about concurrent accesses to the
	 * same dentry?  should be locking the dentry... */
	dcache_put(d_root); // TODO: should set a d_flag too
}

/* Helper to alloc and initialize a generic dentry.
 *
 * If the name is longer than the inline name, it will kmalloc a buffer, so
 * don't worry about the storage for *name after calling this. */
struct dentry *get_dentry(struct super_block *sb, struct dentry *parent,
                          char *name)
{
	assert(name);
	size_t name_len = strnlen(name, MAX_FILENAME_SZ);	/* not including \0! */
	struct dentry *dentry = kmem_cache_alloc(dentry_kcache, 0);
	char *l_name = 0;

	//memset(dentry, 0, sizeof(struct dentry));
	atomic_set(&dentry->d_refcnt, 1);	/* this ref is returned */
	spinlock_init(&dentry->d_lock);
	TAILQ_INIT(&dentry->d_subdirs);
	dentry->d_time = 0;
	dentry->d_sb = sb;					/* storing a ref here... */
	dentry->d_mount_point = FALSE;
	dentry->d_mounted_fs = 0;
	dentry->d_parent = parent;
	if (parent)							/* no parent for rootfs mount */
		atomic_inc(&parent->d_refcnt);
	dentry->d_flags = 0;				/* related to its dcache state */
	dentry->d_fs_info = 0;
	SLIST_INIT(&dentry->d_bucket);
	if (name_len < DNAME_INLINE_LEN) {
		strncpy(dentry->d_iname, name, name_len);
		dentry->d_iname[name_len] = '\0';
		qstr_builder(dentry, 0);
	} else {
		l_name = kmalloc(name_len + 1, 0);
		assert(l_name);
		strncpy(l_name, name, name_len);
		l_name[name_len] = '\0';
		qstr_builder(dentry, l_name);
	}
	return dentry;
}

/* Adds a dentry to the dcache. */
void dcache_put(struct dentry *dentry)
{
	// TODO: prob should do something with the dentry flags
	spin_lock(&dcache_lock);
	SLIST_INSERT_HEAD(&dcache, dentry, d_hash);
	spin_unlock(&dcache_lock);
}