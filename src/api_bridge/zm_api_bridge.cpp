//
// zm_api_bridge.cpp ZoneMinder API Bridge
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

#include "zm_api_bridge.h"

#include "zm_monitor_list.h"
#include "zm_monitor.h"
#include "zm_stats.h"
#include "zm_logger.h"
#include "zm_event.h"
#include "zm_config.h"

/* STL / helpers */
#include <vector>
#include <algorithm>
#include <mutex>
#include <cstring>
#include <cstdlib>

/* ─────────────────────────  internal helpers  ───────────────────────── */

#define MON_OR_RET(id, ret)                                            \
    auto mon = MonitorList::GetInstance().GetMonitor(id);              \
    if (!mon) { ZM_WARN("zmbridge: monitor %u not found", id); return ret; }

struct Subscriber { void* fn; void* ud; };

static std::vector<Subscriber> g_eventSubs;
static std::vector<Subscriber> g_logSubs;
static std::mutex              g_subMx;

/* Central helper so recurring code is tiny */
static void publish(std::vector<Subscriber>& vec,
                    auto invoker)
{
    std::lock_guard<std::mutex> lk(g_subMx);
    for (auto &s : vec)
        invoker(s);
}

/* ─────────────────────────  section 0 – memory ────── */
void* zm_alloc(size_t n) { return std::malloc(n); }
void  zm_free (void *p)  { std::free(p); }

/* ─────────────────────────  section 1 – monitor────── */
int zm_monitor_add(const char *name,
                   const char *src,
                   const char *func,
                   uint32_t   *out_id)
{
    int id = MonitorList::GetInstance()
                 .CreateMonitor(name, src, func);
    if (out_id) *out_id = id;
    return id < 0 ? -1 : 0;
}
int zm_monitor_delete(uint32_t id)
{ return MonitorList::GetInstance().DeleteMonitor(id) ? 0 : -1; }

int zm_monitor_start(uint32_t id)
{ MON_OR_RET(id, -1); return mon->Start() ? 0 : -1; }

int zm_monitor_stop(uint32_t id)
{ MON_OR_RET(id, -1); return mon->Stop() ? 0 : -1; }

int zm_monitor_reload(uint32_t id)
{ MON_OR_RET(id, -1); return mon->Reload() ? 0 : -1; }

int zm_monitor_set_func(uint32_t id, const char *f)
{ MON_OR_RET(id, -1); return mon->SetFunction(f) ? 0 : -1; }

int zm_monitor_set_enabled(uint32_t id, int en)
{ MON_OR_RET(id, -1); return mon->SetEnabled(en) ? 0 : -1; }

/* ─────────────────────────  section 2 – alarm ────── */
int zm_force_alarm(uint32_t id, uint32_t dur)
{ MON_OR_RET(id, -1); mon->ForceAlarm(dur); return 0; }

int zm_clear_alarm(uint32_t id)
{ MON_OR_RET(id, -1); mon->ClearAlarm(); return 0; }

/* ─────────────────────────  section 3 – stats ────── */
int zm_get_monitor_stats(uint32_t id, ZmMonitorStats *o)
{
    MON_OR_RET(id, -1);
    const auto &s = mon->GetCaptureStats();
    o->fps          = s.fps;
    o->drops        = s.dropped;
    o->shm_fill_pct = s.shmFill;
    o->alarmed      = (mon->GetState() == Monitor::State::Alarm);
    return 0;
}

void zm_get_core_stats(ZmCoreStats *o)
{
    o->disk_pct      = zm::stats::DiskPercent();
    o->shm_pct       = zm::stats::ShmPercent();
    o->db_ms         = zm::stats::DbLatencyMs();
    o->cpu_load_x10  = zm::stats::CpuLoadx10();
}

/* async event bus */
void zm_subscribe_events(ZmEventCb cb, void *ud)
{
    std::lock_guard<std::mutex> lk(g_subMx);
    g_eventSubs.push_back({(void*)cb, ud});
}
void zm_unsubscribe_events(ZmEventCb cb, void *ud)
{
    std::lock_guard<std::mutex> lk(g_subMx);
    g_eventSubs.erase(std::remove_if(g_eventSubs.begin(),
                                     g_eventSubs.end(),
        [&](Subscriber&s){ return s.fn==(void*)cb && s.ud==ud; }),
        g_eventSubs.end());
}

/*  ‼ HOOK: call this one-liner from Monitor.cpp when you already create
        the JSON string describing state change or new event          */
void zm_emit_event_json(const std::string &j)
{
    publish(g_eventSubs, [&](Subscriber &s)
    { ((ZmEventCb)s.fn)(j.c_str(), s.ud); });
}

/* ─────────────────────── section 4 – snapshots ────── */
uint8_t* zm_get_jpeg_snapshot(uint32_t id, size_t *len)
{
    MON_OR_RET(id, nullptr);
    auto img = mon->CurrentJpeg();
    *len = img.size;
    uint8_t *mem = (uint8_t*)std::malloc(img.size);
    std::memcpy(mem, img.data, img.size);
    return mem;
}

uint8_t* zm_get_raw_frame_rgb(uint32_t id,size_t*w,size_t*h)
{
    MON_OR_RET(id, nullptr);
    auto frm = mon->CurrentRawRGB(*w,*h);
    if (!frm.data) return nullptr;
    uint8_t *mem=(uint8_t*)std::malloc(frm.size);
    std::memcpy(mem, frm.data, frm.size);
    return mem;
}

/* ─────────────────────────  section 5 – PTZ ────── */
int zm_ptz_move(uint32_t id,int pan,int tilt,int zoom)
{ MON_OR_RET(id, -1); return mon->PanTiltZoom(pan,tilt,zoom)?0:-1; }

int zm_ptz_focus(uint32_t id,int f)
{ MON_OR_RET(id, -1); return mon->Focus(f)?0:-1; }

int zm_ptz_preset(uint32_t id,uint32_t p)
{ MON_OR_RET(id, -1); return mon->Preset(p)?0:-1; }

/* ─────────────────────────  section 6 – options ── */
int zm_get_option(const char *k, char *b, size_t cap)
{
    std::string v = Config::Value(k);
    if (v.size() + 1 > cap) return -1;
    std::strcpy(b, v.c_str());
    return (int)v.size() + 1;
}
int zm_set_option(const char *k, const char *v)
{ Config::Set(k, v); return 0; }

void zm_reload_options(void) { Config::Reload(); }

/* ───────────────────────  section 7 – events DB ── */
int zm_event_delete(uint32_t eid)
{ return Event::Delete(eid) ? 0 : -1; }

int zm_event_move(uint32_t eid, uint32_t sid)
{ return Event::Move(eid, sid) ? 0 : -1; }

/* ───────────────────────  section 8 – logs ─────── */
void zm_subscribe_logs(ZmLogCb cb, void *ud)
{
    std::lock_guard<std::mutex> lk(g_subMx);
    g_logSubs.push_back({(void*)cb, ud});
}
void zm_unsubscribe_logs(ZmLogCb cb, void *ud)
{
    std::lock_guard<std::mutex> lk(g_subMx);
    g_logSubs.erase(std::remove_if(g_logSubs.begin(),g_logSubs.end(),
        [&](Subscriber&s){ return s.fn==(void*)cb && s.ud==ud; }),
        g_logSubs.end());
}

/*  ‼ HOOK: add one line in zm_logger.cpp::Log::Send()  */
void zm_emit_log(uint32_t lvl, const char *msg)
{
    publish(g_logSubs, [&](Subscriber&s){
        ((ZmLogCb)s.fn)(lvl, msg, s.ud);
    });
}

/* simple pull: you can implement Log::Fetch quickly in zm_logger.cpp */
size_t zm_log_query(uint64_t from_id, uint32_t min_lvl,
                    ZmLogEntry*out, size_t cap)
{
    return Log::Fetch(from_id, min_lvl, out, cap);
}
