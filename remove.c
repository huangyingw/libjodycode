/* libjodycode: stdio calls
 *
 * Copyright (C) 2014-2023 by Jody Bruchon <jody@jodybruchon.com>
 * Released under The MIT License
 */

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "likely_unlikely.h"
#include "libjodycode.h"

#ifdef UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#endif


/* Delete a file, converting for Windows if necessary */
extern int jc_remove(const char *pathname)
{
	int retval;
#ifdef UNICODE
	JC_WCHAR_T *widename;
#endif

	if (unlikely(pathname == NULL)) {
		jc_errno = EFAULT;
		return -1;
	}

#ifdef UNICODE
	if (jc_string_to_wstring(pathname, &widename) != 0) {
		jc_errno = ENOMEM;
		return -1;
	}
	retval = DeleteFileW(widename) ? 0 : -1;
	free(widename);
	if (retval != 0) jc_errno = jc_GetLastError();
#else
	retval = remove(pathname);
	if (retval != 0) jc_errno = errno;
#endif
	return retval;
}
