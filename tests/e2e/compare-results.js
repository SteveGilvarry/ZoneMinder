/**
 * Compare Results Between Chromium and WebKit
 *
 * This script analyzes the test results from both browsers and highlights
 * the differences to identify Safari-specific issues.
 *
 * Usage: node compare-results.js
 */

const fs = require('fs');
const path = require('path');

const RESULTS_DIR = path.join(__dirname, 'results');

// Colors for console output
const colors = {
  reset: '\x1b[0m',
  bright: '\x1b[1m',
  red: '\x1b[31m',
  green: '\x1b[32m',
  yellow: '\x1b[33m',
  blue: '\x1b[34m',
  magenta: '\x1b[35m',
  cyan: '\x1b[36m',
};

function log(color, ...args) {
  console.log(color + args.join(' ') + colors.reset);
}

function header(text) {
  console.log('\n' + colors.bright + colors.cyan + '='.repeat(80));
  console.log('  ' + text);
  console.log('='.repeat(80) + colors.reset + '\n');
}

function section(text) {
  console.log('\n' + colors.bright + colors.blue + '─── ' + text + ' ───' + colors.reset + '\n');
}

/**
 * Load JSON file
 */
function loadJSON(filename) {
  try {
    const filepath = path.join(RESULTS_DIR, filename);
    if (!fs.existsSync(filepath)) {
      return null;
    }
    const content = fs.readFileSync(filepath, 'utf8');
    return JSON.parse(content);
  } catch (error) {
    console.error(`Error loading ${filename}:`, error.message);
    return null;
  }
}

/**
 * Compare browser features
 */
function compareBrowserFeatures(chromiumLogs, webkitLogs) {
  header('BROWSER FEATURE COMPARISON');

  if (!chromiumLogs || !webkitLogs) {
    log(colors.red, '❌ Missing browser logs');
    return;
  }

  const chromiumFeatures = chromiumLogs.browserInfo?.features || {};
  const webkitFeatures = webkitLogs.browserInfo?.features || {};

  console.log('Feature Support:');
  console.log('┌────────────────────────────┬──────────┬──────────┐');
  console.log('│ Feature                    │ Chromium │  WebKit  │');
  console.log('├────────────────────────────┼──────────┼──────────┤');

  const allFeatures = new Set([
    ...Object.keys(chromiumFeatures),
    ...Object.keys(webkitFeatures)
  ]);

  allFeatures.forEach(feature => {
    const chromiumSupport = chromiumFeatures[feature] ? '✅' : '❌';
    const webkitSupport = webkitFeatures[feature] ? '✅' : '❌';
    const diff = chromiumFeatures[feature] !== webkitFeatures[feature];

    const color = diff ? colors.yellow : colors.reset;
    console.log(
      color +
      `│ ${feature.padEnd(26)} │   ${chromiumSupport}    │   ${webkitSupport}    │` +
      colors.reset
    );
  });

  console.log('└────────────────────────────┴──────────┴──────────┘');
}

/**
 * Compare event logs
 */
function compareEventLogs(chromiumLogs, webkitLogs) {
  header('EVENT LOG COMPARISON');

  if (!chromiumLogs || !webkitLogs) {
    log(colors.red, '❌ Missing event logs');
    return;
  }

  const chromiumEvents = chromiumLogs.logs || [];
  const webkitEvents = webkitLogs.logs || [];

  // Group events by category
  const chromiumByCategory = groupByCategory(chromiumEvents);
  const webkitByCategory = groupByCategory(webkitEvents);

  const allCategories = new Set([
    ...Object.keys(chromiumByCategory),
    ...Object.keys(webkitByCategory)
  ]);

  console.log('Events by Category:');
  console.log('┌────────────────┬──────────┬──────────┬──────────┐');
  console.log('│ Category       │ Chromium │  WebKit  │   Diff   │');
  console.log('├────────────────┼──────────┼──────────┼──────────┤');

  allCategories.forEach(category => {
    const chromiumCount = chromiumByCategory[category]?.length || 0;
    const webkitCount = webkitByCategory[category]?.length || 0;
    const diff = webkitCount - chromiumCount;
    const diffStr = diff > 0 ? `+${diff}` : diff.toString();
    const color = Math.abs(diff) > 5 ? colors.yellow : colors.reset;

    console.log(
      color +
      `│ ${category.padEnd(14)} │ ${chromiumCount.toString().padStart(8)} │ ${webkitCount.toString().padStart(8)} │ ${diffStr.padStart(8)} │` +
      colors.reset
    );
  });

  console.log('└────────────────┴──────────┴──────────┴──────────┘');

  // Find unique events
  section('Unique Event Messages');

  const chromiumMessages = new Set(chromiumEvents.map(e => `${e.category}:${e.message}`));
  const webkitMessages = new Set(webkitEvents.map(e => `${e.category}:${e.message}`));

  const onlyInChromium = [...chromiumMessages].filter(m => !webkitMessages.has(m));
  const onlyInWebkit = [...webkitMessages].filter(m => !chromiumMessages.has(m));

  if (onlyInChromium.length > 0) {
    log(colors.green, `\n✅ Events only in Chromium (${onlyInChromium.length}):`);
    onlyInChromium.slice(0, 10).forEach(msg => console.log(`   - ${msg}`));
    if (onlyInChromium.length > 10) {
      console.log(`   ... and ${onlyInChromium.length - 10} more`);
    }
  }

  if (onlyInWebkit.length > 0) {
    log(colors.yellow, `\n⚠️  Events only in WebKit/Safari (${onlyInWebkit.length}):`);
    onlyInWebkit.slice(0, 10).forEach(msg => console.log(`   - ${msg}`));
    if (onlyInWebkit.length > 10) {
      console.log(`   ... and ${onlyInWebkit.length - 10} more`);
    }
  }
}

function groupByCategory(events) {
  return events.reduce((acc, event) => {
    if (!acc[event.category]) {
      acc[event.category] = [];
    }
    acc[event.category].push(event);
    return acc;
  }, {});
}

/**
 * Compare timing reports
 */
function compareTimingReports(chromiumTiming, webkitTiming) {
  header('VIDEO LOADING TIMING COMPARISON');

  if (!chromiumTiming || !webkitTiming) {
    log(colors.yellow, '⚠️  Missing timing reports from one or both browsers');
    return;
  }

  const chromiumStats = chromiumTiming.stats || {};
  const webkitStats = webkitTiming.stats || {};

  console.log('Loading Time Statistics:');
  console.log('┌─────────────┬──────────┬──────────┬──────────┐');
  console.log('│ Metric      │ Chromium │  WebKit  │   Diff   │');
  console.log('├─────────────┼──────────┼──────────┼──────────┤');

  ['average', 'min', 'max'].forEach(metric => {
    const chromiumValue = chromiumStats[metric] || 0;
    const webkitValue = webkitStats[metric] || 0;
    const diff = webkitValue - chromiumValue;
    const diffStr = diff > 0 ? `+${diff.toFixed(1)}` : diff.toFixed(1);
    const color = Math.abs(diff) > 100 ? colors.red : colors.reset;

    console.log(
      color +
      `│ ${metric.padEnd(11)} │ ${chromiumValue.toFixed(1).padStart(7)}ms │ ${webkitValue.toFixed(1).padStart(7)}ms │ ${diffStr.padStart(7)}ms │` +
      colors.reset
    );
  });

  console.log('└─────────────┴──────────┴──────────┴──────────┘');

  if (webkitStats.average > chromiumStats.average * 2) {
    log(colors.red, '\n🐛 CRITICAL: WebKit is more than 2x slower than Chromium!');
  } else if (webkitStats.average > chromiumStats.average * 1.5) {
    log(colors.yellow, '\n⚠️  WARNING: WebKit is significantly slower than Chromium');
  } else {
    log(colors.green, '\n✅ Loading times are comparable');
  }
}

/**
 * Compare event reports
 */
function compareEventReports(chromiumEvents, webkitEvents) {
  header('TIMER & EVENT BEHAVIOR COMPARISON');

  if (!chromiumEvents || !webkitEvents) {
    log(colors.yellow, '⚠️  Missing event reports from one or both browsers');
    return;
  }

  const chromiumEvt = chromiumEvents.events || {};
  const webkitEvt = webkitEvents.events || {};

  console.log('Event Counts:');
  console.log('┌────────────────────┬──────────┬──────────┐');
  console.log('│ Event Type         │ Chromium │  WebKit  │');
  console.log('├────────────────────┼──────────┼──────────┤');

  ['timer', 'timerDrift', 'scroll', 'viewport'].forEach(type => {
    const chromiumCount = chromiumEvt[type] || 0;
    const webkitCount = webkitEvt[type] || 0;
    const color = chromiumCount !== webkitCount ? colors.yellow : colors.reset;

    console.log(
      color +
      `│ ${type.padEnd(18)} │ ${chromiumCount.toString().padStart(8)} │ ${webkitCount.toString().padStart(8)} │` +
      colors.reset
    );
  });

  console.log('└────────────────────┴──────────┴──────────┘');

  // Scroll end method
  const chromiumMethod = chromiumEvt.scrollEndMethod || 'unknown';
  const webkitMethod = webkitEvt.scrollEndMethod || 'unknown';

  console.log('\nScroll End Detection:');
  console.log(`  Chromium: ${chromiumMethod}`);
  console.log(`  WebKit:   ${webkitMethod}`);

  if (chromiumMethod !== webkitMethod) {
    log(colors.yellow, '\n⚠️  Different scroll end methods detected!');
    if (webkitMethod === 'fallback') {
      log(colors.yellow, '   → WebKit using fallback timeout (no native scrollend support)');
    }
  }

  // Timer drift
  const chromiumDrift = chromiumEvents.timerDriftDetails?.length || 0;
  const webkitDrift = webkitEvents.timerDriftDetails?.length || 0;

  if (webkitDrift > chromiumDrift) {
    log(colors.yellow, `\n⚠️  WebKit has ${webkitDrift - chromiumDrift} more timer drift events`);
  }
}

/**
 * Check for bug reports
 */
function checkBugReports() {
  header('BUG REPORTS');

  const bugReports = [];
  const files = fs.readdirSync(RESULTS_DIR);

  files.forEach(file => {
    if (file.includes('BUG-REPORT')) {
      const report = loadJSON(file);
      if (report) {
        bugReports.push({ file, ...report });
      }
    }
  });

  if (bugReports.length === 0) {
    log(colors.green, '✅ No bug reports found - tests passed!');
    return;
  }

  log(colors.red, `🐛 Found ${bugReports.length} bug report(s):`);

  bugReports.forEach(report => {
    console.log(`\n  File: ${report.file}`);
    console.log(`  Browser: ${report.browser}`);
    console.log(`  Issue: ${report.issue}`);
    if (report.missingMonitors) {
      console.log(`  Missing Monitors: ${report.missingMonitors.join(', ')}`);
    }
  });
}

/**
 * Main comparison
 */
function main() {
  log(colors.bright + colors.magenta, '\n🔍 ZoneMinder Safari Montage Test Results Comparison\n');

  // Check if results directory exists
  if (!fs.existsSync(RESULTS_DIR)) {
    log(colors.red, '❌ Results directory not found. Run tests first!');
    log(colors.yellow, '\nRun: npm test');
    process.exit(1);
  }

  // Load all reports
  const chromiumInitialLogs = loadJSON('chromium-initial-load-logs.json');
  const webkitInitialLogs = loadJSON('webkit-initial-load-logs.json');
  const chromiumScrollLogs = loadJSON('chromium-scroll-test-logs.json');
  const webkitScrollLogs = loadJSON('webkit-scroll-test-logs.json');
  const chromiumTiming = loadJSON('chromium-timing-report.json');
  const webkitTiming = loadJSON('webkit-timing-report.json');
  const chromiumEvents = loadJSON('chromium-event-report.json');
  const webkitEvents = loadJSON('webkit-event-report.json');

  // Compare browser features
  compareBrowserFeatures(chromiumInitialLogs, webkitInitialLogs);

  // Compare event logs
  if (chromiumScrollLogs && webkitScrollLogs) {
    compareEventLogs(chromiumScrollLogs, webkitScrollLogs);
  }

  // Compare timing
  compareTimingReports(chromiumTiming, webkitTiming);

  // Compare events
  compareEventReports(chromiumEvents, webkitEvents);

  // Check for bugs
  checkBugReports();

  // Summary
  header('SUMMARY & RECOMMENDATIONS');

  const issues = [];

  // Check for scrollend support
  if (webkitInitialLogs?.browserInfo?.features?.scrollEndEvent === false) {
    issues.push({
      severity: 'high',
      message: 'WebKit does not support native scrollend event',
      recommendation: 'Verify fallback timeout is working correctly in montage.js line 734-741'
    });
  }

  // Check for timing differences
  if (chromiumTiming && webkitTiming) {
    const ratio = webkitTiming.stats.average / chromiumTiming.stats.average;
    if (ratio > 2) {
      issues.push({
        severity: 'critical',
        message: `Video loading is ${ratio.toFixed(1)}x slower in WebKit`,
        recommendation: 'Investigate MonitorStream.start() method - possible async/await issue'
      });
    }
  }

  // Check for timer drift
  if (webkitEvents && chromiumEvents) {
    const driftDiff = (webkitEvents.events?.timerDrift || 0) - (chromiumEvents.events?.timerDrift || 0);
    if (driftDiff > 5) {
      issues.push({
        severity: 'medium',
        message: `WebKit has ${driftDiff} more timer drift events`,
        recommendation: 'Safari may be throttling timers - consider adjusting intervals'
      });
    }
  }

  if (issues.length === 0) {
    log(colors.green, '✅ No major issues detected!');
  } else {
    issues.forEach(issue => {
      const color = issue.severity === 'critical' ? colors.red :
                    issue.severity === 'high' ? colors.yellow :
                    colors.blue;

      log(color, `\n[${issue.severity.toUpperCase()}] ${issue.message}`);
      console.log(`  → ${issue.recommendation}`);
    });
  }

  log(colors.bright + colors.cyan, '\n' + '='.repeat(80) + '\n');
}

// Run comparison
main();
