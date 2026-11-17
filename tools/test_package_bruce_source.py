"""Keep private-data rejection when updating the corresponding-source exporter."""
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import json
import zipfile

from package_bruce_source import package


class SourcePrivacyTests(unittest.TestCase):
    def test_pinned_submodule_files_are_included(self):
        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            source = base / "source"
            (source / ".git").mkdir(parents=True)
            names = ["LICENSE", "README-DUALBOOT-RELEASE.md", "platformio.ini",
                     "src/core/dualboot_nvs.c"]
            vendor_file = "lib/vendor/LICENSE"
            for name in names + [vendor_file]:
                path = source / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text("source input")
            root_listing = "\0".join(names + ["lib/vendor"]).encode()
            recursive_listing = "\0".join(
                "100644 " + "0" * 40 + " 0\t" + name
                for name in names + [vendor_file]).encode()
            output = base / "source.zip"
            with patch("package_bruce_source.check_bruce_source"), \
                    patch("package_bruce_source.subprocess.check_output", side_effect=[
                        root_listing, recursive_listing, b"1700000000\n"]):
                package(source, output)
            with zipfile.ZipFile(output) as archive:
                self.assertEqual(archive.read(vendor_file), b"source input")
                manifest = json.loads(archive.read("RELEASE-SOURCE.json"))
                self.assertIn(vendor_file, manifest["files"])

    def test_private_inputs_are_rejected_before_creating_an_archive(self):
        for name in (".env", "nvs.bin", "flash_dump.bin", "key.u2f", "cert_key.u2f",
                     "test.key", "test.pfx", "test.p12", "test.keystore"):
            with self.subTest(name=name), tempfile.TemporaryDirectory() as temporary:
                base = Path(temporary)
                source = base / "source"
                (source / ".git").mkdir(parents=True)
                (source / name).write_text("synthetic private-data sentinel")
                output = base / "source.zip"
                with patch("package_bruce_source.check_bruce_source"), \
                        patch("package_bruce_source.subprocess.check_output",
                              side_effect=[(name + "\0").encode(), b""]):
                    with self.assertRaisesRegex(ValueError, "Private data"):
                        package(source, output)
                self.assertFalse(output.exists())

    def test_existing_archives_are_never_overwritten(self):
        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            output = base / "source.zip"
            output.write_bytes(b"existing artifact")
            with self.assertRaisesRegex(ValueError, "new archive"):
                package(base / "source", output)
            self.assertEqual(output.read_bytes(), b"existing artifact")


if __name__ == "__main__":
    unittest.main()
