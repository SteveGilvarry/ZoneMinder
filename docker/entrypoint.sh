#!/bin/bash
set -e

echo "ZoneMinder Development Container Starting..."

# Wait for MySQL to be ready
echo "Waiting for MySQL to be ready..."
max_attempts=30
attempt=0
until mysql -h"${ZM_DB_HOST}" -u"${ZM_DB_USER}" -p"${ZM_DB_PASS}" -e "SELECT 1" &>/dev/null; do
    attempt=$((attempt + 1))
    if [ $attempt -ge $max_attempts ]; then
        echo "ERROR: MySQL is not available after $max_attempts attempts"
        exit 1
    fi
    echo "MySQL not ready yet, waiting... (attempt $attempt/$max_attempts)"
    sleep 2
done

echo "MySQL is ready!"

# Check if database is initialized
if ! mysql -h"${ZM_DB_HOST}" -u"${ZM_DB_USER}" -p"${ZM_DB_PASS}" "${ZM_DB_NAME}" -e "SHOW TABLES LIKE 'Config'" | grep -q Config; then
    echo "Initializing ZoneMinder database..."

    # Create database structure
    if [ -f /usr/share/zoneminder/db/zm_create.sql ]; then
        mysql -h"${ZM_DB_HOST}" -u"${ZM_DB_USER}" -p"${ZM_DB_PASS}" "${ZM_DB_NAME}" < /usr/share/zoneminder/db/zm_create.sql
        echo "Database schema created"
    fi

    # Update database with any changes
    if [ -x /usr/bin/zmupdate.pl ]; then
        /usr/bin/zmupdate.pl -nointeractive
        echo "Database updated"
    fi
else
    echo "Database already initialized, checking for updates..."
    if [ -x /usr/bin/zmupdate.pl ]; then
        /usr/bin/zmupdate.pl -nointeractive -version=$(cat /usr/share/zoneminder/www/api/app/Config/VERSION) || true
    fi
fi

# Update ZM configuration for Docker environment
echo "Updating ZoneMinder configuration..."
mysql -h"${ZM_DB_HOST}" -u"${ZM_DB_USER}" -p"${ZM_DB_PASS}" "${ZM_DB_NAME}" -e "
    UPDATE Config SET Value='${ZM_DB_HOST}' WHERE Name='ZM_DB_HOST';
    UPDATE Config SET Value='${ZM_DB_NAME}' WHERE Name='ZM_DB_NAME';
    UPDATE Config SET Value='${ZM_DB_USER}' WHERE Name='ZM_DB_USER';
    UPDATE Config SET Value='${ZM_DB_PASS}' WHERE Name='ZM_DB_PASS';
" || true

# Set proper permissions
echo "Setting permissions..."
chown -R www-data:www-data /var/cache/zoneminder /var/log/zm /var/run/zm /var/tmp/zm || true

# Start ZoneMinder daemons
echo "Starting ZoneMinder services..."
if [ -x /usr/bin/zmpkg.pl ]; then
    /usr/bin/zmpkg.pl start || true
fi

echo "ZoneMinder is ready!"
echo "Access the web interface at http://localhost:8080"
echo ""
echo "Development mode active:"
echo "  - PHP errors are displayed"
echo "  - Web directory is mounted from ./web"
echo "  - PHPMyAdmin available at http://localhost:8081"
echo ""

# Execute the main command
exec "$@"
