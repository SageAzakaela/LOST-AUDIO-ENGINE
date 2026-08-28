from __future__ import annotations

import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent
DIST = ROOT / "dist"
OUT_ZIP = DIST / "LostAudioEngine-web-itchio.zip"

INCLUDE = [
    "index.html",
    "styles.css",
    "README.md",
    "audio",
    "src",
    "lame",
    "occlusion-engine",
    "television-engine",
    "comms-engine",
    "conference-engine",
    "tape-engine",
    "cartridge-engine",
    "cd-engine",
    "camcorder-engine",
]


def iter_files(rel: str):
    p = ROOT / rel
    if p.is_file():
        yield p
        return
    if not p.is_dir():
        return
    for f in p.rglob("*"):
        if f.is_file():
            yield f


def main() -> int:
    DIST.mkdir(exist_ok=True)
    files: list[Path] = []
    for rel in INCLUDE:
        files.extend(iter_files(rel))

    files = sorted({f.resolve() for f in files})
    if not files:
        raise SystemExit("No files to zip")

    with zipfile.ZipFile(OUT_ZIP, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for f in files:
            arc = f.relative_to(ROOT).as_posix()
            z.write(f, arcname=arc)

    with zipfile.ZipFile(OUT_ZIP, "r") as z:
        names = z.namelist()
        bad = sum("\\" in n for n in names)
        if bad:
            raise SystemExit(f"Zip contains {bad} entries with backslashes")

    print(f"Wrote {OUT_ZIP} ({len(files)} files)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
