#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "libjodycode.h"
#include "likely_unlikely.h"

/* Apple clonefile() is basically a hard link */
#ifdef ENABLE_DEDUPE
 #ifdef __APPLE__
  #include <sys/attr.h>
  #include <copyfile.h>
  #ifndef NO_CLONEFILE
   #include <sys/clonefile.h>
   #define ENABLE_CLONEFILE_LINK 1
  #endif /* NO_CLONEFILE */
 #endif /* __APPLE__ */
#endif /* ENABLE_DEDUPE */

#ifdef __linux__
 /* Use built-in static dedupe header if requested */
 #if defined STATIC_DEDUPE_H || !defined FILE_DEDUPE_RANGE_SAME
  #include "linux-dedupe-static.h"
 #else
  #include <linux/fs.h>
 #endif
 #include <sys/ioctl.h>
 #define KERNEL_DEDUP_MAX_SIZE 16777216
#endif /* __linux__ */

extern int jc_dedupe(struct jc_fileinfo_batch *batch, unsigned int count)
{
#ifdef __linux__
	struct file_dedupe_range *fdr;
	struct file_dedupe_range_info *fdri;
	struct jc_fileinfo *srcfile;
	long max_files;
	unsigned int i, j;
	int retval, src_fd = -1;

	if (unlikely(batch == NULL || count < 2)) goto error_bad_params;

	/* All batch statuses default to general failure */
	for (i = 0; i < count; i++) batch->files[i].status = ECANCELED;

	max_files = sysconf(_SC_PAGESIZE);
	if (unlikely(max_files < (long)sizeof(struct file_dedupe_range_info))) max_files = 1;
	else max_files = sysconf(_SC_PAGESIZE) / (long)sizeof(struct file_dedupe_range_info);
	if (count < max_files) max_files = count;

	fdr = (struct file_dedupe_range *)calloc(1, (size_t)((long)sizeof(struct file_dedupe_range) + ((long)sizeof(struct file_dedupe_range_info) * max_files)));
	if (unlikely(fdr == NULL)) goto error_oom;
	for (i = 1; i < max_files; i++) fdr->info[i].dest_fd = -1;

	/* cherry-pick first file */
	srcfile = &(batch->files[0]);
	errno = 0;
	src_fd = open(srcfile->dirent->d_name, O_RDONLY);
	if (unlikely(src_fd < 0)) goto error_with_errno;

	/* Run batches of files */
	for (i = 1, j = 0; i < count; i++, j++) {
		struct jc_fileinfo *curfile = &(batch->files[i]);
		off_t remain;

		/* Don't pass hard links or data on different devices to dedupe */
		if (srcfile->stat->st_dev != curfile->stat->st_dev || srcfile->stat->st_ino == curfile->stat->st_ino)
			goto error_hardlinked;

		/* Opening the file is required to use ioctl_fideduperange */
		fdri = &(fdr->info[j]);
		fdri->dest_fd = open(curfile->dirent->d_name, O_RDONLY);
		if (fdri->dest_fd == -1) goto error_with_errno;
		fdri->status = FILE_DEDUPE_RANGE_SAME;
		fdr->dest_count++;

		/* FIXME: Redo this entire loop */
		/* Dedupe src <--> dest, 16 MiB or less at a time */
		remain = srcfile->stat->st_size;
		/* Consume data blocks until no data remains */
		while (remain) {
			fdr->src_offset = (uint64_t)(srcfile->stat->st_size - remain);
			fdri->dest_offset = fdr->src_offset;
			fdr->src_length = (uint64_t)(remain <= KERNEL_DEDUP_MAX_SIZE ? remain : KERNEL_DEDUP_MAX_SIZE);
			errno = 0;
			if (ioctl(src_fd, FIDEDUPERANGE, fdr) == -1) goto error_with_errno;
			remain -= (off_t)fdr->src_length;
		}

		/* Handle any errors */
		if (fdri->status != FILE_DEDUPE_RANGE_SAME || errno != 0) goto error_dedupe;
		close((int)fdri->dest_fd);
	}

	retval = 0;
	goto cleanup;

error_oom:
	jc_errno = ENOMEM;
	return -1;
error_bad_params:
	jc_errno = EFAULT;
	return -1;
error_hardlinked:
	jc_errno = EMLINK;
	retval = -1;
	goto cleanup;
error_dedupe:
	jc_errno = EACCES;
	retval = -1;
	goto cleanup;
error_with_errno:
	jc_errno = errno;
	return -1;

cleanup:
	if (src_fd >= 0) {
		close(src_fd);
		if (fdr != NULL) {
			for (i = 0; i < count; i++) {
				if (fdr->info[i].dest_fd >= 0)
					close((int)fdr->info[i].dest_fd);
			}
			free(fdr);
		}
	}
	return retval;
}
#endif /* __linux__ */

#if 0
static void revert_failed(const char * const restrict orig, const char * const restrict current)
{
	fprintf(stderr, "\nwarning: couldn't revert the file to its original name\n");
	fprintf(stderr, "original: "); jc_fwprint(stderr, orig, 1);
	fprintf(stderr, "current:  "); jc_fwprint(stderr, current, 1);
	exit_status = EXIT_FAILURE;
	return;
}


/* linktype: 0=symlink, 1=hardlink, 2=clonefile() */
extern int jc_linkfiles(struct jc_fileinfo *files, const int count, const int linktype)
{
	int *srcfile;
	file_t ** restrict dupelist;
	unsigned int x = 0;
	size_t name_len = 0;
	int i, success;
#ifndef NO_SYMLINKS
	unsigned int symsrc;
	char rel_path[PATHBUF_SIZE];
#endif
#if defined ON_WINDOWS || defined ENABLE_CLONEFILE_LINK
	struct JC_STAT s;
#endif
#ifdef ENABLE_CLONEFILE_LINK
	unsigned int srcfile_preserved_flags = 0;
	unsigned int dupfile_preserved_flags = 0;
	unsigned int dupfile_original_flags = 0;
	struct timeval dupfile_original_tval[2];
#endif


	for (i = 1; i < count; count++) {
		/* Link every file to the first file */
		if (linktype != 0) {
#ifndef NO_HARDLINKS
			srcfile = 0;
			x = 2;
#else
			return -1;
#endif
		} else {
#ifndef NO_SYMLINKS
			x = 1;
			/* Symlinks should target a normal file if one exists */
			srcfile = NULL;
			for (symsrc = 0; symsrc <= count; symsrc++) {
				if (!ISFLAG(dupelist[symsrc]->flags, FF_IS_SYMLINK)) {
					srcfile = symsrc;
					break;
				}
			}
			/* If no normal file exists, abort */
			if (srcfile == NULL) goto linkfile_loop;
#else
			linkfiles_nosupport("soft", "symlink");
#endif
		}
		if (linktype == 2) {
#ifdef ENABLE_CLONEFILE_LINK
			if (jc_stat(srcfile->d_name, &s) != 0) {
				fprintf(stderr, "warning: stat() on source file failed, skipping:\n[SRC] ");
				jc_fwprint(stderr, srcfile->d_name, 1);
				exit_status = EXIT_FAILURE;
				goto linkfile_loop;
			}

			/* macOS unexpectedly copies the compressed flag when copying metadata
			 * (which can result in files being unreadable), so we want to retain
			 * the compression flag of srcfile */
			srcfile_preserved_flags = s.st_flags & UF_COMPRESSED;
#else
			linkfiles_nosupport("clone", "clonefile");
#endif
		}
		for (; x <= count; x++) {
			if (linktype == 1 || linktype == 2) {
				/* Can't hard link files on different devices */
				if (srcfile->device != dupelist[x]->device) {
					fprintf(stderr, "warning: hard link target on different device, not linking:\n-//-> ");
					jc_fwprint(stderr, dupelist[x]->d_name, 1);
					exit_status = EXIT_FAILURE;
					continue;
				} else {
					/* The devices for the files are the same, but we still need to skip
					 * anything that is already hard linked (-L and -H both set) */
					if (srcfile->inode == dupelist[x]->inode) {
						continue;
					}
				}
			} else {
				/* Symlink prerequisite check code can go here */
				/* Do not attempt to symlink a file to itself or to another symlink */
#ifndef NO_SYMLINKS
				if (ISFLAG(dupelist[x]->flags, FF_IS_SYMLINK) &&
						ISFLAG(dupelist[symsrc]->flags, FF_IS_SYMLINK)) continue;
				if (x == symsrc) continue;
#endif
			}

			/* Do not attempt to hard link files for which we don't have write access */
			if (
#ifdef ON_WINDOWS
				!JC_S_ISRO(dupelist[x]->mode) &&
#endif
				(jc_access(dupelist[x]->d_name, JC_W_OK) != 0))
			{
				fprintf(stderr, "warning: link target is a read-only file, not linking:\n-//-> ");
				jc_fwprint(stderr, dupelist[x]->d_name, 1);
				exit_status = EXIT_FAILURE;
				continue;
			}
			/* Check file pairs for modification before linking */
			/* Safe linking: don't actually delete until the link succeeds */
			i = file_has_changed(srcfile);
			if (i) {
				fprintf(stderr, "warning: source file modified since scanned; changing source file:\n[SRC] ");
				jc_fwprint(stderr, dupelist[x]->d_name, 1);
				srcfile = dupelist[x];
				exit_status = EXIT_FAILURE;
				continue;
			}
			if (file_has_changed(dupelist[x])) {
				fprintf(stderr, "warning: target file modified since scanned, not linking:\n-//-> ");
				jc_fwprint(stderr, dupelist[x]->d_name, 1);
				exit_status = EXIT_FAILURE;
				continue;
			}
#ifdef ON_WINDOWS
			/* For Windows, the hard link count maximum is 1023 (+1); work around
			 * by skipping linking or changing the link source file as needed */
			if (jc_stat(srcfile->d_name, &s) != 0) {
				fprintf(stderr, "warning: win_stat() on source file failed, changing source file:\n[SRC] ");
				jc_fwprint(stderr, dupelist[x]->d_name, 1);
				srcfile = dupelist[x];
				exit_status = EXIT_FAILURE;
				continue;
			}
			if (s.st_nlink >= 1024) {
				fprintf(stderr, "warning: maximum source link count reached, changing source file:\n[SRC] ");
				srcfile = dupelist[x];
				exit_status = EXIT_FAILURE;
				continue;
			}
			if (jc_stat(dupelist[x]->d_name, &s) != 0) continue;
			if (s.st_nlink >= 1024) {
				fprintf(stderr, "warning: maximum destination link count reached, skipping:\n-//-> ");
				jc_fwprint(stderr, dupelist[x]->d_name, 1);
				exit_status = EXIT_FAILURE;
				continue;
			}
#endif
#ifdef ENABLE_CLONEFILE_LINK
			if (linktype == 2) {
				if (jc_stat(dupelist[x]->d_name, &s) != 0) {
					fprintf(stderr, "warning: stat() on destination file failed, skipping:\n-##-> ");
					jc_fwprint(stderr, dupelist[x]->d_name, 1);
					exit_status = EXIT_FAILURE;
					continue;
				}

				/* macOS unexpectedly copies the compressed flag when copying metadata
				 * (which can result in files being unreadable), so we want to ignore
				 * the compression flag on dstfile in favor of the one from srcfile */
				dupfile_preserved_flags = s.st_flags & ~(unsigned int)UF_COMPRESSED;
				dupfile_original_flags = s.st_flags;
				dupfile_original_tval[0].tv_sec = s.st_atime;
				dupfile_original_tval[0].tv_usec = 0;
				dupfile_original_tval[1].tv_sec = s.st_mtime;
				dupfile_original_tval[1].tv_usec = 0;
			}
#endif

			/* Make sure the name will fit in the buffer before trying */
			name_len = strlen(dupelist[x]->d_name) + 11;
			if (name_len > PATHBUF_SIZE) continue;
			/* Assemble a temporary file name */
			strcpy(tempname, dupelist[x]->d_name);
			strcat(tempname, "._link_.tmp");
			/* Rename the destination file to the temporary name */
			i = jc_rename(dupelist[x]->d_name, tempname);
			if (i != 0) {
				fprintf(stderr, "warning: cannot move link target to a temporary name, not linking:\n-//-> ");
				jc_fwprint(stderr, dupelist[x]->d_name, 1);
				exit_status = EXIT_FAILURE;
				/* Just in case the rename succeeded yet still returned an error, roll back the rename */
				jc_rename(tempname, dupelist[x]->d_name);
				continue;
			}

			/* Create the desired hard link with the original file's name */
			errno = 0;
			success = 0;
			if (linktype == 1) {
				if (jc_link(srcfile->d_name, dupelist[x]->d_name) == 0) success = 1;
#ifdef ENABLE_CLONEFILE_LINK
			} else if (linktype == 2) {
				if (clonefile(srcfile->d_name, dupelist[x]->d_name, 0) == 0) {
					if (copyfile(tempname, dupelist[x]->d_name, NULL, COPYFILE_METADATA) == 0) {
						/* If the preserved flags match what we just copied from the original dupfile, we're done.
						 * Otherwise, we need to update the flags to avoid data loss due to differing compression flags */
						if (dupfile_original_flags == (srcfile_preserved_flags | dupfile_preserved_flags)) {
							success = 1;
						} else if (chflags(dupelist[x]->d_name, srcfile_preserved_flags | dupfile_preserved_flags) == 0) {
							/* chflags overrides the timestamps that were restored by copyfile, so we need to reapply those as well */
							if (utimes(dupelist[x]->d_name, dupfile_original_tval) == 0) {
								success = 1;
							} else clonefile_error("utimes", dupelist[x]->d_name);
						} else clonefile_error("chflags", dupelist[x]->d_name);
					} else clonefile_error("copyfile", dupelist[x]->d_name);
				} else clonefile_error("clonefile", dupelist[x]->d_name);
#endif /* ENABLE_CLONEFILE_LINK */
			}
#ifndef NO_SYMLINKS
			else {
				i = jc_make_relative_link_name(srcfile->d_name, dupelist[x]->d_name, rel_path);
				if (i < 0) {
					fprintf(stderr, "warning: make_relative_link_name() failed (%d)\n", i);
				} else if (i == 1) {
					fprintf(stderr, "warning: files to be linked have the same canonical path; not linking\n");
				} else if (symlink(rel_path, dupelist[x]->d_name) == 0) success = 1;
			}
#endif /* NO_SYMLINKS */
			if (!success) {
				/* The link failed. Warn the user and put the link target back */
				exit_status = EXIT_FAILURE;
				i = jc_rename(tempname, dupelist[x]->d_name);
				if (i != 0) revert_failed(dupelist[x]->d_name, tempname);
				continue;
			}

			/* Remove temporary file to clean up; if we can't, reverse the linking */
			i = jc_remove(tempname);
			if (i != 0) {
				/* If the temp file can't be deleted, there may be a permissions problem
				 * so reverse the process and warn the user */
				fprintf(stderr, "\nwarning: can't delete temp file, reverting: ");
				jc_fwprint(stderr, tempname, 1);
				exit_status = EXIT_FAILURE;
				i = jc_remove(dupelist[x]->d_name);
				/* This last error really should not happen, but we can't assume it won't */
				if (i != 0) fprintf(stderr, "\nwarning: couldn't remove link to restore original file\n");
				else {
					i = jc_rename(tempname, dupelist[x]->d_name);
					if (i != 0) revert_failed(dupelist[x]->d_name, tempname);
				}
			}
		}
#if !defined NO_SYMLINKS || defined ENABLE_CLONEFILE_LINK
linkfile_loop:
#endif
	}

	return;
}

#endif // 0
