/**
 * Safari Montage Debug Utility
 *
 * This file provides comprehensive debugging for Safari-specific issues with the montage view.
 * It logs timing, events, scroll behavior, and video loading to help identify differences
 * between Chrome and Safari.
 *
 * Usage:
 * 1. Include this file in montage.php before montage.js
 * 2. Open browser console to see detailed logs
 * 3. Compare logs between Chrome and Safari
 * 4. Use DEBUG_VISUAL mode to see on-screen indicators
 */

(function() {
  'use strict';

  // ============================================================================
  // CONFIGURATION
  // ============================================================================

  const DEBUG_CONFIG = {
    // Enable/disable different debugging features
    ENABLE_LOGGING: true,           // Console logging
    ENABLE_VISUAL: true,            // Visual on-screen indicators
    ENABLE_TIMING: true,            // Performance timing
    ENABLE_EVENT_TRACKING: true,    // Track all events
    ENABLE_SCROLL_TRACKING: true,   // Detailed scroll tracking
    ENABLE_VIDEO_TRACKING: true,    // Video element tracking
    ENABLE_TIMER_TRACKING: true,    // setTimeout/setInterval tracking

    // Log levels
    LOG_VERBOSE: true,              // Verbose logging
    LOG_WARNINGS: true,             // Show warnings
    LOG_ERRORS: true,               // Show errors

    // Export logs to localStorage for comparison
    EXPORT_LOGS: true,
    MAX_LOG_ENTRIES: 1000,
  };

  // ============================================================================
  // BROWSER DETECTION
  // ============================================================================

  const BrowserInfo = {
    isSafari: /^((?!chrome|android).)*safari/i.test(navigator.userAgent),
    isChrome: /chrome/i.test(navigator.userAgent) && !/edge/i.test(navigator.userAgent),
    isFirefox: /firefox/i.test(navigator.userAgent),
    isEdge: /edge/i.test(navigator.userAgent),
    userAgent: navigator.userAgent,

    // Safari-specific version detection
    safariVersion: function() {
      if (!this.isSafari) return null;
      const match = navigator.userAgent.match(/Version\/(\d+\.\d+)/);
      return match ? parseFloat(match[1]) : null;
    }(),

    // Feature detection
    features: {
      scrollEndEvent: 'onscrollend' in window,
      resizeObserver: typeof ResizeObserver !== 'undefined',
      intersectionObserver: typeof IntersectionObserver !== 'undefined',
      requestIdleCallback: typeof requestIdleCallback !== 'undefined',
      getBoundingClientRect: typeof Element.prototype.getBoundingClientRect !== 'undefined',
    }
  };

  // ============================================================================
  // LOGGING SYSTEM
  // ============================================================================

  const LogSystem = {
    logs: [],
    startTime: performance.now(),

    log: function(category, message, data = {}) {
      if (!DEBUG_CONFIG.ENABLE_LOGGING) return;

      const timestamp = performance.now() - this.startTime;
      const logEntry = {
        timestamp: timestamp.toFixed(2),
        category: category,
        message: message,
        data: data,
        browser: BrowserInfo.isSafari ? 'Safari' : (BrowserInfo.isChrome ? 'Chrome' : 'Other'),
        userAgent: BrowserInfo.userAgent
      };

      this.logs.push(logEntry);

      // Limit log size
      if (this.logs.length > DEBUG_CONFIG.MAX_LOG_ENTRIES) {
        this.logs.shift();
      }

      // Console output with color coding
      const color = this.getCategoryColor(category);
      console.log(
        `%c[${timestamp.toFixed(2)}ms] [${category}] ${message}`,
        `color: ${color}; font-weight: bold;`,
        data
      );

      // Export to localStorage if enabled
      if (DEBUG_CONFIG.EXPORT_LOGS) {
        this.exportToStorage();
      }
    },

    getCategoryColor: function(category) {
      const colors = {
        'SCROLL': '#3498db',
        'VIDEO': '#e74c3c',
        'TIMER': '#f39c12',
        'EVENT': '#9b59b6',
        'VIEWPORT': '#1abc9c',
        'BROWSER': '#34495e',
        'PERFORMANCE': '#16a085',
        'ERROR': '#c0392b',
        'WARNING': '#e67e22',
      };
      return colors[category] || '#7f8c8d';
    },

    exportToStorage: function() {
      try {
        localStorage.setItem('zm_safari_debug_logs', JSON.stringify({
          browser: BrowserInfo,
          logs: this.logs,
          exportTime: new Date().toISOString()
        }));
      } catch (e) {
        console.error('Failed to export logs to localStorage:', e);
      }
    },

    downloadLogs: function() {
      const dataStr = JSON.stringify({
        browser: BrowserInfo,
        logs: this.logs,
        exportTime: new Date().toISOString()
      }, null, 2);

      const blob = new Blob([dataStr], {type: 'application/json'});
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = `zm-debug-${BrowserInfo.isSafari ? 'safari' : 'chrome'}-${Date.now()}.json`;
      document.body.appendChild(a);
      a.click();
      document.body.removeChild(a);
      URL.revokeObjectURL(url);
    },

    compareLogs: function(safariLogsJson, chromeLogsJson) {
      console.log('=== LOG COMPARISON ===');
      const safariLogs = JSON.parse(safariLogsJson);
      const chromeLogs = JSON.parse(chromeLogsJson);

      console.log('Safari log count:', safariLogs.logs.length);
      console.log('Chrome log count:', chromeLogs.logs.length);

      // Find events that happen in one but not the other
      const safariEvents = safariLogs.logs.map(l => `${l.category}:${l.message}`);
      const chromeEvents = chromeLogs.logs.map(l => `${l.category}:${l.message}`);

      const onlyInSafari = safariEvents.filter(e => !chromeEvents.includes(e));
      const onlyInChrome = chromeEvents.filter(e => !safariEvents.includes(e));

      console.log('Events only in Safari:', onlyInSafari);
      console.log('Events only in Chrome:', onlyInChrome);

      return {safariLogs, chromeLogs, onlyInSafari, onlyInChrome};
    }
  };

  // ============================================================================
  // VISUAL DEBUG INDICATORS
  // ============================================================================

  const VisualDebug = {
    debugPanel: null,

    init: function() {
      if (!DEBUG_CONFIG.ENABLE_VISUAL) return;

      // Create debug panel
      this.debugPanel = document.createElement('div');
      this.debugPanel.id = 'safari-debug-panel';
      this.debugPanel.style.cssText = `
        position: fixed;
        top: 10px;
        right: 10px;
        background: rgba(0, 0, 0, 0.85);
        color: #00ff00;
        font-family: monospace;
        font-size: 11px;
        padding: 10px;
        border-radius: 5px;
        z-index: 10000;
        max-width: 350px;
        max-height: 400px;
        overflow-y: auto;
        box-shadow: 0 4px 6px rgba(0,0,0,0.3);
      `;

      document.body.appendChild(this.debugPanel);
      this.update('Initialized', 'Debug panel ready');
    },

    update: function(label, value) {
      if (!this.debugPanel) return;

      const entry = document.createElement('div');
      entry.style.cssText = 'margin-bottom: 5px; border-bottom: 1px solid #333; padding-bottom: 3px;';
      entry.innerHTML = `
        <span style="color: #ffd700;">${label}:</span>
        <span style="color: #00ff00;">${value}</span>
        <span style="color: #888; font-size: 9px;"> [${performance.now().toFixed(0)}ms]</span>
      `;

      this.debugPanel.insertBefore(entry, this.debugPanel.firstChild);

      // Keep only last 20 entries
      while (this.debugPanel.children.length > 20) {
        this.debugPanel.removeChild(this.debugPanel.lastChild);
      }
    },

    highlightElement: function(element, color = 'red', duration = 2000) {
      if (!element) return;

      const originalOutline = element.style.outline;
      element.style.outline = `3px solid ${color}`;

      setTimeout(() => {
        element.style.outline = originalOutline;
      }, duration);
    },

    addMonitorOverlay: function(monitorId, status) {
      const monitor = document.getElementById('liveStream' + monitorId);
      if (!monitor) return;

      let overlay = monitor.parentElement.querySelector('.debug-overlay');
      if (!overlay) {
        overlay = document.createElement('div');
        overlay.className = 'debug-overlay';
        overlay.style.cssText = `
          position: absolute;
          top: 0;
          left: 0;
          background: rgba(0, 0, 0, 0.7);
          color: white;
          padding: 5px;
          font-family: monospace;
          font-size: 10px;
          z-index: 1000;
          pointer-events: none;
        `;
        monitor.parentElement.style.position = 'relative';
        monitor.parentElement.appendChild(overlay);
      }

      overlay.textContent = `M${monitorId}: ${status}`;
    }
  };

  // ============================================================================
  // SCROLL TRACKING
  // ============================================================================

  const ScrollTracker = {
    lastScrollTime: 0,
    scrollCount: 0,
    scrollEndTimeout: null,
    isScrolling: false,

    init: function() {
      if (!DEBUG_CONFIG.ENABLE_SCROLL_TRACKING) return;

      LogSystem.log('SCROLL', 'Scroll tracker initialized', {
        hasScrollEnd: BrowserInfo.features.scrollEndEvent
      });

      // Track scroll start
      let scrollStartHandler = () => {
        if (!this.isScrolling) {
          this.isScrolling = true;
          LogSystem.log('SCROLL', 'Scroll started', {
            scrollY: window.scrollY,
            timestamp: performance.now()
          });
          VisualDebug.update('Scroll Status', 'SCROLLING');
        }
      };

      // Track scroll events
      let scrollHandler = (e) => {
        this.lastScrollTime = performance.now();
        this.scrollCount++;

        if (DEBUG_CONFIG.LOG_VERBOSE) {
          LogSystem.log('SCROLL', 'Scroll event', {
            scrollY: window.scrollY,
            deltaTime: this.lastScrollTime - LogSystem.startTime,
            eventType: e.type
          });
        }

        scrollStartHandler();

        // Debounced scroll end detection for browsers without scrollend
        clearTimeout(this.scrollEndTimeout);
        this.scrollEndTimeout = setTimeout(() => {
          this.onScrollEnd();
        }, 150);
      };

      // Native scrollend event (if available)
      if (BrowserInfo.features.scrollEndEvent) {
        document.addEventListener('scrollend', () => {
          LogSystem.log('SCROLL', 'Native scrollend event fired');
          this.onScrollEnd();
        });

        document.getElementById('content')?.addEventListener('scrollend', () => {
          LogSystem.log('SCROLL', 'Native scrollend event fired on #content');
          this.onScrollEnd();
        });
      } else {
        LogSystem.log('WARNING', 'scrollend event not supported - using fallback');
      }

      // Legacy scroll events
      document.addEventListener('scroll', scrollHandler);
      document.getElementById('content')?.addEventListener('scroll', scrollHandler);

      // Track resize (can trigger viewport changes)
      window.addEventListener('resize', () => {
        LogSystem.log('EVENT', 'Window resized', {
          width: window.innerWidth,
          height: window.innerHeight
        });
      });
    },

    onScrollEnd: function() {
      this.isScrolling = false;
      LogSystem.log('SCROLL', 'Scroll ended', {
        finalScrollY: window.scrollY,
        totalScrollEvents: this.scrollCount,
        timestamp: performance.now()
      });
      VisualDebug.update('Scroll Status', 'IDLE');
      VisualDebug.update('Scroll Count', this.scrollCount);

      // Log visible monitors
      this.logVisibleMonitors();
    },

    logVisibleMonitors: function() {
      if (typeof monitors === 'undefined') return;

      const visibleMonitors = [];
      const hiddenMonitors = [];

      for (let i = 0; i < monitors.length; i++) {
        const monitor = monitors[i];
        const element = monitor.getElement();
        if (!element) continue;

        const rect = element.getBoundingClientRect();
        const isVisible = (
          rect.top < window.innerHeight &&
          rect.bottom > 0 &&
          rect.left < window.innerWidth &&
          rect.right > 0
        );

        if (isVisible) {
          visibleMonitors.push(monitor.id);
        } else {
          hiddenMonitors.push(monitor.id);
        }
      }

      LogSystem.log('VIEWPORT', 'Monitor visibility after scroll', {
        visible: visibleMonitors,
        hidden: hiddenMonitors
      });

      VisualDebug.update('Visible Monitors', visibleMonitors.join(', '));
    }
  };

  // ============================================================================
  // VIDEO TRACKING
  // ============================================================================

  const VideoTracker = {
    videoStates: new Map(),

    init: function() {
      if (!DEBUG_CONFIG.ENABLE_VIDEO_TRACKING) return;

      LogSystem.log('VIDEO', 'Video tracker initialized');

      // Override MonitorStream.start if it exists
      this.hookMonitorStream();

      // Set up mutation observer to track new video elements
      this.observeVideoElements();
    },

    hookMonitorStream: function() {
      // Wait for MonitorStream to be defined
      const checkInterval = setInterval(() => {
        if (typeof MonitorStream !== 'undefined') {
          clearInterval(checkInterval);
          this.wrapMonitorStreamMethods();
        }
      }, 100);

      // Timeout after 10 seconds
      setTimeout(() => clearInterval(checkInterval), 10000);
    },

    wrapMonitorStreamMethods: function() {
      const originalStart = MonitorStream.prototype.start;
      MonitorStream.prototype.start = function(...args) {
        const startTime = performance.now();
        LogSystem.log('VIDEO', `Monitor ${this.id} starting`, {
          player: this.player,
          started: this.started,
          Go2RTCEnabled: this.Go2RTCEnabled,
          RTSP2WebEnabled: this.RTSP2WebEnabled,
          janusEnabled: this.janusEnabled
        });
        VisualDebug.addMonitorOverlay(this.id, 'STARTING');

        const result = originalStart.apply(this, args);

        const duration = performance.now() - startTime;
        LogSystem.log('VIDEO', `Monitor ${this.id} start completed`, {
          duration: duration.toFixed(2) + 'ms',
          activePlayer: this.activePlayer
        });
        VisualDebug.addMonitorOverlay(this.id, `STARTED (${this.activePlayer})`);

        return result;
      };

      const originalStop = MonitorStream.prototype.stop;
      MonitorStream.prototype.stop = function(...args) {
        LogSystem.log('VIDEO', `Monitor ${this.id} stopping`, {
          activePlayer: this.activePlayer
        });
        VisualDebug.addMonitorOverlay(this.id, 'STOPPING');

        const result = originalStop.apply(this, args);

        LogSystem.log('VIDEO', `Monitor ${this.id} stopped`);
        VisualDebug.addMonitorOverlay(this.id, 'STOPPED');

        return result;
      };

      LogSystem.log('VIDEO', 'MonitorStream methods wrapped successfully');
    },

    observeVideoElements: function() {
      const observer = new MutationObserver((mutations) => {
        mutations.forEach((mutation) => {
          mutation.addedNodes.forEach((node) => {
            if (node.nodeType === 1) { // Element node
              if (node.tagName === 'VIDEO' || node.tagName === 'VIDEO-STREAM' || node.tagName === 'IMG') {
                this.trackVideoElement(node);
              }
            }
          });
        });
      });

      observer.observe(document.body, {
        childList: true,
        subtree: true
      });
    },

    trackVideoElement: function(element) {
      LogSystem.log('VIDEO', `New ${element.tagName} element added`, {
        id: element.id,
        src: element.src,
        tagName: element.tagName
      });

      // Add event listeners
      const events = ['loadstart', 'loadeddata', 'loadedmetadata', 'canplay', 'canplaythrough',
                      'play', 'pause', 'error', 'stalled', 'waiting', 'playing'];

      events.forEach(eventName => {
        element.addEventListener(eventName, (e) => {
          LogSystem.log('VIDEO', `Video event: ${eventName}`, {
            id: element.id,
            currentTime: element.currentTime,
            readyState: element.readyState
          });
        });
      });
    }
  };

  // ============================================================================
  // TIMER TRACKING
  // ============================================================================

  const TimerTracker = {
    timers: new Map(),
    intervals: new Map(),

    init: function() {
      if (!DEBUG_CONFIG.ENABLE_TIMER_TRACKING) return;

      // Wrap setTimeout
      const originalSetTimeout = window.setTimeout;
      window.setTimeout = function(callback, delay, ...args) {
        const timerId = originalSetTimeout.call(window, function() {
          if (DEBUG_CONFIG.LOG_VERBOSE) {
            LogSystem.log('TIMER', 'setTimeout fired', {
              delay: delay,
              actualDelay: performance.now() - startTime
            });
          }
          TimerTracker.timers.delete(timerId);
          return callback.apply(this, args);
        }, delay, ...args);

        const startTime = performance.now();
        TimerTracker.timers.set(timerId, {
          type: 'timeout',
          delay: delay,
          startTime: startTime,
          stack: new Error().stack
        });

        return timerId;
      };

      // Wrap setInterval
      const originalSetInterval = window.setInterval;
      window.setInterval = function(callback, delay, ...args) {
        let count = 0;
        const intervalId = originalSetInterval.call(window, function() {
          count++;
          if (DEBUG_CONFIG.LOG_VERBOSE && count % 10 === 0) {
            LogSystem.log('TIMER', `setInterval fired (${count} times)`, {
              delay: delay,
              count: count
            });
          }
          return callback.apply(this, args);
        }, delay, ...args);

        TimerTracker.intervals.set(intervalId, {
          type: 'interval',
          delay: delay,
          startTime: performance.now(),
          count: 0
        });

        return intervalId;
      };

      LogSystem.log('TIMER', 'Timer tracking initialized');
    }
  };

  // ============================================================================
  // VIEWPORT MONITORING
  // ============================================================================

  const ViewportMonitor = {
    init: function() {
      // Wrap isOutOfViewport function when it's available
      const checkInterval = setInterval(() => {
        if (typeof isOutOfViewport !== 'undefined') {
          clearInterval(checkInterval);
          this.wrapIsOutOfViewport();
        }
      }, 100);

      setTimeout(() => clearInterval(checkInterval), 10000);
    },

    wrapIsOutOfViewport: function() {
      const originalIsOutOfViewport = window.isOutOfViewport;
      window.isOutOfViewport = function(elem) {
        const result = originalIsOutOfViewport(elem);

        if (DEBUG_CONFIG.LOG_VERBOSE) {
          LogSystem.log('VIEWPORT', 'isOutOfViewport check', {
            elementId: elem.id,
            result: result,
            rect: elem.getBoundingClientRect()
          });
        }

        return result;
      };

      LogSystem.log('VIEWPORT', 'isOutOfViewport function wrapped');
    }
  };

  // ============================================================================
  // PERFORMANCE MONITORING
  // ============================================================================

  const PerformanceMonitor = {
    marks: new Map(),

    mark: function(name) {
      const time = performance.now();
      this.marks.set(name, time);
      LogSystem.log('PERFORMANCE', `Mark: ${name}`, {time: time.toFixed(2)});
    },

    measure: function(name, startMark) {
      const endTime = performance.now();
      const startTime = this.marks.get(startMark);
      if (startTime) {
        const duration = endTime - startTime;
        LogSystem.log('PERFORMANCE', `Measure: ${name}`, {
          duration: duration.toFixed(2) + 'ms',
          start: startTime.toFixed(2),
          end: endTime.toFixed(2)
        });
        return duration;
      }
    }
  };

  // ============================================================================
  // INITIALIZATION
  // ============================================================================

  function init() {
    LogSystem.log('BROWSER', 'Safari Montage Debug initialized', {
      browserInfo: BrowserInfo,
      config: DEBUG_CONFIG
    });

    VisualDebug.init();
    ScrollTracker.init();
    VideoTracker.init();
    TimerTracker.init();
    ViewportMonitor.init();

    // Add download button to debug panel
    if (DEBUG_CONFIG.ENABLE_VISUAL && VisualDebug.debugPanel) {
      const downloadBtn = document.createElement('button');
      downloadBtn.textContent = 'Download Logs';
      downloadBtn.style.cssText = `
        background: #3498db;
        color: white;
        border: none;
        padding: 5px 10px;
        cursor: pointer;
        border-radius: 3px;
        margin-top: 10px;
        width: 100%;
      `;
      downloadBtn.onclick = () => LogSystem.downloadLogs();
      VisualDebug.debugPanel.appendChild(downloadBtn);
    }

    // Expose API to window for manual testing
    window.SafariDebug = {
      LogSystem: LogSystem,
      VisualDebug: VisualDebug,
      BrowserInfo: BrowserInfo,
      downloadLogs: () => LogSystem.downloadLogs(),
      compareLogs: (safari, chrome) => LogSystem.compareLogs(safari, chrome),
      getStoredLogs: () => localStorage.getItem('zm_safari_debug_logs')
    };

    LogSystem.log('BROWSER', 'Debug API exposed to window.SafariDebug');
  }

  // Start when DOM is ready
  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', init);
  } else {
    init();
  }

})();
