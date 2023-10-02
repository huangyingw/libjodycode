/*
 * Windows-native code for getting stat()-like information
 *
 * Copyright (C) 2016-2023 by Jody Bruchon <jody@jodybruchon.com>
 * Released under The MIT License
 */


#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include "likely_unlikely.h"
#include "libjodycode.h"

#ifdef ON_WINDOWS
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <windows.h>
#endif


/* Convert NT epoch to UNIX epoch */
extern time_t jc_nttime_to_unixtime(const uint64_t * const restrict timestamp)
{
	uint64_t newstamp;

	memcpy(&newstamp, timestamp, sizeof(uint64_t));
	newstamp /= 10000000LL;
	if (unlikely(newstamp <= 11644473600LL)) return 0;
	newstamp -= 11644473600LL;
	return (time_t)newstamp;
}


/* Convert UNIX epoch to NT epoch */
extern time_t jc_unixtime_to_nttime(const uint64_t * const restrict timestamp)
{
	uint64_t newstamp;

	memcpy(&newstamp, timestamp, sizeof(uint64_t));
	newstamp += 11644473600LL;
	newstamp *= 10000000LL;
	if (unlikely(newstamp <= 11644473600LL)) return 0;
	return (time_t)newstamp;
}


/* Get stat()-like extra information for a file on Windows */
extern int jc_stat(const char *filename, struct JC_STAT *buf)
{
	int retval;
#ifdef ON_WINDOWS
	HANDLE hFile = INVALID_HANDLE_VALUE;
	BY_HANDLE_FILE_INFORMATION bhfi;
	uint64_t timetemp;
#endif

	if (unlikely(!buf)) return JC_ENULL;

#ifdef ON_WINDOWS
 #ifdef UNICODE
	JC_WCHAR_T *widename;

	if (jc_string_to_wstring(filename, &widename) != 0) return JC_EALLOC;
	hFile = CreateFileW(widename, 0, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
	free(widename);
 #else
	hFile = CreateFileA(filename, 0, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
 #endif
	if (unlikely(hFile == INVALID_HANDLE_VALUE)) goto win_failure;
	if (unlikely(GetFileInformationByHandle(hFile, &bhfi) == 0)) goto win_failure;

	buf->st_ino = ((uint64_t)(bhfi.nFileIndexHigh) << 32) + (uint64_t)bhfi.nFileIndexLow;
	buf->st_size = ((int64_t)(bhfi.nFileSizeHigh) << 32) + (int64_t)bhfi.nFileSizeLow;
	timetemp = ((uint64_t)(bhfi.ftCreationTime.dwHighDateTime) << 32) + bhfi.ftCreationTime.dwLowDateTime;
	buf->st_ctime = jc_nttime_to_unixtime(&timetemp);
	timetemp = ((uint64_t)(bhfi.ftLastWriteTime.dwHighDateTime) << 32) + bhfi.ftLastWriteTime.dwLowDateTime;
	buf->st_mtime = jc_nttime_to_unixtime(&timetemp);
	timetemp = ((uint64_t)(bhfi.ftLastAccessTime.dwHighDateTime) << 32) + bhfi.ftLastAccessTime.dwLowDateTime;
	buf->st_atime = jc_nttime_to_unixtime(&timetemp);
	buf->st_dev = (uint32_t)bhfi.dwVolumeSerialNumber;
	buf->st_nlink = (uint32_t)bhfi.nNumberOfLinks;
	buf->st_mode = (uint32_t)bhfi.dwFileAttributes;

	CloseHandle(hFile);
	retval = 0;
#else
	retval = stat(filename, buf);
	if (retval != 0) jc_errno = errno;
#endif
	return retval;

#ifdef ON_WINDOWS
win_failure:
	jc_errno = jc_GetLastError();
	if (hFile != INVALID_HANDLE_VALUE) CloseHandle(hFile);
	return -1;
#endif
}
