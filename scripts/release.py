"""Package validated PlatformIO factory images and generate a static Pages site."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil

ROOT = Path(__file__).resolve().parents[1]
BOARDS = {
    "xiao_esp32s3": ("ESP32-S3", 0, 8 * 1024 * 1024),
    "xiao_esp32c3": ("ESP32-C3", 0, 4 * 1024 * 1024),
    "xiao_esp32c6": ("ESP32-C6", 0, 4 * 1024 * 1024),
    "xiao_esp32c5": ("ESP32-C5", 0x2000, 8 * 1024 * 1024),
}


def version(tag=""):
    value = (ROOT / "VERSION").read_text(encoding="utf-8").strip()
    if not re.fullmatch(r"\d+\.\d+\.\d+", value):
        raise ValueError("VERSION must contain major.minor.patch")
    if tag and tag != f"v{value}":
        raise ValueError(f"Tag {tag!r} does not match VERSION {value!r}")
    return value


def filename(board, value):
    return f"ChainOSCPad-MIDI-{value}-{board}-factory.bin"


def validate_image(data, board):
    _, boot_offset, flash_size = BOARDS[board]
    if not 0x10020 < len(data) <= flash_size:
        raise ValueError(f"Invalid image size: {board}")
    if data[boot_offset] != 0xE9 or data[0x10000] != 0xE9:
        raise ValueError(f"Invalid bootloader/application header: {board}")
    if data[0x8000:0x8002] != b"\xaa\x50":
        raise ValueError(f"Missing ESP partition table: {board}")


def package(board, build_dir, output, value):
    source = build_dir / board
    data = (source / "firmware.factory.bin").read_bytes()
    validate_image(data, board)
    # A factory image starts at address zero, including C5's 0x2000 padding.
    for part, offset in (("bootloader.bin", BOARDS[board][1]),
                         ("partitions.bin", 0x8000), ("firmware.bin", 0x10000)):
        expected = (source / part).read_bytes()
        if not expected or data[offset:offset + len(expected)] != expected:
            raise ValueError(f"Factory image does not match {board}/{part}")
    output.mkdir(parents=True, exist_ok=True)
    name = filename(board, value)
    digest = hashlib.sha256(data).hexdigest()
    (output / name).write_bytes(data)
    (output / (name + ".sha256")).write_text(f"{digest}  {name}\n", encoding="utf-8")
    print(f"Packaged {name} ({len(data)} bytes)")


def build_site(firmware_dir, output, value):
    # Validate the complete set before publishing any site output.
    images = {}
    for board in BOARDS:
        name = filename(board, value)
        data = (firmware_dir / name).read_bytes()
        validate_image(data, board)
        checksum = (firmware_dir / (name + ".sha256")).read_text(encoding="utf-8").strip()
        if checksum != f"{hashlib.sha256(data).hexdigest()}  {name}":
            raise ValueError(f"SHA256 mismatch: {name}")
        images[board] = data
    shutil.copytree(ROOT / "docs", output, dirs_exist_ok=True)
    installer = output / "installer"
    binaries = installer / "firmware"
    binaries.mkdir(parents=True, exist_ok=True)
    builds = []
    for board, data in images.items():
        name = filename(board, value)
        (binaries / name).write_bytes(data)
        shutil.copy2(firmware_dir / (name + ".sha256"), binaries / (name + ".sha256"))
        build = {"chipFamily": BOARDS[board][0],
                 "parts": [{"path": f"firmware/{name}", "offset": 0}]}
        manifest = {"name": "ChainOSCPad MIDI", "version": value,
                    "new_install_prompt_erase": True, "builds": [build]}
        (installer / f"manifest-{board}.json").write_text(
            json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
        builds.append(build)
    manifest["builds"] = builds
    (installer / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    (output / ".nojekyll").touch()
    print(f"Generated Web Installer {value}: {output}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    check = commands.add_parser("version")
    check.add_argument("--tag", default="")
    pack = commands.add_parser("package")
    pack.add_argument("--board", required=True, choices=BOARDS)
    pack.add_argument("--build-dir", type=Path, default=ROOT / ".pio/build")
    pack.add_argument("--output", type=Path, default=ROOT / "dist")
    site = commands.add_parser("site")
    site.add_argument("--firmware-dir", type=Path, default=ROOT / "dist")
    site.add_argument("--output", type=Path, default=ROOT / "_site")
    args = parser.parse_args()
    value = version(getattr(args, "tag", ""))
    if args.command == "version":
        print(value)
    elif args.command == "package":
        package(args.board, args.build_dir, args.output, value)
    else:
        build_site(args.firmware_dir, args.output, value)


if __name__ == "__main__":
    main()
