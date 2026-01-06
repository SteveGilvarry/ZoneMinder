/**
 * Headless browser screenshot utility for ZoneMinder UI testing
 * This script can be used by Claude to capture and verify UI changes
 */

const puppeteer = require('puppeteer');
const fs = require('fs');
const path = require('path');

(async () => {
  const browser = await puppeteer.launch({
    headless: true,
    args: [
      '--no-sandbox',
      '--disable-setuid-sandbox',
      '--disable-dev-shm-usage',
      '--disable-gpu'
    ]
  });

  const page = await browser.newPage();

  // Set viewport for consistent screenshots
  await page.setViewport({ width: 1920, height: 1080 });

  try {
    // Get URL from environment or default to localhost
    const baseUrl = process.env.ZM_URL || 'http://zoneminder';

    console.log(`Navigating to ${baseUrl}...`);
    await page.goto(baseUrl, { waitUntil: 'networkidle2', timeout: 30000 });

    // Create screenshots directory if it doesn't exist
    const screenshotDir = '/workspace/screenshots';
    if (!fs.existsSync(screenshotDir)) {
      fs.mkdirSync(screenshotDir, { recursive: true });
    }

    // Take full page screenshot
    const timestamp = new Date().toISOString().replace(/[:.]/g, '-');
    const screenshotPath = path.join(screenshotDir, `zm-ui-${timestamp}.png`);

    await page.screenshot({
      path: screenshotPath,
      fullPage: true
    });

    console.log(`Screenshot saved to: ${screenshotPath}`);

    // Get page title and URL for verification
    const title = await page.title();
    const url = await page.url();

    console.log(`Page Title: ${title}`);
    console.log(`Current URL: ${url}`);

    // Check for JavaScript errors
    const errors = [];
    page.on('pageerror', error => {
      errors.push(error.message);
    });

    // Wait a bit to catch any async errors
    await page.waitForTimeout(2000);

    if (errors.length > 0) {
      console.log('\nJavaScript Errors Detected:');
      errors.forEach(err => console.log(`  - ${err}`));
    } else {
      console.log('\nNo JavaScript errors detected ✓');
    }

    // Get some basic page info
    const info = await page.evaluate(() => {
      return {
        hasBody: !!document.body,
        bodyText: document.body?.innerText?.substring(0, 200),
        headings: Array.from(document.querySelectorAll('h1, h2, h3')).map(h => h.innerText),
        formCount: document.querySelectorAll('form').length,
        linkCount: document.querySelectorAll('a').length
      };
    });

    console.log('\nPage Info:');
    console.log(`  Forms: ${info.formCount}`);
    console.log(`  Links: ${info.linkCount}`);
    console.log(`  Headings: ${info.headings.join(', ')}`);

  } catch (error) {
    console.error('Error capturing screenshot:', error.message);
    process.exit(1);
  } finally {
    await browser.close();
  }
})();
