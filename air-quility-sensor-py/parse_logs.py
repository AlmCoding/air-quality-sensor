#!/usr/bin/env python3
"""Parse the binary air-quality logs into a chronologically sorted NumPy array."""

import argparse
import struct
import sys
from datetime import datetime, timedelta
from pathlib import Path
from zoneinfo import ZoneInfo

import numpy as np


MAGIC = 0xA5C3
ENTRIES_PER_BLOCK = 12
BLOCK_SIZE = 8 + ENTRIES_PER_BLOCK * 8
SAMPLE_INTERVAL = timedelta(minutes=5)
SWISS_TIMEZONE = ZoneInfo("Europe/Zurich")
BLOCK_HEADER = struct.Struct("<HHI")
ENTRY = struct.Struct("<HhHH")

RECORD_DTYPE = np.dtype(
    [
        ("unix_timestamp", np.int64),
        ("timestamp", "U25"),
        ("co2", np.uint16),
        ("temp", np.float64),
        ("humidity", np.float64),
        ("open", np.uint16),
    ]
)


def crc16_ccitt_false(data: bytes) -> int:
    """Calculate CRC-16/CCITT-FALSE (poly 0x1021, initial value 0xFFFF)."""
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


def parse_file(path: Path) -> list[tuple[int, str, int, float, float, int]]:
    """Parse all complete blocks in one binary log file."""
    data = path.read_bytes()
    if len(data) % BLOCK_SIZE:
        raise ValueError(
            f"{path}: file size {len(data)} is not a multiple of {BLOCK_SIZE} bytes"
        )

    records = []
    for offset in range(0, len(data), BLOCK_SIZE):
        block = data[offset : offset + BLOCK_SIZE]
        magic, stored_crc, block_timestamp = BLOCK_HEADER.unpack_from(block)
        if magic != MAGIC:
            raise ValueError(
                f"{path}: invalid magic 0x{magic:04X} at byte offset {offset}"
            )
        crc_data = bytearray(block)
        crc_data[2:4] = b"\x00\x00"
        calculated_crc = crc16_ccitt_false(crc_data)
        if stored_crc != calculated_crc:
            raise ValueError(
                f"{path}: CRC mismatch at byte offset {offset} "
                f"(stored 0x{stored_crc:04X}, calculated 0x{calculated_crc:04X})"
            )

        for index in range(ENTRIES_PER_BLOCK):
            co2, temp_x100, humidity_x100, open_seconds = ENTRY.unpack_from(
                block, 8 + index * ENTRY.size
            )
            unix_timestamp = block_timestamp + index * int(
                SAMPLE_INTERVAL.total_seconds()
            )
            sample_time = datetime.fromtimestamp(unix_timestamp, SWISS_TIMEZONE)
            records.append(
                (
                    unix_timestamp,
                    sample_time.isoformat(sep=" "),
                    co2,
                    temp_x100 / 100.0,
                    humidity_x100 / 100.0,
                    open_seconds,
                )
            )

    return records


def parse_logs(log_dir: Path | str | None = None) -> np.ndarray:
    """Parse every ``.bin`` file in a directory and return sorted records.
    The structured array has ``unix_timestamp`` and Swiss-local ``timestamp``
    fields, plus ``co2`` (ppm), ``temp`` (°C), ``humidity`` (%), and ``open``
    (seconds).
    """
    directory = (
        Path(log_dir)
        if log_dir is not None
        else Path(__file__).resolve().parent / "log_files"
    )
    if not directory.is_dir():
        raise FileNotFoundError(f"Log directory does not exist: {directory}")

    records = []
    for path in sorted(directory.glob("*.bin")):
        records.extend(parse_file(path))

    result = np.array(records, dtype=RECORD_DTYPE)
    return np.sort(result, order="unix_timestamp")


def fill_gaps(records: np.ndarray) -> np.ndarray:
    """Insert samples until no gap exceeds 90% of two normal sample intervals.
    Generated samples use zero for sensor values.
    """
    ordered_records = np.sort(records, order="unix_timestamp")
    if len(ordered_records) < 2:
        return ordered_records

    interval_seconds = int(SAMPLE_INTERVAL.total_seconds())
    max_gap_seconds = interval_seconds * 2 * 0.9
    filled_records = []

    for next_record in ordered_records:
        next_timestamp = int(next_record["unix_timestamp"])
        while filled_records and (
            next_timestamp - filled_records[-1][0] > max_gap_seconds
        ):
            timestamp = filled_records[-1][0] + interval_seconds
            timestamp_text = datetime.fromtimestamp(
                timestamp, SWISS_TIMEZONE
            ).isoformat(sep=" ")
            filled_records.append(
                (
                    timestamp,
                    timestamp_text,
                    0, 0.0, 0.0, 0
                )
            )

        filled_records.append(tuple(next_record))

    return np.array(filled_records, dtype=RECORD_DTYPE)


def pretty_time(timestamp: str) -> str:
    """Format a Swiss-local timestamp without seconds."""
    return datetime.fromisoformat(timestamp).strftime("%Y-%m-%d %H:%M")


def pretty_print(records: np.ndarray) -> None:
    """Print records with timestamps formatted to the minute."""
    display_records = records.copy()
    display_records["timestamp"] = [
        pretty_time(timestamp) for timestamp in display_records["timestamp"]
    ]
    print(display_records)


def print_diffs(original_records: np.ndarray, new_records: np.ndarray) -> None:
    """Print each added sample and its distance to the next sample."""
    original_timestamps = set(original_records["unix_timestamp"].tolist())
    for index, record in enumerate(new_records[:-1]):
        if int(record["unix_timestamp"]) not in original_timestamps:
            next_timestamp = int(new_records[index + 1]["unix_timestamp"])
            distance = timedelta(
                seconds=next_timestamp - int(record["unix_timestamp"])
            )
            print(
                f"Added sample {pretty_time(record['timestamp'])}; "
                f"distance to next sample: {distance}"
            )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--log-dir",
        type=Path,
        default=Path(__file__).resolve().parent / "log_files",
        help="directory containing .bin log files",
    )
    args = parser.parse_args()

    try:
        parsed_records = parse_logs(args.log_dir)
        records = fill_gaps(parsed_records)
    except (OSError, ValueError) as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 1

    print(
        f"Parsed {len(parsed_records)} samples from {args.log_dir}; "
        f"{len(records)} samples after gap filling"
    )
    print_diffs(parsed_records, records)
    pretty_print(records)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
