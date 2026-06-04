#pragma once
#include <stdbool.h>
#include <stddef.h>

#define VIEWFMX_MAX_UPCOMING 5
#define VIEWFMX_TITLE_LEN    128
#define VIEWFMX_NAME_LEN     64
#define VIEWFMX_TIME_LEN     32   /* ISO 8601 */

typedef struct {
    char   title[VIEWFMX_TITLE_LEN];
    bool   is_private;
    char   start_time[VIEWFMX_TIME_LEN];
    char   end_time[VIEWFMX_TIME_LEN];
    int    minutes_remaining;  /* only valid for current_meeting */
} ViewFMX_Meeting;

typedef struct {
    char   room_name[VIEWFMX_NAME_LEN];
    bool   is_busy;
    bool   has_current;
    ViewFMX_Meeting current;
    int    upcoming_count;
    ViewFMX_Meeting upcoming[VIEWFMX_MAX_UPCOMING];
} ViewFMX_RoomData;

/* Return 0 on success, non-zero on error. */
typedef int (*viewfmx_fetch_fn)(void *ctx, ViewFMX_RoomData *out);
typedef int (*viewfmx_book_fn)(void *ctx, int duration_minutes);

typedef struct {
    viewfmx_fetch_fn fetch_status;
    viewfmx_book_fn  book_room;
    void            *ctx;
} ViewFMX_DataProvider;
