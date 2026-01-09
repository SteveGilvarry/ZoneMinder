# ZoneMinder E2E Tests - Safari Montage Debugging

Automated end-to-end tests using Playwright to compare Safari (WebKit) and Chrome (Chromium) behavior in the montage view.

## 🎯 Purpose

These tests automate the debugging process for Safari-specific issues where:
- Video players are slow to load in montage view
- Images appear initially but disappear when scrolling
- Behavior works fine in Chrome but not in Safari

## 📋 Prerequisites

- Node.js 16+ installed
- ZoneMinder running and accessible
- At least one monitor configured in ZoneMinder

## 🚀 Quick Start

### 1. Install Dependencies

```bash
cd /home/user/ZoneMinder/tests/e2e
npm install
npx playwright install
```

This will install Playwright and download the browser binaries (Chromium and WebKit).

### 2. Configure Environment

Create a `.env` file or set environment variables:

```bash
# Optional: Set ZoneMinder URL if not using default
export ZM_URL="http://localhost/zm"

# Optional: Set credentials if not using defaults
export ZM_USERNAME="admin"
export ZM_PASSWORD="admin"
```

### 3. Run Tests

**Run all tests in both browsers:**
```bash
npm run test:both
```

**Run only Safari (WebKit) tests:**
```bash
npm run test:safari
```

**Run only Chrome (Chromium) tests:**
```bash
npm run test:chrome
```

**Run with UI mode (interactive):**
```bash
npm run test:ui
```

**Run in debug mode:**
```bash
npm run test:debug
```

### 4. Compare Results

After running tests in both browsers:

```bash
npm run compare
```

This will analyze and compare the results, highlighting differences between browsers.

### 5. View Reports

```bash
npm run show:report
```

Opens an HTML report with detailed test results, screenshots, and videos.

## 📊 Test Suites

### 1. Initial Page Load Test
- Verifies monitors are detected
- Checks how many monitors start streaming
- Captures timing for initial load
- Takes screenshots

### 2. Scroll Behavior Test
- Tests scrolling up and down
- Verifies monitors start/stop based on visibility
- Detects if monitors disappear after scrolling (the bug!)
- Compares scrollend event support

### 3. Video Loading Timing Test
- Measures how long each monitor takes to start
- Compares average, min, and max loading times
- Identifies monitors that start but never complete

### 4. Timer and Event Behavior Test
- Analyzes timer drift (setTimeout/setInterval accuracy)
- Compares scroll event handling
- Checks viewport detection frequency

### 5. Long Session Stability Test
- Runs for 30 seconds
- Takes periodic snapshots of monitor states
- Detects if monitors stop unexpectedly

## 📁 Results Structure

After running tests, results are saved in `results/`:

```
results/
├── chromium-initial-load-logs.json       # Debug logs from initial load
├── chromium-scroll-test-logs.json        # Debug logs from scroll test
├── chromium-timing-report.json           # Video loading timing data
├── chromium-event-report.json            # Timer and event data
├── chromium-stability-report.json        # Long-term stability data
├── webkit-initial-load-logs.json         # Same for Safari
├── webkit-scroll-test-logs.json
├── webkit-timing-report.json
├── webkit-event-report.json
├── webkit-stability-report.json
├── chromium-*.png                        # Screenshots
├── webkit-*.png
├── *-BUG-REPORT.json                     # Bug reports (if issues found)
└── html-report/                          # HTML test report
```

## 🔍 Interpreting Results

### Comparison Script Output

The comparison script (`npm run compare`) produces a detailed analysis:

#### 1. Browser Feature Comparison
Shows which features are supported in each browser:
- ✅ = Supported
- ❌ = Not supported

Key feature to watch: **scrollEndEvent**

#### 2. Event Log Comparison
Compares event counts by category:
- SCROLL events
- VIDEO events
- TIMER events
- VIEWPORT events

Large differences indicate where behavior diverges.

#### 3. Video Loading Timing
Compares loading time statistics:
- Average time
- Min time
- Max time

If WebKit is >2x slower, there's likely an issue with video initialization.

#### 4. Timer & Event Behavior
Shows timer drift and scroll end detection method.

#### 5. Bug Reports
Lists any detected bugs (e.g., monitors disappearing after scroll).

### Example Healthy Output

```
=== BROWSER FEATURE COMPARISON ===
Feature Support:
┌────────────────────────────┬──────────┬──────────┐
│ Feature                    │ Chromium │  WebKit  │
├────────────────────────────┼──────────┼──────────┤
│ scrollEndEvent             │   ✅     │   ❌     │  ← Expected difference
│ resizeObserver             │   ✅     │   ✅     │
│ intersectionObserver       │   ✅     │   ✅     │
└────────────────────────────┴──────────┴──────────┘

=== VIDEO LOADING TIMING ===
Loading Time Statistics:
┌─────────────┬──────────┬──────────┬──────────┐
│ Metric      │ Chromium │  WebKit  │   Diff   │
├─────────────┼──────────┼──────────┼──────────┤
│ average     │    45.2ms │    52.1ms │    +6.9ms │  ← Small difference OK
│ min         │    32.1ms │    38.4ms │    +6.3ms │
│ max         │    89.5ms │   102.3ms │   +12.8ms │
└─────────────┴──────────┴──────────┴──────────┘

✅ Loading times are comparable
```

### Example Problematic Output

```
=== VIDEO LOADING TIMING ===
Loading Time Statistics:
┌─────────────┬──────────┬──────────┬──────────┐
│ Metric      │ Chromium │  WebKit  │   Diff   │
├─────────────┼──────────┼──────────┼──────────┤
│ average     │    45.2ms │  2103.5ms │ +2058.3ms │  ← PROBLEM!
│ min         │    32.1ms │  1834.2ms │ +1802.1ms │
│ max         │    89.5ms │  3421.8ms │ +3332.3ms │
└─────────────┴──────────┴──────────┴──────────┘

🐛 CRITICAL: WebKit is more than 2x slower than Chromium!

=== BUG REPORTS ===
🐛 Found 1 bug report(s):
  File: webkit-BUG-REPORT.json
  Browser: webkit
  Issue: Monitors disappeared after scrolling
  Missing Monitors: 3, 4
```

## 🐛 Common Issues Found

### Issue 1: scrollend Event Not Supported
**Symptom:** WebKit shows `scrollEndMethod: 'fallback'`

**Fix:** Verify the fallback timeout in `montage.js` line 734-741 is working correctly.

### Issue 2: Videos Load Slowly in WebKit
**Symptom:** WebKit timing 2x+ slower than Chromium

**Fix:** Check `MonitorStream.start()` for async/await issues or WebRTC initialization problems.

### Issue 3: Monitors Disappear After Scrolling
**Symptom:** Bug report generated showing missing monitors after scroll

**Fix:** Check `isOutOfViewport()` function and scroll end detection.

### Issue 4: Timer Drift in WebKit
**Symptom:** WebKit has many more timer drift events

**Fix:** Safari throttles timers. Consider increasing intervals or using requestAnimationFrame.

## 🛠️ Troubleshooting

### Tests Fail to Start

**Problem:** ZoneMinder not accessible
```bash
# Check ZoneMinder is running
curl http://localhost/zm

# Set correct URL
export ZM_URL="http://your-server/zm"
```

**Problem:** Login fails
```bash
# Set correct credentials
export ZM_USERNAME="your-username"
export ZM_PASSWORD="your-password"
```

### No Results Generated

**Problem:** Tests timeout before completing
- Increase timeout in `playwright.config.js`
- Check if monitors are streaming (ZoneMinder issue, not test issue)

### WebKit Not Installed

```bash
# Reinstall Playwright browsers
npx playwright install webkit
```

## 📝 Manual Testing

You can also run the debug mode manually without Playwright:

1. Navigate to: `http://your-zm-server/zm/index.php?view=montage&debug=safari`
2. Open browser console
3. Use the debug panel and manual commands (see `SAFARI_DEBUG_QUICK_REFERENCE.md`)

## 🔧 Advanced Usage

### Run Specific Test

```bash
npx playwright test safari-montage-debug.spec.js -g "Initial page load"
```

### Run with Headed Mode (See Browser)

```bash
npx playwright test --headed
```

### Record Test

```bash
npx playwright codegen http://localhost/zm/index.php?view=montage
```

### Debug Specific Browser

```bash
npx playwright test --project=webkit --debug
```

### Generate New Report

```bash
npx playwright show-report
```

## 📚 Additional Resources

- **Full Testing Guide:** `/home/user/ZoneMinder/SAFARI_MONTAGE_DEBUG_INSTRUCTIONS.md`
- **Quick Reference:** `/home/user/ZoneMinder/SAFARI_DEBUG_QUICK_REFERENCE.md`
- **Debug Script:** `/home/user/ZoneMinder/web/js/safari-montage-debug.js`
- **Playwright Docs:** https://playwright.dev/

## 🤝 Contributing

To add new tests:

1. Add test case to `safari-montage-debug.spec.js`
2. Update comparison logic in `compare-results.js` if needed
3. Run tests and verify results
4. Update this README with new test description

## 📄 License

GPL-2.0 (same as ZoneMinder)
