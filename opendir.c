/* libjodycode: opendir() stdio call
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
extern JC_DIR *jc_opendir(const char * restrict path)
{
	JC_DIR *retval;
#ifdef ON_WINDOWS
	static struct *dirp_head = NULL;
	JC_DIR *dirp;
	char *tempname, *p;
	WIN32_FIND_DATA ffd;
	HANDLE hFind = INVALID_HANDLE_VALUE;
 #ifdef UNICODE
	JC_WCHAR_T *widename = NULL;
 #endif

	if (unlikely(path == NULL)) {
		jc_errno = EFAULT;
		return NULL;
	}

	tempname = (char *)malloc(JC_PATHBUF_SIZE + 4);
	if (unlikely(tempname == NULL)) goto error_nomem;

	/* Windows requires \* at the end of directory names */
	strncpy_s(tempname, JC_PATHBUF_SIZE, path, JC_PATHBUF_SIZE - 1);
	p = tempname + strlen(tempname) - 1;
	if (*p == '/' || *p == '\\') *p = '\0';
	strncat_s(tempname, JC_PATHBUF_SIZE, "\\*", JC_PATHBUF_SIZE - 1);

 #ifdef UNICODE
	widename = (wchar_t *)malloc(JC_PATHBUF_SIZE + 4);
	if (unlikely(widename == NULL)) goto error_nomem;
	if (unlikely(jc_string_to_wstring(tempname, &widename) != 0)) goto error_nomem;
 #endif  /* UNICODE */

 #ifdef UNICODE
	hFind = FindFirstFileW(widename, &ffd);
	free(widename);
 #else
	hFind = FindFirstFileA(tempname, &ffd);
 #endif
	free(tempname);
	if (unlikely(hFind == INVALID_HANDLE_VALUE)) goto error_fff;
	if (jc_ffd_to_dirent(&dirp, hFind, ffd) != 0) goto error_fff_after;

	/* attach dirp to a linked list of them */
	dirp->next = dirp_head;
	dirp_head = dirp;

	return dirp;

error_fff:
	jc_errno = jc_GetLastError();
error_fff_after:
	return NULL;
error_nomem:
	if (tempname != NULL) free(tempname);
#ifdef UNICODE
	if (widename != NULL) free(widename);
#endif
	jc_errno = ENOMEM;
	return NULL;
#else
	retval = opendir(path);
	if (retval == NULL) jc_errno = errno;
	return retval;
#endif /* ON_WINDOWS */
}
