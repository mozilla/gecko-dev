/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_system_fuse_h__
#define mozilla_system_fuse_h__

//#include <pthread.h>
//#include <private/android_filesystem_config.h>

#define CLOUD_STORAGE_MAX_WRITE (256 * 1024)
#define CLOUD_STORAGE_MAX_READ (128 * 1024)
#define CLOUD_STORAGE_MAX_REQUEST_SIZE (sizeof(FuseInHeader) + sizeof(FuseWriteIn) + CLOUD_STORAGE_MAX_WRITE)

#define FUSE_KERNEL_VERSION 7
#define FUSE_KERNEL_MINOR_VERSION 13
#define FUSE_ROOT_ID 1
#define FUSE_UNKNOWN_INO 0xffffffff

#define FUSE_ASYNC_READ         (1 << 0)
#define FUSE_POSIX_LOCKS        (1 << 1)
#define FUSE_FILE_OPS           (1 << 2)
#define FUSE_ATOMIC_O_TRUNC     (1 << 3)
#define FUSE_EXPORT_SUPPORT     (1 << 4)
#define FUSE_BIG_WRITES         (1 << 5)
#define FUSE_DONT_MASK          (1 << 6)
#define FUSE_SPLICE_WRITE       (1 << 7)
#define FUSE_SPLICE_MOVE        (1 << 8)
#define FUSE_SPLICE_READ        (1 << 9)
#define FUSE_FLOCK_LOCKS        (1 << 10)
#define FUSE_HAS_IOCTL_DIR      (1 << 11)
#define FUSE_AUTO_INVAL_DATA    (1 << 12)
#define FUSE_DO_READDIRPLUS     (1 << 13)
#define FUSE_READDIRPLUS_AUTO   (1 << 14)
#define FUSE_ASYNC_DIO          (1 << 15)
#define FUSE_WRITEBACK_CACHE    (1 << 16)
#define FUSE_NO_OPEN_SUPPORT    (1 << 17)

typedef struct SFuseAttr {
  __u64  ino;
  __u64  size;
  __u64  blocks;
  __u64  atime;
  __u64  mtime;
  __u64  ctime;
  __u32  atimensec;
  __u32  mtimensec;
  __u32  ctimensec;
  __u32  mode;
  __u32  nlink;
  __u32  uid;
  __u32  gid;
  __u32  rdev;
  __u32  blksize;
  __u32  padding;
} FuseAttr;

typedef struct FuseEntryOut {
  __u64  nodeid;       // Inode ID 
  __u64  generation;   // Inode generation: nodeid:gen must
                       //   be unique for the fs's lifetime
  __u64  entry_valid;  // Cache timeout for the name
  __u64  attr_valid;   // Cache timeout for the attributes
  __u32  entry_valid_nsec;
  __u32  attr_valid_nsec;
  FuseAttr attr;
} FuseEntryOut;

typedef struct SFuseForgetIn {
  __u64  nlookup;
} FuseForgetIn;

typedef struct SFuseGetAttrIn {
  __u32  getattr_flags;
  __u32  dummy;
  __u64  fh;
} FuseGetAttrIn;

typedef struct SFuseAttrOut {
  __u64  attr_valid;  // Cache timeout for the attributes
  __u32  attr_valid_nsec;
  __u32  dummy;
  FuseAttr attr;
} FuseAttrOut;

typedef struct SFuseMkNodIn {
  __u32  mode;
  __u32  rdev;
  __u32  umask;
  __u32  padding;
} FuseMkNodIn;

typedef struct SFuseMkDirIn {
  __u32  mode;
  __u32  umask;
} FuseMkDirIn;

typedef struct SFuseRenameIn {
  __u64  newdir;
} FuseRenameIn;

typedef struct SFuseLinkIn {
  __u64  oldnodeid;
} FuseLinkIn;

typedef struct SFuseSetAttrIn {
  __u32  valid;
  __u32  padding;
  __u64  fh;
  __u64  size;
  __u64  lock_owner;
  __u64  atime;
  __u64  mtime;
  __u64  unused2;
  __u32  atimensec;
  __u32  mtimensec;
  __u32  unused3;
  __u32  mode;
  __u32  unused4;
  __u32  uid;
  __u32  gid;
  __u32  unused5;
} FuseSetAttrIn;

typedef struct SFuseOpenIn {
  __u32  flags;
  __u32  unused;
} FuseOpenIn;

typedef struct SFuseCreateIn {
  __u32  flags;
  __u32  mode;
  __u32  umask;
  __u32  padding;
} FuseCreateIn;

typedef struct SFuseOpenOut {
  __u64  fh;
  __u32  open_flags;
  __u32  padding;
} FuseOpenOut;

typedef struct SFuseReleaseIn {
  __u64  fh;
  __u32  flags;
  __u32  release_flags;
  __u64  lock_owner;
} FuseReleaseIn;

typedef struct SFuseFlushIn {
  __u64  fh;
  __u32  unused;
  __u32  padding;
  __u64  lock_owner;
} FuseFlushIn;

typedef struct SFuseReadIn {
  __u64  fh;
  __u64  offset;
  __u32  size;
  __u32  read_flags;
  __u64  lock_owner;
  __u32  flags;
  __u32  padding;
} FuseReadIn;

typedef struct SFuseWriteIn {
  __u64  fh;
  __u64  offset;
  __u32  size;
  __u32  write_flags;
  __u64  lock_owner;
  __u32  flags;
  __u32  padding;
} FuseWriteIn;

typedef struct SFuseWriteOut {
  __u32  size;
  __u32  padding;
} FuseWriteOut;

typedef struct SFuseKstatfs {
  __u64  blocks;
  __u64  bfree;
  __u64  bavail;
  __u64  files;
  __u64  ffree;
  __u32  bsize;
  __u32  namelen;
  __u32  frsize;
  __u32  padding;
  __u32  spare[6];
} FuseKstatfs;


typedef struct SFuseStatfsOut {
  FuseKstatfs st;
} FuseStatfsOut;

typedef struct SFuseFsyncIn {
  __u64  fh;
  __u32  fsync_flags;
  __u32  padding;
} FuseFsyncIn;

typedef struct SFuseSetXAttrIn {
  __u32  size;
  __u32  flags;
} FuseSetXAttrIn;

typedef struct SFuseGetXAttrIn {
  __u32  size;
  __u32  padding;
} FuseGetXAttrIn;

typedef struct SFuseGetXAttrOut {
  __u32  size;
  __u32  padding;
} FuseGetXAttrOut;

typedef struct SFuseFileLock {
  __u64  start;
  __u64  end;
  __u32  type;
  __u32  pid; // tgid
} FuseFileLock;

typedef struct SFuseLkIn {
  __u64  fh;
  __u64  owner;
  FuseFileLock lk;
  __u32  lk_flags;
  __u32  padding;
} FuseLkIn;

typedef struct SFuseLkOut {
  FuseFileLock lk;
} FuseLkOut;

typedef struct SFuseAccessIn {
  __u32  mask;
  __u32  padding;
} FuseAccessIn;

typedef struct SFuseInitIn {
  __u32 major;
  __u32 minor;
  __u32 max_readahead;
  __u32  flags;
} FuseInitIn;

typedef struct SFuseInitOut {
  __u32  major;
  __u32  minor;
  __u32  max_readahead;
  __u32  flags;
  __u16   max_background;
  __u16   congestion_threshold;
  __u32  max_write;
} FuseInitOut;

typedef struct SCuseInitIn {
  __u32  major;
  __u32  minor;
  __u32  unused;
  __u32  flags;
} CuseInitIn;

typedef struct SCuseInitOut {
  __u32  major;
  __u32  minor;
  __u32  unused;
  __u32  flags;
  __u32  max_read;
  __u32  max_write;
  __u32  dev_major;    // chardev major
  __u32  dev_minor;    // chardev minor
  __u32  spare[10];
} CuseInitOut;

typedef struct SFuseInterruptIn {
  __u64  unique;
} FuseInterruptIn;

typedef struct SFuseBmapIn {
  __u64  block;
  __u32  blocksize;
  __u32  padding;
} FuseBmapIn;

typedef struct SFuseBmapOut {
  __u64  block;
} FuseBmapOut;

typedef struct SFuseIoctlIn {
  __u64  fh;
  __u32  flags;
  __u32  cmd;
  __u64  arg;
  __u32  in_size;
  __u32  out_size;
} FuseIoctlIn;

typedef struct SFuseIoctlOut {
  __s32  result;
  __u32  flags;
  __u32  in_iovs;
  __u32  out_iovs;
} FuseIoctlOut;

typedef struct SFusePollIn {
  __u64  fh;
  __u64  kh;
  __u32  flags;
  __u32   padding;
} FusePollIn;

typedef struct SFusePollOut {
  __u32  revents;
  __u32  padding;
} FusePollOut;

typedef struct SFuseNotifyPollWakeupOut {
  __u64  kh;
} FuseNotifyPollWakeupOut;

typedef struct SFuseInHeader {
  __u32  len;
  __u32  opcode;
  __u64  unique;
  __u64  nodeid;
  __u32  uid;
  __u32  gid;
  __u32  pid;
  __u32  padding;
} FuseInHeader;

typedef struct SFuseOutHeader {
  __u32  len;
  __s32  error;
  __u64  unique;
} FuseOutHeader;

typedef struct SFuse {
  //pthread_mutex_t lock;
  __u64 next_generation;
  __u64 rootnid;
  int fd;
} Fuse;

typedef struct SFuseHandler {
  Fuse* fuse;
  int token;
  union {
    __u8 request_buffer[CLOUD_STORAGE_MAX_REQUEST_SIZE];
    __u8 read_buffer[CLOUD_STORAGE_MAX_READ];
  };
} FuseHandler;

#define FUSE_NAME_OFFSET offsetof(FuseDirent, name)
#define FUSE_DIRENT_ALIGN(x) (((x) + sizeof(__u64) - 1) & ~(sizeof(__u64) - 1))
#define FUSE_DIRENT_SIZE(d) \
	FUSE_DIRENT_ALIGN(FUSE_NAME_OFFSET + (d)->namelen)


typedef struct SFuseDirent {
  __u64 ino;
  __u64 off;
  __u32 namelen;
  __u32 type;
  char name[0];
} FuseDirent;

typedef enum eFuseOpcode {
  FUSE_LOOKUP      = 1,
  FUSE_FORGET      = 2,  // no reply 
  FUSE_GETATTR     = 3,
  FUSE_SETATTR     = 4,
  FUSE_READLINK    = 5,
  FUSE_SYMLINK     = 6,
  FUSE_MKNOD       = 8,
  FUSE_MKDIR       = 9,
  FUSE_UNLINK      = 10,
  FUSE_RMDIR       = 11,
  FUSE_RENAME      = 12,
  FUSE_LINK        = 13,
  FUSE_OPEN        = 14,
  FUSE_READ        = 15,
  FUSE_WRITE       = 16,
  FUSE_STATFS      = 17,
  FUSE_RELEASE     = 18,
  FUSE_FSYNC       = 20,
  FUSE_SETXATTR    = 21,
  FUSE_GETXATTR    = 22,
  FUSE_LISTXATTR   = 23,
  FUSE_REMOVEXATTR = 24,
  FUSE_FLUSH       = 25,
  FUSE_INIT        = 26,
  FUSE_OPENDIR     = 27,
  FUSE_READDIR     = 28,
  FUSE_RELEASEDIR  = 29,
  FUSE_FSYNCDIR    = 30,
  FUSE_GETLK       = 31,
  FUSE_SETLK       = 32,
  FUSE_SETLKW      = 33,
  FUSE_ACCESS      = 34,
  FUSE_CREATE      = 35,
  FUSE_INTERRUPT   = 36,
  FUSE_BMAP        = 37,
  FUSE_DESTROY     = 38,
  FUSE_IOCTL       = 39,
  FUSE_POLL        = 40,
  // CUSE specific operations
  CUSE_INIT        = 4096,
} FuseOpcode;

#endif // end mozilla_system_fuse_h__
