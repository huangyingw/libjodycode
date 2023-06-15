/* Cross-platform alarms
 * This file is part of libjodycode; see README for license information */

#ifdef ON_WINDOWS
 #define _WIN32_WINNT 0x0500
 #define WIN32_LEAN_AND_MEAN
 #include <windows.h>
#endif

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "likely_unlikely.h"

int jc_alarm_ring = 0;

#ifdef ON_WINDOWS
static HANDLE hTimer;
#else
static int jc_alarm_repeat = 0;
#endif /* ON_WINDOWS */


#ifdef ON_WINDOWS
void CALLBACK jc_catch_alarm(PVOID arg1, BOOLEAN arg2)
{
	(void)arg1; (void)arg2;
	jc_alarm_ring = 1;
	return 0;
}


extern int jc_start_alarm(const unsigned int seconds, const int repeat)
{
	int secs = seconds * 1000;
	int period;

	if (repeat != 0) period = seconds;
	if (!CreateTimerQueueTimer(&hTimer, NULL, (WAITORTIMERCALLBACK)jc_catch_alarm, 0, secs, period, 0))
		return -8;
	jc_alarm_ring = 1;
	return 0;
}


extern int jc_stop_alarm(void)
{
	if (CloseHandle(hTimer) == 0) return -8;
	return 0;
}

#else /* not ON_WINDOWS */
void jc_catch_alarm(const int signum)
{
	(void)signum;

	jc_alarm_ring = 1;
	if (jc_alarm_repeat != 0) alarm(1);
	return;
}


extern int jc_start_alarm(const unsigned int seconds, const int repeat)
{
	struct sigaction sa_run;

	sa_run.sa_handler = jc_catch_alarm;
	memset(&sa_run, 0, sizeof(struct sigaction));
	if (repeat != 0) jc_alarm_repeat = 1;
	if (sigaction(SIGALRM, &sa_run, NULL) != 0) return -8;
	alarm(seconds);
	return 0;
}


extern int jc_stop_alarm(void)
{
	struct sigaction sa_stop;

	memset(&sa_stop, 0, sizeof(struct sigaction));
	sa_stop.sa_handler = SIG_DFL;
	jc_alarm_repeat = 0;
	if (sigaction(SIGALRM, &sa_stop, NULL) != 0) return -8;
	alarm(0);
	return 0;
}
#endif /* ON_WINDOWS */
