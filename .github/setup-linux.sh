#!/bin/bash -e

DEPS="docbook-xsl libpcsclite-dev xsltproc gengetopt libcmocka-dev help2man pcscd check softhsm2 pcsc-tools libtool make autoconf autoconf-archive automake libssl-dev zlib1g-dev pkg-config libreadline-dev openssl git"

if [ "$1" == "clang-tidy" ]; then
	DEPS="$DEPS clang-tidy"
elif [ "$1" == "cac" ]; then
	DEPS="$DEPS libglib2.0-dev libnss3-dev gnutls-bin libusb-dev libudev-dev flex libnss3-tools"
elif [ "$1" == "oseid" ]; then
	DEPS="$DEPS socat gawk xxd"
elif [ "$1" == "piv" ]; then
	DEPS="$DEPS ant cmake openjdk-8-jdk"
elif [ "$1" == "mingw" ]; then
	DEPS="$DEPS wine binutils-mingw-w64-x86-64 gcc-mingw-w64-x86-64 mingw-w64"
elif [ "$1" == "mingw32" ]; then
	DEPS="$DEPS wine binutils-mingw-w64-i686 gcc-mingw-w64-i686"
fi

# make sure we do not get prompts
export DEBIAN_FRONTEND=noninteractive
sudo apt-get update
sudo apt-get install -y build-essential $DEPS

if [ "$1" == "piv" ]; then
	sudo update-java-alternatives -s java-1.8.0-openjdk-amd64
	sudo update-alternatives --get-selections | grep ^java
	export PATH="/usr/lib/jvm/java-8-openjdk-amd64/bin/:$PATH"
	export JAVA_HOME=/usr/lib/jvm/java-8-openjdk-amd64/
	env | grep -i openjdk
fi

if [ "$1" == "mingw" -o "$1" == "mingw32" ]; then
	if [ ! -f "$(winepath 'C:/Program Files (x86)/Inno Setup 5/ISCC.exe')" ]; then
		/sbin/start-stop-daemon --start --quiet --pidfile /tmp/custom_xvfb_99.pid --make-pidfile --background --exec /usr/bin/Xvfb -- :99 -ac -screen 0 1280x1024x16
		export DISPLAY=:99.0
		[ -d isetup ] || mkdir isetup
		pushd isetup
		[ -f isetup-5.5.6.exe ] || wget http://files.jrsoftware.org/is/5/isetup-5.5.6.exe
		wine isetup-5.5.6.exe /SILENT /VERYSILENT /SP- /SUPPRESSMSGBOXES /NORESTART
		popd
	fi
fi
