# Data Transmission

```{seealso}
For the basic concepts behind data transmission and processing in Constellation
check the [chapter in the operator’s guide](../../operator_guide/concepts/data.md).
```

## Transmitting Data

::::{tab-set}
:::{tab-item} C++
:sync: cxx

Any satellite that wishes to transmit measurement data for storage should inherit from the
{cpp:class}`TransmitterSatellite <constellation::satellite::TransmitterSatellite>` class instead of the regular
{cpp:class}`Satellite <constellation::satellite::Satellite>` class.
This class implements the connection and transmission to data receivers in the Constellation in a transparent way.

Data will only be transmitted in the {bdg-secondary}`RUN` state. It is always preceded by a begin-of-run (BOR) message sent
by the framework after the `starting()` function has successfully been executed, and it is followed by a end-of-run (EOR)
message send automatically after the `stopping()` function has succeeded.

Data is sent in three steps. First, a data record is created, optionally allocating the number of blocks it will contain if
known already. Subsequently, these blocks are added to the message:

```cpp
// Creating a new data message with two blocks pre-allocated:
auto data_record = newDataRecord(2);
data_record.addBlock(std::move(data_0));
data_record.addBlock(std::move(data_1));
```

```{hint}
C++ Move semantics `std::move` are strongly encouraged here in order to avoid copying memory as described below.
```

Finally, the message is sent to the connected receiver:

```cpp
sendDataRecord(std::move(data_record));
```

If the transmitter fails to send the data within a configured time window, an exception is thrown and the satellite
transitions into the {bdg-secondary}`ERROR` state.

It is possible to check if any component in the data transmission chain is data rate limited allowing handle this scenario
on the hardware or software level (e.g. dropping data) by checking if a record can be sent immediately:

```cpp
if(!canSendRecord()) {
  device->set_busy();
}
```

The framework does not drop data itself and a sequence number scheme is implemented to ensure the completeness of the data.
Data can be dropped in the satellite by creating a new data record and discarding the old one. Runs where this happened will
be marked as `INCOMPLETE` in the run metadata.

```{note}
It is not required to check if a record can be sent immediately before every call to to send a data record if no explicit
action is be taken. If a record can not be sent immediately, the sending function will block until the record can be sent or
the run is aborted due to a data sending timeout.
```

:::
:::{tab-item} Python
:sync: python

Any satellite that wishes to transmit measurement data for storage should inherit from the
{py:class}`TransmitterSatellite <core.transmitter_satellite.TransmitterSatellite>` class instead of the regular
{py:class}`Satellite <core.satellite.Satellite>` class.
This class implements the connection and transmission to data receivers in the Constellation in a transparent way.

Data will only be transmitted in the {bdg-secondary}`RUN` state. It is always preceded by a begin-of-run (BOR) message sent
by the framework after the {py:func}`do_starting() <core.satellite.Satellite.do_starting>` function has successfully been
executed, and it is followed by a end-of-run (EOR) message send automatically after the
{py:func}`do_stopping() <core.satellite.Satellite.do_stopping>` function has succeeded.

Data is sent in three steps. First, a data record is created. Subsequently, data blocks are added to the message:

```python
data_record = self.new_data_record()
data_record.add_block(data_0)
data_record.add_block(data_1)
```

Finally, the message is sent to the connected receiver:

```python
self.send_data_record(data_record)
```

If the transmitter fails to send the data within a configured time window, an exception is thrown and the satellite
transitions into the {bdg-secondary}`ERROR` state.

It is possible to check if any component in the data transmission chain is data rate limited allowing handle this scenario
on the hardware or software level (e.g. dropping data) by checking if a record can be sent immediately::

```python
if not self.can_send_record():
    self.device.set_busy()
}
```

The framework does not drop data itself and a sequence number scheme is implemented to ensure the completeness of the data.
Data can be dropped in the satellite by creating a new data record and discarding the old one. Runs where this happened will
be marked as `INCOMPLETE` in the run metadata.

```{note}
It is not required to check if a record can be sent immediately before every call to to send a data record if no explicit
action is be taken. If a record can not be sent immediately, the sending function will block until the record can be sent or
the run is aborted due to a data sending timeout.
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

::::{tab-set}
:::{tab-item} C++
:sync: cxx

Constellation provides the option to attach metadata to each message sent by the satellite. There are three possibilities:

* Metadata available at the beginning of the run such as additional hardware information or firmware revisions can be attached
  to the begin-of-run (BOR) message. This has to be performed in the `starting()` function:

  ```cpp
  void ExampleSatellite::starting(std::string_view /*run_identifier*/) {
      setBORTag("firmware_version", version);
  }
  ```

  In addition to these user-provided tags, the BOR message also contains the full satellite configuration.

* Similarly, for metadata only available at the end of the run such as aggregate statistics, end-of-run (EOR) tags can be set
  in the `stopping()` function:

  ```cpp
  void ExampleSatellite::stopping() {
      setEORTag("total_pixels", pixel_count);
  }
  ```

  In addition to these user-provided tags, the payload of the EOR message contains aggregate data on the run provided by the
  framework such as the total number of messages sent.

* Finally, metadata can be attached to each individual data message sent during the run:

  ```cpp
  // Create a new message
  auto msg = newDataRecord();

  // Add timestamps in picoseconds
  msg.addTag("timestamp_begin", ts_start_pico);
  msg.addTag("timestamp_end", ts_end_pico);
  ```

:::
:::{tab-item} Python
:sync: python

Constellation provides the option to attach metadata in the form of dictionaries
to each message sent by the satellite. There are three possibilities:

* Metadata available at the beginning of the run such as additional hardware information or firmware revisions can be attached
  to the begin-of-run (BOR) message. This has to be performed in the {py:func}`do_starting() <core.satellite.Satellite.do_starting>` method:

  ```python
  def do_starting(self, run_identifier: str) -> str:
      self.bor = {"something": "interesting", "more_important": "stuff"}
      return "Started"
  ```

  In addition to these user-provided tags, the payload of the BOR message contains the full satellite configuration.

* Similarly, for metadata only available at the end of the run such as aggregate statistics, end-of-run (EOR) tags can be set
  in the {py:func}`do_stopping() <core.satellite.Satellite.do_stopping>` method:

  ```python
  def do_stoping(self) -> str:
      self.eor = {"something": "interesting", "more_important": "stuff"}
      return "Stopped"
  ```

* Finally, metadata can be attached to each individual data message sent during the run:

  ```python
  def do_run(self, run_identifier: str) -> str:
      while not self._state_thread_evt.is_set():
          data = np.linspace(0, 2 * np.pi, 1024, endpoint=False)
          tags = {"dtype": str(data.dtype), "other_info": 12345}
          data_record = self.new_data_record(tags)
          data_record.add_block(data.tobytes())
          self.send_data_record(data_record)
      return "Finished run"
   ```

:::
::::
