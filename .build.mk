
#
# Travis rules
#

travis_deps_linux:
	sudo apt-get update -qq
	sudo apt-get install -y -qq libpolkit-gobject-1-dev zlib1g-dev apache2\
	  mysql-server php5 php5-mysql build-essential libmysqlclient-dev\
	  libssl-dev libbz2-dev libpcre3-dev libdbi-perl libarchive-zip-perl\
	  libdate-manip-perl libdevice-serialport-perl libmime-perl\
	  libwww-perl libdbd-mysql-perl libsys-mmap-perl yasm automake autoconf\
	  cmake libjpeg-turbo8-dev apache2-mpm-prefork libapache2-mod-php5\
	  php5-cli libtheora-dev libvorbis-dev libvpx-dev libx264-dev\
	  libmp4v2-dev libvlccore-dev libvlc-dev 2>&1 > /dev/null
	
travis_ffmpeg_linux:
	git clone -b n3.0 --depth=1 git://source.ffmpeg.org/ffmpeg.git
	cd ffmpeg && \
	./configure --enable-shared --enable-swscale --enable-gpl --enable-libx264 \
	    --enable-libvpx --enable-libvorbis --enable-libtheora && \
	make -j `grep processor /proc/cpuinfo|wc -l` && \
	sudo make install && \
	sudo make install-libs

travis_test_linux: travis_deps_linux travis_ffmpeg_linux
	mysql -uroot -e "CREATE DATABASE IF NOT EXISTS zm"
	mysql -uroot -e "GRANT ALL ON zm.* TO 'zmuser'@'localhost' IDENTIFIED BY 'zmpass'"
	mysql -uroot -e "FLUSH PRIVILEGES"
	cmake -DCMAKE_INSTALL_PREFIX="/usr"
	make
	sudo make install
	mysql -uzmuser -pzmpass < db/zm_create.sql
	mysql -uzmuser -pzmpass zm < db/test.monitor.sql
	sudo zmpkg.pl start
	sudo zmfilter.pl -f purgewhenfull