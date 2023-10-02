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
static int out_mode = _O_TEXT;
static int err_mode = _O_TEXT;
#endif

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
/* Set output modes to TEXT, BINARY, or UTF-16 */
extern void jc_set_output_modes(unsigned int modes)
{
	/* Mode is selected by setting a bit flag:
	 * 0x01 = 1: set stdout mode to UTF-16
	 * 0x02 = 1: set stderr mode to UTF-16
	 * 0x04 = 1: set stdout mode to UTF-16 only if it is a terminal
	 * 0x08 = 1: set stderr mode to UTF-16 only if it is a terminal
	 * 0x10 = 1: set flagged outputs to text mode instead
	 * If not setting UTF-16, all modes default to BINARY without 0x10 set
	 * */
	if (modes & 0x10U) {
		out_mode = (modes & 0x01U) ? _O_TEXT : out_mode;
		err_mode = (modes & 0x02U) ? _O_TEXT : err_mode;
		return;
	}
	if (modes & 0x04U) {
		/* Only use UTF-16 for terminal output, else use UTF-8 */
		if (!_isatty(_fileno(stdout))) out_mode = _O_BINARY;
		else out_mode = _O_U16TEXT;
	} else if (modes & 0x08U) {
		if (!_isatty(_fileno(stderr))) err_mode = _O_BINARY;
		else err_mode = _O_U16TEXT;
	} else {
		out_mode = (modes & 0x01U) ? _O_U16TEXT : _O_BINARY;
		err_mode = (modes & 0x02U) ? _O_U16TEXT : _O_BINARY;
	}
	return;
}


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


/* Print a string that is wide on Windows but normal on POSIX */
extern int jc_fwprint(FILE * const restrict stream, const char * const restrict str, const int cr)
{
#ifdef UNICODE
	int retval;
	int stream_mode;
	JC_WCHAR_T *wstr;

	stream_mode = (stream == stderr) ? err_mode : out_mode;

	if (stream_mode == _O_U16TEXT) {
		/* Convert to wide string and send to wide console output */
		if(jc_string_to_wstring(str, &wstr) != 0) return JC_EALLOC;
		fflush(stream);
		_setmode(_fileno(stream), stream_mode);
		if (cr == 2) retval = fwprintf(stream, L"%S%C", wstr, 0);
		else retval = fwprintf(stream, L"%S%S", wstr, cr == 1 ? L"\n" : L"");
		fflush(stream);
		_setmode(_fileno(stream), _O_TEXT);
		free(wstr);
		return retval;
	} else {
#endif
		if (cr == 2) return fprintf(stream, "%s%c", str, 0);
		else return fprintf(stream, "%s%s", str, cr == 1 ? "\n" : "");
#ifdef UNICODE
	}
#endif
}


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
