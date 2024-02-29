/* libjodycode: datetime string to UNIX epoch conversion
 *
 * Copyright (C) 2020-2024 by Jody Bruchon <jody@jodybruchon.com>
 * Released under The MIT License
 */

#include <string.h>
#include <time.h>
#include "likely_unlikely.h"
#include "libjodycode.h"

#define NTTIME_CONSTANT 116444736000000000ULL;
#define NTTIME_NSEC 10000000ULL;

#define REQ_NUM(a) { if (a < '0' || a > '9') return JC_EDATETIME; }
#define ATONUM(a,b) (a = b - '0')
/* Fast multiplies by 100 (*64 + *32 + *4) and 10 (*8 + *2)
 * for platforms where multiply instructions are expensive */
#ifdef STRTOEPOCH_USE_SHIFT_MULTIPLY
 #define MUL100(a) ((a << 6) + (a << 5) + (a << 2))
 #define MUL10(a) ((a << 3) + a + a)
#else
 #define MUL100(a) (a * 100)
 #define MUL10(a) (a * 10)
#endif /* STRTOEPOCH_USE_SHIFT_MULTIPLY */


/* Accepts date[time] strings "YYYY-MM-DD" or "YYYY-MM-DD HH:MM:SS"
 * and returns the number of seconds since the Unix Epoch a la mktime()
 * or returns -1 on any error */
extern time_t jc_strtoepoch(const char * const datetime)
{
	time_t secs = 0;  /* 1970-01-01 00:00:00 */
	const char * restrict p = datetime;
	int i;
	struct tm tm;

	if (unlikely(datetime == NULL || *datetime == '\0')) return JC_ENULL;
	memset(&tm, 0, sizeof(struct tm));

	/* This code replaces "*10" with shift<<3 + add + add */
	/* Process year */
	tm.tm_year = 1000;
	REQ_NUM(*p); if (*p == '2') tm.tm_year = 2000; p++;
	REQ_NUM(*p); ATONUM(i, *p); tm.tm_year += MUL100(i); p++;
	REQ_NUM(*p); ATONUM(i, *p); tm.tm_year += MUL10(i); p++;
	REQ_NUM(*p); ATONUM(i, *p); tm.tm_year += i; p++;
	tm.tm_year -= 1900;  /* struct tm year is since 1900 */
	if (*p != '-') return JC_EDATETIME;
	p++;
	/* Process month (0-11, not 1-12) */
	REQ_NUM(*p); ATONUM(i, *p); tm.tm_mon = MUL10(i); p++;
	REQ_NUM(*p); ATONUM(i, *p); tm.tm_mon += (i - 1); p++;
	if (*p != '-') return JC_EDATETIME;
	p++;
	/* Process day */
	REQ_NUM(*p); ATONUM(i, *p); tm.tm_mday = MUL10(i); p++;
	REQ_NUM(*p); ATONUM(i, *p); tm.tm_mday += i; p++;
	/* If YYYY-MM-DD is specified only, skip the time part */
	if (*p == '\0') goto skip_time;
	if (*p != ' ') return JC_EDATETIME; else p++;
	/* Process hours */
	REQ_NUM(*p); ATONUM(i, *p); tm.tm_hour = MUL10(i); p++;
	REQ_NUM(*p); ATONUM(i, *p); tm.tm_hour += i; p++;
	if (*p != ':') return JC_EDATETIME;
	p++;
	/* Process minutes */
	REQ_NUM(*p); ATONUM(i, *p); tm.tm_min = MUL10(i); p++;
	REQ_NUM(*p); ATONUM(i, *p); tm.tm_min += i; p++;
	if (*p != ':') return JC_EDATETIME;
	p++;
	/* Process seconds */
	REQ_NUM(*p); ATONUM(i, *p); tm.tm_sec = MUL10(i); p++;
	REQ_NUM(*p); ATONUM(i, *p); tm.tm_sec += i; p++;
	/* Junk after datetime string should cause an error */
	if (*p != '\0') return JC_EDATETIME;
skip_time:
	tm.tm_isdst = -1;  /* Let the host library decide if DST is in effect */
	secs = mktime(&tm);
	return secs;
}


#ifdef ON_WINDOWS
extern int jc_nttime_to_unixtime(uint64_t *nttime, struct JC_TIMESPEC *unixtime)
{
	if (unlikely(nttime == NULL || *nttime <= NTTIME_CONSTANT || unixtime == NULL)) return -1;
	unixtime->tv_sec = (*nttime - NTTIME_CONSTANT) / NTTIME_NSEC;
	unixtime->tv_nsec = ((*nttime - NTTIME_CONSTANT) % NTTIME_NSEC) * 100;
	return 0;
}


extern int jc_unixtime_to_nttime(struct JC_TIMESPEC *unixtime, uint64_t *nttime)
{
	if (unlikely(nttime == NULL || unixtime == NULL)) return -1;
	*nttime = (unixtime->tv_sec * NTTIME_NSEC) + (unixtime->tv_nsec / 100) + NTTIME_CONSTANT;
	if (unlikely(*nttime <= NTTIME_CONSTANT)) return -1;
	return 0;
}
#endif  /* ON_WINDOWS */
