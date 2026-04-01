# Data Transmission

```{seealso}
For the basic concepts behind data transmission and processing in Constellation
check the [chapter in the operator’s guide](../../operator_guide/concepts/data.md).
```

## Transmitting Data

Any satellite that wishes to transmit measurement data for storage has to inherit from the
{cpp:class}`TransmitterSatellite <constellation::satellite::TransmitterSatellite>` /
{py:class}`TransmitterSatellite <core.transmitter_satellite.TransmitterSatellite>` class instead of the regular
{cpp:class}`Satellite <constellation::satellite::Satellite>` / {py:class}`Satellite <core.satellite.Satellite>` class.
This class implements the connection and transmission to data receivers in the Constellation in a transparent way.

For Python, the main function for the satellite should also use the {py:class}`TransmitterSatelliteArgumentParser <core.transmitter_satellite.TransmitterSatelliteArgumentParser>` instead of the default {py:class}`SatelliteArgumentParser <core.satellite.SatelliteArgumentParser>`.

Data will only be transmitted in the {bdg-secondary}`RUN` state. It is always preceded by a begin-of-run (BOR) message sent
by the framework after the `starting()` / {py:func}`do_starting() <core.satellite.Satellite.do_starting>` function has
successfully been executed, and it is followed by a end-of-run (EOR) message send automatically after the `stopping()` /
{py:func}`do_stopping() <core.satellite.Satellite.do_stopping>` function has succeeded.

Data is sent in three steps. First, a data record is created.
If known already, C++ offers the possibility to pre-allocate the number of blocks it will contain.
Subsequently, these blocks are added to the record:

::::{tab-set}
:::{tab-item} C++
:sync: cxx

```cpp
// Creating a new data record with two blocks pre-allocated:
auto data_record = newDataRecord(2);
data_record.addBlock(std::move(data_0));
data_record.addBlock(std::move(data_1));
```

:::
:::{tab-item} Python
:sync: python

```python
data_record = self.new_data_record()
data_record.add_block(data_0)
data_record.add_block(data_1)
```

:::
::::


```{hint}
For C++, move semantics `std::move` are strongly encouraged here in order to avoid copying memory as described below.
```

Finally, the record is sent to the connected receiver:

::::{tab-set}
:::{tab-item} C++
:sync: cxx

```cpp
sendDataRecord(std::move(data_record));
```

:::
:::{tab-item} Python
:sync: python

```python
self.send_data_record(data_record)
```

:::
::::


If the transmitter fails to send the data within a configured time window, an exception is thrown and the satellite
transitions into the {bdg-secondary}`ERROR` state.

It is possible to check if any component in the data transmission chain is data rate limited allowing handle this scenario
on the hardware or software level (e.g. dropping data) by checking if a record can be sent immediately:

::::{tab-set}
:::{tab-item} C++
:sync: cxx

```cpp
if(!canSendRecord()) {
  device->set_busy();
}
```

:::
:::{tab-item} Python
:sync: python

```python
if not self.can_send_record():
    self.device.set_busy()
```

:::
::::


The framework does not drop data itself and a sequence number scheme is implemented to ensure the completeness of the data.
Data can be dropped in the satellite by creating a new data record and discarding the old one. Runs where this happened will
be marked as `INCOMPLETE` in the run metadata.

```{note}
It is not required to check if a record can be sent immediately before every call to to send a data record if no explicit
action is be taken. If a record can not be sent immediately, the sending function will block until the record can be sent or
the run is aborted due to a data sending timeout.
```

## *Not* Transmitting Data

In some cases, data transmission might not be intended at all times.
Satellite operators are able to switch off data transmission from any satellite using the `_data.disable_transmission` configuration key.
However, sometimes it is necessary to switch off data transmission programmatically.
An example would be a satellite that, depending on its configuration, controls an instrument that produces data, like an ADC, or one that does not such as a power supply.

For this purpose, the transmission of data can be forced off in the satellite code by calling

::::{tab-set}
:::{tab-item} C++
:sync: cxx

```cpp
disable_data_transmission();
```

:::
:::{tab-item} Python
:sync: python

```python
# TODO
```

:::
::::


## Data Format & Performance

Constellation makes no assumption on the data stored in data records. All data is stored in block, handled as binary blob
and transmitted as such. A data record can contain any number of blocks.

::::{tab-set}
:::{tab-item} C++
:sync: cxx

The {cpp:func}`DataRecord::addBlock() <constellation::satellite::TransmitterSatellite::DataRecord::addBlock()>` method takes so-called payload buffer as argument.
Consequently, the data to be transmitted has to be converted into such a {cpp:class}`PayloadBuffer <constellation::message::PayloadBuffer>`.
For the most common C++ ranges like `std::vector` or `std::array`, moving the object into the payload buffer with `std::move()` is sufficient.

:::
:::{tab-item} Python
:sync: python

The {py:func}`DataRecord.add_block() <core.message.cdtp2.DataRecord.add_block>` method takes a {py:class}`bytes` object as argument.

```{tip}
If the data to be sent is stored in a numpy array, it can be converted into a {py:class}`bytes` object using the `tobytes()` method.
```

:::
::::

```{seealso}
Data rate benchmarks can be found in the application developer guide under
[Increase Data Rate in C++](..//howtos/data_transmission_speed.md).
```

## Metadata

Constellation provides the option to attach metadata to each record sent by the satellite. There are three possibilities:

* Metadata available at the beginning of the run such as additional hardware information or firmware revisions can be attached
  to the begin-of-run (BOR) message. This has to be performed in the `starting()` / {py:func}`do_starting() <core.satellite.Satellite.do_starting>` function:

  ::::{tab-set}
  :::{tab-item} C++
  :sync: cxx

  ```cpp
  void ExampleSatellite::starting(std::string_view /*run_identifier*/) {
      setBORTag("firmware_version", version);
  }
  ```

  :::
  :::{tab-item} Python
  :sync: python

  ```python
  def do_starting(self, run_identifier: str) -> str:
      self.bor = {"something": "interesting", "more_important": "stuff"}
      return "Started"
  ```

  :::
  ::::

  In addition to these user-provided tags, the BOR message also contains the full satellite configuration.

* Similarly, for metadata only available at the end of the run such as aggregate statistics, end-of-run (EOR) tags can be set
  in the `stopping()` / {py:func}`do_stopping() <core.satellite.Satellite.do_stopping>` function:

  ::::{tab-set}
  :::{tab-item} C++
  :sync: cxx

  ```cpp
  void ExampleSatellite::stopping() {
      setEORTag("total_pixels", pixel_count);
  }
  ```

  :::
  :::{tab-item} Python
  :sync: python

  ```python
  def do_stopping(self) -> str:
      self.eor = {"something": "interesting", "more_important": "stuff"}
      return "Stopped"
  ```

  :::
  ::::

  In addition to these user-provided tags, the payload of the EOR message contains aggregate data on the run provided by the
  framework such as the total number of records sent.

* Finally, metadata can be attached to each individual data record sent during the run:

  ::::{tab-set}
  :::{tab-item} C++
  :sync: cxx

  ```cpp
  // Create a new record
  auto msg = newDataRecord();

  // Add timestamps in picoseconds
  msg.addTag("timestamp_begin", ts_start_pico);
  msg.addTag("timestamp_end", ts_end_pico);
  ```

  :::
  :::{tab-item} Python
  :sync: python

  ```python
  def do_run(self) -> str:
      while not self.stop_requested():
          data = np.linspace(0, 2 * np.pi, 1024, endpoint=False)
          tags = {"dtype": str(data.dtype), "other_info": 12345}
          data_record = self.new_data_record(tags)
          data_record.add_block(data.tobytes())
          self.send_data_record(data_record)
      return "Finished run"
  ```

  :::
  ::::
