#!/bin/bash
# SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
#
# Builds Gordeck and packages the graphical installer:
#   dist/Gordeck-Instalador.tar.gz
# with the flatpak bundle, install.sh, add-to-steam.py and the Steam artwork.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REPO="$ROOT/.flatpak-repo"
PKG_NAME="Gordeck-Instalador"
PKG="$ROOT/dist/$PKG_NAME"

echo "==> Building Gordeck"
flatpak run org.flatpak.Builder --ccache --force-clean --repo="$REPO" \
	"$ROOT/build-dir" "$ROOT/flatpak-build.yaml"

echo "==> Packaging"
rm -rf "$PKG"
mkdir -p "$PKG/steam-art"
flatpak build-bundle --runtime-repo=https://dl.flathub.org/repo/flathub.flatpakrepo \
	"$REPO" "$PKG/gordeck.flatpak" com.bnx.gordeck stable
cp "$ROOT/installer/install.sh" "$ROOT/installer/add-to-steam.py" "$ROOT/installer/LEIA-ME.txt" \
	"$ROOT/installer/Instalar Gordeck.desktop" "$PKG/"
cp "$ROOT"/installer/steam-art/*.png "$PKG/steam-art/"
chmod +x "$PKG/install.sh" "$PKG/add-to-steam.py" "$PKG/Instalar Gordeck.desktop"

tar -C "$ROOT/dist" -czf "$ROOT/dist/$PKG_NAME.tar.gz" "$PKG_NAME"
echo "==> Done: $ROOT/dist/$PKG_NAME.tar.gz"
