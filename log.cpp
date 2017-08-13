// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "error.h"

static const char *logfile = NULL;

void setlogfile(const char *const file)
{
	logfile = file;
}

void log(const char *const what, ...)
{
	if (!logfile)
		return;

	struct timeval tv;
	gettimeofday(&tv, NULL);

	time_t tv_temp = tv.tv_sec;
	struct tm tm;
	localtime_r(&tv_temp, &tm);

	char *msg = NULL;
	va_list ap;
	va_start(ap, what);
	(void)vasprintf(&msg, what, ap);
	va_end(ap);

	char *temp = NULL;
	asprintf(&temp, "%04d-%02d-%02d %02d:%02d:%02d.%06ld %s", 
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec, tv.tv_usec,
			msg);
	free(msg);

	FILE *fh = fopen(logfile, "a+");
	if (!fh)
		error_exit(true, "Cannot access %s", logfile);

	fprintf(fh, "%s\n", temp);
	fclose(fh);

	printf("%s\n", temp);
	free(temp);
}
