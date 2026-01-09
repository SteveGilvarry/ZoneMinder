# Safari Montage Debug - Quick Reference

## Quick Start

### 1. Enable Debug Mode
```
http://your-zm-server/zm/index.php?view=montage&debug=safari
```

### 2. Open Browser Console
- **Chrome:** F12 or Cmd+Option+I
- **Safari:** Cmd+Option+I (enable Developer menu first in Safari Preferences)

### 3. Test & Download Logs
1. Load the page
2. Scroll up and down
3. Wait 30 seconds
4. Click "Download Logs" button in debug panel
5. Repeat in other browser
6. Compare logs

## Console Commands Cheat Sheet

### Check Debug Status
```javascript
// Is debug loaded?
typeof SafariDebug !== 'undefined'

// Browser info
SafariDebug.BrowserInfo

// Current config
SafariDebug.BrowserInfo.features
```

### Get Logs
```javascript
// Download logs
SafariDebug.downloadLogs()

// View logs in console
SafariDebug.LogSystem.logs

// Get stored logs from localStorage
SafariDebug.getStoredLogs()

// Copy logs to clipboard
copy(JSON.stringify(SafariDebug.LogSystem.logs, null, 2))
```

### Compare Logs
```javascript
// Method 1: Load from files
fetch('/path/to/safari-logs.json').then(r => r.text()).then(s => window.safari = s)
fetch('/path/to/chrome-logs.json').then(r => r.text()).then(c => window.chrome = c)
SafariDebug.compareLogs(window.safari, window.chrome)

// Method 2: From localStorage (if you have both)
const safariLogs = /* paste Safari log JSON here */
const chromeLogs = /* paste Chrome log JSON here */
SafariDebug.compareLogs(JSON.stringify(safariLogs), JSON.stringify(chromeLogs))
```

### Monitor Status
```javascript
// List all monitors
monitors

// Check specific monitor status
monitors[0].started        // Is it started?
monitors[0].activePlayer   // Which player? (go2rtc, zms, etc.)
monitors[0].id             // Monitor ID

// Get monitor element
monitors[0].getElement()

// Check if monitor is visible
isOutOfViewport(monitors[0].getElement())

// Force start/stop
monitors[0].start()
monitors[0].stop()
```

### Scroll State
```javascript
// Check scroll support
'onscrollend' in window

// Check viewport
window.scrollY
window.innerHeight

// Check visible monitors
monitors.filter(m => !isOutOfViewport(m.getElement()).all).map(m => m.id)
```

### Manual Testing
```javascript
// Trigger scroll handler manually
on_scroll()

// Check if init is complete
monitorInitComplete

// Check changed monitors queue
changedMonitors

// Force all monitors to start
startMonitors()
```

## What to Look For in Logs

### 🔴 Critical Issues

**No scrollend support:**
```
[WARNING] scrollend event not supported - using fallback
```
→ Check if `on_scroll()` is being called after scrolling stops

**Video not starting:**
```
[VIDEO] Monitor X starting
// No follow-up "start completed" message
```
→ Video initialization is hanging

**Long delays:**
```
[VIDEO] Monitor X start completed {duration: "2000ms"}  // >500ms is concerning
```
→ Something is blocking video initialization

**Monitors stopping immediately:**
```
[VIDEO] Monitor X starting
[VIDEO] Monitor X stopping  // <100ms later
```
→ Viewport detection might be broken

### 🟡 Warning Signs

**Slow scroll end detection:**
```
[SCROLL] Scroll event (last one)
// 200ms+ gap
[SCROLL] Scroll ended
```
→ Fallback timeout might be too long

**Viewport miscalculation:**
```
[VIEWPORT] Monitor visibility after scroll {visible: [], hidden: [1,2,3]}
// But monitors 1,2,3 are clearly on screen
```
→ `isOutOfViewport()` is broken

**Timer drift:**
```
[TIMER] setTimeout fired {delay: 100, actualDelay: 250}
```
→ Safari might be throttling timers

## Common Patterns

### ✅ Healthy Chrome Log Pattern
```
[BROWSER] Safari Montage Debug initialized
[SCROLL] Scroll tracker initialized {hasScrollEnd: true}
[VIDEO] Video tracker initialized
[TIMER] Timer tracking initialized
[VIEWPORT] isOutOfViewport function wrapped
[VIDEO] Monitor 1 starting
[VIDEO] Monitor 1 start completed {duration: "45ms"}
[VIDEO] Monitor 2 starting
[VIDEO] Monitor 2 start completed {duration: "52ms"}
[SCROLL] Scroll started
[SCROLL] Native scrollend event fired
[SCROLL] Scroll ended
[VIEWPORT] Monitor visibility after scroll {visible: [1,2], hidden: [3,4]}
```

### ❌ Problematic Safari Log Pattern
```
[BROWSER] Safari Montage Debug initialized
[SCROLL] Scroll tracker initialized {hasScrollEnd: false}  ← No scrollend
[VIDEO] Video tracker initialized
[TIMER] Timer tracking initialized
[VIEWPORT] isOutOfViewport function wrapped
[VIDEO] Monitor 1 starting
// No "start completed" message  ← Video didn't load
[SCROLL] Scroll started
// No scroll end event  ← Scroll handler not firing
```

## Quick Fixes to Try

### If scrollend is missing:
Edit `/home/user/ZoneMinder/web/skins/classic/views/js/montage.js` line 729-742

**Reduce timeout from 100ms to 50ms:**
```javascript
document.onscroll = () => {
  clearTimeout(window.scrollEndTimer);
  window.scrollEndTimer = setTimeout(on_scroll, 50);  // Changed from 100
};
```

### If videos load slowly:
Check if ResizeObserver is firing:
```javascript
// In console
observerMontage
```

Try manually triggering:
```javascript
setTriggerChangedMonitors()
```

### If viewport detection is wrong:
Check header height calculation (montage.js line 779):
```javascript
const elem = document.getElementById('liveStream1');
const bounding = elem.getBoundingClientRect();
const headerHeight = document.getElementById('navbar-container').offsetHeight +
                     document.getElementById('header').offsetHeight;
console.log('Bounding:', bounding, 'Header:', headerHeight);
```

## Keyboard Shortcuts in Debug Mode

None currently - but you can add them by modifying `safari-montage-debug.js`:

```javascript
// Add to init() function:
document.addEventListener('keydown', (e) => {
  if (e.ctrlKey && e.key === 'd') {
    SafariDebug.downloadLogs();
  }
});
```

## One-Liner Tests

```javascript
// Test 1: Are monitors loaded?
console.log('Monitors:', monitors.length, monitors.map(m => m.id))

// Test 2: Are any started?
console.log('Started:', monitors.filter(m => m.started).map(m => m.id))

// Test 3: Which are visible?
console.log('Visible:', monitors.filter(m => !isOutOfViewport(m.getElement()).all).map(m => m.id))

// Test 4: Scroll support?
console.log('scrollend:', 'onscrollend' in window, 'ResizeObserver:', typeof ResizeObserver !== 'undefined')

// Test 5: Debug loaded?
console.log('Debug:', typeof SafariDebug !== 'undefined', SafariDebug.BrowserInfo.isSafari ? 'Safari' : 'Chrome')
```

## Files to Check

| File | Purpose | Line # to Check |
|------|---------|-----------------|
| `montage.js` | Scroll handlers | 729-742 |
| `montage.js` | on_scroll() function | 759-774 |
| `montage.js` | isOutOfViewport() | 776-797 |
| `montage.js` | ResizeObserver | 1030-1047 |
| `MonitorStream.js` | start() function | 344-556 |
| `MonitorStream.js` | stop() function | 558-615 |
| `functions.php` | Debug injection | 1597-1601 |

## Log Categories Color Guide

When viewing console:
- **Blue** = SCROLL events
- **Red** = VIDEO events
- **Orange** = TIMER events
- **Purple** = EVENT tracking
- **Teal** = VIEWPORT checks
- **Dark gray** = BROWSER info
- **Green** = PERFORMANCE
- **Dark red** = ERROR
- **Yellow** = WARNING

## Disable Debug

```javascript
// Remove cookie
document.cookie = "SAFARI_DEBUG=; expires=Thu, 01 Jan 1970 00:00:00 UTC; path=/;";

// Then reload without ?debug=safari in URL
location.href = '/zm/index.php?view=montage';
```

## Emergency Reset

If debug mode breaks the page:

1. Open console immediately
2. Run:
   ```javascript
   localStorage.clear();
   document.cookie = "SAFARI_DEBUG=; expires=Thu, 01 Jan 1970 00:00:00 UTC; path=/;";
   ```
3. Navigate without debug parameter:
   ```
   location.href = '/zm/index.php?view=montage';
   ```

Or edit `functions.php` and comment out debug injection:
```php
if ( $basename == 'montage' ) {
  // Debug mode disabled
  /*
  if (isset($_GET['debug']) && $_GET['debug'] == 'safari' || isset($_COOKIE['SAFARI_DEBUG'])) {
    echo '<script src="'.cache_bust('js/safari-montage-debug.js').'"></script>'.PHP_EOL;
  }
  */
}
```

## Report Template

When reporting findings:

```
## Browser Test Results

**Browser:** Safari 17.2 / Chrome 120
**OS:** macOS Sonoma / Windows 11
**Monitors:** 4 cameras
**Resolution:** 1920x1080

### Symptoms
- Videos don't load when scrolling in Safari
- Works fine in Chrome

### Log Comparison
- Events only in Safari: [list]
- Events only in Chrome: [list]

### Key Findings
1. scrollend event: NOT SUPPORTED in Safari
2. Video loading time: Safari 2000ms vs Chrome 50ms
3. Viewport detection: Working correctly in both

### Screenshots
[Attach debug panel screenshot]

### Log Files
- safari-logs.json (attached)
- chrome-logs.json (attached)
```
