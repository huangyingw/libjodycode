/* libjodycode: closedir() stdio call
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


/* De-allocate a directory struct and remove from the list */
static void jc_destroy_dirp(JC_DIR *dirp)
{
	JC_DIR *prev, *cur;

	if (dirp == NULL) return;

	for (prev = NULL, cur = dirp_head; cur != dirp && cur != NULL; prev = cur, cur = cur->next);
	if (cur != dirp) return;
	if (dirp == dirp_head) {
		dirp_head = cur->next;
		free(dirp);
		return;
	}
	prev->next = cur->next;
	free(dirp);
	return;
}


/* Close a directory */
extern int jc_closedir(JC_DIR *dirp)
{
	int retval;
#ifdef ON_WINDOWS
	if (unlikely(dirp == NULL)) {
		jc_errno = EBADF;
		return -1;
	}

	retval = FindClose(dirp->hFind);
	jc_destroy_dirp(dirp);
	if (retval != 0) jc_errno = jc_GetLastError();
	return -1;

#else

	retval = closedir(dirp);
	if (retval != 0) jc_errno = errno;
	return retval;
#endif /* ON_WINDOWS */
}
