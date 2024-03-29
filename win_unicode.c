/* libjodycode: Windows Unicode support utility code
 *
 * Copyright (C) 2014-2024 by Jody Bruchon <jody@jodybruchon.com>
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
 JC_DIR *dirp_head = NULL;
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
	*wstring = (JC_WCHAR_T *)malloc(JC_PATHBUF_SIZE + 4);
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
		strncpy_s(argv[counter], (size_t)len + 1, temp, (size_t)len + 1);
	}
	return 0;
}
#endif /* UNICODE */


#ifdef ON_WINDOWS
/* Copy WIN32_FIND_FILE data to DIR data for a JC_DIR
 * The first call will allocate a JC_DIR and copy into it
 * Set hFind/ffd to NULL for subsequent calls on the same dirp */
extern int jc_ffd_to_dirent(JC_DIR **dirp, HANDLE hFind, WIN32_FIND_DATA *ffd)
{
#ifdef UNICODE
	char *tempname;
#endif
	size_t len;

	if (unlikely(dirp == NULL)) goto error_null;

	if (hFind == NULL) {
		if (unlikely(*dirp == NULL)) goto error_null;
		ffd = &((*dirp)->ffd);
	}

 #ifdef UNICODE
	/* Must count bytes after conversion to allocate correct size */
	tempname = (char *)malloc(JC_PATHBUF_SIZE + 4);
	if (unlikely(tempname == NULL)) goto error_nomem;
	if (unlikely(!W2M(ffd->cFileName, tempname))) goto error_name;
	len = strlen(tempname) + 1;
	*dirp = (JC_DIR *)calloc(1, sizeof(JC_DIR) + len);
	if (unlikely(*dirp == NULL)) goto error_nomem;
	strcpy_s((*dirp)->dirent.d_name, len, tempname);
	free(tempname);
 #else
	len = strlen(ffd->cFileName) + 1;
	*dirp = (JC_DIR *)calloc(1, sizeof(JC_DIR) + len);
	if (unlikely(*dirp == NULL)) goto error_nomem;
	strcpy_s((*dirp)->dirent.d_name, len, ffd->cFileName);
 #endif
	/* (*dirp)->dirent.ino = 0; // implicit via calloc() - Windows Find*File() doesn't return inode numbers */
	(*dirp)->dirent.d_namlen = (uint32_t)len;
	if (unlikely(ffd->dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) (*dirp)->dirent.d_type = JC_DT_LNK;
	else if (ffd->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) (*dirp)->dirent.d_type = JC_DT_DIR;
	else (*dirp)->dirent.d_type = JC_DT_REG;

	/* First call: init ffd/hFind + mark cached FindFirstFile() dirent */
	if (hFind != NULL) {
		memcpy(&((*dirp)->ffd), ffd, sizeof(WIN32_FIND_DATA));
		(*dirp)->hFind = hFind;
		(*dirp)->cached = 1;
	}

	return 0;

#ifdef UNICODE
error_name:
	jc_errno = jc_GetLastError();
#endif
error_nomem:
#ifdef UNICODE
	if (tempname != NULL) free(tempname);
#endif
	jc_errno = ENOMEM;
	return -1;
error_null:
	jc_errno = EFAULT;
	return -1;
}
#endif  /* ON_WINDOWS */
