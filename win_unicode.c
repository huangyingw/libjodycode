/* libjodycode: Windows Unicode support helper code
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

#ifdef ON_WINDOWS
 #define WIN32_LEAN_AND_MEAN
 #include <windows.h>
 #include <io.h>
#endif  /* ON_WINDOWS */

#ifdef ON_WINDOWS
/* Convert slashes to backslashes in a file path */
extern void jc_slash_convert(char *path)
{
	while (*path != '\0') {
		if (*path == '/') *path = '\\';
		path++;
	}
	return;
}
#endif


#ifdef UNICODE
/* Copy a string to a wide string - wstring must be freed by the caller */
extern int jc_string_to_wstring(const char * const restrict string, JC_WCHAR_T **wstring)
{
	if (unlikely(wstring == NULL)) return JC_ENULL;
	*wstring = (JC_WCHAR_T *)malloc(PATH_MAX + 4);
	if (unlikely(*wstring == NULL)) return JC_EALLOC;
	if (unlikely(!M2W(string, *wstring))) {
		free(*wstring);
		return JC_EMBWC;
	}
	return 0;
}


/* Copy Windows wide character arguments to UTF-8 */
extern int jc_widearg_to_argv(int argc, JC_WCHAR_T **wargv, char **argv)
{
	static char temp[JC_PATHBUF_SIZE * 2];
	int len;

	if (unlikely(!argv)) return JC_ENULL;
	for (int counter = 0; counter < argc; counter++) {
		len = W2M(wargv[counter], &temp);
		if (unlikely(len < 1)) {
			jc_errno = jc_GetLastError();
			return JC_EBADARGV;
		}

		argv[counter] = (char *)malloc((size_t)len + 1);
		if (unlikely(!argv[counter])) return JC_EALLOC;
		strncpy(argv[counter], temp, (size_t)len + 1);
	}
	return 0;
}
#endif /* UNICODE */


#ifdef ON_WINDOWS
/* Copy WIN32_FIND_FILE data to DIR data for a JC_DIR */
extern int jc_ffd_to_dirent(JC_DIR **dirp, HANDLE hFind, WIN32_FIND_DATA ffd)
{
#ifdef UNICODE
	char *tempname;
#endif

	if (dirp == NULL) {
		jc_errno = EFAULT;
		return -1;
	}

 #ifdef UNICODE
	/* Must count bytes after conversion to allocate correct size */
	tempname = (char *)malloc(JC_PATHBUF_SIZE + 4);
	if (unlikely(tempname == NULL)) goto error_nomem;
	if (unlikely(!W2M(ffd.cFileName, tempname))) goto error_name;
	*dirp = (JC_DIR *)calloc(1, sizeof(JC_DIR) + strlen(tempname) + 1);
	if (unlikely(*dirp == NULL)) goto error_nomem;
	strcpy((*dirp)->dirent.d_name, tempname);
	free(tempname);
 #else
	*dirp = (JC_DIR *)calloc(1, sizeof(JC_DIR) + strlen(ffd.cFileName) + 1);
	if (unlikely(*dirp == NULL)) goto error_nomem;
	strcpy((*dirp)->dirent.d_name, ffd.cFileName);
 #endif
	// TODO: populate JC_DIR stuff
	return 0;

error_name:
	jc_errno = jc_GetLastError();
error_nomem:
#ifdef UNICODE
	if (tempname != NULL) free(tempname);
#endif
	jc_errno = ENOMEM;
	return -1;
}
#endif  /* ON_WINDOWS */
