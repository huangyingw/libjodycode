/* Bare header for Linux dedupe API */
#ifndef LINUX_DEDUPESTATIC_H
#define LINUX_DEDUPESTATIC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <linux/types.h>
#include <linux/ioctl.h>
#define FILE_DEDUPE_RANGE_SAME    0
#define FILE_DEDUPE_RANGE_DIFFERS 1
struct file_dedupe_range_info {
	__s64 dest_fd;
	__u64 dest_offset;
	__u64 bytes_deduped;
	__s32 status;
	__u32 reserved;
};
struct file_dedupe_range {
	__u64 src_offset;
	__u64 src_length;
	__u16 dest_count;
	__u16 reserved1;
	__u32 reserved2;
	struct file_dedupe_range_info info[0];
};
#define FIDEDUPERANGE _IOWR(0x94, 54, struct file_dedupe_range)

#ifdef __cplusplus
}
#endif

#endif /* LINUX_DEDUPESTATIC_H */
