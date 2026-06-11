/* libc shims for ESP-IDF's newlib. */

#include <stdint.h>
#include <time.h>

/* newlib does not provide timegm(); room_screen.c needs it to convert
 * ISO 8601 UTC timestamps. Days-from-civil algorithm, no TZ involved. */
time_t timegm(struct tm *tm)
{
    int64_t y = tm->tm_year + 1900;
    unsigned m = tm->tm_mon + 1;
    unsigned d = tm->tm_mday;

    y -= m <= 2;
    int64_t  era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (153u * (m + (m > 2 ? -3 : 9)) + 2u) / 5u + d - 1u;
    unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
    int64_t days = era * 146097 + (int64_t)doe - 719468;

    return (time_t)(days * 86400
                    + tm->tm_hour * 3600
                    + tm->tm_min * 60
                    + tm->tm_sec);
}
