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
#include "zm_monitor.h"      // Monitor class
#include "zm_logger.h"

#include <vector>
#include <mutex>
#include <algorithm>
#include <cstring>
#include <cstdlib>

/* helpers */
static std::shared_ptr<Monitor> find_mon(uint32_t id)
{
    return Monitor::Map().count(id) ? Monitor::Map()[id] : nullptr;
}
#define CHECK_MON(id, ret) \
    auto mon = find_mon(id); \
    if(!mon){ Warning("zmbridge: monitor %u not found", id); return ret; }

void *zm_alloc(size_t n){ return std::malloc(n); }
void  zm_free(void *p)  { std::free(p); }

/* ─── alarm ───────────────────────────────────────────── */
int zm_force_alarm(uint32_t id,uint32_t dur){
    CHECK_MON(id,-1); mon->ForceAlarmOn(dur); return 0;
}
int zm_clear_alarm(uint32_t id){
    CHECK_MON(id,-1); mon->ForceAlarmOff();   return 0;
}

/* ─── snapshot ────────────────────────────────────────── */
uint8_t *zm_get_jpeg_snapshot(uint32_t id,size_t *len){
    CHECK_MON(id,nullptr);
    Image img = mon->GetCurrentJpeg();
    *len = img.size;
    uint8_t *mem=(uint8_t*)std::malloc(img.size);
    std::memcpy(mem,img.data,img.size);
    return mem;
}

/* ─── simple state query ─────────────────────────────── */
int zm_is_alarm(uint32_t id){
    auto mon = find_mon(id); if(!mon) return -1;
    return mon->GetState()==Monitor::ALARM;
}

/* ─── event + log buses ──────────────────────────────── */
struct Sub { void*fn; void*ud; };
static std::vector<Sub> ev,lx;
static std::mutex mx;

void zm_subscribe_events(ZmEventCb cb,void*ud){ std::lock_guard l(mx); ev.push_back({(void*)cb,ud}); }
void zm_unsubscribe_events(ZmEventCb cb,void*ud){ std::lock_guard l(mx); ev.erase(std::remove_if(ev.begin(),ev.end(),[&](Sub&s){return s.fn==(void*)cb&&s.ud==ud;}),ev.end()); }
void zm_emit_event_json(const std::string&j){
    std::lock_guard l(mx); for(auto&s:ev) ((ZmEventCb)s.fn)(j.c_str(),s.ud);
}

void zm_subscribe_logs(ZmLogCb cb,void*ud){ std::lock_guard l(mx); lx.push_back({(void*)cb,ud}); }
void zm_unsubscribe_logs(ZmLogCb cb,void*ud){ std::lock_guard l(mx); lx.erase(std::remove_if(lx.begin(),lx.end(),[&](Sub&s){return s.fn==(void*)cb&&s.ud==ud;}),lx.end()); }
void zm_emit_log(int lvl,const char*msg){
    std::lock_guard l(mx); for(auto&s:lx) ((ZmLogCb)s.fn)(lvl,msg,s.ud);
}
