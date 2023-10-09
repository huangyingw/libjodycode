/* libjodycode: readdir() stdio call
 *
 * Copyright (C) 2014-2023 by Jody Bruchon <jody@jodybruchon.com>
 * Released under The MIT License
 */

#ifndef ON_WINDOWS
 #include <dirent.h>
#endif
#include <errno.h>
#include <stdio.h>
#include "likely_unlikely.h"
#include "libjodycode.h"

#ifdef UNICODE
 #define WIN32_LEAN_AND_MEAN
 #include <windows.h>
#endif


/* Open a directory; handle Windows doing readdir() equivalent too */
extern JC_DIRENT *jc_readdir(JC_DIR *dirp)
{
#ifdef ON_WINDOWS
	int i;

	if (unlikely(dirp == NULL)) goto error_bad_dirp;
	
	/* Save this for jc_closedir() */
//	for (prev = NULL, cur = dirp_head; cur != dirp && cur != NULL; prev = cur, cur = cur->next);
//	if (cur != dirp) goto error_bad_dirp;

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
	retval = readdir(path);
	if (retval == NULL) jc_errno = errno;
	return retval;
#endif /* ON_WINDOWS */
}
