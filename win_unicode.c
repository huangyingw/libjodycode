/* Jody Bruchon's Windows Unicode helper routines
 *
 * Copyright (C) 2014-2023 by Jody Bruchon <jody@jodybruchon.com>
 * Released under The MIT License
 */

#include <errno.h>
#include <fcntl.h>
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

/* Convert slashes to backslashes in a file path */
extern void jc_slash_convert(char *path)
{
	while (*path != '\0') {
		if (*path == '/') *path = '\\';
		path++;
	}
	return;
}


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


/* Copy Windows wide character arguments to UTF-8 */
extern int jc_widearg_to_argv(int argc, JC_WCHAR_T **wargv, char **argv)
{
	static char temp[PATHBUF_SIZE * 2];
	int len;

	if (unlikely(!argv)) return -6;
	for (int counter = 0; counter < argc; counter++) {
		len = W2M(wargv[counter], &temp);
		if (unlikely(len < 1)) return -7;

		argv[counter] = (char *)malloc((size_t)len + 1);
		if (unlikely(!argv[counter])) jc_oom("widearg_to_argv()");
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
	int stream_mode = out_mode;
	JC_WCHAR_T *wstr;

	if (stream == stderr) stream_mode = err_mode;

	if (stream_mode == _O_U16TEXT) {
		/* Convert to wide string and send to wide console output */
		if(jc_string_to_wstring(str, &wstr) != 0) return -7;
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


/* Open a file, converting the name for Unicode on Windows if necessary */
extern FILE *jc_fopen(const char *pathname, const JC_WCHAR_T *mode)
{
#ifdef UNICODE
	FILE *fp;
	JC_WCHAR_T *widename;
#endif

	if (unlikely(pathname == NULL || mode == NULL)) {
		errno = EFAULT;
		return NULL;
	}

#ifdef UNICODE
	if (jc_string_to_wstring(pathname, &widename) != 0) {
		errno = ENOMEM;
		return NULL;
	}
	fp = _wfopen(widename, mode);
	free(widename);
	return fp;
#else
	return fopen(pathname, mode);
#endif
}


/* Check file exist/read/write, converting for Windows if necessary */
extern int jc_access(const char *pathname, int mode)
{
#ifdef UNICODE
	int retval;
	JC_WCHAR_T *widename;
#endif

	if (unlikely(pathname == NULL)) {
		errno = EFAULT;
		return -1;
	}

#ifdef UNICODE
	if (jc_string_to_wstring(pathname, &widename) != 0) {
		errno = ENOMEM;
		return -1;
	}
	retval = _waccess(widename, mode);
	free(widename);
	return retval;
#else
	return access(pathname, mode);
#endif
}


/* Rename a file, converting for Windows if necessary */
extern int jc_rename(const char * const restrict oldpath, const char * restrict newpath)
{
#ifdef UNICODE
	int retval;
	JC_WCHAR_T *wideold, *widenew;
#endif

	if (unlikely(oldpath == NULL || newpath == NULL)) {
		errno = EFAULT;
		return -1;
	}

#ifdef UNICODE
	if (unlikely(jc_string_to_wstring(oldpath, &wideold) != 0 || jc_string_to_wstring(newpath, &widenew) != 0)) {
		errno = ENOMEM;
		return -1;
	}
	retval = MoveFileW(wideold, widenew) ? 0 : -1;
	free(wideold); free(widenew);
	return retval;
#else
	return rename(oldpath, newpath);
#endif
}


/* Delete a file, converting for Windows if necessary */
extern int jc_remove(const char *pathname)
{
#ifdef UNICODE
	int retval;
	JC_WCHAR_T *widename;
#endif

	if (unlikely(pathname == NULL)) {
		errno = EFAULT;
		return -1;
	}

#ifdef UNICODE
	if (jc_string_to_wstring(pathname, &widename) != 0) {
		errno = ENOMEM;
		return -1;
	}
	retval = DeleteFileW(widename) ? 0 : 1;
	free(widename);
	return retval;
#else
	return remove(pathname);
#endif
}


/* Hard link a file, converting for Windows if necessary */
extern int jc_link(const char *path1, const char *path2)
{
#ifdef ON_WINDOWS
	int retval = -1;
#ifdef UNICODE
	JC_WCHAR_T *widename1, *widename2;
#endif
#endif

	if (unlikely(path1 == NULL || path2 == NULL)) {
		errno = EFAULT;
		return -1;
	}

#ifdef ON_WINDOWS
 #ifdef UNICODE
	if (jc_string_to_wstring(path1, &widename1) != 0 || jc_string_to_wstring(path2, &widename2) != 0) {
		errno = ENOMEM;
		return -1;
	}
	if (CreateHardLinkW((LPCWSTR)widename2, (LPCWSTR)widename1, NULL) == TRUE) retval = 0;
	free(widename1); free(widename2);
 #else
	if (CreateHardLink(path2, path1, NULL) == TRUE) retval = 0;
 #endif  /* UNICODE */
	return retval;
#else
	return link(path1, path2);
#endif  /* ON_WINDOWS */
}


#ifdef UNICODE
/* Copy a string to a wide string - wstring must be freed by the caller */
extern int jc_string_to_wstring(const char * const restrict string, JC_WCHAR_T **wstring)
{
	if (unlikely(wstring == NULL)) return -1;
	*wstring = (JC_WCHAR_T *)malloc(PATH_MAX + 4);
	if (unlikely(*wstring == NULL)) return -2;
	if (unlikely(!M2W(string, *wstring))) {
		free(*wstring);
		return -3;
	}
	return 0;
}
#endif
