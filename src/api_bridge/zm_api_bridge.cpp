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

#include "zm_monitor.h"      // Monitor, Monitor::Find()
#include "zm_event.h"
#include "zm_config.h"
#include "zm_logger.h"

#include <vector>
#include <mutex>
#include <algorithm>
#include <cstring>
#include <cstdlib>

/* ─────────── helpers ─────────── */

static std::shared_ptr<Monitor> find_mon(uint32_t id)
{
    return Monitor::Find(id);           // thread-safe helper
}
#define MON_OR_RET(id, ret)                                   \
    auto mon = find_mon(id);                                  \
    if (!mon) { Warn("zmbridge: monitor %u not found", id); return ret; }

/* subscriber lists */
struct Sub { void *fn; void *ud; };
static std::vector<Sub> g_evt, g_log;
static std::mutex       g_mx;

static void publish(std::vector<Sub>& vec, auto inv)
{
    std::lock_guard<std::mutex> l(g_mx);
    for (auto &s: vec) inv(s);
}

/* ─────────── 0. memory ───────── */
void* zm_alloc(size_t n) { return std::malloc(n); }
void  zm_free (void *p ) { std::free(p); }

/* ─────────── 1. monitor ──────── */
int zm_monitor_add(const char *name,const char *url,
                   const char *fn,uint32_t *out_id)
{
    int id = Monitor::Create(name,url,fn);
    if (out_id) *out_id = id;
    return id<0 ? -1 : 0;
}
int zm_monitor_delete(uint32_t id){ return Monitor::Delete(id)?0:-1; }
int zm_monitor_start (uint32_t id){ MON_OR_RET(id,-1); return mon->Start()?0:-1; }
int zm_monitor_stop  (uint32_t id){ MON_OR_RET(id,-1); return mon->Stop()?0:-1; }
int zm_monitor_reload(uint32_t id){ MON_OR_RET(id,-1); return mon->Reload()?0:-1; }
int zm_monitor_set_func(uint32_t id,const char*f){
    MON_OR_RET(id,-1); return mon->SetFunction(f)?0:-1;
}
int zm_monitor_set_enabled(uint32_t id,int en){
    MON_OR_RET(id,-1); return mon->SetEnabled(en)?0:-1;
}

/* ─────────── 2. alarm ───────── */
int zm_force_alarm(uint32_t id,uint32_t d){ MON_OR_RET(id,-1); mon->ForceAlarm(d); return 0;}
int zm_clear_alarm(uint32_t id)           { MON_OR_RET(id,-1); mon->ClearAlarm();  return 0;}

/* ─────────── 3. stats / events ─ */
int zm_get_monitor_stats(uint32_t id, ZmMonitorStats *o)
{
    MON_OR_RET(id,-1);
    const auto&s=mon->GetCaptureStats();
    o->fps=s.fps; o->drops=s.dropped; o->shm_fill_pct=s.shmFill;
    o->alarmed = (mon->GetState()==Monitor::State::Alarm);
    return 0;
}
void zm_get_core_stats(ZmCoreStats *o)
{   /* real numbers need extra work; keep zeros for now */ memset(o,0,sizeof(*o)); }

void zm_subscribe_events(ZmEventCb cb,void*ud){
    std::lock_guard<std::mutex> l(g_mx); g_evt.push_back({(void*)cb,ud});
}
void zm_unsubscribe_events(ZmEventCb cb,void*ud){
    std::lock_guard<std::mutex> l(g_mx);
    g_evt.erase(std::remove_if(g_evt.begin(),g_evt.end(),
      [&](Sub&s){return s.fn==(void*)cb && s.ud==ud;}), g_evt.end());
}
/*  ‼ insert this call inside zm_monitor.cpp when JSON event is ready */
void zm_emit_event_json(const std::string &j){
    publish(g_evt,[&](Sub&s){ ((ZmEventCb)s.fn)(j.c_str(),s.ud); });
}

/* ─────────── 4. snapshots ───── */
uint8_t* zm_get_jpeg_snapshot(uint32_t id,size_t*len){
    MON_OR_RET(id,nullptr);
    auto jpg=mon->CurrentJpeg(); *len=jpg.size;
    auto*mem=(uint8_t*)std::malloc(jpg.size);
    std::memcpy(mem,jpg.data,jpg.size); return mem;
}
uint8_t* zm_get_raw_frame_rgb(uint32_t id,size_t*w,size_t*h){
    MON_OR_RET(id,nullptr);
    auto f=mon->CurrentRawRGB(*w,*h);
    if(!f.data) return nullptr;
    auto*mem=(uint8_t*)std::malloc(f.size);
    std::memcpy(mem,f.data,f.size); return mem;
}

/* ─────────── 5. PTZ ─────────── */
int zm_ptz_move(uint32_t id,int p,int t,int z){
    MON_OR_RET(id,-1); return mon->PanTiltZoom(p,t,z)?0:-1;
}
int zm_ptz_focus(uint32_t id,int f){
    MON_OR_RET(id,-1); return mon->Focus(f)?0:-1;
}
int zm_ptz_preset(uint32_t id,uint32_t pid){
    MON_OR_RET(id,-1); return mon->Preset(pid)?0:-1;
}

/* ─────────── 6. options ─────── */
int zm_get_option(const char*k,char*b,size_t cap){
    std::string v=Config::Value(k);
    if(v.size()+1>cap) return -1;
    std::strcpy(b,v.c_str()); return (int)v.size()+1;
}
int zm_set_option(const char*k,const char*v){ Config::Set(k,v); return 0; }
void zm_reload_options(void){ Config::Reload(); }

/* ─────────── 7. event DB ────── */
int zm_event_delete(uint32_t eid){ return Event::Delete(eid)?0:-1; }
int zm_event_move  (uint32_t eid,uint32_t sid){ return Event::Move(eid,sid)?0:-1; }

/* ─────────── 8. logs ────────── */
void zm_subscribe_logs(ZmLogCb cb,void*ud){
    std::lock_guard<std::mutex> l(g_mx); g_log.push_back({(void*)cb,ud});
}
void zm_unsubscribe_logs(ZmLogCb cb,void*ud){
    std::lock_guard<std::mutex> l(g_mx);
    g_log.erase(std::remove_if(g_log.begin(),g_log.end(),
      [&](Sub&s){return s.fn==(void*)cb && s.ud==ud;}), g_log.end());
}
/*  ‼ add one line in zm_logger.cpp::Log::Send()  */
void zm_emit_log(uint32_t lvl,const char*msg){
    publish(g_log,[&](Sub&s){ ((ZmLogCb)s.fn)(lvl,msg,s.ud); });
}
size_t zm_log_query(uint64_t from,uint32_t lvl,
                    ZmLogEntry*out,size_t cap)
{ /* simple stub; real impl needs a small query helper */
    return 0;
}
