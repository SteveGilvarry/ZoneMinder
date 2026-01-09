# Safari Montage Video Loading Debug Instructions

This guide will help you debug the Safari-specific issue where video players are slow to load and images disappear when scrolling in the montage view.

## Issue Description

**Symptoms:**
- Video players are slow to load in Safari montage view
- Some images load initially but disappear when scrolling
- Network and server-side checks show no issues
- Problem is likely related to JavaScript timers or events not triggering properly in Safari (works fine in Chrome)

## Setup

### 1. Ensure Debug Script is Available

The debug script is located at:
```
/home/user/ZoneMinder/web/js/safari-montage-debug.js
```

The injection code has been added to:
```
/home/user/ZoneMinder/web/skins/classic/includes/functions.php
```

### 2. Enable Debug Mode

There are two ways to enable debug mode:

**Option A: URL Parameter (Recommended for testing)**
```
http://your-zm-server/zm/index.php?view=montage&debug=safari
```

**Option B: Set Cookie (Persistent across sessions)**
In browser console:
```javascript
document.cookie = "SAFARI_DEBUG=1; path=/";
```

Then navigate to:
```
http://your-zm-server/zm/index.php?view=montage
```

## Testing Procedure

### Phase 1: Collect Baseline Data from Chrome

1. **Open Chrome** and navigate to montage view with debug enabled:
   ```
   http://your-zm-server/zm/index.php?view=montage&debug=safari
   ```

2. **Open Chrome DevTools** (F12 or Cmd+Option+I)
   - Go to the Console tab
   - You should see colored debug logs starting with timestamps

3. **Verify Debug Panel Appears**
   - Look for a dark debug panel in the top-right corner of the screen
   - It should show real-time debug information

4. **Perform Test Scenario:**
   a. **Initial Load:**
      - Note which monitors load immediately
      - Check console for "VIDEO" category logs
      - Look for "Monitor X starting" and "Monitor X start completed" messages

   b. **Scroll Test:**
      - Scroll down slowly
      - Watch the debug panel for "Scroll Status"
      - Note which monitors start/stop as you scroll
      - Check for "VIEWPORT" logs showing monitor visibility changes

   c. **Timing Test:**
      - Let the page sit idle for 30 seconds
      - Observe if all visible monitors are streaming
      - Check for any "TIMER" warnings

5. **Download Chrome Logs:**
   - Click the "Download Logs" button in the debug panel (top-right)
   - Save the file as `zm-debug-chrome-[timestamp].json`

### Phase 2: Collect Safari Data

1. **Open Safari** and navigate to montage view with debug enabled:
   ```
   http://your-zm-server/zm/index.php?view=montage&debug=safari
   ```

2. **Open Safari Web Inspector** (Cmd+Option+I)
   - Go to the Console tab
   - Enable "Show console" if not visible

3. **Verify Debug Panel Appears**
   - The debug panel should appear in the top-right corner
   - Note if browser is correctly detected as "Safari"

4. **Perform Same Test Scenario as Chrome:**
   - Follow the exact same steps as Phase 1
   - Take note of any differences in behavior

5. **Download Safari Logs:**
   - Click the "Download Logs" button in the debug panel
   - Save the file as `zm-debug-safari-[timestamp].json`

### Phase 3: Compare Logs

1. **Load Both Log Files:**
   - Open browser console (either Chrome or Safari)
   - Load the Safari log JSON:
     ```javascript
     fetch('/path/to/zm-debug-safari-[timestamp].json')
       .then(r => r.text())
       .then(safariLogs => {
         window.safariLogs = safariLogs;
       });
     ```
   - Load the Chrome log JSON:
     ```javascript
     fetch('/path/to/zm-debug-chrome-[timestamp].json')
       .then(r => r.text())
       .then(chromeLogs => {
         window.chromeLogs = chromeLogs;
       });
     ```

2. **Run Comparison:**
   ```javascript
   window.SafariDebug.compareLogs(window.safariLogs, window.chromeLogs);
   ```

3. **Analyze Output:**
   - Look at "Events only in Safari" - events that happen in Safari but not Chrome
   - Look at "Events only in Chrome" - events that happen in Chrome but not Safari
   - Pay special attention to:
     - SCROLL events (especially scrollend)
     - VIDEO start/stop events
     - TIMER events
     - VIEWPORT visibility checks

## What to Look For

### Key Areas to Investigate:

#### 1. Scroll Event Detection
**Chrome logs should show:**
```
[SCROLL] Scroll started
[SCROLL] Scroll event
[SCROLL] Native scrollend event fired  <-- This might be missing in Safari
[SCROLL] Scroll ended
[VIEWPORT] Monitor visibility after scroll
```

**Safari might show:**
```
[SCROLL] Scroll started
[SCROLL] Scroll event
[WARNING] scrollend event not supported - using fallback  <-- Safari-specific
[SCROLL] Scroll ended (from timeout fallback)
[VIEWPORT] Monitor visibility after scroll
```

**❗ Problem Indicators:**
- Safari doesn't trigger scroll end events at all
- Scroll end events happen much later in Safari
- VIEWPORT checks don't happen after scrolling in Safari

#### 2. Video Loading Timing
**Look for timing differences:**
```
[VIDEO] Monitor 1 starting
[VIDEO] Monitor 1 start completed {duration: "XXms"}  <-- Compare XXms between browsers
```

**❗ Problem Indicators:**
- Safari takes significantly longer to start monitors (>500ms difference)
- Monitors start in Safari but never report completion
- "Monitor X stopping" happens immediately after "starting" in Safari

#### 3. Timer Behavior
**Check for timer issues:**
```
[TIMER] setTimeout fired {delay: 100, actualDelay: XXX}
```

**❗ Problem Indicators:**
- Actual delay is much longer than requested delay in Safari
- Timers don't fire at all in Safari
- setInterval frequency is much lower in Safari

#### 4. ResizeObserver
**Look for resize events:**
```
[PERFORMANCE] resizeInterval triggered
```

**❗ Problem Indicators:**
- ResizeObserver doesn't fire in Safari
- ResizeObserver fires too frequently in Safari
- Monitor elements aren't being resized properly

### 5. Viewport Calculations
**Check viewport detection:**
```
[VIEWPORT] isOutOfViewport check {elementId: "liveStream1", result: {...}}
```

**❗ Problem Indicators:**
- Elements are incorrectly detected as out of viewport in Safari
- getBoundingClientRect returns unexpected values in Safari
- Header height calculation is wrong in Safari

## Debug Panel Indicators

The visual debug panel shows real-time status:

```
┌─────────────────────────────┐
│ SAFARI DEBUG PANEL          │
├─────────────────────────────┤
│ Visible Monitors: 1, 2, 3   │
│ Scroll Count: 5             │
│ Scroll Status: IDLE         │
│ Monitor 1: STARTED (zms)    │
│ Monitor 2: STOPPED          │
├─────────────────────────────┤
│ [Download Logs Button]      │
└─────────────────────────────┘
```

**Watch for:**
- Monitors stuck in "STARTING" state
- Monitors showing "STOPPED" when they should be visible
- Scroll Status stuck in "SCROLLING"
- Visible Monitors list not updating when scrolling

## Advanced Debugging

### Manual API Calls

You can manually inspect state using the debug API:

```javascript
// Check browser detection
SafariDebug.BrowserInfo;

// Get current logs
SafariDebug.LogSystem.logs;

// Force a log entry
SafariDebug.LogSystem.log('TEST', 'Manual test message', {foo: 'bar'});

// Get stored logs from localStorage
SafariDebug.getStoredLogs();

// Download logs immediately
SafariDebug.downloadLogs();
```

### Monitor Specific Events

To track a specific monitor:

```javascript
// In console, monitor #1
const monitor = monitors[0];
console.log('Monitor 1 state:', {
  id: monitor.id,
  started: monitor.started,
  player: monitor.player,
  activePlayer: monitor.activePlayer,
  element: monitor.getElement()
});

// Force start a monitor
monitors[0].start();

// Check if element is in viewport
const element = document.getElementById('liveStream1');
const viewport = isOutOfViewport(element);
console.log('Monitor 1 viewport:', viewport);
```

### Check Scroll Event Listeners

```javascript
// See all scroll event listeners (Chrome)
getEventListeners(document);
getEventListeners(document.getElementById('content'));

// Check if scrollend is supported
console.log('scrollend supported:', 'onscrollend' in window);
```

## Common Issues & Solutions

### Issue 1: Debug Panel Not Appearing

**Symptoms:** No debug panel visible in top-right corner

**Solutions:**
1. Check console for JavaScript errors
2. Verify debug mode is enabled (check URL has `?debug=safari` or cookie is set)
3. Check if script is loaded:
   ```javascript
   console.log('SafariDebug loaded:', typeof SafariDebug !== 'undefined');
   ```
4. Clear cache and reload (Shift+F5 or Cmd+Shift+R)

### Issue 2: No Logs Appearing

**Symptoms:** Debug panel shows but no log entries

**Solutions:**
1. Check `DEBUG_CONFIG.ENABLE_LOGGING` is true:
   ```javascript
   // Edit safari-montage-debug.js line ~13
   ENABLE_LOGGING: true
   ```
2. Check console is showing all log levels (not filtered)
3. Verify monitors are loading:
   ```javascript
   console.log('Monitors:', monitors);
   ```

### Issue 3: Can't Download Logs

**Symptoms:** Download button doesn't work

**Solutions:**
1. Check browser allows downloads
2. Manually copy from localStorage:
   ```javascript
   console.log(localStorage.getItem('zm_safari_debug_logs'));
   ```
3. Copy from console:
   ```javascript
   copy(JSON.stringify(SafariDebug.LogSystem.logs, null, 2));
   ```

## Expected Findings

Based on the issue description, you should likely find:

### Hypothesis 1: scrollend Event Not Supported
- Safari doesn't support native `scrollend` event
- Fallback timeout might not be working correctly
- Monitor start/stop not triggered after scroll

### Hypothesis 2: Timer Delays in Safari
- setTimeout/setInterval fire less frequently in Safari
- ResizeObserver might be throttled
- `waitingMonitorsPlaced` polling might not work correctly

### Hypothesis 3: Video Element Creation Issues
- Video elements might not be created properly in Safari
- WebRTC/HLS initialization might fail silently
- IMG fallback might not load properly

### Hypothesis 4: Viewport Detection Issues
- `getBoundingClientRect()` might return different values in Safari
- Sticky header height calculation might be wrong
- Monitors incorrectly detected as "out of viewport"

## Next Steps After Testing

Once you've identified the differences:

1. **Share the comparison results:**
   - Post the output of `compareLogs()`
   - Highlight which category of events differs most
   - Note any error messages unique to Safari

2. **Focus on the problematic area:**
   - If scroll events: Fix scroll end detection
   - If video events: Fix video player initialization
   - If timer events: Adjust timer intervals/delays
   - If viewport: Fix viewport calculations

3. **Create targeted fixes:**
   - Based on findings, we can create Safari-specific polyfills
   - Adjust timing parameters for Safari
   - Add fallback mechanisms

## Disabling Debug Mode

When finished testing:

**Remove URL parameter:**
```
http://your-zm-server/zm/index.php?view=montage
```

**Clear cookie:**
```javascript
document.cookie = "SAFARI_DEBUG=; expires=Thu, 01 Jan 1970 00:00:00 UTC; path=/;";
```

**Or** edit `/home/user/ZoneMinder/web/skins/classic/includes/functions.php` and comment out lines 1598-1601.

## Support

If you encounter issues with the debug script itself:

1. Check the browser console for errors
2. Verify the script file exists at `/web/js/safari-montage-debug.js`
3. Check PHP error logs for issues loading the script
4. Try disabling and re-enabling debug mode

## File Locations

- **Debug Script:** `/home/user/ZoneMinder/web/js/safari-montage-debug.js`
- **PHP Injection:** `/home/user/ZoneMinder/web/skins/classic/includes/functions.php` (lines 1597-1601)
- **Montage JS:** `/home/user/ZoneMinder/web/skins/classic/views/js/montage.js`
- **MonitorStream JS:** `/home/user/ZoneMinder/web/js/MonitorStream.js`
- **This Document:** `/home/user/ZoneMinder/SAFARI_MONTAGE_DEBUG_INSTRUCTIONS.md`
