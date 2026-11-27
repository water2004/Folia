"""Check the shipped Windows packages, including paths embedded by native compilers."""
import argparse
import hashlib
from pathlib import Path
import subprocess
import tempfile
from zipfile import ZipFile


def verify_checksum(package):
    with package.open("rb") as file:
        digest = hashlib.sha256()
        for block in iter(lambda: file.read(1024 * 1024), b""):
            digest.update(block)
    checksum = package.with_name(package.name + ".sha256")
    if checksum.read_text(encoding="utf-8").strip() != f"{digest.hexdigest()}  {package.name}":
        raise ValueError(f"Incorrect SHA-256 sidecar for {package.name}")


def path_signatures(paths):
    signatures = set()
    for path in paths:
        absolute = path.resolve()
        if absolute == Path(absolute.anchor):
            raise ValueError(f"Choose a specific directory to check: {absolute}")
        for spelling in (str(absolute), absolute.as_posix()):
            for encoding in ("utf-8", "utf-16-le"):
                signatures.add(spelling.encode(encoding).lower())
    return signatures


def check_paths(name, data, signatures):
    folded = data.lower()
    if any(signature in folded for signature in signatures):
        raise ValueError(f"Build-machine directory embedded in {name}")


def verify(msi, portable, extra_paths):
    for package in (msi, portable):
        verify_checksum(package)
    repo = Path(__file__).resolve().parents[2]
    signatures = path_signatures([repo, Path.home(), *extra_paths])
    with tempfile.TemporaryDirectory() as directory, ZipFile(portable) as archive:
        target = Path(directory)
        subprocess.run(["msiexec.exe", "/a", str(msi.resolve()), "/qn", "/norestart",
                        f"TARGETDIR={target}"], check=True, timeout=120)
        check_paths("MSI database", (target / msi.name).read_bytes(), signatures)
        installed = {}
        for root, prefix in ((target / "LocalApp/Programs/Folia", ""),
                             (target / "LocalApp/Folia/Assets", "Assets/")):
            for file in root.rglob("*"):
                if file.is_file():
                    installed[prefix + file.relative_to(root).as_posix()] = file
        entries = [entry for entry in archive.infolist() if not entry.is_dir()]
        names = {entry.filename for entry in entries}
        if len(entries) != len(names) or names != installed.keys():
            raise ValueError(f"Package inventories differ or contain duplicates: {sorted(names ^ installed.keys())}")
        if not {"Folia.exe", "folia_svg_normalizer.dll", "FoliaNativeMermaid.dll"} <= names:
            raise ValueError("Missing application or native renderer")
        # The exe encodes the installation layout. WinUI builds its metadata and
        # resource index separately for each executable. All other files must match.
        generated = {"Folia.exe", "Folia.pri", "Folia.winmd"}
        for entry in entries:
            name = entry.filename
            packaged = archive.read(entry)  # also checks this ZIP member's CRC
            extracted = installed[name].read_bytes()
            check_paths(f"ZIP/{name}", packaged, signatures)
            if packaged != extracted:
                check_paths(f"MSI/{name}", extracted, signatures)
                if name not in generated:
                    raise ValueError(f"Contents differ between distributions: {name}")
    print(f"Verified SHA-256, matching inventories and build-machine path checks for {len(entries)} files in each package.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("msi", type=Path)
    parser.add_argument("portable_zip", type=Path)
    parser.add_argument("--forbid-path", type=Path, action="append", default=[])
    args = parser.parse_args()
    verify(args.msi, args.portable_zip, args.forbid_path)
