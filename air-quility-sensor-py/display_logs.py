#!/usr/bin/env python3
"""Plot CO2, temperature, and humidity from the binary air-quality logs."""

import argparse
import sys
from datetime import datetime
from pathlib import Path

import matplotlib.dates as mdates
import matplotlib.pyplot as plt

from parse_logs import SWISS_TIMEZONE, fill_gaps, parse_logs


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
        records = fill_gaps(parse_logs(args.log_dir))
    except (OSError, ValueError) as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 1

    if not len(records):
        print(f"Error: No samples found in {args.log_dir}", file=sys.stderr)
        return 1

    times = [
        datetime.fromtimestamp(timestamp, SWISS_TIMEZONE)
        for timestamp in records["unix_timestamp"]
    ]
    figure, axes = plt.subplots(3, 1, sharex=True, figsize=(12, 8))
    series = (
        ("co2", "CO₂ (ppm)", "tab:blue"),
        ("temp", "Temperature (°C)", "tab:orange"),
        ("humidity", "Humidity (%)", "tab:green"),
    )

    for axis, (field, label, color) in zip(axes, series):
        axis.plot(times, records[field], color=color)
        axis.set_ylabel(label)
        axis.grid(True, alpha=0.3)

    axes[-1].xaxis.set_major_locator(mdates.AutoDateLocator())
    axes[-1].xaxis.set_major_formatter(
        mdates.DateFormatter("%Y-%m-%d\n%H:%M", tz=SWISS_TIMEZONE)
    )
    axes[-1].set_xlabel("Time (Europe/Zurich)")
    figure.suptitle("Air Quality Sensor Logs")
    figure.tight_layout()
    plt.show()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
