"""Cartridge mount marker regression, including a sector-aligned FAT."""
from pathlib import Path
import hashlib
import json
import struct
import sys
import tempfile
import unittest
import zlib

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "source-kit/patcher"))
from assemble_release import ReleaseWriter
from assemble_demo import crc16


def digest(data):
    return hashlib.sha256(data).hexdigest()


class CartridgeMarkerTest(unittest.TestCase):
    def test_fat_padding_and_sector_boundary(self):
        for count in (1, 63, 64, 65, 1230):
            with self.subTest(files=count), tempfile.TemporaryDirectory() as folder:
                root = Path(folder)
                engine = root / "engine"
                engine.mkdir()
                (root / "licenses").mkdir()
                notices = b"Synthetic test fixture; no original game content.\n"
                (root / "licenses/BINARY-NOTICES.txt").write_bytes(notices)
                prefix = bytearray(1024)
                for at, offset in ((0x20, 512), (0x30, 768)):
                    struct.pack_into("<4I", prefix, at, offset, 0x02000000, 0x02000000, 64)
                packed = zlib.compress(prefix)
                banner = bytes(range(256)) * 8
                (engine / "runner.zlib").write_bytes(packed)
                (engine / "banner.zlib").write_bytes(zlib.compress(banner))
                (engine / "runner.json").write_text(json.dumps(dict(
                    sha256=digest(packed), restored_sha256=digest(prefix),
                    source_tables=[], banner_sha256=digest(banner),
                    binary_notices=dict(file="BINARY-NOTICES.txt", bytes=len(notices),
                                        sha256=digest(notices)))))
                resources = {f"file{i:04d}.bin": bytes([i % 256]) * (i % 17 + 1)
                             for i in range(count)}
                expected = {name: dict(bytes=len(data), sha256=digest(data))
                            for name, data in resources.items()}
                output = root / "test.nds"
                writer = ReleaseWriter(None, engine, output, expected)
                try:
                    for name, data in reversed(list(resources.items())):
                        writer.write(name, data)
                    result = writer.finish()
                finally:
                    writer.close()
                blob = output.read_bytes()
                fat_at, fat_bytes = struct.unpack_from("<II", blob, 0x48)
                banner_at = struct.unpack_from("<I", blob, 0x68)[0]
                self.assertEqual(fat_bytes, count * 8)
                self.assertEqual(blob[fat_at + fat_bytes:fat_at + fat_bytes + 8], b"NitroFS!")
                self.assertGreaterEqual(banner_at, fat_at + fat_bytes + 8)
                self.assertEqual(blob[banner_at:banner_at + len(banner)], banner)
                self.assertEqual(struct.unpack_from("<H", blob, 0x15e)[0], crc16(blob[:0x15e]))
                for i, (name, data) in enumerate(sorted(resources.items())):
                    start, end = struct.unpack_from("<II", blob, fat_at + i * 8)
                    self.assertEqual(blob[start:end], data, name)
                self.assertEqual(result["nds_sha256"], digest(blob))


if __name__ == "__main__":
    unittest.main()
