#!/usr/bin/env python3
"""Download files from the ESP32 LittleFS file server."""

import argparse
import json
import os
import posixpath
import sys
import tempfile
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path
from typing import Any


SERVER_URL = "http://10.166.234.48"
CHUNK_SIZE = 64 * 1024


def api_url(server_url: str, endpoint: str, remote_path: str) -> str:
    query = urllib.parse.urlencode({"path": remote_path})
    return f"{server_url.rstrip('/')}{endpoint}?{query}"


def list_directory(server_url: str, remote_path: str, timeout: float) -> list[dict[str, Any]]:
    url = api_url(server_url, "/api/files", remote_path)
    with urllib.request.urlopen(url, timeout=timeout) as response:
        payload = json.loads(response.read())

    if not isinstance(payload, dict) or not isinstance(payload.get("files"), list):
        raise ValueError(f"Invalid directory listing returned for {remote_path}")

    entries = payload["files"]
    for entry in entries:
        if (
            not isinstance(entry, dict)
            or not isinstance(entry.get("name"), str)
            or not isinstance(entry.get("isDir"), bool)
        ):
            raise ValueError(f"Invalid entry in directory listing for {remote_path}")
    return entries


def normalize_remote_path(path: str) -> str:
    if not path.startswith("/"):
        raise ValueError(f"Invalid LittleFS path: {path!r}")

    parts = path.split("/")[1:]
    if any(part in (".", "..") for part in parts):
        raise ValueError(f"Invalid LittleFS path: {path!r}")

    return "/" + "/".join(part for part in parts if part)


def get_remote_entries(server_url: str, timeout: float) -> list[str]:
    pending = ["/"]
    visited: set[str] = set()
    files: list[str] = []

    while pending:
        directory = pending.pop()
        if directory in visited:
            continue
        visited.add(directory)

        for entry in list_directory(server_url, directory, timeout):
            name = entry["name"]
            remote_path = normalize_remote_path(
                name if name.startswith("/") else posixpath.join(directory, name)
            )
            parent = posixpath.dirname(remote_path.rstrip("/")) or "/"
            if parent != directory:
                raise ValueError(f"Server returned a non-immediate entry: {name!r}")

            if entry["isDir"]:
                pending.append(remote_path)
            else:
                files.append(remote_path)

    return files


def local_path_for_remote(output_dir: Path, remote_path: str) -> Path:
    relative_parts = remote_path.strip("/").split("/")
    if not relative_parts or relative_parts == [""]:
        raise ValueError(f"Invalid file path returned by server: {remote_path!r}")

    root = output_dir.resolve()
    destination = root.joinpath(*relative_parts)
    if not destination.resolve().is_relative_to(root):
        raise ValueError(f"File path escapes the download directory: {remote_path!r}")
    return destination


def download_file(
    server_url: str, remote_path: str, destination: Path, timeout: float
) -> bool:
    url = api_url(server_url, "/api/files/read", remote_path)
    destination.parent.mkdir(parents=True, exist_ok=True)

    with urllib.request.urlopen(url, timeout=timeout) as response:
        content_length_header = response.headers.get("Content-Length")
        expected_size = None
        if content_length_header is not None:
            try:
                expected_size = int(content_length_header)
            except ValueError as exc:
                raise ValueError(
                    f"Invalid Content-Length for {remote_path}: {content_length_header!r}"
                ) from exc
            if expected_size < 0:
                raise ValueError(f"Invalid Content-Length for {remote_path}: {expected_size}")

        if (
            expected_size is not None
            and destination.is_file()
            and destination.stat().st_size == expected_size
        ):
            return False

        temporary_path: str | None = None
        try:
            with tempfile.NamedTemporaryFile(
                mode="wb", dir=destination.parent, prefix=f".{destination.name}.", delete=False
            ) as temporary_file:
                temporary_path = temporary_file.name
                downloaded_size = 0
                while chunk := response.read(CHUNK_SIZE):
                    temporary_file.write(chunk)
                    downloaded_size += len(chunk)

            if expected_size is not None and downloaded_size != expected_size:
                raise OSError(
                    f"Incomplete download for {remote_path}: expected "
                    f"{expected_size} bytes, received {downloaded_size}"
                )

            if destination.is_file() and destination.stat().st_size == downloaded_size:
                return False

            os.replace(temporary_path, destination)
            temporary_path = None
            return True
        finally:
            if temporary_path is not None:
                os.unlink(temporary_path)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--server", default=SERVER_URL, help="ESP32 server base URL")
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path("log_files"),
        help="local download directory (default: ./log_files)",
    )
    parser.add_argument("--timeout", type=float, default=30.0, help="HTTP timeout in seconds")
    args = parser.parse_args()

    try:
        remote_files = get_remote_entries(args.server, args.timeout)
        downloaded = 0
        skipped = 0
        for remote_path in remote_files:
            destination = local_path_for_remote(args.output_dir, remote_path)
            if download_file(args.server, remote_path, destination, args.timeout):
                downloaded += 1
                print(f"Downloaded: '{remote_path}' (size: {destination.stat().st_size} bytes)")
            else:
                skipped += 1
                print(f"Already present (same size): '{remote_path}' (size: {destination.stat().st_size} bytes)")
        print(f"Done: {downloaded} downloaded, {skipped} skipped.")
    except (OSError, ValueError, urllib.error.URLError) as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
