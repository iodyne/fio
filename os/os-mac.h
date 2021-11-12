#ifndef FIO_OS_APPLE_H
#define FIO_OS_APPLE_H

#define	FIO_OS	os_mac

#include <errno.h>
#include <fcntl.h>
#include <sys/disk.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <unistd.h>
#include <signal.h>
#include <mach/mach_init.h>
#include <machine/endian.h>
#include <libkern/OSByteOrder.h>

#include "../file.h"

#define FIO_USE_GENERIC_INIT_RANDOM_STATE
#define FIO_HAVE_GETTID
#define FIO_HAVE_CHARDEV_SIZE
#define FIO_HAVE_NATIVE_FALLOCATE
#define FIO_HAVE_TRIM

/*
 * macOS doesn't support sync_file_range() for files, but since we only want
 * this option for raw disk devices, we implement this to DKIOCSYNCHRONIZE.
 */

#define	CONFIG_SYNC_FILE_RANGE
#define	SYNC_FILE_RANGE_WAIT_BEFORE	0x1
#define	SYNC_FILE_RANGE_WRITE		0x2
#define	SYNC_FILE_RANGE_WAIT_AFTER	0x4

#define OS_MAP_ANON		MAP_ANON

#define fio_swap16(x)	OSSwapInt16(x)
#define fio_swap32(x)	OSSwapInt32(x)
#define fio_swap64(x)	OSSwapInt64(x)

#ifdef CONFIG_PTHREAD_GETAFFINITY
#define FIO_HAVE_GET_THREAD_AFFINITY
#define fio_get_thread_affinity(mask)	\
	pthread_getaffinity_np(pthread_self(), sizeof(mask), &(mask))
#endif

#ifndef CONFIG_CLOCKID_T
typedef unsigned int clockid_t;
#endif

#define FIO_OS_DIRECTIO
static inline int fio_set_odirect(struct fio_file *f)
{
	if (fcntl(f->fd, F_NOCACHE, 1) == -1)
		return errno;
	return 0;
}

static inline int blockdev_size(struct fio_file *f, unsigned long long *bytes)
{
	uint32_t block_size;
	uint64_t block_count;

	if (ioctl(f->fd, DKIOCGETBLOCKCOUNT, &block_count) == -1)
		return errno;
	if (ioctl(f->fd, DKIOCGETBLOCKSIZE, &block_size) == -1)
		return errno;

	*bytes = block_size;
	*bytes *= block_count;
	return 0;
}

static inline int chardev_size(struct fio_file *f, unsigned long long *bytes)
{
	/*
	 * Could be a raw block device, this is better than just assuming
	 * we can't get the size at all.
	 */
	if (!blockdev_size(f, bytes))
		return 0;

	*bytes = -1ULL;
	return 0;
}

static inline int blockdev_invalidate_cache(struct fio_file *f)
{
	return ENOTSUP;
}

static inline unsigned long long os_phys_mem(void)
{
	int mib[2] = { CTL_HW, HW_PHYSMEM };
	unsigned long long mem;
	size_t len = sizeof(mem);

	sysctl(mib, 2, &mem, &len, NULL, 0);
	return mem;
}

#ifndef CONFIG_HAVE_GETTID
static inline int gettid(void)
{
	return mach_thread_self();
}
#endif

static inline bool fio_fallocate(struct fio_file *f, uint64_t offset, uint64_t len)
{
	fstore_t store = {F_ALLOCATEALL, F_PEOFPOSMODE, offset, len};
	if (fcntl(f->fd, F_PREALLOCATE, &store) != -1) {
		if (ftruncate(f->fd, len) == 0)
			return true;
	}

	return false;
}

static inline int os_trim(struct fio_file *f, unsigned long long start,
			  unsigned long long len)
{
	dk_extent_t x;
	dk_unmap_t u;

	x.offset = start;
	x.length = len;

	u.extents = &x;
	u.extentsCount = 1;
	u.options = 0;

	if (!ioctl(f->fd, DKIOCUNMAP, &u))
		return 0;

	return errno;
}

static inline int sync_file_range(int fd, uint64_t offset, uint64_t nbytes,
		    unsigned int flags)
{
	dk_synchronize_t s;

	bzero(&s, sizeof (s));
	s.offset = offset;
	s.length = nbytes;
	s.options = 0;

	if (!ioctl(fd, DKIOCSYNCHRONIZE, &s))
		return (0);

	return errno;
}

#endif
