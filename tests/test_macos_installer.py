import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import build_macos_installer as installer


class InstallerSourceTests(unittest.TestCase):
    def fixture(self, path, **overrides):
        manifest = {"commit": "a" * 40, "native_validation_architectures": ["arm64", "x86_64"],
                    "products": [{"format": "VST3", "binary_sha256": "good"},
                                 {"format": "Components", "binary_sha256": "good"}]}
        manifest.update(overrides)
        (path / "manifest.json").write_text(json.dumps(manifest))
        return manifest

    def test_refuses_different_source_commit(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp)
            self.fixture(path)
            with self.assertRaisesRegex(ValueError, "source"):
                installer.checked_manifest(path, "b" * 40)

    def test_requires_both_native_validations(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp)
            self.fixture(path, native_validation_architectures=["arm64"])
            with self.assertRaisesRegex(ValueError, "Both native CPU"):
                installer.checked_manifest(path, "a" * 40)

    def test_refuses_modified_binary(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp)
            self.fixture(path)
            with patch.object(installer, "products", return_value=[{"name": "Test"}]), \
                 patch.object(installer, "inspect_bundle", return_value={"format": "VST3", "binary_sha256": "modified"}), \
                 self.assertRaisesRegex(ValueError, "Manifest mismatch"):
                installer.checked_manifest(path, "a" * 40)

    def test_accepts_exact_validated_records(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp)
            manifest = self.fixture(path)
            with patch.object(installer, "products", return_value=[{"name": "Test"}]), \
                 patch.object(installer, "inspect_bundle", side_effect=manifest["products"]):
                self.assertEqual(installer.checked_manifest(path, "a" * 40), manifest)


if __name__ == "__main__":
    unittest.main()
