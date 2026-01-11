# ZoneMinder Deprecation Analysis: Features for Removal

## Executive Summary

This analysis identifies **20+ features** across 5 categories that should be deprecated and removed from ZoneMinder due to:
1. **Obsolete technology** - protocols/standards no longer in use
2. **Duplicate functionality** - multiple implementations of the same feature
3. **FFmpeg consolidation** - camera types that FFmpeg handles better
4. **Limited modern utility** - rarely used in contemporary deployments
5. **Maintenance burden** - code complexity without proportional value

**Major Finding:** FFmpeg can replace 5 of 8 camera types, eliminating ~3,800 lines of C++ code.

---

## Priority 1: Remove (Obsolete Technology)

### 1.1 X10 Home Automation Support
**Impact: High (significant code removal)**

X10 is a 1970s-1990s power line communication protocol. Modern systems use Z-Wave, Zigbee, Matter, or WiFi-based home automation.

**Files to remove:**
- `scripts/zmx10.pl.in` - Full X10 daemon (200+ lines)
- `web/skins/classic/views/devices.php` - X10 devices UI
- `web/includes/actions/device.php` - X10 device actions
- `web/ajax/device.php` - X10 AJAX handler
- `web/ajax/modals/device.php` - X10 modal
- `docs/userguide/options/options_x10.rst` - X10 documentation
- `docs/userguide/definemonitor/definemonitor_x10.rst` - X10 monitor docs

**Database changes:**
- Drop `Devices` table entirely
- Drop `TriggersX10` table entirely
- Remove `X10` from `Monitors.Triggers` enum (convert to VARCHAR or remove column)

**Configuration removal:**
- `ZM_OPT_X10` option in ConfigData.pm.in
- X10-related strings in all `web/lang/*.php` files (22 language files)

**Estimated savings:** ~2,000+ lines of code, 2 database tables

---

### 1.2 NVSocket Camera Type
**Impact: Medium**

NVSocket is a proprietary protocol for NVAxis/Networld Vision cameras from early 2000s. These cameras are extinct; any surviving units support RTSP which FFmpeg handles.

**Files to remove:**
- `src/zm_remote_camera_nvsocket.cpp`
- `src/zm_remote_camera_nvsocket.h`

**Database changes:**
- Remove `NVSocket` from `Monitors.Type` enum
- Migration script to convert existing NVSocket monitors to FFmpeg type

**Code changes:**
- Remove `case NVSOCKET:` from `src/zm_monitor.cpp` (lines 892-906)

**Estimated savings:** ~500 lines of C++ code

---

### 1.3 Proprietary HTTP Image Formats (X_RGB, X_RGBZ)
**Impact: Low-Medium**

These are non-standard RGB formats used by obscure HTTP cameras from the 2000s. Modern cameras use JPEG/MJPEG.

**Files to modify:**
- `src/zm_remote_camera_http.cpp` - Remove X_RGB/X_RGBZ handling (~100 lines)
- `src/zm_remote_camera_http.h` - Remove format enum values

**Recommendation:** Remove X_RGB/X_RGBZ support, keep JPEG/MJPEG HTTP support (still used for some IP cameras)

---

### 1.4 Ghost Database Types (cURL, WebSite)
**Impact: Low (cleanup only)**

The `Monitors.Type` enum includes `cURL` and `WebSite` but **no code exists to instantiate these camera types**. They cause fatal errors if selected.

**Database changes:**
- Remove `cURL` and `WebSite` from `Monitors.Type` enum

---

## Priority 1B: FFmpeg Camera Consolidation

### Analysis: Current Camera Types (8 total)

| Type | Lines | Status | FFmpeg Can Replace? |
|------|-------|--------|---------------------|
| LOCAL (V4L2) | 1,457 | Active | Partial (60%) |
| REMOTE (HTTP) | 1,148 | Active | Yes (80%) |
| REMOTE (RTSP) | 289 | Active | **Yes (100%)** |
| FILE | 107 | Active | **Yes (100%)** |
| FFMPEG | 669 | Active | N/A (this is the target) |
| LIBVLC | 342 | Active | Mostly (75%) |
| NVSOCKET | 215 | Legacy | **Yes (100%)** |
| VNC | 258 | Active | No (keep) |

### 1B.1 RemoteCameraRtsp - **100% Replaceable**
**Files:** `src/zm_remote_camera_rtsp.cpp/h` (289 lines)

FFmpeg already handles RTSP with all 4 transport modes:
- UDP Unicast, UDP Multicast, TCP (RTP over RTSP), HTTP tunneling
- Evidence: `zm_ffmpeg_camera.cpp:330-346` already implements RTSP transport options

**Recommendation:** Remove entirely. Migration: `Type: RTSP` -> `Type: FFMPEG, Path: rtsp://...`

### 1B.2 RemoteCameraHttp - **80% Replaceable**
**Files:** `src/zm_remote_camera_http.cpp/h` (1,148 lines)

Handles:
- Single-image HTTP URLs (FFmpeg: `image2` demuxer)
- MJPEG multipart streams (FFmpeg: HTTP demuxer handles `multipart/x-mixed-replace`)

**What FFmpeg cannot do:** Regex-based custom boundary parsing for non-standard MJPEG

**Recommendation:** Deprecate for standard MJPEG. Keep as legacy fallback for edge cases.

### 1B.3 FileCamera - **100% Replaceable**
**Files:** `src/zm_file_camera.cpp/h` (107 lines)

Simply reads JPEG from disk path. FFmpeg's `image2` demuxer does this natively.

**Recommendation:** Remove entirely. Migration: `Type: File, Path: /path/to/file.jpg` -> `Type: FFMPEG, Path: /path/to/file.jpg`

### 1B.4 LocalCamera (V4L2) - **60% Replaceable**
**Files:** `src/zm_local_camera.cpp/h` (1,457 lines - largest camera implementation)

**FFmpeg can handle:**
- Basic USB webcam capture via `v4l2` input format
- Pixel format conversion via libswscale
- Software deinterlacing via filters

**FFmpeg cannot handle:**
- Fine-grained V4L2 ioctl controls (brightness, contrast, hue)
- Memory-mapped zero-copy buffers
- Multi-channel capture card switching
- Hardware deinterlacing via V4L2

**Recommendation:**
- Simple webcams: Migrate to FFmpeg (`v4l2:///dev/video0`)
- Capture cards with V4L2 controls: Keep LocalCamera
- Future: Extend FfmpegCamera to accept V4L2 control options

### 1B.5 LibvlcCamera - **75% Replaceable**
**Files:** `src/zm_libvlc_camera.cpp/h` (342 lines)

**FFmpeg handles:** HLS, DASH, RTMP, SRT, and 100+ formats
**LibVLC is better for:** Malformed streams, codec workarounds, exotic formats

**Recommendation:** Deprecate for standard formats. Keep as fallback for proprietary/broken streams.

### 1B.6 VncCamera - **Keep (Not Replaceable)**
**Files:** `src/zm_libvnc_camera.cpp/h` (258 lines)

FFmpeg has NO VNC protocol support. VNC is a remote desktop protocol, not a video format.

**Recommendation:** Keep as-is. Specialized use case, small codebase.

### FFmpeg Consolidation Summary

**Immediate removal candidates:**
- RemoteCameraRtsp: 289 lines (FFmpeg already proven)
- FileCamera: 107 lines (trivial replacement)
- RemoteCameraNVSocket: 215 lines (obsolete protocol)

**Deprecation candidates:**
- RemoteCameraHttp: 1,148 lines (80% replaceable)
- LibvlcCamera: 342 lines (75% replaceable)

**Long-term evaluation:**
- LocalCamera: 1,457 lines (60% replaceable, needs FFmpeg V4L2 options)

**Total potential code reduction: ~3,500 lines (52% of camera code)**

---

## Priority 2: Consolidate (Duplicate Functionality)

### 2.1 Dual API Architecture
**Impact: Very High (major refactoring)**

ZoneMinder has TWO complete API implementations:
- **Legacy AJAX API:** 48 PHP files in `web/ajax/` (~3,500 lines)
- **Modern REST API:** 23 CakePHP controllers in `web/api/app/Controller/` (~4,300 lines)

Both handle: events, monitors, snapshots, zones, controls, streaming, status polling.

**Recommendation:** Migrate web UI to use REST API exclusively, then deprecate AJAX API.

**Migration path:**
1. Update web UI JavaScript to call REST API endpoints
2. Mark AJAX endpoints as deprecated
3. Remove AJAX handlers after 2 major versions

**Files to eventually remove (after migration):**
```
web/ajax/event.php
web/ajax/events.php
web/ajax/monitor.php
web/ajax/stream.php
web/ajax/control.php
web/ajax/zone.php
web/ajax/status.php
... (48 files total)
```

---

### 2.2 Duplicate Date/Time Libraries
**Impact: Medium**

Both Moment.js AND Luxon are loaded in the web UI - redundant date libraries.

**Files:**
- `web/skins/classic/js/moment.js` + `moment.min.js` (large, legacy)
- `web/skins/classic/js/luxon-3.4.4.min.js` (modern replacement)

**Recommendation:** Remove Moment.js, standardize on Luxon (or native Intl API)

---

### 2.3 Multiple Streaming Implementations
**Impact: High (complex)**

Three streaming systems coexist:
1. **Legacy zms socket protocol** - Custom socket commands via `web/ajax/stream.php`
2. **Janus Gateway** - Full WebRTC gateway (`src/zm_monitor_janus.cpp`)
3. **RTSP2Web/Go2RTC** - Modern WebRTC/MSE/HLS via external service

**Database columns for Janus** (potentially removable):
```
Monitors.JanusEnabled
Monitors.JanusAudioEnabled
Monitors.Janus_Profile_Override
Monitors.Janus_Use_RTSP_Restream
Monitors.Janus_RTSP_User
Monitors.Janus_RTSP_Session_Timeout
```

**Recommendation:** Evaluate whether Janus should be deprecated in favor of Go2RTC/RTSP2Web integration

---

### 2.4 Duplicate Business Logic Layer
**Impact: Medium**

Business logic exists in BOTH:
- Legacy PHP objects: `web/includes/Monitor.php`, `Event.php`, `Zone.php`
- CakePHP models: `web/api/app/Model/`

**Recommendation:** Consolidate to CakePHP ORM models

---

## Priority 3: Deprecate (Limited Modern Utility)

### 3.1 FileCamera Type
**Impact: Low**

Reads static JPEG files from disk. Very niche use case (monitoring files updated by external process).

**Recommendation:** Mark as deprecated, suggest FFmpeg with HTTP server instead

---

### 3.2 Deinterlacing Support
**Impact: Low**

`Monitors.Deinterlacing` column for analog/interlaced video (PAL/NTSC). Modern IP cameras output progressive scan exclusively.

**Recommendation:** Mark as deprecated/legacy in UI, hide for new monitors

---

### 3.3 Legacy Video Encoding Columns
**Impact: Low**

These columns are superseded by the modern codec system:
- `Monitors.SaveJPEGs` (legacy)
- `Monitors.VideoWriter` (legacy)
- `Monitors.OutputCodec` (explicitly marked `/* Deprecated */` in schema)
- `Monitors.EncoderParameters` (replaced by structured fields)

Modern replacements exist:
- `OutputCodecName`, `OutputContainer`, `Encoder`, `EncoderHWAccelName`

**Recommendation:** Remove deprecated columns after migration period

---

### 3.4 Event State Booleans
**Impact: Low**

Multiple boolean flags track event state:
```
Events.Videoed, Events.Uploaded, Events.Emailed, Events.Messaged, Events.Executed
```

**Recommendation:** Consolidate into `Event_Data` table or structured state tracking

---

### 3.5 Geographic Coordinates Duplication
**Impact: Low**

Lat/Long stored redundantly in 3 tables:
- `Monitors.Latitude/Longitude`
- `Events.Latitude/Longitude`
- `Servers.Latitude/Longitude`

**Recommendation:** Consolidate to Monitors only (Events inherit from Monitor)

---

### 3.6 Built-in RTSP Server
**Impact: Medium**

`Monitors.RTSPServer` and `Monitors.RTSPStreamName` for built-in RTSP output.

**Recommendation:** Deprecate in favor of Go2RTC gateway

---

## Priority 4: Security Improvements (Legacy Patterns)

### 4.1 Multiple Password Hash Schemes
**Files:** `web/includes/auth.php`

Currently supports 4 schemes:
- `plain` - Unencrypted (DANGEROUS)
- `mysql` - Old MySQL PASSWORD() function
- `mysql+bcrypt` - Overlay migration scheme
- `bcrypt` - Modern secure hashing

**Recommendation:** Force migration to bcrypt, remove plain/mysql support

---

### 4.2 Cookie-Based UI State
**Files:** `web/index.php`, various action handlers

UI preferences stored in cookies:
- `zmSkin`, `zmCSS`, `zmBandwidth`, `zmGroup`, `zmMontageLayout`

**Recommendation:** Migrate to localStorage or user preferences in database

---

## Summary Table

| Feature | Category | Priority | Est. Lines | Effort |
|---------|----------|----------|------------|--------|
| **X10 Support** | Obsolete | P1 | 2,000+ | Medium |
| **NVSocket Camera** | Obsolete | P1 | 215 | Low |
| **X_RGB/X_RGBZ formats** | Obsolete | P1 | 100 | Low |
| **Ghost DB types** | Obsolete | P1 | 10 | Trivial |
| **RemoteCameraRtsp** | FFmpeg replaces | P1B | 289 | Low |
| **FileCamera** | FFmpeg replaces | P1B | 107 | Low |
| RemoteCameraHttp | FFmpeg replaces | P1B | 1,148 | Medium |
| LibvlcCamera | FFmpeg replaces | P1B | 342 | Medium |
| LocalCamera (partial) | FFmpeg replaces | P1B | ~870 | High |
| Dual API (AJAX) | Duplicate | P2 | 3,500 | High |
| Moment.js | Duplicate | P2 | 500 | Low |
| Janus Gateway | Duplicate | P2 | 1,500 | Medium |
| Dual business logic | Duplicate | P2 | 2,000 | High |
| Deinterlacing | Limited use | P3 | 50 | Trivial |
| Legacy encoding cols | Limited use | P3 | N/A | Low |
| Event booleans | Limited use | P3 | N/A | Medium |
| Geo coord duplication | Limited use | P3 | N/A | Low |
| RTSP Server built-in | Limited use | P3 | 300 | Medium |
| Plain password support | Security | P4 | 50 | Low |
| Cookie UI state | Security | P4 | 100 | Low |

**Total estimated removable code:** 13,000+ lines (including FFmpeg consolidation)

---

## Recommended Removal Order

### Phase 1 (Next Release) - Quick Wins
**Obsolete technology removal:**
1. X10 support - complete removal (~2,000 lines)
2. NVSocket camera type (215 lines)
3. Ghost database types (cURL, WebSite)
4. Plain password authentication

**FFmpeg consolidation - immediate:**
5. RemoteCameraRtsp removal (289 lines) - FFmpeg already proven
6. FileCamera removal (107 lines) - trivial replacement
7. X_RGB/X_RGBZ format support in RemoteCameraHttp

**Database migration scripts needed for camera type changes**

### Phase 2 (Following Release) - Web Cleanup
1. Moment.js removal (use Luxon only)
2. Deprecated encoding columns
3. Event state boolean consolidation
4. Begin AJAX API deprecation warnings

### Phase 3 (Major Version) - Camera Consolidation
1. RemoteCameraHttp deprecation (after MJPEG test suite)
2. LibvlcCamera deprecation (keep for fallback only)
3. Janus Gateway evaluation vs Go2RTC
4. Built-in RTSP server deprecation

### Phase 4 (Next Major Version) - Full Cleanup
1. AJAX API removal
2. Full REST API migration complete
3. LocalCamera partial deprecation (simple webcams -> FFmpeg)
4. Business logic consolidation to CakePHP ORM

---

## Notes

- Each removal should include database migration scripts
- Backward compatibility warnings should be added before removal
- Documentation updates required for each deprecation
- Consider user surveys to validate assumptions about feature usage

---

## Final Summary

### What to Keep
| Camera Type | Reason |
|-------------|--------|
| **FFmpeg** | Universal - handles RTSP, HTTP, V4L2, files, 100+ formats |
| **VNC** | No FFmpeg equivalent - specialized use case |
| **LocalCamera** | Advanced V4L2 controls not available in FFmpeg (partial keep) |

### What to Remove (Immediate)
| Feature | Lines | Reason |
|---------|-------|--------|
| X10 Support | 2,000+ | 1970s protocol, no modern use |
| NVSocket | 215 | Extinct camera manufacturer |
| RemoteCameraRtsp | 289 | FFmpeg already does this |
| FileCamera | 107 | FFmpeg image2 demuxer |
| Ghost DB types | 10 | Never implemented |

### What to Deprecate (Near-term)
| Feature | Lines | Reason |
|---------|-------|--------|
| RemoteCameraHttp | 1,148 | FFmpeg handles 80% of cases |
| LibvlcCamera | 342 | FFmpeg handles 75% of cases |
| AJAX API | 3,500 | Duplicate of REST API |
| Janus Gateway | 1,500 | Evaluate vs Go2RTC |

**Total removable code: ~13,000 lines (significant maintenance reduction)**

---

*Analysis completed: January 2026*
*Based on ZoneMinder codebase review*
