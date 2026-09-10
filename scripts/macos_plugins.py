"""Build evidence and packaging for the Mac plugin fleet. Requires macOS tools."""
import argparse
import hashlib
import json
from pathlib import Path
import platform
import plistlib
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
ARCHITECTURES = {"arm64", "x86_64"}
FORMATS = {"VST3": ".vst3", "Components": ".component"}


def run(*args, **kwargs):
    return subprocess.run([str(a) for a in args], check=True, text=True,
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT, **kwargs).stdout


def products(projects=None):
    result = []
    for path in sorted(ROOT.glob("*-vst3/CMakeLists.txt")):
        if projects and path.parent.name not in projects:
            continue
        source = path.read_text()
        result.append(dict(project=path.parent.name,
                           target=re.search(r"juce_add_plugin\((\w+)", source)[1],
                           name=re.search(r'PRODUCT_NAME\s+"([^"]+)"', source)[1],
                           bundle_id=re.search(r'BUNDLE_ID\s+"([^"]+)"', source)[1],
                           code=re.search(r"PLUGIN_CODE\s+(\w+)", source)[1]))
    if projects and {p["project"] for p in result} != set(projects):
        raise ValueError("Unknown plugin project in selection")
    return result


def portable_dependencies(output):
    dependencies = []
    for line in output.splitlines():
        if " (compatibility version " not in line:
            continue
        path = line.strip().split(" (compatibility version ", 1)[0]
        if not path.startswith(("/System/Library/", "/usr/lib/")):
            raise ValueError(f"Non-system runtime dependency: {path}")
        dependencies.append(path)
    if not dependencies:
        raise ValueError("No Mach-O dependencies were inspected")
    return dependencies


def inspect_bundle(bundle, product, fmt):
    info = plistlib.loads((bundle / "Contents/Info.plist").read_bytes())
    if info["CFBundleIdentifier"] != product["bundle_id"]:
        raise ValueError(f"Bundle identity changed: {bundle}")
    binary = bundle / "Contents/MacOS" / info["CFBundleExecutable"]
    arches = set(run("lipo", "-archs", binary).strip().split())
    if arches != ARCHITECTURES:
        raise ValueError(f"Expected universal binary, found {arches}: {bundle}")
    minimum = info.get("LSMinimumSystemVersion")
    if minimum and tuple(map(int, minimum.split("."))) > (11, 0):
        raise ValueError(f"Deployment target too new: {minimum}")
    for arch in sorted(arches):
        load_commands = run("otool", "-arch", arch, "-l", binary)
        # Modern toolchains use LC_BUILD_VERSION. The dedicated check avoids
        # confusing SDK versions or dylib compatibility versions with minos.
        minos = re.findall(r"\bminos\s+(\d+\.\d+(?:\.\d+)?)", load_commands)
        if not minos:
            minos = re.findall(r"cmd LC_VERSION_MIN_MACOSX\s+cmdsize \d+\s+version (\S+)", load_commands)
        if len(minos) != 1 or tuple(map(int, minos[0].split("."))) > (11, 0, 0):
            raise ValueError(f"Unexpected Mach-O minimum OS for {arch}: {minos}")
        portable_dependencies(run("otool", "-arch", arch, "-L", binary))
    run("codesign", "--verify", "--strict", "--verbose=2", bundle)
    if fmt == "Components":
        component = info["AudioComponents"][0]
        if (component["type"], component["subtype"], component["manufacturer"]) != ("aufx", product["code"], "LsAu"):
            raise ValueError(f"Unexpected Audio Unit identity: {component}")
        if not component.get("sandboxSafe"):
            raise ValueError("Audio Unit is missing sandbox-safe declaration")
    return {"product": product["name"], "project": product["project"], "format": fmt,
            "bundle_id": info["CFBundleIdentifier"], "version": info["CFBundleShortVersionString"],
            "architectures": sorted(arches), "binary_sha256": hashlib.sha256(binary.read_bytes()).hexdigest()}


def stage(args):
    chosen = products(args.projects)
    args.output.mkdir(parents=True, exist_ok=False)
    records = []
    for product in chosen:
        for fmt, suffix in FORMATS.items():
            build_format = "AU" if fmt == "Components" else fmt
            source = args.build / product["project"] / (product["target"] + "_artefacts") / "Release" / build_format / (product["name"] + suffix)
            target = args.output / fmt / source.name
            target.parent.mkdir(exist_ok=True)
            run("ditto", source, target)
            # Ad-hoc signatures permit local validation on Apple silicon.
            # They do not identify a publisher or satisfy notarization.
            run("codesign", "--force", "--sign", "-", target)
            records.append(inspect_bundle(target, product, fmt))
    (args.output / "build-manifest.json").write_text(json.dumps({
        "commit": run("git", "rev-parse", "HEAD", cwd=ROOT).strip(),
        "trust": "ad-hoc development signatures; not notarized",
        "deployment_target": "11.0", "products": records,
        "xcode": run("xcodebuild", "-version").strip(),
        "cmake": run("cmake", "--version").splitlines()[0],
        "juce_commit": run("git", "rev-parse", "HEAD", cwd=args.build / "_deps/juce-src").strip()
    }, indent=2) + "\n")


def validate(args):
    args.logs.mkdir(parents=True, exist_ok=True)
    records = []
    failed = False
    for product in products(args.projects):
        for fmt, suffix in FORMATS.items():
            bundle = args.bundles / fmt / (product["name"] + suffix)
            record = inspect_bundle(bundle, product, fmt)
            commands = [("pluginval", [args.pluginval, "--strictness-level", "5", "--random-seed", "12345",
                        "--sample-rates", "44100,48000,96000", "--block-sizes", "64,128,256,512,1024",
                        "--timeout-ms", "60000", "--validate", bundle.resolve()])]
            if fmt == "Components":
                destination = Path.home() / "Library/Audio/Plug-Ins/Components" / bundle.name
                destination.parent.mkdir(parents=True, exist_ok=True)
                if destination.exists():
                    raise FileExistsError(f"Refusing to replace an installed AU: {destination}")
                run("ditto", bundle, destination)
                commands.insert(0, ("auval", ["auval", "-v", "aufx", product["code"], "LsAu"]))
            record["checks"] = {}
            for label, command in commands:
                log = args.logs / f'{product["project"]}-{fmt}-{label}.txt'
                print(f'Validating {platform.machine()} {product["name"]} {fmt} ({label})', flush=True)
                try:
                    output = run(*command, timeout=300)
                    passed = True
                except subprocess.CalledProcessError as error:
                    output, passed = error.stdout, False
                except subprocess.TimeoutExpired as error:
                    output = str(error.stdout) + "\nVALIDATION TIMED OUT\n"
                    passed = False
                log.write_text(output or "")
                record["checks"][label] = passed
                failed |= not passed
                if not passed:
                    print((output or "")[-6000:], flush=True)
            records.append(record)
    (args.logs / "validation.json").write_text(json.dumps({
        "architecture": platform.machine(), "macos": platform.mac_ver()[0],
        "commit": run("git", "rev-parse", "HEAD", cwd=ROOT).strip(),
        "passed": not failed, "products": records}, indent=2) + "\n")
    if failed:
        raise SystemExit("Mac plugin validation failed; see retained logs")


def package(args):
    # Each build shard is checked natively on both CPU families. Require the
    # evidence to match the exact bytes that will be handed off.
    reports = [json.loads(p.read_text()) for p in args.evidence.rglob("validation.json")]
    commit = run("git", "rev-parse", "HEAD", cwd=ROOT).strip()
    inspected = []
    for product in products():
        for fmt, suffix in FORMATS.items():
            bundle = args.bundles / fmt / (product["name"] + suffix)
            record = inspect_bundle(bundle, product, fmt)
            for arch in sorted(ARCHITECTURES):
                matches = [r for report in reports if report["architecture"] == arch and report["commit"] == commit and report["passed"]
                           for r in report["products"] if r["project"] == product["project"] and r["format"] == fmt
                           and r["binary_sha256"] == record["binary_sha256"] and all(r["checks"].values())]
                if len(matches) != 1:
                    raise ValueError(f'Missing or ambiguous passing {arch} evidence: {product["name"]} {fmt}')
            inspected.append(record)
    (args.bundles / "MAC-BETA-README.txt").write_text((ROOT / "installer/macos/BETA-README.txt").read_text())
    (args.bundles / "manifest.json").write_text(json.dumps({
        "commit": commit, "products": inspected, "native_validation_architectures": sorted(ARCHITECTURES),
        "trust": "ad-hoc signed; NOT notarized; developer preview only",
        "minimum_macos_target": "11.0", "daw_sessions_verified": []}, indent=2) + "\n")
    args.output.mkdir(parents=True, exist_ok=True)
    archive = args.output / f'BE-Digital-Mac-universal-dev-{commit[:8]}.zip'
    if archive.exists():
        raise FileExistsError(archive)
    run("ditto", "-c", "-k", "--sequesterRsrc", "--keepParent", args.bundles, archive)
    (args.output / "SHA256SUMS.txt").write_text(f'{hashlib.sha256(archive.read_bytes()).hexdigest()}  {archive.name}\n')
    print(archive)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    st = sub.add_parser("stage")
    st.add_argument("--build", type=Path, required=True)
    st.add_argument("--output", type=Path, required=True)
    va = sub.add_parser("validate")
    va.add_argument("--bundles", type=Path, required=True)
    va.add_argument("--pluginval", type=Path, required=True)
    va.add_argument("--logs", type=Path, required=True)
    for p in (st, va):
        p.add_argument("--projects", type=lambda value: value.split(";"))
    pa = sub.add_parser("package")
    pa.add_argument("--bundles", type=Path, required=True)
    pa.add_argument("--evidence", type=Path, required=True)
    pa.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if platform.system() != "Darwin":
        parser.error("Mac bundle tools must run on macOS")
    {"stage": stage, "validate": validate, "package": package}[args.command](args)


if __name__ == "__main__":
    main()
