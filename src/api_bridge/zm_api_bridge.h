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

/* ─────────────────────────  memory helpers  ───────────────────── */
void *zm_alloc(size_t bytes);
void  zm_free (void *ptr);

/* ─────────────────────────  monitor control  ─────────────────── */
int zm_monitor_add(const char *name,
                   const char *source_url,
                   const char *function,
                   uint32_t   *out_id /*nullable*/);
int zm_monitor_delete     (uint32_t id);
int zm_monitor_start      (uint32_t id);
int zm_monitor_stop       (uint32_t id);
int zm_monitor_reload     (uint32_t id);
int zm_monitor_set_func   (uint32_t id, const char *function);
int zm_monitor_set_enabled(uint32_t id, int enabled /*0/1*/);

/* ─────────────────────────  alarm  ───────────────────────────── */
int zm_force_alarm(uint32_t id, uint32_t duration_ms /*0 = default*/);
int zm_clear_alarm(uint32_t id);

/* ─────────────────────────  stats / events  ──────────────────── */
struct ZmMonitorStats { uint32_t fps, drops, shm_fill_pct, alarmed; };
struct ZmCoreStats    { uint32_t disk_pct, shm_pct, db_ms, cpu_load_x10; };

int  zm_get_monitor_stats(uint32_t id, struct ZmMonitorStats *out);
void zm_get_core_stats   (struct ZmCoreStats *out);

typedef void (*ZmEventCb)(const char *json_utf8, void *userdata);
void zm_subscribe_events  (ZmEventCb cb, void *userdata);
void zm_unsubscribe_events(ZmEventCb cb, void *userdata);

/* ─────────────────────────  frames / snapshots  ──────────────── */
uint8_t *zm_get_jpeg_snapshot(uint32_t id, size_t *len);
uint8_t *zm_get_raw_frame_rgb(uint32_t id,
                              size_t *width, size_t *height);
/* caller releases with zm_free() */

/* ─────────────────────────  PTZ  ─────────────────────────────── */
int zm_ptz_move  (uint32_t id, int pan, int tilt, int zoom);
int zm_ptz_focus (uint32_t id, int focus);
int zm_ptz_preset(uint32_t id, uint32_t preset_id);

/* ─────────────────────────  global options  ─────────────────── */
int  zm_get_option(const char *key, char *buf, size_t buf_cap);
int  zm_set_option(const char *key, const char *val);
void zm_reload_options(void);

/* ─────────────────────────  event DB  ───────────────────────── */
int zm_event_delete(uint32_t event_id);
int zm_event_move  (uint32_t event_id, uint32_t new_storage_id);

/* ─────────────────────────  logs  ───────────────────────────── */
struct ZmLogEntry {
    uint64_t    id;
    uint32_t    level;
    uint64_t    ts_epoch_ms;
    const char *msg;   /* internal pointer – do not free */
};
size_t zm_log_query(uint64_t from_id,
                    uint32_t min_level,
                    struct ZmLogEntry *out,
                    size_t   cap);

typedef void (*ZmLogCb)(uint32_t level, const char *msg, void *userdata);
void zm_subscribe_logs  (ZmLogCb, void *userdata);
void zm_unsubscribe_logs(ZmLogCb, void *userdata);

/* ─────────────────────────────────────────────────────────────── */
#ifdef __cplusplus
}
#endif
#endif /* ZM_API_BRIDGE_H */
