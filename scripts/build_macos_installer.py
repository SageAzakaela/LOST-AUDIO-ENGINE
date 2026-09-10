"""Create a normal Mac Installer package from an already validated beta ZIP."""
import argparse
import hashlib
import json
from pathlib import Path
import plistlib
import subprocess
import tempfile
import xml.etree.ElementTree as ET

from macos_plugins import FORMATS, ROOT, inspect_bundle, products, run


def checked_manifest(bundles, expected_commit):
    manifest = json.loads((bundles / "manifest.json").read_text())
    if manifest["commit"] != expected_commit:
        raise ValueError("Downloaded build source does not match the selected CI run")
    if set(manifest["native_validation_architectures"]) != {"arm64", "x86_64"}:
        raise ValueError("Both native CPU validations are required")
    actual = []
    for product in products():
        for fmt, suffix in FORMATS.items():
            record = inspect_bundle(bundles / fmt / (product["name"] + suffix), product, fmt)
            matches = [r for r in manifest["products"] if r == record]
            if len(matches) != 1:
                raise ValueError(f'Manifest mismatch: {product["name"]} {fmt}')
            actual.append(record)
    if len(manifest["products"]) != len(actual):
        raise ValueError("Unexpected extra manifest products")
    return manifest


def build(args):
    manifest = checked_manifest(args.bundles, args.source_commit)
    args.output.mkdir(parents=True, exist_ok=True)
    package = args.output.resolve() / f'BE-Digital-Mac-Beta-{args.source_commit[:8]}.pkg'
    if package.exists():
        raise FileExistsError(package)
    readme = (ROOT / "installer/macos/INSTALL.txt").read_text()
    with tempfile.TemporaryDirectory(prefix="be-mac-installer-") as temporary:
        temp = Path(temporary)
        payload = temp / "payload"
        for fmt in FORMATS:
            run("ditto", args.bundles / fmt, payload / "Library/Audio/Plug-Ins" / fmt)
        info = payload / "Library/Application Support/B&E Digital/Lost Audio Mac Beta"
        info.mkdir(parents=True)
        (info / "INSTALL.txt").write_text(readme)
        (info / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
        components = temp / "components.plist"
        run("pkgbuild", "--analyze", "--root", payload, components)
        entries = plistlib.loads(components.read_bytes())
        # macOS must use the standard plugin folders, even when another copy
        # exists elsewhere. Avoid Installer's automatic bundle relocation.
        for entry in entries:
            entry["BundleIsRelocatable"] = False
            entry["BundleIsVersionChecked"] = False
            entry["BundleHasStrictIdentifier"] = True
        components.write_bytes(plistlib.dumps(entries))
        identifier = "com.bedigital.lostaudio.mac.beta"
        component_pkg = temp / "plugins.pkg"
        run("pkgbuild", "--root", payload, "--component-plist", components,
            "--identifier", identifier, "--version", "0.1.0", "--install-location", "/", component_pkg)
        resources = temp / "resources"
        resources.mkdir()
        (resources / "welcome.txt").write_text(readme)
        xml = ET.Element("installer-gui-script", {"minSpecVersion": "2"})
        ET.SubElement(xml, "title").text = "B&E Digital Mac Plugin Beta"
        ET.SubElement(xml, "welcome", {"file": "welcome.txt", "mime-type": "text/plain"})
        ET.SubElement(xml, "options", {"customize": "never", "require-scripts": "false", "hostArchitectures": "arm64,x86_64"})
        ET.SubElement(xml, "domains", {"enable_anywhere": "false", "enable_currentUserHome": "false", "enable_localSystem": "true"})
        volume = ET.SubElement(xml, "volume-check")
        allowed = ET.SubElement(volume, "allowed-os-versions")
        ET.SubElement(allowed, "os-version", {"min": "11.0"})
        outline = ET.SubElement(xml, "choices-outline")
        ET.SubElement(outline, "line", {"choice": "plugins"})
        choice = ET.SubElement(xml, "choice", {"id": "plugins", "title": "VST3 and Audio Units", "visible": "false"})
        ET.SubElement(choice, "pkg-ref", {"id": identifier})
        ET.SubElement(xml, "pkg-ref", {"id": identifier, "version": "0.1.0", "onConclusion": "none"}).text = "plugins.pkg"
        distribution = temp / "distribution.xml"
        ET.ElementTree(xml).write(distribution, encoding="utf-8", xml_declaration=True)
        run("productbuild", "--distribution", distribution, "--resources", resources,
            "--package-path", temp, package)
        # Expand the final archive and confirm there are no custom install scripts.
        expanded = temp / "expanded"
        run("pkgutil", "--expand", package, expanded)
        if list(expanded.rglob("Scripts")):
            raise ValueError("Unexpected executable installer scripts")
    (args.output / "INSTALL.txt").write_text(readme)
    (args.output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    (args.output / "SHA256SUMS.txt").write_text(f'{hashlib.sha256(package.read_bytes()).hexdigest()}  {package.name}\n')
    print(package)


def verify_install(args):
    manifest = json.loads((args.package_dir / "manifest.json").read_text())
    if manifest["commit"] != args.source_commit:
        raise ValueError("Unexpected installed source version")
    expected = (args.package_dir / "SHA256SUMS.txt").read_text().split()
    if len(expected) != 2 or Path(expected[1]).name != expected[1]:
        raise ValueError("Invalid package checksum record")
    package = args.package_dir / expected[1]
    if hashlib.sha256(package.read_bytes()).hexdigest() != expected[0]:
        raise ValueError("Installer checksum mismatch")
    run("sudo", "installer", "-pkg", package, "-target", "/")
    logs = args.package_dir / "install-validation"
    logs.mkdir(exist_ok=True)
    for product in products():
        for fmt, suffix in FORMATS.items():
            bundle = Path("/Library/Audio/Plug-Ins") / fmt / (product["name"] + suffix)
            record = inspect_bundle(bundle, product, fmt)
            if record not in manifest["products"]:
                raise ValueError(f"Installed bytes do not match validated bundle: {bundle}")
        output = run("auval", "-v", "aufx", product["code"], "LsAu", timeout=300)
        (logs / (product["project"] + "-installed-auval.txt")).write_text(output)
    (logs / "result.json").write_text(json.dumps({"passed": True, "commit": args.source_commit,
        "verified_bundles": len(manifest["products"]), "installer_sha256": expected[0]}, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    make = sub.add_parser("build")
    make.add_argument("--bundles", type=Path, required=True)
    make.add_argument("--output", type=Path, required=True)
    verify = sub.add_parser("verify-install")
    verify.add_argument("--package-dir", type=Path, required=True)
    for p in (make, verify):
        p.add_argument("--source-commit", required=True)
    args = parser.parse_args()
    if __import__("platform").system() != "Darwin":
        parser.error("This installer tool requires macOS")
    {"build": build, "verify-install": verify_install}[args.command](args)
