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
    return Monitor::Load(id, 0, Monitor::QUERY);
}
#define MON_OR_RET(id, ret) \
    auto mon = find_mon(id); \
    if(!mon){ Warning("zmbridge: monitor %u not found", id); return ret; }

/* ─────────── 1. monitor ──────── */
int zm_monitor_start(uint32_t id){ 
    MON_OR_RET(id,-1);
    
    // Store previous state
    bool was_enabled = mon->Enabled();
    
    // Perform action (returns void)
    mon->actionEnable();
    
    // Allow time for state change to take effect
    sleep(1);
    
    // Re-load the monitor to get updated state
    auto updated_mon = find_mon(id);
    if (!updated_mon) {
        Error("Failed to reload monitor %u after enabling", id);
        return -1;
    }
    
    // Check if the state changed as expected
    return updated_mon->Enabled() ? 0 : -1; 
}

int zm_monitor_stop(uint32_t id){ 
    MON_OR_RET(id,-1);
    
    // Store previous state
    bool was_enabled = mon->Enabled();
    
    // Perform action (returns void)
    mon->actionDisable();
    
    // Allow time for state change to take effect
    sleep(1);
    
    // Re-load the monitor to get updated state
    auto updated_mon = find_mon(id);
    if (!updated_mon) {
        Error("Failed to reload monitor %u after disabling", id);
        return -1;
    }
    
    // Check if the state changed as expected
    return !updated_mon->Enabled() ? 0 : -1;
}

int zm_monitor_reload(uint32_t id){ 
    MON_OR_RET(id,-1);
    
    // Perform action (returns void)
    mon->actionReload();
    
    // Allow time for reload to take effect
    sleep(1);
    
    // Re-load the monitor to verify it's still available
    auto updated_mon = find_mon(id);
    if (!updated_mon) {
        Error("Failed to reload monitor %u after reload action", id);
        return -1;
    }
    
    return 0; // If we can still load the monitor, consider it a success
}

/* ─────────── 2. alarm ───────── */
int zm_force_alarm(uint32_t id, uint32_t d) {
    MON_OR_RET(id,-1);
    mon->ForceAlarmOn(d, "API Bridge", nullptr);
    return 0;
}

int zm_clear_alarm(uint32_t id) {
    MON_OR_RET(id,-1);
    mon->ForceAlarmOff();
    return 0;
}

/* ─── snapshot ────────────────────────────────────────── */
uint8_t *zm_get_jpeg_snapshot(uint32_t id, size_t *len) {
    MON_OR_RET(id, nullptr);
    if (!mon->connect()) {
        Error("Can't connect to capture daemon: %d %s", mon->Id(), mon->Name());
        return nullptr;
    }
    
    ZMPacket *packet = mon->getSnapshot(-1);  // -1 means get the latest snapshot
    if (!packet) {
        Error("Failed to get snapshot from monitor %d", id);
        *len = 0;
        return nullptr;
    }
    
    Image *img = packet->image;
    if (!img || !img->Size()) {
        Error("Invalid image in snapshot from monitor %d", id);
        delete packet;
        *len = 0;
        return nullptr;
    }
    
    *len = img->Size();
    uint8_t *mem = (uint8_t*)std::malloc(*len);
    if (!mem) {
        Error("Failed to allocate memory for snapshot from monitor %d", id);
        delete packet;
        *len = 0;
        return nullptr;
    }
    
    std::memcpy(mem, img->Buffer(), *len);
    delete packet;  // Clean up the packet after we're done
    
    return mem;
}

/* ─── simple state query ─────────────────────────────── */
int zm_is_alarm(uint32_t id){
    MON_OR_RET(id, -1);
    return (mon->GetState() == Monitor::ALARM) ? 1 : 0;
}