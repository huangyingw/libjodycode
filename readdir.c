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
	JC_DIRENT *retval;
#ifdef ON_WINDOWS
	JC_DIR **prev, **cur;

	if (unlikely(dirp == NULL)) goto error_bad_dirp;
	
	/* Scan dirp list for a cached dirent */
	for (prev = NULL, cur = dirp_head; cur != dirp && cur != NULL; prev = cur, cur = cur->next);
	if (cur != dirp) goto error_bad_dirp;

	// TODO: return cached value if present

	// TODO: FindNextFile() and return result

	return retval;
#else
	errno = 0;
	retval = readdir(path);
	if (retval == NULL) jc_errno = errno;
	return retval;
#endif /* ON_WINDOWS */
error_bad_dirp:
	jc_errno = EFAULT;
	return NULL;
}
