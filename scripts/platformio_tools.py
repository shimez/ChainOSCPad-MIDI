"""Support tool archives that retain their architecture subdirectory on Windows."""

from pathlib import Path

Import("env")

platform = env.PioPlatform()
for package, architecture in (
    ("toolchain-riscv32-esp", "riscv32-esp-elf"),
    ("toolchain-xtensa-esp-elf", "xtensa-esp-elf"),
):
    directory = platform.get_package_dir(package)
    if not directory:
        continue
    root = Path(directory)
    nested_bin = root / architecture / "bin"
    if not (root / "bin").is_dir() and nested_bin.is_dir():
        env.PrependENVPath("PATH", str(nested_bin))
