#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
#
# Generates the Steam library artwork of Gordeck (capsules, hero, logo, icon)
# as SVG and renders them to PNG with rsvg-convert.
#
# Usage: ./generate.py   (writes into the directory of this script)

import os
import subprocess
import sys

OUT = os.path.dirname(os.path.abspath(__file__))
FONT = "Noto Sans, DejaVu Sans, sans-serif"

DEFS = """
<defs>
  <linearGradient id="bg" x1="0" y1="0" x2="0" y2="1">
    <stop offset="0" stop-color="#1d4fa8"/>
    <stop offset="0.55" stop-color="#0b1a38"/>
    <stop offset="1" stop-color="#050913"/>
  </linearGradient>
  <linearGradient id="beam" x1="0" y1="0" x2="1" y2="0">
    <stop offset="0" stop-color="#6cb4ff" stop-opacity="0"/>
    <stop offset="0.5" stop-color="#6cb4ff" stop-opacity="0.16"/>
    <stop offset="1" stop-color="#6cb4ff" stop-opacity="0"/>
  </linearGradient>
  <linearGradient id="tile" x1="0" y1="0" x2="0" y2="1">
    <stop offset="0" stop-color="#1d4fa8"/>
    <stop offset="0.6" stop-color="#0b1a38"/>
    <stop offset="1" stop-color="#060b16"/>
  </linearGradient>
  <radialGradient id="tileglow" cx="256" cy="230" r="230" gradientUnits="userSpaceOnUse">
    <stop offset="0" stop-color="#2f8cff" stop-opacity="0.55"/>
    <stop offset="1" stop-color="#2f8cff" stop-opacity="0"/>
  </radialGradient>
  <linearGradient id="letter" x1="0" y1="96" x2="0" y2="416" gradientUnits="userSpaceOnUse">
    <stop offset="0" stop-color="#ffffff"/>
    <stop offset="1" stop-color="#b9c6dd"/>
  </linearGradient>
  <linearGradient id="play" x1="0" y1="186" x2="0" y2="326" gradientUnits="userSpaceOnUse">
    <stop offset="0" stop-color="#8fd0ff"/>
    <stop offset="1" stop-color="#2f8cff"/>
  </linearGradient>
  <linearGradient id="word" x1="0" y1="0" x2="0" y2="1">
    <stop offset="0" stop-color="#ffffff"/>
    <stop offset="1" stop-color="#c9d6ee"/>
  </linearGradient>
</defs>
"""


def icon(x, y, size, tile=True):
	"""The Gordeck icon (same as gui/res/gordeck.svg), placed at x/y with the given size"""
	s = size / 512.0
	parts = []
	if tile:
		parts.append('<rect x="16" y="16" width="480" height="480" rx="112" fill="url(#tile)"/>')
		parts.append('<rect x="16" y="16" width="480" height="480" rx="112" fill="url(#tileglow)"/>')
		parts.append('<rect x="17.5" y="17.5" width="477" height="477" rx="110.5" fill="none" stroke="#6cb4ff" stroke-opacity="0.35" stroke-width="3"/>')
	parts.append('<path d="M 360.7 151.3 A 148 148 0 1 0 404 256 L 352 256" fill="none" stroke="url(#letter)" '
			'stroke-width="62" stroke-linecap="round" stroke-linejoin="round"/>')
	parts.append('<path d="M 212 196 L 212 316 Q 212 330 224 323 L 300 263 Q 310 256 300 249 L 224 189 Q 212 182 212 196 Z" fill="url(#play)"/>')
	return '<g transform="translate({} {}) scale({})">{}</g>'.format(x, y, s, "".join(parts))


def background(w, h, glow_cx=0.5, glow_cy=0.4):
	return f"""
<rect width="{w}" height="{h}" fill="url(#bg)"/>
<radialGradient id="glow_{w}_{h}" cx="{glow_cx * w}" cy="{glow_cy * h}" r="{max(w, h) * 0.6}" gradientUnits="userSpaceOnUse">
  <stop offset="0" stop-color="#2f8cff" stop-opacity="0.55"/>
  <stop offset="1" stop-color="#2f8cff" stop-opacity="0"/>
</radialGradient>
<rect width="{w}" height="{h}" fill="url(#glow_{w}_{h})"/>
<g transform="rotate(-24 {w / 2} {h / 2})">
  <rect x="{-w}" y="{h * 0.22}" width="{w * 3}" height="{h * 0.07}" fill="url(#beam)"/>
  <rect x="{-w}" y="{h * 0.36}" width="{w * 3}" height="{h * 0.025}" fill="url(#beam)"/>
  <rect x="{-w}" y="{h * 0.72}" width="{w * 3}" height="{h * 0.05}" fill="url(#beam)"/>
</g>
"""


def wordmark(x, y, size, anchor="middle"):
	return (f'<text x="{x}" y="{y}" font-family="{FONT}" font-weight="900" font-size="{size}" '
			f'letter-spacing="{size * 0.06}" fill="url(#word)" text-anchor="{anchor}">GORDECK</text>')


def tagline(x, y, size, anchor="middle"):
	return (f'<text x="{x}" y="{y}" font-family="{FONT}" font-weight="700" font-size="{size}" '
			f'letter-spacing="{size * 0.18}" fill="#6cb4ff" text-anchor="{anchor}">REMOTE PLAY</text>')


def svg(w, h, body):
	return f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" viewBox="0 0 {w} {h}">{DEFS}{body}</svg>\n'


ARTWORK = {
	# portrait capsule shown in the library grid
	"capsule": (600, 900, background(600, 900) + icon(150, 190, 300) + wordmark(300, 640, 92) + tagline(300, 705, 30)),
	# wide capsule, used for recently played and the shelves
	"wide": (920, 430, background(920, 430, 0.3, 0.45) + icon(70, 85, 260) + wordmark(370, 230, 104, "start") + tagline(376, 292, 34, "start")),
	# hero image at the top of the game page, the logo is drawn over it
	"hero": (1920, 620, background(1920, 620, 0.72, 0.5)
		+ '<g opacity="0.22">' + icon(1240, -120, 860, tile=False) + '</g>'),
	# transparent logo shown over the hero image
	"logo": (1280, 360, icon(0, 20, 320) + wordmark(370, 225, 170, "start") + tagline(378, 312, 52, "start")),
	# icon
	"icon": (256, 256, icon(0, 0, 256)),
}


def main():
	for name, (w, h, body) in ARTWORK.items():
		svg_path = os.path.join(OUT, name + ".svg")
		png_path = os.path.join(OUT, name + ".png")
		with open(svg_path, "w") as f:
			f.write(svg(w, h, body))
		subprocess.run(["rsvg-convert", "-w", str(w), "-h", str(h), svg_path, "-o", png_path], check=True)
		print("wrote", png_path)


if __name__ == "__main__":
	sys.exit(main())
