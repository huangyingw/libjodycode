/* libjodycode: stdio calls
 *
 * Copyright (C) 2014-2023 by Jody Bruchon <jody@jodybruchon.com>
 * Released under The MIT License
 */

#include <errno.h>
#include <stdio.h>
#include "likely_unlikely.h"
#include "libjodycode.h"

#ifdef UNICODE
 #define WIN32_LEAN_AND_MEAN
 #include <windows.h>
#endif


/* Open a file, converting the name for Unicode on Windows if necessary */
extern FILE *jc_fopen(const char *pathname, const JC_WCHAR_T *mode)
{
	FILE *fp;
#ifdef UNICODE
	JC_WCHAR_T *widename;
#endif

	if (unlikely(pathname == NULL || mode == NULL)) {
		jc_errno = EFAULT;
		return NULL;
	}

#ifdef UNICODE
	if (jc_string_to_wstring(pathname, &widename) != 0) {
		jc_errno = ENOMEM;
		return NULL;
	}
	fp = _wfopen(widename, mode);
	free(widename);
#else
	fp = fopen(pathname, mode);
#endif
	if (fp == NULL) jc_errno = errno;
	return fp;
}
