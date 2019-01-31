#include <inc/stdio.h>
#include <inc/assert.h>
#include <inc/types.h>

#ifndef JOS_INC_TIME_H
#define JOS_INC_TIME_H

struct tm
{
    int tm_sec;                   /* Seconds.     [0-60] */
    int tm_min;                   /* Minutes.     [0-59] */
    int tm_hour;                  /* Hours.       [0-23] */
    int tm_mday;                  /* Day.         [1-31] */
    int tm_mon;                   /* Month.       [0-11] */
    int tm_year;                  /* Year - 1900.  */
};

int d_to_s(int d)
{
    return d * 24 * 60 * 60;
}


/* Converts Gregorian date to seconds since 1970-01-01 00:00:00.
 * Assumes input in normal date format, i.e. 1980-12-31 23:59:59
 * => year=1980, mon=12, day=31, hour=23, min=59, sec=59.
 */
int32_t unixtime(uint32_t year0, uint32_t mon0,
		uint32_t day, uint32_t hour,
		uint32_t min, uint32_t sec)
{
	uint32_t mon = mon0, year = year0;

	/* 1..12 -> 11,12,1..10 */
	if (0 >= (int32_t) (mon -= 2)) {
		mon += 12;	/* Puts Feb last since it has leap day */
		year -= 1;
	}

	return ((((uint32_t)
					(year/4 - year/100 + year/400 + 367*mon/12 + day) +
					year*365 - 719499
		 )*24 + hour /* now have hours */
		)*60 + min /* now have minutes */
	       )*60 + sec; /* finally seconds */
}

/*
 * Convert Gregorian date to seconds since 01-01-1970 00:00:00.
 * 
 * The incoming struct tm must have month from 0 to 11 instead of 1-12
 * The incoming struct tm must have days from 1 to 31 instead of 0-30
 * The year is from 1900. So (tm->year = 119) means 2019.
 *
 * That is to say it must adhere to the "struct tm" agreement at the top of this header.
 */
int32_t timestamp_artem(struct tm *tm)
{
	return (int32_t) unixtime(tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
			tm->tm_hour, tm->tm_min, tm->tm_sec);
}

#define LEAPS_THRU_END_OF(y) ((y)/4 - (y)/100 + (y)/400)

bool is_leap_year(unsigned int year)
{
	return (!(year % 4) && (year % 100)) || !(year % 400);
}

/*
 * The number of days in the month.
 */
int32_t rtc_month_days(uint32_t month, uint32_t year)
{
	// hack, because this is a header. Ugh.
	const unsigned char rtc_days_in_month[] = {
		31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
	};

	return rtc_days_in_month[month] + (is_leap_year(year) && month == 1);
}

/*
 * The number of days since January 1. (0 to 365)
 */
int32_t rtc_year_days(uint32_t day, uint32_t month, uint32_t year)
{
	// hack, because this is a header. Ugh.
	const unsigned short rtc_ydays[2][13] = {
		/* Normal years */
		{ 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 },
		/* Leap years */
		{ 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366 }
	};

	return rtc_ydays[is_leap_year(year)][month] + day-1;
}

/*
 * Convert seconds since 01-01-1970 00:00:00 to Gregorian date.
 */
void mktime_artem(uint32_t time, struct tm *tm)
{
	uint32_t month, year;
	int32_t days;

	days = time / 86400;
	time -= (uint32_t) days * 86400;

	year = 1970 + days / 365;
	days -= (year - 1970) * 365
		+ LEAPS_THRU_END_OF(year - 1)
		- LEAPS_THRU_END_OF(1970 - 1);
	if (days < 0) {
		year -= 1;
		days += 365 + is_leap_year(year);
	}
	tm->tm_year = year - 1900;

	for (month = 0; month < 11; month++) {
		int32_t newdays;

		newdays = days - rtc_month_days(month, year);
		if (newdays < 0)
			break;
		days = newdays;
	}
	tm->tm_mon = month;
	tm->tm_mday = days + 1;

	tm->tm_hour = time / 3600;
	time -= tm->tm_hour * 3600;
	tm->tm_min = time / 60;
	tm->tm_sec = time - tm->tm_min * 60;
}

// What is this behaviour?
//
// (gdb) p time
// $8 = {tm_sec = 0, tm_min = 0, tm_hour = 0, tm_mday = 0, tm_mon = 0, tm_year = 0}
// (gdb) call timestamp(&time)
// $9 = 946684800
// (gdb) 
//
// And this?
// (gdb) p time
// $11 = {tm_sec = 0, tm_min = 0, tm_hour = 0, tm_mday = 0, tm_mon = 0, tm_year = 70}
// (gdb) call timestamp(&time)
// $12 = -1139207296
int timestamp(struct tm *time)
{
	// changed to call custom function
	return timestamp_artem(time);
}

// Why the hell does this make days starting from zero?
// 1549040504 == 1st of February 2019 17:1:44
// but
// mktime(1549040504) = {tm_sec = 44, tm_min = 1, tm_hour = 17, tm_mday = 0, tm_mon = 1, tm_year = 119} 
// Note: 
// 	tm_day = 0
// You are breaking "struct tm" agreement. Not cool.
void mktime(int time, struct tm *tm)
{
	// changed to call custom function
	mktime_artem(time, tm);
}

void print_datetime(struct tm *tm)
{
    cprintf("%04d-%02d-%02d %02d:%02d:%02d\n",
        tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
        tm->tm_hour, tm->tm_min, tm->tm_sec);
}

void snprint_datetime(char *buf, int size, struct tm *tm)
{
    assert(size >= 10 + 1 + 8 + 1);
    snprintf(buf, size,
          "%04d-%02d-%02d %02d:%02d:%02d",
          tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
          tm->tm_hour, tm->tm_min, tm->tm_sec);
}

#endif
