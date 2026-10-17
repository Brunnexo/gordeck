#!/bin/bash
# SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
#
# Installs the dependencies, builds Gordeck natively, runs the unit tests
# and creates a distribution package (.deb / .rpm) when supported.
# Meant to run as root inside a container of the target distribution.
#
# Environment:
#   BUILD_DIR  build directory (default: build)
#   DIST_DIR   where packages are copied to (default: dist)

set -euo pipefail

BUILD_DIR="${BUILD_DIR:-build}"
DIST_DIR="${DIST_DIR:-dist}"

. /etc/os-release
echo "==> Building on $PRETTY_NAME"

GENERATOR=""
case "$ID" in
	ubuntu|debian)
		export DEBIAN_FRONTEND=noninteractive
		apt-get update
		apt-get install -y --no-install-recommends \
			build-essential cmake ninja-build pkg-config git file dpkg-dev ca-certificates \
			python3 python3-protobuf protobuf-compiler \
			libssl-dev libopus-dev libavcodec-dev libavutil-dev libsdl2-dev libevdev-dev libudev-dev \
			qtbase5-dev libqt5opengl5-dev libqt5svg5-dev qtmultimedia5-dev qttools5-dev qttools5-dev-tools
		GENERATOR=DEB
		;;
	fedora)
		dnf install -y \
			gcc gcc-c++ cmake ninja-build pkgconf-pkg-config git rpm-build \
			python3 python3-protobuf protobuf-compiler \
			openssl-devel opus-devel libavcodec-free-devel libavutil-free-devel SDL2-devel libevdev-devel systemd-devel \
			qt5-qtbase-devel qt5-qtsvg-devel qt5-qtmultimedia-devel qt5-linguist
		GENERATOR=RPM
		;;
	arch)
		pacman -Syu --noconfirm --needed \
			base-devel cmake ninja git \
			python python-protobuf protobuf \
			openssl opus ffmpeg sdl2 libevdev \
			qt5-base qt5-svg qt5-multimedia qt5-tools
		;;
	*)
		echo "Unsupported distribution: $ID" >&2
		exit 1
		;;
esac

echo "==> Configuring"
cmake -S . -B "$BUILD_DIR" -G Ninja \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCHIAKI_ENABLE_TESTS=ON \
	-DCHIAKI_ENABLE_CLI=ON \
	-DCHIAKI_ENABLE_GUI=ON

echo "==> Building"
cmake --build "$BUILD_DIR"

echo "==> Testing"
"$BUILD_DIR/test/chiaki-unit"

if [ -n "$GENERATOR" ]; then
	echo "==> Packaging ($GENERATOR)"
	(cd "$BUILD_DIR" && cpack -G "$GENERATOR")
	mkdir -p "$DIST_DIR"
	version="$(sed -n 's/^set(CHIAKI_VERSION_\(MAJOR\|MINOR\|PATCH\) \([0-9]*\))$/\2/p' CMakeLists.txt | paste -sd.)"
	for f in "$BUILD_DIR"/*.deb "$BUILD_DIR"/*.rpm; do
		[ -f "$f" ] || continue
		# e.g. gordeck-2.1.1-ubuntu-24.04-x86_64.deb
		cp "$f" "$DIST_DIR/gordeck-$version-$ID-${VERSION_ID:-rolling}-$(uname -m).${f##*.}"
	done
	ls -la "$DIST_DIR"
fi
