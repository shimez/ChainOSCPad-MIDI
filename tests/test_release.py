"""Check that incomplete/corrupt release artifacts cannot become an installer."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location("release", Path(__file__).resolve().parents[1] / "scripts/release.py")
release = importlib.util.module_from_spec(spec)
spec.loader.exec_module(release)


class ReleaseTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.build = self.root / "build"
        self.dist = self.root / "dist"
        self.site = self.root / "site"
        for board, (_, offset, _) in release.BOARDS.items():
            folder = self.build / board
            folder.mkdir(parents=True)
            data = bytearray(b"\xff" * 0x10100)
            for name, address, value in (("bootloader.bin", offset, b"\xe9boot"),
                                         ("partitions.bin", 0x8000, b"\xaa\x50partition"),
                                         ("firmware.bin", 0x10000, b"\xe9application")):
                (folder / name).write_bytes(value)
                data[address:address + len(value)] = value
            (folder / "firmware.factory.bin").write_bytes(data)

    def package_all(self):
        for board in release.BOARDS:
            release.package(board, self.build, self.dist, "0.1.0")

    def test_complete_site_and_c5_padding(self):
        self.package_all()
        release.build_site(self.dist, self.site, "0.1.0")
        manifest = json.loads((self.site / "installer/manifest.json").read_text())
        self.assertEqual({b["chipFamily"] for b in manifest["builds"]},
                         {"ESP32-S3", "ESP32-C3", "ESP32-C6", "ESP32-C5"})
        c5 = next(b for b in manifest["builds"] if b["chipFamily"] == "ESP32-C5")
        part = c5["parts"][0]
        self.assertEqual(part["offset"], 0)
        data = (self.site / "installer" / part["path"]).read_bytes()
        self.assertEqual(data[:0x2000], b"\xff" * 0x2000)
        self.assertEqual(data[0x2000], 0xE9)

    def test_corrupt_artifact_rejected(self):
        self.package_all()
        path = self.dist / release.filename("xiao_esp32s3", "0.1.0")
        data = bytearray(path.read_bytes())
        data[-1] ^= 1
        path.write_bytes(data)
        with self.assertRaisesRegex(ValueError, "SHA256 mismatch"):
            release.build_site(self.dist, self.site, "0.1.0")
        self.assertFalse(self.site.exists())

    def test_missing_board_rejected(self):
        self.package_all()
        (self.dist / release.filename("xiao_esp32c5", "0.1.0")).unlink()
        with self.assertRaises(FileNotFoundError):
            release.build_site(self.dist, self.site, "0.1.0")
        self.assertFalse(self.site.exists())

    def test_mismatched_build_parts_rejected(self):
        (self.build / "xiao_esp32s3/firmware.bin").write_bytes(b"\xe9different-build")
        with self.assertRaisesRegex(ValueError, "does not match"):
            release.package("xiao_esp32s3", self.build, self.dist, "0.1.0")

    def test_tag_version_mismatch_rejected(self):
        with self.assertRaisesRegex(ValueError, "does not match"):
            release.version("v999.0.0")


if __name__ == "__main__":
    unittest.main()
