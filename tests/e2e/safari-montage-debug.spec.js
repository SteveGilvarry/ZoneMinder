/**
 * Safari Montage Video Loading Automated Test
 *
 * This Playwright test automates the comparison between Chromium and WebKit (Safari)
 * to identify where video loading and scrolling behavior differs.
 *
 * Usage:
 *   npx playwright test safari-montage-debug.spec.js
 *   npx playwright test safari-montage-debug.spec.js --project=webkit  # Safari only
 *   npx playwright test safari-montage-debug.spec.js --project=chromium  # Chrome only
 */

const { test, expect } = require('@playwright/test');
const fs = require('fs');
const path = require('path');

// Configuration
const ZM_BASE_URL = process.env.ZM_URL || 'http://localhost/zm';
const ZM_USERNAME = process.env.ZM_USERNAME || 'admin';
const ZM_PASSWORD = process.env.ZM_PASSWORD || 'admin';
const RESULTS_DIR = path.join(__dirname, 'results');

// Ensure results directory exists
if (!fs.existsSync(RESULTS_DIR)) {
  fs.mkdirSync(RESULTS_DIR, { recursive: true });
}

/**
 * Helper: Login to ZoneMinder
 */
async function loginToZoneMinder(page) {
  await page.goto(`${ZM_BASE_URL}/index.php`);

  // Check if already logged in
  const isLoggedIn = await page.locator('#navbar-container').isVisible().catch(() => false);
  if (isLoggedIn) {
    console.log('Already logged in');
    return;
  }

  // Fill login form
  await page.fill('input[name="username"]', ZM_USERNAME);
  await page.fill('input[name="password"]', ZM_PASSWORD);
  await page.click('button[type="submit"]');

  // Wait for login to complete
  await page.waitForSelector('#navbar-container', { timeout: 10000 });
}

/**
 * Helper: Extract debug logs from page
 */
async function extractDebugLogs(page) {
  return await page.evaluate(() => {
    if (typeof SafariDebug !== 'undefined') {
      return {
        browserInfo: SafariDebug.BrowserInfo,
        logs: SafariDebug.LogSystem.logs,
        timestamp: new Date().toISOString()
      };
    }
    return null;
  });
}

/**
 * Helper: Wait for monitors to be initialized
 */
async function waitForMonitorsInit(page, timeout = 30000) {
  await page.waitForFunction(
    () => typeof monitors !== 'undefined' && monitors.length > 0,
    { timeout }
  );
}

/**
 * Helper: Get monitor states
 */
async function getMonitorStates(page) {
  return await page.evaluate(() => {
    if (typeof monitors === 'undefined') return [];

    return monitors.map(m => ({
      id: m.id,
      started: m.started,
      activePlayer: m.activePlayer,
      element: {
        id: m.getElement()?.id,
        nodeName: m.getElement()?.nodeName,
        src: m.getElement()?.src || m.getElement()?.getAttribute('src')
      }
    }));
  });
}

/**
 * Helper: Check which monitors are visible in viewport
 */
async function getVisibleMonitors(page) {
  return await page.evaluate(() => {
    if (typeof monitors === 'undefined' || typeof isOutOfViewport === 'undefined') {
      return { visible: [], hidden: [] };
    }

    const visible = [];
    const hidden = [];

    monitors.forEach(m => {
      const element = m.getElement();
      if (!element) return;

      const viewport = isOutOfViewport(element);
      if (viewport.all) {
        hidden.push(m.id);
      } else {
        visible.push(m.id);
      }
    });

    return { visible, hidden };
  });
}

/**
 * Helper: Take screenshot with timestamp
 */
async function takeTimestampedScreenshot(page, browserName, label) {
  const timestamp = new Date().toISOString().replace(/[:.]/g, '-');
  const filename = `${browserName}-${label}-${timestamp}.png`;
  const filepath = path.join(RESULTS_DIR, filename);
  await page.screenshot({ path: filepath, fullPage: true });
  return filepath;
}

/**
 * Main test suite
 */
test.describe('Safari Montage Video Loading', () => {

  test.beforeEach(async ({ page, browserName }) => {
    // Enable debug mode via cookie
    await page.context().addCookies([{
      name: 'SAFARI_DEBUG',
      value: '1',
      domain: new URL(ZM_BASE_URL).hostname,
      path: '/'
    }]);

    console.log(`\n📱 Testing on: ${browserName}`);
  });

  test('Initial page load and monitor detection', async ({ page, browserName }) => {
    // Login
    await loginToZoneMinder(page);

    // Navigate to montage view with debug enabled
    await page.goto(`${ZM_BASE_URL}/index.php?view=montage&debug=safari`);

    // Wait for debug panel to appear
    await page.waitForSelector('#safari-debug-panel', { timeout: 5000 });

    // Wait for monitors to initialize
    await waitForMonitorsInit(page);

    // Take initial screenshot
    const screenshotPath = await takeTimestampedScreenshot(page, browserName, 'initial-load');
    console.log(`📸 Screenshot saved: ${screenshotPath}`);

    // Get monitor count
    const monitorCount = await page.evaluate(() => monitors.length);
    console.log(`📊 Found ${monitorCount} monitors`);

    // Wait a bit for monitors to start loading
    await page.waitForTimeout(3000);

    // Get monitor states
    const states = await getMonitorStates(page);
    console.log(`🎬 Monitor states:`, states);

    // Check how many are actually started
    const startedCount = states.filter(m => m.started).length;
    console.log(`✅ Started: ${startedCount}/${monitorCount}`);

    // Extract debug logs
    const debugLogs = await extractDebugLogs(page);
    if (debugLogs) {
      const logPath = path.join(RESULTS_DIR, `${browserName}-initial-load-logs.json`);
      fs.writeFileSync(logPath, JSON.stringify(debugLogs, null, 2));
      console.log(`📝 Logs saved: ${logPath}`);
    }

    // Expectations
    expect(monitorCount).toBeGreaterThan(0);
    expect(debugLogs).not.toBeNull();
    expect(debugLogs.browserInfo).toBeDefined();
  });

  test('Scroll behavior and monitor visibility', async ({ page, browserName }) => {
    // Login and navigate
    await loginToZoneMinder(page);
    await page.goto(`${ZM_BASE_URL}/index.php?view=montage&debug=safari`);
    await waitForMonitorsInit(page);

    // Get initial visible monitors
    const initialVisible = await getVisibleMonitors(page);
    console.log(`👁️  Initially visible monitors: ${initialVisible.visible.join(', ')}`);
    console.log(`🙈 Initially hidden monitors: ${initialVisible.hidden.join(', ')}`);

    // Take screenshot before scroll
    await takeTimestampedScreenshot(page, browserName, 'before-scroll');

    // Scroll down slowly (simulate user scrolling)
    const scrollDistance = 500;
    console.log(`⬇️  Scrolling down ${scrollDistance}px...`);
    await page.evaluate((distance) => {
      window.scrollBy({ top: distance, behavior: 'smooth' });
    }, scrollDistance);

    // Wait for scroll to complete
    // Check if browser supports scrollend
    const hasScrollEnd = await page.evaluate(() => 'onscrollend' in window);
    console.log(`🔍 Browser supports scrollend: ${hasScrollEnd}`);

    if (hasScrollEnd) {
      // Wait for native scrollend event
      await page.evaluate(() => {
        return new Promise(resolve => {
          document.addEventListener('scrollend', resolve, { once: true });
          // Timeout fallback
          setTimeout(resolve, 2000);
        });
      });
    } else {
      // Wait for fallback timeout
      await page.waitForTimeout(500);
    }

    // Take screenshot after scroll
    await takeTimestampedScreenshot(page, browserName, 'after-scroll');

    // Get visible monitors after scroll
    const afterScrollVisible = await getVisibleMonitors(page);
    console.log(`👁️  After scroll visible: ${afterScrollVisible.visible.join(', ')}`);
    console.log(`🙈 After scroll hidden: ${afterScrollVisible.hidden.join(', ')}`);

    // Get monitor states after scroll
    const statesAfterScroll = await getMonitorStates(page);
    console.log(`🎬 Monitor states after scroll:`, statesAfterScroll);

    // Extract debug logs
    const debugLogs = await extractDebugLogs(page);
    if (debugLogs) {
      const logPath = path.join(RESULTS_DIR, `${browserName}-scroll-test-logs.json`);
      fs.writeFileSync(logPath, JSON.stringify(debugLogs, null, 2));
      console.log(`📝 Scroll logs saved: ${logPath}`);

      // Analyze scroll events
      const scrollLogs = debugLogs.logs.filter(l => l.category === 'SCROLL');
      console.log(`📊 Scroll events captured: ${scrollLogs.length}`);

      const scrollEndLogs = scrollLogs.filter(l => l.message.includes('ended'));
      console.log(`🏁 Scroll end events: ${scrollEndLogs.length}`);
    }

    // Scroll back up
    console.log(`⬆️  Scrolling back up...`);
    await page.evaluate((distance) => {
      window.scrollBy({ top: -distance, behavior: 'smooth' });
    }, scrollDistance);

    if (hasScrollEnd) {
      await page.evaluate(() => {
        return new Promise(resolve => {
          document.addEventListener('scrollend', resolve, { once: true });
          setTimeout(resolve, 2000);
        });
      });
    } else {
      await page.waitForTimeout(500);
    }

    // Take screenshot after scroll back
    await takeTimestampedScreenshot(page, browserName, 'after-scroll-back');

    // Get final visible monitors
    const finalVisible = await getVisibleMonitors(page);
    console.log(`👁️  After scroll back visible: ${finalVisible.visible.join(', ')}`);

    // Check if monitors came back
    const missingMonitors = initialVisible.visible.filter(
      id => !finalVisible.visible.includes(id)
    );

    if (missingMonitors.length > 0) {
      console.log(`⚠️  WARNING: Monitors disappeared after scroll: ${missingMonitors.join(', ')}`);

      // This is the bug we're looking for!
      const bugReport = {
        browser: browserName,
        issue: 'Monitors disappeared after scrolling',
        missingMonitors: missingMonitors,
        initialVisible: initialVisible.visible,
        finalVisible: finalVisible.visible,
        debugLogs: debugLogs
      };

      const reportPath = path.join(RESULTS_DIR, `${browserName}-BUG-REPORT.json`);
      fs.writeFileSync(reportPath, JSON.stringify(bugReport, null, 2));
      console.log(`🐛 BUG REPORT saved: ${reportPath}`);
    }
  });

  test('Video loading timing comparison', async ({ page, browserName }) => {
    await loginToZoneMinder(page);
    await page.goto(`${ZM_BASE_URL}/index.php?view=montage&debug=safari`);
    await waitForMonitorsInit(page);

    // Wait for initial load
    await page.waitForTimeout(5000);

    // Extract debug logs
    const debugLogs = await extractDebugLogs(page);

    if (debugLogs) {
      // Analyze video loading times
      const videoLogs = debugLogs.logs.filter(l => l.category === 'VIDEO');
      const startingLogs = videoLogs.filter(l => l.message.includes('starting'));
      const completedLogs = videoLogs.filter(l => l.message.includes('start completed'));

      console.log(`🎬 Video start events: ${startingLogs.length}`);
      console.log(`✅ Video completed events: ${completedLogs.length}`);

      // Calculate average loading time
      const loadingTimes = completedLogs
        .map(log => {
          const match = log.data.duration?.match(/(\d+\.?\d*)ms/);
          return match ? parseFloat(match[1]) : null;
        })
        .filter(t => t !== null);

      if (loadingTimes.length > 0) {
        const avgTime = loadingTimes.reduce((a, b) => a + b, 0) / loadingTimes.length;
        const maxTime = Math.max(...loadingTimes);
        const minTime = Math.min(...loadingTimes);

        console.log(`⏱️  Video loading times:`);
        console.log(`   Average: ${avgTime.toFixed(2)}ms`);
        console.log(`   Min: ${minTime.toFixed(2)}ms`);
        console.log(`   Max: ${maxTime.toFixed(2)}ms`);

        const timingReport = {
          browser: browserName,
          loadingTimes: loadingTimes,
          stats: {
            average: avgTime,
            min: minTime,
            max: maxTime,
            count: loadingTimes.length
          }
        };

        const reportPath = path.join(RESULTS_DIR, `${browserName}-timing-report.json`);
        fs.writeFileSync(reportPath, JSON.stringify(timingReport, null, 2));
        console.log(`⏱️  Timing report saved: ${reportPath}`);
      } else {
        console.log(`⚠️  WARNING: No video loading times captured!`);
      }

      // Check for videos that started but never completed
      const stuckVideos = startingLogs.filter(startLog => {
        const monitorId = startLog.message.match(/Monitor (\d+)/)?.[1];
        if (!monitorId) return false;

        return !completedLogs.some(completeLog =>
          completeLog.message.includes(`Monitor ${monitorId}`)
        );
      });

      if (stuckVideos.length > 0) {
        console.log(`⚠️  WARNING: ${stuckVideos.length} videos started but never completed:`);
        stuckVideos.forEach(log => console.log(`   - ${log.message}`));
      }
    }
  });

  test('Timer and event behavior', async ({ page, browserName }) => {
    await loginToZoneMinder(page);
    await page.goto(`${ZM_BASE_URL}/index.php?view=montage&debug=safari`);
    await waitForMonitorsInit(page);

    // Wait for timers to fire
    await page.waitForTimeout(5000);

    // Extract debug logs
    const debugLogs = await extractDebugLogs(page);

    if (debugLogs) {
      // Analyze timer behavior
      const timerLogs = debugLogs.logs.filter(l => l.category === 'TIMER');
      console.log(`⏲️  Timer events captured: ${timerLogs.length}`);

      // Check for timer drift
      const driftLogs = timerLogs.filter(log => {
        if (log.data.delay && log.data.actualDelay) {
          const drift = log.data.actualDelay - log.data.delay;
          return drift > 50; // More than 50ms drift
        }
        return false;
      });

      if (driftLogs.length > 0) {
        console.log(`⚠️  Timer drift detected in ${driftLogs.length} events`);
        driftLogs.forEach(log => {
          const drift = log.data.actualDelay - log.data.delay;
          console.log(`   Drift: ${drift.toFixed(2)}ms (expected: ${log.data.delay}ms, actual: ${log.data.actualDelay}ms)`);
        });
      }

      // Check for scroll events
      const scrollLogs = debugLogs.logs.filter(l => l.category === 'SCROLL');
      const scrollEndMethod = debugLogs.browserInfo.features.scrollEndEvent ? 'native' : 'fallback';

      console.log(`📜 Scroll events: ${scrollLogs.length}`);
      console.log(`🔧 Scroll end method: ${scrollEndMethod}`);

      // Check for viewport events
      const viewportLogs = debugLogs.logs.filter(l => l.category === 'VIEWPORT');
      console.log(`🔍 Viewport checks: ${viewportLogs.length}`);

      const eventReport = {
        browser: browserName,
        browserInfo: debugLogs.browserInfo,
        events: {
          timer: timerLogs.length,
          timerDrift: driftLogs.length,
          scroll: scrollLogs.length,
          scrollEndMethod: scrollEndMethod,
          viewport: viewportLogs.length
        },
        timerDriftDetails: driftLogs
      };

      const reportPath = path.join(RESULTS_DIR, `${browserName}-event-report.json`);
      fs.writeFileSync(reportPath, JSON.stringify(eventReport, null, 2));
      console.log(`📊 Event report saved: ${reportPath}`);
    }
  });

  test('Long session stability (30 seconds)', async ({ page, browserName }) => {
    await loginToZoneMinder(page);
    await page.goto(`${ZM_BASE_URL}/index.php?view=montage&debug=safari`);
    await waitForMonitorsInit(page);

    console.log(`⏳ Running 30-second stability test...`);

    // Take periodic snapshots
    const snapshots = [];
    const duration = 30000; // 30 seconds
    const interval = 5000; // Every 5 seconds

    for (let i = 0; i <= duration; i += interval) {
      await page.waitForTimeout(Math.min(interval, duration - i));

      const snapshot = {
        timestamp: i,
        monitorStates: await getMonitorStates(page),
        visibleMonitors: await getVisibleMonitors(page)
      };

      snapshots.push(snapshot);
      console.log(`   ${i / 1000}s: ${snapshot.monitorStates.filter(m => m.started).length} monitors running`);
    }

    // Extract final debug logs
    const debugLogs = await extractDebugLogs(page);

    const stabilityReport = {
      browser: browserName,
      duration: duration,
      snapshots: snapshots,
      debugLogs: debugLogs
    };

    const reportPath = path.join(RESULTS_DIR, `${browserName}-stability-report.json`);
    fs.writeFileSync(reportPath, JSON.stringify(stabilityReport, null, 2));
    console.log(`📊 Stability report saved: ${reportPath}`);

    // Check for monitors that stopped unexpectedly
    const initialRunning = snapshots[0].monitorStates.filter(m => m.started).map(m => m.id);
    const finalRunning = snapshots[snapshots.length - 1].monitorStates.filter(m => m.started).map(m => m.id);
    const stoppedMonitors = initialRunning.filter(id => !finalRunning.includes(id));

    if (stoppedMonitors.length > 0) {
      console.log(`⚠️  WARNING: Monitors stopped unexpectedly: ${stoppedMonitors.join(', ')}`);
    }
  });
});
