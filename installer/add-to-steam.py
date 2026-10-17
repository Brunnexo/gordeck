#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
#
# Adds Gordeck to the Steam library as a non-Steam game, including its
# icon and artwork (capsules, hero and logo). Steam must not be running,
# because it overwrites shortcuts.vdf when it exits.
#
# Usage: add-to-steam.py [--art-dir DIR] [--remove-legacy] [--dry-run]

import argparse
import binascii
import os
import shutil
import struct
import subprocess
import sys

APP_ID = "com.bnx.gordeck"
APP_NAME = "Gordeck"
EXE = '"/usr/bin/flatpak"'
START_DIR = '"/usr/bin/"'
LAUNCH_OPTIONS = '"run" "--branch=stable" "--arch=x86_64" "--command=gordeck" "{}"'.format(APP_ID)
LEGACY_APP_ID = "re.chiaki.Chiaki4deck"

STEAM_DIRS = [
	"~/.local/share/Steam",
	"~/.steam/steam",
	"~/.var/app/com.valvesoftware.Steam/.local/share/Steam",
]

# Binary VDF (shortcuts.vdf) types
TYPE_MAP = 0x00
TYPE_STRING = 0x01
TYPE_INT = 0x02
TYPE_MAP_END = 0x08


class VdfParser:
	def __init__(self, data):
		self.data = data
		self.pos = 0

	def read_string(self):
		end = self.data.index(b"\0", self.pos)
		s = self.data[self.pos:end].decode("utf-8", "surrogateescape")
		self.pos = end + 1
		return s

	def read_map(self):
		result = {}
		while True:
			t = self.data[self.pos]
			self.pos += 1
			if t == TYPE_MAP_END:
				return result
			key = self.read_string()
			if t == TYPE_MAP:
				result[key] = self.read_map()
			elif t == TYPE_STRING:
				result[key] = self.read_string()
			elif t == TYPE_INT:
				result[key] = struct.unpack("<i", self.data[self.pos:self.pos + 4])[0]
				self.pos += 4
			else:
				raise ValueError("Unsupported VDF type 0x{:02x} at {}".format(t, self.pos - 1))


def vdf_load(data):
	if not data:
		return {"shortcuts": {}}
	return VdfParser(data).read_map()


def vdf_dump(obj):
	out = bytearray()

	def write_string(s):
		out.extend(s.encode("utf-8", "surrogateescape"))
		out.append(0)

	def write_map(m):
		for key, value in m.items():
			if isinstance(value, dict):
				out.append(TYPE_MAP)
				write_string(key)
				write_map(value)
			elif isinstance(value, int):
				out.append(TYPE_INT)
				write_string(key)
				out.extend(struct.pack("<i", value))
			else:
				out.append(TYPE_STRING)
				write_string(key)
				write_string(value)
		out.append(TYPE_MAP_END)

	write_map(obj)
	return bytes(out)


def shortcut_app_id():
	"""Same id Steam would generate for this shortcut: crc32(exe + name) with the top bit set"""
	crc = binascii.crc32((EXE + APP_NAME).encode("utf-8")) & 0xffffffff
	return crc | 0x80000000


def to_signed(value):
	return struct.unpack("<i", struct.pack("<I", value))[0]


def to_unsigned(value):
	return struct.unpack("<I", struct.pack("<i", value))[0]


def find_user_dirs():
	users = []
	for steam_dir in STEAM_DIRS:
		userdata = os.path.join(os.path.expanduser(steam_dir), "userdata")
		if not os.path.isdir(userdata):
			continue
		for name in sorted(os.listdir(userdata)):
			# "0" and "anonymous" are not real accounts
			if name.isdigit() and name != "0":
				path = os.path.realpath(os.path.join(userdata, name))
				if path not in users:
					users.append(path)
	return users


def steam_running():
	try:
		return subprocess.run(["pgrep", "-x", "steam"], stdout=subprocess.DEVNULL).returncode == 0
	except FileNotFoundError:
		return False


def is_legacy_shortcut(entry):
	text = " ".join(str(entry.get(k, "")) for k in ("exe", "LaunchOptions", "icon", "ShortcutPath"))
	return LEGACY_APP_ID in text


def is_gordeck_shortcut(entry):
	text = " ".join(str(entry.get(k, "")) for k in ("exe", "LaunchOptions"))
	return APP_ID in text or entry.get("appname", entry.get("AppName")) == APP_NAME


def install_artwork(user_dir, art_dir, app_id, dry_run):
	grid_dir = os.path.join(user_dir, "config", "grid")
	files = {
		"capsule.png": "{}p.png".format(app_id),	# portrait capsule
		"wide.png": "{}.png".format(app_id),		# wide capsule
		"hero.png": "{}_hero.png".format(app_id),
		"logo.png": "{}_logo.png".format(app_id),
		"icon.png": "{}_icon.png".format(app_id),
	}
	if not dry_run:
		os.makedirs(grid_dir, exist_ok=True)
	for src, dst in files.items():
		src_path = os.path.join(art_dir, src)
		if not os.path.isfile(src_path):
			print("  artwork missing: " + src_path)
			continue
		if not dry_run:
			shutil.copyfile(src_path, os.path.join(grid_dir, dst))
	return os.path.join(grid_dir, files["icon.png"])


def process_user(user_dir, art_dir, remove_legacy, dry_run):
	config_dir = os.path.join(user_dir, "config")
	vdf_path = os.path.join(config_dir, "shortcuts.vdf")
	data = b""
	if os.path.isfile(vdf_path):
		with open(vdf_path, "rb") as f:
			data = f.read()
	root = vdf_load(data)
	shortcuts = root.setdefault("shortcuts", {})

	app_id = shortcut_app_id()
	icon_path = install_artwork(user_dir, art_dir, app_id, dry_run)

	entries = []
	removed_legacy = 0
	existing = None
	for key in sorted(shortcuts.keys(), key=lambda k: int(k) if k.isdigit() else 0):
		entry = shortcuts[key]
		if remove_legacy and is_legacy_shortcut(entry):
			removed_legacy += 1
			continue
		if existing is None and is_gordeck_shortcut(entry):
			existing = entry
		entries.append(entry)

	entry = existing if existing is not None else {}
	is_new = existing is None
	entry.update({
		"appid": to_signed(app_id),
		"appname": APP_NAME,
		"exe": EXE,
		"StartDir": START_DIR,
		"icon": icon_path,
		"ShortcutPath": "",
		"LaunchOptions": LAUNCH_OPTIONS,
	})
	# older Steam versions wrote these keys capitalized, don't keep both spellings
	for old in ("AppName", "Exe"):
		entry.pop(old, None)
	if is_new:
		entry.update({
			"IsHidden": 0,
			"AllowDesktopConfig": 1,
			"AllowOverlay": 1,
			"OpenVR": 0,
			"Devkit": 0,
			"DevkitGameID": "",
			"DevkitOverrideAppID": 0,
			"LastPlayTime": 0,
			"FlatpakAppID": "",
			"sortas": "",
			"tags": {},
		})
		entries.append(entry)

	root["shortcuts"] = {str(i): e for i, e in enumerate(entries)}
	new_data = vdf_dump(root)

	if not dry_run:
		os.makedirs(config_dir, exist_ok=True)
		backup = vdf_path + ".bak-gordeck"
		# keep the backup of the original file when running again
		if data and not os.path.exists(backup):
			with open(backup, "wb") as f:
				f.write(data)
		tmp = vdf_path + ".tmp-gordeck"
		with open(tmp, "wb") as f:
			f.write(new_data)
		os.replace(tmp, vdf_path)

	print("  {} Gordeck (id {}){}".format(
			"added" if is_new else "updated", app_id,
			", removed {} old Chiaki4deck shortcut(s)".format(removed_legacy) if removed_legacy else ""))


def remove_from_user(user_dir, dry_run):
	vdf_path = os.path.join(user_dir, "config", "shortcuts.vdf")
	if not os.path.isfile(vdf_path):
		return
	with open(vdf_path, "rb") as f:
		root = vdf_load(f.read())
	shortcuts = root.get("shortcuts", {})
	entries = [e for k, e in sorted(shortcuts.items(), key=lambda i: int(i[0]) if i[0].isdigit() else 0)
			if not is_gordeck_shortcut(e)]
	removed = len(shortcuts) - len(entries)
	if not dry_run and removed:
		root["shortcuts"] = {str(i): e for i, e in enumerate(entries)}
		with open(vdf_path, "wb") as f:
			f.write(vdf_dump(root))
		app_id = shortcut_app_id()
		grid_dir = os.path.join(user_dir, "config", "grid")
		for name in ("{}p.png", "{}.png", "{}_hero.png", "{}_logo.png", "{}_icon.png"):
			path = os.path.join(grid_dir, name.format(app_id))
			if os.path.isfile(path):
				os.remove(path)
	print("  removed {} Gordeck shortcut(s)".format(removed))


def has_legacy_shortcut():
	for user_dir in find_user_dirs():
		vdf_path = os.path.join(user_dir, "config", "shortcuts.vdf")
		if not os.path.isfile(vdf_path):
			continue
		with open(vdf_path, "rb") as f:
			root = vdf_load(f.read())
		if any(is_legacy_shortcut(e) for e in root.get("shortcuts", {}).values()):
			return True
	return False


def main():
	parser = argparse.ArgumentParser(description="Add Gordeck to the Steam library")
	parser.add_argument("--art-dir", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "steam-art"),
			help="directory with capsule.png, wide.png, hero.png, logo.png and icon.png")
	parser.add_argument("--remove-legacy", action="store_true",
			help="remove shortcuts of the old Chiaki4deck flatpak")
	parser.add_argument("--has-legacy", action="store_true",
			help="only check for shortcuts of the old Chiaki4deck flatpak (exit code 0 if found)")
	parser.add_argument("--remove", action="store_true", help="remove Gordeck from the Steam library instead")
	parser.add_argument("--dry-run", action="store_true", help="don't write anything")
	parser.add_argument("--force", action="store_true", help="don't check if Steam is running")
	parser.add_argument("--steam-dir", help="use this Steam directory instead of searching for it")
	args = parser.parse_args()

	if args.steam_dir:
		STEAM_DIRS[:] = [args.steam_dir]

	if args.has_legacy:
		return 0 if has_legacy_shortcut() else 1

	if not args.force and not args.dry_run and steam_running():
		print("Steam is running. Close Steam first, it overwrites the shortcuts when it exits.", file=sys.stderr)
		return 2

	users = find_user_dirs()
	if not users:
		print("No Steam user found. Open Steam and log in at least once.", file=sys.stderr)
		return 3

	for user_dir in users:
		print(user_dir)
		if args.remove:
			remove_from_user(user_dir, args.dry_run)
		else:
			process_user(user_dir, args.art_dir, args.remove_legacy, args.dry_run)
	return 0


if __name__ == "__main__":
	sys.exit(main())
