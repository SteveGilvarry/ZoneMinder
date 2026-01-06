# ZoneMinder Docker Development Environment

This setup provides a complete ZoneMinder development environment optimized for **Apple Silicon (M4/ARM64)** Macs, with support for live PHP/UI development and testing.

## 🎯 Features

- ✅ **ARM64 Native** - Optimized for Apple Silicon (M1/M2/M3/M4)
- ✅ **Live Reload** - PHP changes reflect immediately (no rebuild needed)
- ✅ **Full Stack** - MySQL, Apache, PHP, ZoneMinder all configured
- ✅ **PHPMyAdmin** - Easy database management at http://localhost:8081
- ✅ **Development Mode** - PHP errors displayed, debugging enabled

## 📋 Prerequisites

1. **Docker Desktop for Mac** - [Download here](https://www.docker.com/products/docker-desktop/)
   - Ensure Docker Desktop is configured for Apple Silicon
   - Allocate at least 4GB RAM to Docker

2. **Git Submodules** - Initialize if not already done:
   ```bash
   git submodule update --init --recursive
   ```

## 🚀 Quick Start

### 1. Start the Development Environment

```bash
# Build and start all services (first time will take 10-15 minutes)
docker-compose -f docker-compose.dev.yml up --build

# Or run in background:
docker-compose -f docker-compose.dev.yml up -d --build
```

### 2. Access ZoneMinder

- **Web UI**: http://localhost:8080
- **PHPMyAdmin**: http://localhost:8081
  - Username: `zmuser`
  - Password: `zmpass`

### 3. Make PHP Changes

Simply edit files in the `./web` directory - changes will be reflected immediately!

```bash
# Example: Edit the main index page
vim web/index.php

# Refresh browser - changes are live!
```

## 🛠️ Development Workflow

### Viewing Logs

```bash
# View all logs
docker-compose -f docker-compose.dev.yml logs -f

# View specific service logs
docker-compose -f docker-compose.dev.yml logs -f zoneminder
docker-compose -f docker-compose.dev.yml logs -f db

# View Apache error logs
docker exec -it zm-app tail -f /var/log/apache2/error.log

# View ZoneMinder logs
docker exec -it zm-app tail -f /var/log/zm/zmpkg.log
```

### Restarting Services

```bash
# Restart ZoneMinder daemon (after config changes)
docker exec -it zm-app /usr/bin/zmpkg.pl restart

# Restart Apache (after PHP config changes)
docker exec -it zm-app apache2ctl restart

# Restart all containers
docker-compose -f docker-compose.dev.yml restart
```

### Database Access

```bash
# Via PHPMyAdmin
open http://localhost:8081

# Via command line
docker exec -it zm-mysql mysql -uzmuser -pzmpass zm
```

### Accessing Container Shell

```bash
# ZoneMinder container
docker exec -it zm-app bash

# MySQL container
docker exec -it zm-mysql bash
```

## 🧪 Testing PHP Changes

### Example: Testing UI Changes

1. **Edit a PHP file**:
   ```bash
   vim web/skins/classic/views/console.php
   ```

2. **Refresh browser** - Changes appear immediately!

3. **Check for PHP errors**:
   ```bash
   docker exec -it zm-app tail -f /var/log/apache2/error.log
   ```

### Example: Testing API Changes

1. **Edit API code**:
   ```bash
   vim web/api/app/Controller/HostController.php
   ```

2. **Test API endpoint**:
   ```bash
   curl http://localhost:8080/api/host/getVersion.json
   ```

3. **Check API logs**:
   ```bash
   docker exec -it zm-app tail -f /var/log/apache2/error.log
   ```

## 🔧 Advanced Configuration

### Changing PHP Settings

Edit `/etc/php/8.1/apache2/php.ini` in the container, or add to Dockerfile:

```dockerfile
RUN sed -i 's/^upload_max_filesize = .*/upload_max_filesize = 100M/' /etc/php/8.1/apache2/php.ini
```

### Adding PHP Extensions

Add to the Dockerfile's apt-get install line:

```dockerfile
php8.1-redis \
php8.1-memcached \
```

### Customizing MySQL

Edit `docker/mysql/zm.cnf` and restart the database:

```bash
docker-compose -f docker-compose.dev.yml restart db
```

## 🐛 Troubleshooting

### Container won't start

```bash
# Check logs
docker-compose -f docker-compose.dev.yml logs

# Clean rebuild
docker-compose -f docker-compose.dev.yml down -v
docker-compose -f docker-compose.dev.yml up --build
```

### Database connection errors

```bash
# Check MySQL is running
docker-compose -f docker-compose.dev.yml ps

# Check database credentials
docker exec -it zm-mysql mysql -uzmuser -pzmpass -e "SELECT 1"
```

### Changes not appearing

```bash
# Restart Apache
docker exec -it zm-app apache2ctl restart

# Check file permissions
docker exec -it zm-app ls -la /usr/share/zoneminder/www/
```

### Performance issues on Apple Silicon

- The first build takes time as it compiles native ARM64 binaries
- Subsequent starts are fast (<30 seconds)
- If using platform emulation (amd64), expect 30-50% slower performance

## 📦 Managing the Environment

### Stop Services

```bash
# Stop containers (keep data)
docker-compose -f docker-compose.dev.yml stop

# Stop and remove containers (keep data)
docker-compose -f docker-compose.dev.yml down

# Stop and remove everything (including data volumes)
docker-compose -f docker-compose.dev.yml down -v
```

### Rebuild After Code Changes

```bash
# Only needed if you modify C++ code or Dockerfile
docker-compose -f docker-compose.dev.yml build --no-cache
docker-compose -f docker-compose.dev.yml up
```

### Clean Up Disk Space

```bash
# Remove unused Docker resources
docker system prune -a

# Remove only this project's volumes
docker-compose -f docker-compose.dev.yml down -v
```

## 📊 Performance Optimization

### For Apple Silicon Users

The provided setup is already optimized for ARM64:
- Native ARM64 base images for MySQL and OS
- Native compilation of ZoneMinder for ARM64
- No emulation overhead

### Allocate More Resources

In Docker Desktop:
- **Settings** → **Resources**
- Increase **CPUs** to 4+
- Increase **Memory** to 6GB+
- Increase **Disk** if storing many events

## 🎨 For Claude Code: Testing UI Changes

When Claude makes PHP/UI changes, this workflow enables:

1. **Instant feedback** - Edit files, refresh browser
2. **Screenshot testing** - Use headless Chrome/Puppeteer:
   ```bash
   docker run --rm --network zm_zm-network -v $(pwd):/workspace \
     browserless/chrome:latest \
     node /workspace/test-screenshot.js
   ```
3. **Visual verification** - Claude can view screenshots of the rendered UI
4. **API testing** - Test endpoints with curl/WebFetch
5. **Log inspection** - Check for PHP errors automatically

## 🔐 Default Credentials

- **ZoneMinder Web UI**:
  - Username: `admin`
  - Password: `admin` (set during first login)

- **Database**:
  - Host: `db` (from within Docker network)
  - Database: `zm`
  - Username: `zmuser`
  - Password: `zmpass`

- **PHPMyAdmin**:
  - Username: `zmuser`
  - Password: `zmpass`

## 📝 Notes

- The `./web` directory is mounted directly into the container
- Changes to PHP files are **immediate** (no rebuild)
- Changes to C++ code require `docker-compose build`
- Database is persisted in a Docker volume
- Events and images are stored in Docker volumes (not host)

## 🆘 Getting Help

- Check logs: `docker-compose -f docker-compose.dev.yml logs -f`
- Inspect container: `docker exec -it zm-app bash`
- ZoneMinder docs: https://zoneminder.readthedocs.io/
- Docker docs: https://docs.docker.com/

---

**Happy Developing! 🎉**
