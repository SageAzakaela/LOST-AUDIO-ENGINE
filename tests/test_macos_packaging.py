import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location("macos_plugins", Path(__file__).resolve().parents[1] / "scripts/macos_plugins.py")
mac = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mac)


class MacPackagingTests(unittest.TestCase):
    def test_system_frameworks_are_portable(self):
        lines = "plugin:\n\t/System/Library/Frameworks/Cocoa.framework/Versions/A/Cocoa (compatibility version 1.0.0, current version 1.0.0)\n\t/usr/lib/libc++.1.dylib (compatibility version 1.0.0, current version 1900.0.0)"
        self.assertEqual(len(mac.portable_dependencies(lines)), 2)

    def test_unbundled_libraries_block_packaging(self):
        for dependency in ["/opt/homebrew/lib/libfoo.dylib", "/usr/local/lib/libfoo.dylib", "@rpath/libfoo.dylib", "/Users/builder/libfoo.dylib"]:
            with self.subTest(dependency=dependency), self.assertRaises(ValueError):
                mac.portable_dependencies(f'plugin:\n\t{dependency} (compatibility version 1.0.0, current version 1.0.0)')

    def test_missing_dependency_inspection_blocks_packaging(self):
        with self.assertRaises(ValueError):
            mac.portable_dependencies("not a Mach-O file")

    def test_fleet_identities_are_unique(self):
        products = mac.products()
        self.assertEqual(len(products), 13)
        for field in ("name", "bundle_id", "code", "target"):
            self.assertEqual(len({p[field] for p in products}), 13)

    def test_unknown_selection_fails(self):
        with self.assertRaises(ValueError):
            mac.products(["typo-vst3"])


if __name__ == "__main__":
    unittest.main()
