# ZoneMinder Security Audit Report

**Date:** 2026-02-08
**Scope:** Full codebase review (C++, PHP, JavaScript, Perl)
**Methodology:** Manual static analysis across OWASP Top 10 vulnerability categories

---

## Executive Summary

This audit identified **21 security findings** across 7 categories in the ZoneMinder codebase. The most critical issues involve SQL injection in password migration, command injection via unescaped shell arguments, weak authentication hashing (MD5), and plaintext password storage in sessions. Several findings are mitigable by existing access controls (authentication required), but represent defense-in-depth failures.

| Severity | Count |
|----------|-------|
| Critical | 4     |
| High     | 7     |
| Medium   | 7     |
| Low      | 3     |

---

## Finding 1: SQL Injection in Password Migration

**Severity:** Critical
**File:** `web/includes/auth.php:52`
**CWE:** CWE-89 (SQL Injection)

```php
function migrateHash($user, $pass) {
    $bcrypt_hash = password_hash($pass, PASSWORD_BCRYPT);
    $update_password_sql = 'UPDATE Users SET Password=\''.$bcrypt_hash.'\' WHERE Username=\''.$user.'\'';
    dbQuery($update_password_sql);
}
```

**Issue:** The `$user` (username) variable is concatenated directly into the SQL query without parameterization or escaping. While `$bcrypt_hash` output from `password_hash()` has a constrained character set, the username originates from `$_REQUEST['username']` and is not sanitized before reaching this function.

**Impact:** An authenticated attacker could craft a username containing SQL injection payloads during login. Since this runs after successful password validation, the attack window is narrow but real — a user with a malicious username could modify arbitrary database rows.

**Recommendation:** Use parameterized query:
```php
dbQuery('UPDATE Users SET Password=? WHERE Username=?', array($bcrypt_hash, $user));
```

---

## Finding 2: Command Injection via `wget()` Function

**Severity:** Critical
**File:** `web/includes/monitor_probe.php:256`
**CWE:** CWE-78 (OS Command Injection)

```php
function wget($method, $url, $username, $password) {
    exec("wget --keep-session-cookies -O - $url", $output, $result_code);
    return implode("\n", $output);
}
```

**Issue:** The `$url` parameter is inserted directly into a shell command without any escaping (`escapeshellarg()` or `escapeshellcmd()`). If an attacker controls the URL (e.g., via monitor probe configuration), they can inject arbitrary shell commands.

**Attack example:** URL value `http://example.com; rm -rf /` would execute the destructive command.

**Impact:** Remote code execution as the web server user.

**Recommendation:** Use `escapeshellarg()` on the URL parameter, or better yet, use PHP's native curl functions exclusively instead of shelling out to wget.

---

## Finding 3: Command Injection in `daemonControl()`

**Severity:** High
**File:** `web/includes/functions.php:708-720`
**CWE:** CWE-78 (OS Command Injection)

```php
function daemonControl($command, $daemon=false, $args=false) {
    $string = escapeshellcmd(ZM_PATH_BIN).'/zmdc.pl '.$command;
    if ($daemon) {
        $string .= ' ' . $daemon;
        if ($args) {
            $string .= ' ' . $args;
        }
    }
    $string = escapeshellcmd($string);
    exec($string);
}
```

**Issue:** The `$command`, `$daemon`, and `$args` parameters are concatenated into the command string before `escapeshellcmd()` is applied to the whole string. `escapeshellcmd()` escapes shell metacharacters but does NOT prevent argument injection — an attacker can still inject additional arguments. Additionally, individual arguments are not wrapped with `escapeshellarg()`.

**Impact:** Argument injection allowing manipulation of zmdc.pl behavior. While this requires authenticated access with appropriate permissions, it could lead to privilege escalation.

**Recommendation:** Use `escapeshellarg()` on each individual parameter:
```php
$string = escapeshellarg(ZM_PATH_BIN.'/zmdc.pl') . ' ' . escapeshellarg($command);
```

---

## Finding 4: Weak Authentication Hash (MD5)

**Severity:** Critical
**File:** `web/includes/auth.php:190-191, 229-231` and `src/zm_user.cpp:167-179`
**CWE:** CWE-328 (Use of Weak Hash)

```php
// PHP
$authKey = ZM_AUTH_HASH_SECRET.$user['Username'].$user['Password']
           .$remoteAddr.$time[2].$time[3].$time[4].$time[5];
$authHash = md5($authKey);
```

```cpp
// C++
zm::crypto::MD5::Digest md5_digest = zm::crypto::MD5::GetDigestOf(auth_key);
```

**Issue:** The authentication hash relay system uses MD5 to generate authentication tokens. MD5 is cryptographically broken — collision attacks are practical, and the time-based components (hour, day, month, year) provide a very limited keyspace. An attacker who knows or guesses `ZM_AUTH_HASH_SECRET` can forge authentication hashes. The hash is also only rotated hourly (controlled by `ZM_AUTH_HASH_TTL`).

**Impact:** Authentication bypass if the hash secret is compromised or brute-forced. The limited time granularity (hourly) means the same hash is valid for extended periods.

**Recommendation:** Replace MD5 with HMAC-SHA256 for auth hash generation. Consider using shorter TTLs and adding a random nonce.

---

## Finding 5: Plaintext Password Stored in Session

**Severity:** Critical
**File:** `web/includes/auth.php:501-503`
**CWE:** CWE-256 (Plaintext Storage of a Password)

```php
if (ZM_AUTH_RELAY == 'plain') {
    $_SESSION['password'] = $_REQUEST['password'];
}
```

**Issue:** When `ZM_AUTH_RELAY` is set to `'plain'`, the user's plaintext password is stored in the PHP session. Session data is typically written to disk (e.g., `/tmp/sess_*`), making plaintext passwords recoverable by anyone with filesystem access.

**Impact:** Password disclosure to local attackers, other processes on the same server, or via session file backup/leak.

**Recommendation:** Never store plaintext passwords in sessions. Use the hashed auth relay mode exclusively, or store a session-specific token instead.

---

## Finding 6: Auth Relay Transmits Credentials in URL

**Severity:** High
**File:** `web/includes/auth.php:389-391`
**CWE:** CWE-598 (Use of GET Request Method with Sensitive Query Strings)

```php
} else if (ZM_AUTH_RELAY == 'plain') {
    return 'username='.(isset($_SESSION['username'])?$_SESSION['username']:'')
           .'&password='.urlencode(isset($_SESSION['password']) ? $_SESSION['password'] : '');
}
```

**Issue:** In `plain` auth relay mode, the username and password are transmitted as URL query parameters. URLs are logged in web server access logs, browser history, proxy logs, and referer headers.

**Impact:** Credential exposure through multiple logging vectors.

**Recommendation:** Deprecate and remove the `plain` auth relay mode. Use only session-based or JWT token authentication.

---

## Finding 7: XSS in Filter Debug Modal

**Severity:** High
**File:** `web/ajax/modals/filterdebug.php:19`
**CWE:** CWE-79 (Cross-Site Scripting)

```php
echo '<div class="error">Filter not found for id '.$_REQUEST['fid'].'</div>';
```

**Issue:** The `$_REQUEST['fid']` value is reflected directly into HTML output without encoding. Although `validInt()` is called on line 13, the raw `$_REQUEST['fid']` is used on line 19 instead of the validated value when the filter is not found.

**Impact:** Reflected XSS — an attacker can craft a URL that executes JavaScript in the victim's browser session, potentially stealing session cookies or performing actions as the victim.

**Recommendation:** Use `htmlspecialchars($_REQUEST['fid'])` or use the already-validated `$fid` variable instead.

---

## Finding 8: Password Logged in Debug Output

**Severity:** High
**File:** `src/zms.cpp:223-226`
**CWE:** CWE-532 (Insertion of Sensitive Information into Log File)

```cpp
} else if (!strcmp(name, "pass")) {
    password = UriDecode(value);
    Debug(1, "Have %s for password", password.c_str());
} else if (!strcmp(name, "password")) {
    password = UriDecode(value);
    Debug(1, "Have %s for password", password.c_str());
}
```

**Issue:** The plaintext password is logged via `Debug()` when debug logging is enabled. ZMS (streaming server) processes QUERY_STRING from CGI environment, meaning passwords arrive via GET parameters and are also logged in web server access logs.

**Impact:** Password disclosure through log files. Debug logs may persist on disk and be accessible to other administrators or through log aggregation systems.

**Recommendation:** Remove password logging. Log only that a password was received, not its value:
```cpp
Debug(1, "Password parameter received");
```

---

## Finding 9: Debug Logging of Full Request Data

**Severity:** Medium
**File:** `web/index.php:33`
**CWE:** CWE-532 (Insertion of Sensitive Information into Log File)

```php
ZM\Debug(print_r($_REQUEST, true));
```

**Issue:** The entire `$_REQUEST` superglobal is dumped to the debug log on every request. This includes passwords, tokens, auth hashes, and any other sensitive form data submitted by users.

**Impact:** Credentials and tokens leaked to log files whenever debug logging is enabled.

**Recommendation:** Filter sensitive fields before logging, or remove this blanket dump entirely.

---

## Finding 10: Insecure `deletePath()` with `escapeshellcmd()`

**Severity:** High
**File:** `web/includes/functions.php:324`
**CWE:** CWE-78 (OS Command Injection)

```php
if (false === ($output = system('rm -rf "'.escapeshellcmd($path).'"'))) {
```

**Issue:** `escapeshellcmd()` is used instead of `escapeshellarg()`. `escapeshellcmd()` does not escape quotes within the string, meaning a path containing double-quote characters can break out of the quoted context and inject commands. For example, a path like `foo" ; malicious_command ; echo "` would escape the quotes.

**Impact:** If an attacker can influence the `$path` parameter (e.g., through manipulated event paths or storage paths stored in the database), they can execute arbitrary commands.

**Recommendation:** Use `escapeshellarg()`:
```php
system('rm -rf ' . escapeshellarg($path));
```

---

## Finding 11: `getFormChanges()` Column Name Injection

**Severity:** Medium
**File:** `web/includes/functions.php:500-582`
**CWE:** CWE-89 (SQL Injection)

```php
foreach ($newValues as $key=>$value) {
    // ...
    $changes[$key] = "`$key` = ".dbEscape(trim($value));
    // $changes[$key] = "`$key` = NULL";
}
```

**Issue:** The `$key` variable (derived from user-supplied form field names via `$_REQUEST['newMonitor']`, `$_REQUEST['newZone']`, etc.) is interpolated directly into SQL column names with only backtick quoting. While values are properly escaped via `dbEscape()`, the column names are not validated against an allowlist of actual database columns. An attacker could submit form data with crafted key names containing backtick-escape sequences.

**Impact:** Potential SQL injection through crafted form field names, though backtick quoting provides partial protection.

**Recommendation:** Validate `$key` against an allowlist of known column names for the target table.

---

## Finding 12: Open Redirect via `$_REQUEST['REFERER']`

**Severity:** Medium
**File:** `web/includes/actions/zone.php:70`
**CWE:** CWE-601 (Open Redirect)

```php
$redirect = isset($_REQUEST['REFERER']) ? $_REQUEST['REFERER'] : '?view=zones';
```

**Issue:** The `$_REQUEST['REFERER']` value is used directly as a redirect destination without validation. An attacker can craft a URL that redirects authenticated users to a malicious site after performing a zone action.

**Impact:** Phishing attacks leveraging trusted ZoneMinder URLs to redirect to malicious sites.

**Recommendation:** Validate that the redirect URL is relative (starts with `?`) and does not contain external URLs.

---

## Finding 13: `packageControl()` with User-Supplied State Name

**Severity:** High
**File:** `web/includes/actions/state.php:28` and `web/includes/functions.php:702-705`
**CWE:** CWE-78 (OS Command Injection)

```php
// state.php
packageControl($_REQUEST['runState']);

// functions.php
function packageControl($command) {
    $string = ZM_PATH_BIN.'/zmpkg.pl '.escapeshellarg($command);
    exec($string);
}
```

**Issue:** While `packageControl()` does use `escapeshellarg()` (which is good), it passes the raw `$_REQUEST['runState']` value as a command to `zmpkg.pl`. The Perl script `zmpkg.pl` then interprets this command. If `zmpkg.pl` processes the argument insecurely (e.g., passing to further shell commands), this could be exploited. Additionally, there's no validation that `runState` is one of the expected values (start, stop, restart, a saved state name).

**Impact:** Depends on `zmpkg.pl` implementation — potential command injection through the Perl layer.

**Recommendation:** Validate `$_REQUEST['runState']` against known state names from the database before passing to `packageControl()`.

---

## Finding 14: `dnsmasq` Configuration Injection

**Severity:** High
**File:** `web/includes/actions/options.php:117-152`
**CWE:** CWE-94 (Code Injection)

```php
$config = isset($_REQUEST['config']) ? $_REQUEST['config'] : [];
$conf = '';
foreach ($config as $name=>$value) {
    if ($name == 'dhcp-host') {
        foreach ($value as $mac=>$ip) {
            $conf .= $name.'='.$mac.','.$ip.PHP_EOL;
        }
    // ...
    } else {
        $conf .= $name.'='.$value.PHP_EOL;
    }
}
file_put_contents(ZM_PATH_DNSMASQ_CONF, $conf);
exec('sudo -n /bin/systemctl restart dnsmasq.service');
```

**Issue:** User-supplied configuration values are written directly to the dnsmasq configuration file without sanitization. An attacker with System edit permissions could inject arbitrary dnsmasq configuration directives, including `dhcp-script` which executes arbitrary commands when DHCP events occur.

**Impact:** Remote code execution as root (dnsmasq typically runs as root or with elevated privileges).

**Recommendation:** Validate and sanitize all configuration values. Use an allowlist of permitted dnsmasq directives.

---

## Finding 15: SSL Certificate Verification Disabled

**Severity:** Medium
**File:** `web/includes/monitor_probe.php:270`
**CWE:** CWE-295 (Improper Certificate Validation)

```php
curl_setopt($ch, CURLOPT_SSL_VERIFYPEER, false);
```

**Issue:** SSL peer certificate verification is disabled in the curl-based monitor probe function. This allows man-in-the-middle attacks when probing HTTPS camera URLs.

**Impact:** An attacker on the network path can intercept camera credentials and video streams during probe operations.

**Recommendation:** Enable certificate verification by default and provide a per-monitor option to disable it for self-signed certificates.

---

## Finding 16: Session Fixation via `HTTP_X_FORWARDED_FOR`

**Severity:** Medium
**File:** `web/includes/session.php:51`
**CWE:** CWE-384 (Session Fixation)

```php
$_SESSION['remoteAddr'] = (!empty($_SERVER['HTTP_X_FORWARDED_FOR'])
    ? $_SERVER['HTTP_X_FORWARDED_FOR']
    : $_SERVER['REMOTE_ADDR']);
```

**Issue:** The `HTTP_X_FORWARDED_FOR` header is client-controlled and can be spoofed. Using it to set `remoteAddr` in the session undermines IP-based authentication protections (used by `ZM_AUTH_HASH_IPS`). An attacker can set this header to match a legitimate user's IP address.

**Impact:** Bypasses IP-based session validation when ZoneMinder is not behind a trusted reverse proxy.

**Recommendation:** Only trust `X-Forwarded-For` when configured behind a known reverse proxy. Add a configuration option to specify trusted proxy addresses.

---

## Finding 17: Missing `Secure` Flag on Cookies

**Severity:** Medium
**File:** `web/includes/session.php:3-18`
**CWE:** CWE-614 (Sensitive Cookie in HTTPS Session Without 'Secure' Attribute)

```php
function zm_setcookie($cookie, $value, $options=array()) {
    if (!isset($options['path'])) $options['path'] = '/';
    if (!isset($options['expires'])) $options['expires'] = time()+3600*24*30*12*10;
    if (!isset($options['samesite'])) $options['samesite'] = 'Strict';
    // No 'secure' flag set
}
```

**Issue:** The `zm_setcookie()` wrapper does not set the `Secure` flag. Cookies (including session-related ones) will be transmitted over HTTP connections, making them interceptable.

**Impact:** Session hijacking via network sniffing when users access ZoneMinder over HTTP (even briefly).

**Recommendation:** Add `$options['secure'] = true` when HTTPS is detected.

---

## Finding 18: C++ Buffer Overflow Risk in `zm_zone.cpp`

**Severity:** Medium
**File:** `src/zm_zone.cpp:955-985`
**CWE:** CWE-120 (Buffer Copy without Checking Size of Input)

```cpp
sprintf(output+strlen(output), "  Id : %u\n", id);
sprintf(output+strlen(output), "  Label : %s\n", label.c_str());
// ... 15+ more sprintf calls appending to output
```

**Issue:** Multiple `sprintf()` calls append to a buffer (`output`) using `strlen(output)` offset without checking remaining buffer capacity. If the `label` string or accumulated output exceeds the buffer size, a heap/stack buffer overflow occurs.

**Impact:** Memory corruption potentially leading to code execution. The `label` is loaded from the database and could be attacker-controlled.

**Recommendation:** Replace with `snprintf()` and track remaining buffer space, or use `std::string` concatenation.

---

## Finding 19: `strtok()` Usage in `zms.cpp` (Thread Safety)

**Severity:** Low
**File:** `src/zms.cpp:114-121`
**CWE:** CWE-362 (Race Condition)

```cpp
char *q_ptr = (char *)query;
while ((parm_no < 16) && (parms[parm_no] = strtok(q_ptr, "&"))) {
    parm_no++;
    q_ptr = nullptr;
}
for (int p = 0; p < parm_no; p++) {
    char *name = strtok(parms[p], "=");
```

**Issue:** `strtok()` is not thread-safe and uses internal static state. Additionally, `strtok()` modifies the input string (the `QUERY_STRING` environment variable) and the fixed array of 16 parameters could be exceeded by crafted query strings (though the loop check prevents overflow).

**Impact:** Low — ZMS typically runs as a separate CGI process, so thread safety is not the primary concern, but the mutation of environment data is poor practice.

**Recommendation:** Use `strtok_r()` for thread safety, or better yet, use `std::string` parsing.

---

## Finding 20: Plaintext Password Comparison

**Severity:** Low
**File:** `web/includes/auth.php:101-102`
**CWE:** CWE-256 (Plaintext Storage of a Password)

```php
default:
    ZM\Warning('assuming plain text password...');
    $password_correct = ($user['Password'] == $password);
```

**Issue:** When a password doesn't match bcrypt or MySQL hash signatures, it falls through to plaintext comparison. This means passwords stored as plaintext in the database are supported, and the comparison uses `==` (not timing-safe).

**Impact:** Plaintext passwords in the database are a severe risk if the database is compromised. Non-constant-time comparison enables timing attacks.

**Recommendation:** Reject plaintext passwords. Force migration to bcrypt on first login attempt. Use `hash_equals()` for any string comparison involving secrets.

---

## Finding 21: Auth Hash Leaked in Debug Logs

**Severity:** Low
**File:** `src/zm_user.cpp:176` and `src/zm_user.cpp:322`
**CWE:** CWE-532 (Insertion of Sensitive Information into Log File)

```cpp
Debug(1, "Creating auth_key '%s'", auth_key.c_str());
// auth_key contains: secret + username + password hash + IP + time components

Debug(1, "Checking auth_key '%s' -> auth_md5 '%s' == '%s'",
      auth_key.c_str(), auth_md5.c_str(), auth.c_str());
```

**Issue:** The auth key (which contains the `ZM_AUTH_HASH_SECRET`, the username, and the password hash) is logged at debug level 1. Anyone with access to debug logs can extract the hash secret and forge authentication tokens.

**Impact:** Complete authentication bypass if debug logs are accessible.

**Recommendation:** Never log the auth key or hash secret. Log only non-sensitive identifiers.

---

## Remediation Priority

### Immediate (Critical):
1. **Finding 1** — SQL injection in `migrateHash()` — use parameterized query
2. **Finding 2** — Command injection in `wget()` — use `escapeshellarg()` or native PHP curl
3. **Finding 4** — MD5 auth hash — migrate to HMAC-SHA256
4. **Finding 5** — Plaintext password in session — remove plain auth relay

### Short-term (High):
5. **Finding 3** — Command injection in `daemonControl()` — use `escapeshellarg()` per argument
6. **Finding 6** — Credentials in URL — deprecate plain auth relay
7. **Finding 7** — XSS in filterdebug — use `htmlspecialchars()`
8. **Finding 8** — Password in debug logs — remove password logging
9. **Finding 10** — `deletePath()` injection — use `escapeshellarg()`
10. **Finding 13** — State command injection — validate against allowlist
11. **Finding 14** — dnsmasq config injection — sanitize configuration values

### Medium-term (Medium):
12. **Finding 9** — Full request dump in logs
13. **Finding 11** — Column name injection via `getFormChanges()`
14. **Finding 12** — Open redirect
15. **Finding 15** — SSL verification disabled
16. **Finding 16** — `X-Forwarded-For` trust
17. **Finding 17** — Missing `Secure` cookie flag
18. **Finding 18** — Buffer overflow in `zm_zone.cpp`

### Long-term (Low):
19. **Finding 19** — `strtok()` usage
20. **Finding 20** — Plaintext password support
21. **Finding 21** — Auth key in debug logs
