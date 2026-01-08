"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

Provides the class for the Influx satellite
"""

from threading import Lock

from influxdb_client.client.influxdb_client import InfluxDBClient
from influxdb_client.client.write.point import Point
from influxdb_client.client.write_api import WriteOptions

from constellation.core.cmdp import Metric
from constellation.core.configuration import Configuration
from constellation.core.monitoring import StatListener
from constellation.core.satellite import Satellite


class Influx(Satellite, StatListener):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._influxdb_connected = False
        self._influxdb_lock = Lock()

    def do_initializing(self, config: Configuration) -> None:
        with self._influxdb_lock:
            self._influxdb_connected = False

            url = config.get("url", "http://localhost:8086", return_type=str)
            token = config.get("token", return_type=str)
            org = config.get("org", return_type=str)
            self.bucket = config.get("bucket", "constellation", return_type=str)
            interval = int(config.get_num("flush_interval", 2.5, min_val=1) * 1000)

            self.client = InfluxDBClient(url=url, token=token, org=org)
            self.write_api = self.client.write_api(write_options=WriteOptions(flush_interval=interval))

            # Test connection
            self.log.debug('Connecting to InfluxDB with org "%s" at %s', org, url)
            try:
                query_api = self.client.query_api()
                query_api.query(org=org, query="buckets()")
            except Exception as e:
                raise ConnectionError("Could not connect to InfluxDB") from e

            self._influxdb_connected = True
            self.log.info("Connected to InfluxDB")

    def metric_callback(self, metric: Metric) -> None:
        super().metric_callback(metric)

        with self._influxdb_lock:
            if self._influxdb_connected:
                if isinstance(metric.value, (float, int, bool, str)):
                    record = Point(metric.sender).field(metric.name, metric.value).time(metric.time.to_datetime())
                    self.log.trace("Writing metric %s to InfluxDB", metric.name)
                    self.write_api.write(bucket=self.bucket, record=record)
                else:
                    self.log.debug(f"Metric of type {type(metric.value).__name__} cannot be written to InfluxDB")
