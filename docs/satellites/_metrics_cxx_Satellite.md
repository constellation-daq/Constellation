<!-- markdownlint-disable MD041 -->
### Metrics inherited from `Satellite`

| Metric | Description | Value Type | Interval |
|--------|-------------|------------|----------|
| `RUN_ID` | Current run identifier. Updated whenever it changes. | String | - |
| `STATE`  | Current satellite state. Updated when changed. | String | - |
| `CPU_LOAD_AVG` | CPU load average over the past 1 minute, provided in percent. | Float | 10s |
| `MEM_AVAIL` | Available memory in Megabytes. | Integer | 10s |
