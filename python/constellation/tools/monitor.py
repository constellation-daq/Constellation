"""
SPDX-FileCopyrightText: 2026 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2
"""

import logging
import pathlib
import socket
from datetime import datetime
from logging.handlers import RotatingFileHandler
from typing import Any

from constellation.core.base import EPILOG, ConstellationArgumentParser
from constellation.core.listener import MonitoringListener, StandaloneListener
from constellation.core.logging import setup_cli_logging
from constellation.core.protocol.cmdp1 import LogLevel, Metric


def check_output_path(output_path: pathlib.Path) -> pathlib.Path:
    output_path = output_path.resolve(strict=False)
    if output_path.exists():
        if not output_path.is_dir():
            raise FileExistsError(f"Output path {output_path} already exists and is not a directory")
    else:
        output_path.mkdir(parents=True)
    return output_path.resolve(strict=True)


def monitor_sender() -> str:
    return f"Monitor.{socket.gethostname()}"


class SenderLogFilter(logging.Filter):
    def filter(self, record: logging.LogRecord) -> bool:
        if not hasattr(record, "sender"):
            # Only show warning and above
            if record.levelno < LogLevel.WARNING.value:
                return False
            setattr(record, "sender", monitor_sender())  # noqa: B010
        return True


class EscapeFormatter(logging.Formatter):
    def format(self, record: logging.LogRecord) -> str:
        if not hasattr(record, "escaped"):
            record.msg = record.msg.replace('"', '""').replace("\n", "\\n")
            setattr(record, "escaped", True)  # noqa: B010
        return super().format(record)


def create_file_handler(output_path: pathlib.Path, backup_count: int, file_level: str) -> RotatingFileHandler:
    # Create rotating file handler
    file_handler = RotatingFileHandler(
        output_path / "log.csv",
        maxBytes=10**7,  # 10 MB
        backupCount=backup_count,
    )
    file_handler.setLevel(file_level)

    # Format csv-style
    file_handler.addFilter(SenderLogFilter())
    file_handler.setFormatter(
        EscapeFormatter(
            '{asctime}.{msecs:03.0f},{levelname},{sender},{name},"{message}"', style="{", datefmt="%Y-%m-%dT%H:%M:%S"
        )
    )

    return file_handler


def generate_log_topics(min_level: LogLevel) -> list[str]:
    log_topics = []
    for level in LogLevel:
        if level.value >= min_level.value:
            log_topics.append(f"LOG/{level.name}")
    return log_topics


def adapt_default_handlers() -> None:
    # Console handler is the first handler
    console_handler = logging.root.handlers[0]

    # Add sender to log format
    console_handler.addFilter(SenderLogFilter())
    console_handler.setFormatter(logging.Formatter("[{sender}][{name}] {message}", style="{"))

    # CMDP handler is the second handler
    cmdp_handler = logging.root.handlers[1]

    # Remove CMDP handler
    logging.root.removeHandler(cmdp_handler)


class Monitor(StandaloneListener, MonitoringListener):
    def __init__(self, name: str, group: str, interface: list[str] | None, output_path: pathlib.Path | None):
        super().__init__(name, group, interface)
        self.output_path = output_path

    def receive_log(self, record: logging.LogRecord) -> None:
        self.log.handle(record)

    def receive_metric(self, sender: str, metric: Metric, timestamp: datetime, value: Any) -> None:
        self.log.info(
            f"Received {metric.name} metric from {sender} with value {value}{metric.unit}",
            extra={"sender": monitor_sender()},
        )
        if self.output_path is not None:
            path = self.output_path / f"{sender}.{metric.name.lower()}.csv"
            with open(path, "a", encoding="utf-8") as csv:
                csv.write(f"{timestamp.timestamp()}, {value}, '{metric.unit}'\n")


def main(args=None) -> None:
    # Add argument specific for Monitor to parser
    parser = ConstellationArgumentParser(description=main.__doc__, epilog=EPILOG)
    parser.add_argument(
        "-o",
        "--output-path",
        type=pathlib.Path,
        required=False,
        help="Optional path to write log and metric data to.",
    )
    parser.add_argument(
        "--file-level",
        choices=["TRACE", "DEBUG", "INFO", "WARNING", "STATUS", "CRITICAL"],
        default="DEBUG",
        type=str.upper,
        help="The maximum level of log messages to written to the file.",
    )
    parser.add_argument(
        "--backup-count",
        type=int,
        default=10,
        help="The backup count of log files to keep.",
    )
    # Get a dict of the parsed arguments
    args = vars(parser.parse_args(args))

    # Pop argument specific for Monitor
    output_path: pathlib.Path | None = args.pop("output_path")
    file_level: str = args.pop("file_level")
    backup_count: int = args.pop("backup_count")

    # Set up logging
    level = args.pop("level")
    setup_cli_logging(level)

    # Add file handler to root handlers
    if output_path is not None:
        output_path = check_output_path(output_path)
        file_handler = create_file_handler(output_path, backup_count, file_level)
        logging.root.addHandler(file_handler)

    # Start Monitor with remaining args
    mon = Monitor(**args, output_path=output_path)

    # Set topics to listen to
    min_level = LogLevel[level.upper()]
    if output_path is not None:
        min_level = min(min_level, LogLevel[file_level.upper()])
    mon.set_topics(["STAT/"] + generate_log_topics(min_level))

    # Adapt default logging handlers
    adapt_default_handlers()

    # Run listener
    mon.run_listener()


if __name__ == "__main__":
    main()
