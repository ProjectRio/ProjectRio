#!/usr/bin/env python3
# requirements: cryptography
"""Signs Project Rio releases. See docs/autoupdate_overview.md, "Keys" and "Publishing a release".

  sign-release.py keygen --out <file>
  sign-release.py sign --dir <assets folder> --version <version> --key <private key file>
  sign-release.py verify --dir <assets folder> --pubkey <hex>

Run this on a maintainer's own machine. It must never run in CI, which must never be able to sign.
"""
import argparse
import base64
import getpass
import hashlib
import json
import os
from pathlib import Path
import re
import sys

from cryptography.exceptions import InvalidSignature
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey, Ed25519PublicKey

MANIFEST_NAME = "update_manifest.json"
SIGNATURE_NAME = "update_manifest.sig"
WORKFLOW_PATH = Path(__file__).resolve().parent.parent / ".github" / "workflows" / "main.yml"

# Must match the updater, in Source/Core/UpdaterCommon/UpdateManifest.h and .cpp.
PRODUCT = "ProjectRio"
FORMAT = 1
MAX_FILE_SIZE = 64 * 1024
MAX_SIGNATURE_LINES = 8
KEY_ID_LENGTH = 16

# Counted by the "Draft release" job in .github/workflows/main.yml.
KEY_MARKER = "// rio-update-key"


class SigningError(Exception):
    pass


def public_key_hex(public_key):
    raw = public_key.public_bytes(serialization.Encoding.Raw, serialization.PublicFormat.Raw)
    return raw.hex()


def key_id(public_hex):
    return public_hex[:KEY_ID_LENGTH]


def update_keys_entry(public_hex):
    """Returns the public key as an entry for UPDATE_PUBLIC_KEYS in UpdateKeys.h."""
    values = [f"0x{public_hex[i:i + 2]}" for i in range(0, len(public_hex), 2)]
    rows = [", ".join(values[i : i + 11]) for i in range(0, len(values), 11)]
    body = ",\n         ".join(rows)
    return f"    UpdateManifest::PublicKey{{\n        {{{body}}}}},  {KEY_MARKER}"


def hash_file(path):
    digest = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def hash_assets(directory):
    assets = {}
    for path in sorted(directory.iterdir()):
        if path.is_file() and path.name not in (MANIFEST_NAME, SIGNATURE_NAME):
            assets[path.name] = {"sha256": hash_file(path), "size": path.stat().st_size}
    return assets


def required_assets():
    """Returns the asset names the "Collect release assets" step of the release job produces."""
    try:
        workflow = WORKFLOW_PATH.read_text(encoding="utf-8")
    except OSError as e:
        raise SigningError(f"Could not read {WORKFLOW_PATH}: {e}")
    names = re.findall(r"^\s*one\s+'[^']*'\s+(\S+)\s*$", workflow, re.MULTILINE)
    if not names:
        raise SigningError(f"Found no release assets in {WORKFLOW_PATH}.")
    return names


def serialize_manifest(version, assets):
    manifest = {"assets": assets, "format": FORMAT, "product": PRODUCT, "version": version}
    return (json.dumps(manifest, indent=2, sort_keys=True) + "\n").encode("utf-8")


def parse_signature_line(line):
    """Returns (key id, signature bytes), or None if the line is malformed."""
    parts = line.split(" ")
    if len(parts) != 2:
        return None
    try:
        return parts[0], base64.b64decode(parts[1], validate=True)
    except ValueError:
        return None


def load_private_key(path):
    passphrase = getpass.getpass(f"Passphrase for {path}: ").encode("utf-8")
    try:
        key = serialization.load_pem_private_key(path.read_bytes(), password=passphrase)
    except (OSError, TypeError, ValueError) as e:
        raise SigningError(f"Could not load {path}: {e}")
    if not isinstance(key, Ed25519PrivateKey):
        raise SigningError(f"{path} is not an Ed25519 private key.")
    return key


def command_keygen(args):
    out = Path(args.out)
    if out.exists():
        raise SigningError(f"{out} already exists, refusing to overwrite it.")

    passphrase = getpass.getpass("New passphrase: ")
    if not passphrase:
        raise SigningError("The passphrase can't be empty.")
    if passphrase != getpass.getpass("Repeat the passphrase: "):
        raise SigningError("The passphrases don't match.")

    key = Ed25519PrivateKey.generate()
    pem = key.private_bytes(
        serialization.Encoding.PEM,
        serialization.PrivateFormat.PKCS8,
        serialization.BestAvailableEncryption(passphrase.encode("utf-8")),
    )
    # O_EXCL so that this can't replace a key, even if one appeared after the check above.
    fd = os.open(out, os.O_WRONLY | os.O_CREAT | os.O_EXCL | getattr(os, "O_BINARY", 0), 0o600)
    with os.fdopen(fd, "wb") as f:
        f.write(pem)

    public_hex = public_key_hex(key.public_key())
    print(f"Private key: {out}")
    print(f"Public key:  {public_hex}")
    print(f"Key id:      {key_id(public_hex)}")
    print()
    print("Entry for UPDATE_PUBLIC_KEYS in Source/Core/UpdaterCommon/UpdateKeys.h:")
    print(update_keys_entry(public_hex))


def check_existing_manifest(manifest_bytes, version, assets):
    """Stops if the manifest already in the folder isn't one this key can co-sign as it stands."""
    try:
        manifest = json.loads(manifest_bytes.decode("utf-8"))
    except ValueError as e:
        raise SigningError(f"The existing {MANIFEST_NAME} is not valid: {e}")
    if not isinstance(manifest, dict) or manifest.get("version") != version:
        raise SigningError(f"The existing {MANIFEST_NAME} is not for version {version}.")
    if manifest.get("assets") != assets:
        raise SigningError(f"The files in the folder don't match the existing {MANIFEST_NAME}.")


def command_sign(args):
    directory = Path(args.dir)
    if not directory.is_dir():
        raise SigningError(f"{directory} is not a folder.")
    if args.version.startswith(("v", "V")):
        raise SigningError("--version must not start with a v.")
    if not re.fullmatch(r"\d+(\.\d+)*", args.version):
        raise SigningError("--version must be numbers separated by dots.")

    assets = hash_assets(directory)
    if not args.allow_missing_assets:
        missing = [name for name in required_assets() if name not in assets]
        if missing:
            raise SigningError(f"Missing release assets: {', '.join(missing)}")

    manifest_path = directory / MANIFEST_NAME
    signature_path = directory / SIGNATURE_NAME
    if manifest_path.exists() != signature_path.exists():
        raise SigningError(f"Only one of {MANIFEST_NAME} and {SIGNATURE_NAME} exists.")

    co_signing = manifest_path.exists()
    existing_lines = []
    if co_signing:
        manifest_bytes = manifest_path.read_bytes()
        check_existing_manifest(manifest_bytes, args.version, assets)
        existing_lines = signature_path.read_text(encoding="utf-8").split("\n")
        existing_lines = [line for line in existing_lines if line]
    else:
        manifest_bytes = serialize_manifest(args.version, assets)
    if len(manifest_bytes) > MAX_FILE_SIZE:
        raise SigningError(f"{MANIFEST_NAME} is too large for the updater to accept.")

    key = load_private_key(Path(args.key))
    signature = base64.b64encode(key.sign(manifest_bytes)).decode("ascii")
    public_hex = public_key_hex(key.public_key())
    line = f"{key_id(public_hex)} {signature}"

    if line in existing_lines:
        print(f"Key {key_id(public_hex)} has already signed this release.")
        return
    if len(existing_lines) >= MAX_SIGNATURE_LINES:
        raise SigningError(f"{SIGNATURE_NAME} already has {MAX_SIGNATURE_LINES} signatures.")

    if not co_signing:
        manifest_path.write_bytes(manifest_bytes)
    signature_path.write_bytes(("\n".join(existing_lines + [line]) + "\n").encode("utf-8"))

    action = "Added a signature to" if co_signing else "Wrote"
    print(f"{action} {SIGNATURE_NAME} with key {key_id(public_hex)}, {len(assets)} asset(s).")


def report(ok, name, detail=""):
    print(f"{'PASS' if ok else 'FAIL'}  {name}{'  ' + detail if detail else ''}")
    return ok


def check_signature(directory, public_hex):
    try:
        public_key = Ed25519PublicKey.from_public_bytes(bytes.fromhex(public_hex))
    except ValueError as e:
        raise SigningError(f"Invalid public key: {e}")

    manifest_bytes = (directory / MANIFEST_NAME).read_bytes()
    lines = (directory / SIGNATURE_NAME).read_text(encoding="utf-8").split("\n")
    lines = [line for line in lines if line]
    if len(manifest_bytes) > MAX_FILE_SIZE or len(lines) > MAX_SIGNATURE_LINES:
        return report(False, SIGNATURE_NAME, "too large for the updater to accept")

    for line in lines:
        parsed = parse_signature_line(line)
        if parsed is None or parsed[0] != key_id(public_hex):
            continue
        try:
            public_key.verify(parsed[1], manifest_bytes)
            return report(True, SIGNATURE_NAME, f"signed by {key_id(public_hex)}")
        except InvalidSignature:
            pass
    return report(False, SIGNATURE_NAME, f"no valid signature from {key_id(public_hex)}")


def command_verify(args):
    directory = Path(args.dir)
    manifest_path = directory / MANIFEST_NAME
    if not manifest_path.is_file() or not (directory / SIGNATURE_NAME).is_file():
        raise SigningError(f"{directory} needs both {MANIFEST_NAME} and {SIGNATURE_NAME}.")

    ok = check_signature(directory, args.pubkey.lower())

    try:
        manifest = json.loads(manifest_path.read_bytes().decode("utf-8"))
        version, listed = manifest["version"], manifest["assets"]
        assets = {name: (asset["size"], asset["sha256"].lower()) for name, asset in listed.items()}
        if manifest["format"] != FORMAT or manifest["product"] != PRODUCT:
            raise ValueError("wrong format or product")
    except (AttributeError, KeyError, TypeError, ValueError) as e:
        report(False, MANIFEST_NAME, f"unreadable: {e}")
        return 1
    ok = report(True, MANIFEST_NAME, f"version {version}") and ok

    actual = hash_assets(directory)
    for name, (size, sha256) in sorted(assets.items()):
        if name not in actual:
            ok = report(False, name, "missing") and ok
        else:
            matches = actual[name]["size"] == size and actual[name]["sha256"] == sha256
            ok = report(matches, name, "" if matches else "size or sha256 differs") and ok
    for name in sorted(actual.keys() - assets.keys()):
        ok = report(False, name, "not in the manifest") and ok

    print("Release verified." if ok else "Release did NOT verify.")
    return 0 if ok else 1


def main():
    parser = argparse.ArgumentParser(description="Signs Project Rio releases.")
    commands = parser.add_subparsers(dest="command", required=True)

    keygen = commands.add_parser("keygen", help="create a maintainer's key")
    keygen.add_argument("--out", required=True, help="file to write the encrypted private key to")
    keygen.set_defaults(func=command_keygen)

    sign = commands.add_parser("sign", help="sign a release, or co-sign a signed one")
    sign.add_argument("--dir", required=True, help="folder holding the release assets")
    sign.add_argument("--version", required=True, help="version being released, with no leading v")
    sign.add_argument("--key", required=True, help="private key file")
    sign.add_argument(
        "--allow-missing-assets",
        action="store_true",
        help="skip the check for every release asset. Only for test releases, see "
        "docs/autoupdate_overview.md",
    )
    sign.set_defaults(func=command_sign)

    verify = commands.add_parser("verify", help="check a signed release")
    verify.add_argument("--dir", required=True, help="folder holding the release assets")
    verify.add_argument("--pubkey", required=True, help="public key, as hex")
    verify.set_defaults(func=command_verify)

    args = parser.parse_args()
    try:
        return args.func(args) or 0
    except SigningError as e:
        print(f"error: {e}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
