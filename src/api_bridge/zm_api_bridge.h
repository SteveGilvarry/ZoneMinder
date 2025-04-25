//
// zm_api_bridge.h ZoneMinder API Bridge
// Copyright (C) 2025 ZoneMinder Inc
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
//

#ifndef ZM_API_BRIDGE_H
#define ZM_API_BRIDGE_H
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

    void *zm_alloc(size_t); void zm_free(void*);

    /* alarms */
    int zm_force_alarm(uint32_t mon_id, uint32_t dur_ms); /* 0 = use default */
    int zm_clear_alarm(uint32_t mon_id);

    /* snapshot */
    uint8_t *zm_get_jpeg_snapshot(uint32_t mon_id, size_t *len);

    /* state query (only alarm flag) */
    int zm_is_alarm(uint32_t mon_id);   /* returns 0/1 or –1 if monitor missing */

    /* async push (alarm start/stop) */
    typedef void (*ZmEventCb)(const char *json_utf8, void *userdata);
    void zm_subscribe_events(ZmEventCb, void*);
    void zm_unsubscribe_events(ZmEventCb, void*);

    /* async log push (stringified) */
    typedef void (*ZmLogCb)(int level, const char *, void*);
    void zm_subscribe_logs(ZmLogCb, void*);
    void zm_unsubscribe_logs(ZmLogCb, void*);

#ifdef __cplusplus
}
#endif
#endif
