/* libjodycode: readdir() stdio call
 *
 * Copyright (C) 2014-2024 by Jody Bruchon <jody@jodybruchon.com>
 * Released under The MIT License
 */

#include <dirent.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "likely_unlikely.h"
#include "libjodycode.h"

#ifdef UNICODE
 #define WIN32_LEAN_AND_MEAN
 #include <windows.h>
#endif


/* Extract d_namlen from struct dirent */
extern size_t jc_get_d_namlen(JC_DIRENT *dirent)
{
#if defined ON_WINDOWS || defined _DIRENT_HAVE_D_NAMLEN
	if (unlikely(dirent == NULL)) goto error_bad_dirent;
	return dirent->d_namlen;
#elif defined _DIRENT_HAVE_D_RECLEN
	const size_t base = (sizeof(struct dirent) - sizeof(((struct dirent *)0)->d_name)) - offsetof(struct dirent, d_name) - 1;
	size_t skip;

	if (unlikely(dirent == NULL)) goto error_bad_dirent;
	skip = dirent->d_reclen - (sizeof(struct dirent) - sizeof(((struct dirent *)0)->d_name));
	if (skip > 0) skip -= base;
	return skip + strlen(dirent->d_name + skip);
#else
	if (unlikely(dirent == NULL)) goto error_bad_dirent;
	return strlen(dirent->d_name);
#endif
error_bad_dirent:
	jc_errno = EFAULT;
	return 0;
}


/* Open a directory; handle Windows doing readdir() equivalent too */
extern JC_DIRENT *jc_readdir(JC_DIR *dirp)
{
#ifdef ON_WINDOWS
	int i;

	if (unlikely(dirp == NULL)) goto error_bad_dirp;
	
	if (dirp->cached == 1) {
		dirp->cached = 0;
		goto skip_fnf;
	}

	i = FindNextFile(dirp->hFind, &(dirp->ffd));
	if (i == 0) goto error_fnf;
	if (jc_ffd_to_dirent(&dirp, NULL, NULL) != 0) return NULL;

skip_fnf:
	return &(dirp->dirent);

error_bad_dirp:
	jc_errno = EFAULT;
	return NULL;
error_fnf:
	jc_errno = jc_GetLastError();
	return NULL;

#else  /* Non-Windows */

	JC_DIRENT *retval;

	errno = 0;
	retval = readdir(dirp);
	if (retval == NULL) jc_errno = errno;
	return retval;
#endif /* ON_WINDOWS */
}
