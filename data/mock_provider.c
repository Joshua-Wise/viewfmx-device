#include "mock_provider.h"
#include <string.h>

static int mock_fetch(void *ctx, ViewFMX_RoomData *out)
{
    (void)ctx;
    memset(out, 0, sizeof(*out));

    strncpy(out->room_name, "Lakeview A", sizeof(out->room_name) - 1);
    out->is_busy = true;
    out->has_current = true;

    strncpy(out->current.title,      "Q3 Planning",            sizeof(out->current.title) - 1);
    strncpy(out->current.start_time, "2026-06-03T19:00:00Z",   sizeof(out->current.start_time) - 1);
    strncpy(out->current.end_time,   "2026-06-03T20:00:00Z",   sizeof(out->current.end_time) - 1);
    out->current.is_private         = false;
    out->current.minutes_remaining  = 22;

    out->upcoming_count = 2;

    strncpy(out->upcoming[0].title,      "1:1 - Josh",            sizeof(out->upcoming[0].title) - 1);
    strncpy(out->upcoming[0].start_time, "2026-06-03T20:30:00Z", sizeof(out->upcoming[0].start_time) - 1);
    strncpy(out->upcoming[0].end_time,   "2026-06-03T21:00:00Z", sizeof(out->upcoming[0].end_time) - 1);
    out->upcoming[0].is_private = false;

    strncpy(out->upcoming[1].title,      "Staff Meeting",         sizeof(out->upcoming[1].title) - 1);
    strncpy(out->upcoming[1].start_time, "2026-06-04T14:00:00Z", sizeof(out->upcoming[1].start_time) - 1);
    strncpy(out->upcoming[1].end_time,   "2026-06-04T15:00:00Z", sizeof(out->upcoming[1].end_time) - 1);
    out->upcoming[1].is_private = false;

    return 0;
}

static int mock_book(void *ctx, int duration_minutes)
{
    (void)ctx;
    (void)duration_minutes;
    /* Mock always succeeds */
    return 0;
}

void mock_provider_init(ViewFMX_DataProvider *provider)
{
    provider->fetch_status = mock_fetch;
    provider->book_room    = mock_book;
    provider->ctx          = NULL;
}
