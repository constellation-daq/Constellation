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

Data will only be transmitted in the `RUN` state. It is always preceded by a begin-of-run (BOR) message sent by the framework
after the `starting()` function has successfully been executed, and it is followed by a end-of-run (EOR) message send
automatically after the `stopping()` function has succeeded.

Data messages are created and sent in three steps. First, the data message is created, optionally allocating the number of
frames it will contain if known already. Subsequently, these frames are added to the message:

```cpp
// Creating a new data message with two frames pre-allocated:
auto msg = newDataMessage(2);
msg.addFrame(std::move(frame0));
msg.addFrame(std::move(frame1));
```

```{hint}
C++ Move semantics `std::move` are strongly encouraged here in order to avoid copying memory as described below.
```

Finally, the message is send to the connected receiver via one of the following two methods:

* The data can be sent with a pre-configured timeout. If the transmitter fails to send the data within this configured time
  window, an exception is thrown and the satellite transitions into the `ERROR` state. This is the most commonly used method
  of transmitting data and ensuring that there is no data loss.

  ```cpp
  sendDataMessage(msg);
  ```

* The second option is to handle potential issues in transmitting the data in satellite code. In this case, the message
  should be sent via

  ```cpp
  auto sent = trySendDataMessage(msg);
  ```

  The boolean return value indicates if the sending was successful or failed. Either another attempt of sending the message
  can be undertaken, or the message can be discarded. It should be noted that the {cpp:func}`trySendDataMessage() <constellation::satellite::TransmitterSatellite::trySendDataMessage()>` method is annotated
  with the `[[nodiscard]]` keyword, indicating that the return value cannot be discarded and *has* to be used.

Data messages contain a header with the canonical name of the sending satellite, the current system time when creating the
message and a continuous sequence number. This means there is no need to separately count messages in user code.

:::
:::{tab-item} Python
:sync: python

Any satellite that wishes to send data should inherit from the `DataSender`
class instead of from the `Satellite` base class. This will provide additional
facilities to queue data which will be transmitted via CDTP to a `DataReceiver`
such as the `H5DataWriter`. The transmission is limited to the `RUN` state,
however. Any data sending must therefore be limited to the `do_run` method:

```python
class RandomDataSender(DataSender):
    """Satellite which sends a numpy array with random values via CDTP."""

    def do_run(self, run_identifier: str) -> str:
        """Example implementation that generates random values."""
        # Timestamp when started
        t0 = time.time_ns()
        # Track number of packets sent
        num = 0

        # Run loop
        while not self._state_thread_evt.is_set():
            # Calculate our demo samples.
            # In a real application, this data could
            # e.g. come from a hardware device.
            samples = np.linspace(0, 2 * np.pi, 1024, endpoint=False)
            fs = random.uniform(0, 3)
            data_load = np.sin(2 * np.pi * fs * samples)
            # Put our data samples into the queue:
            self.data_queue.put((data_load.tobytes(), {"dtype": f"{data_load.dtype}"}))
            if num%10 == 0:
                self.log_cdtp_s.debug(f"Queueing data packet {num}")
            num += 1
            time.sleep(0.25)

        t1 = time.time_ns()
        self.log_cdtp_s.info(f"total time for {num} evt / {num * len(data_load) / 1024 / 1024}MB: {(t1 - t0) / 1000000000}s")
        return "Finished acquisition"
```

This example implements a satellite that sends arrays of random data. In order
to queue it for transmission, the data have to be inserted into a Queue that is
available as `self.data_queue` as a tuple consisting of the actual data and the
metadata (a dictionary).

```{note}
It should be noted that the data need to be provided as `bytes`, or as `list[bytes]` if it should be send in multiple frames.
See notes on the data format and performance below for more information.
```

The full call therefore becomes:

```python
self.data_queue.put(tuple(data: bytes, meta: dict[str, Any]))
```

More information on the metadata can be found in the final section of this document.

:::
::::

## Data Format & Performance

::::{tab-set}
:::{tab-item} C++
:sync: cxx

Constellation makes no assumption on the data stored in message frames. All data is stored in frames, handled as binary blob
and transmitted as such. The message frames of data messages are designed for minimum data copy and maximum speed.
A data message can contain any number of frames.

The {cpp:func}`DataMessage::addFrame() <constellation::satellite::TransmitterSatellite::DataMessage::addFrame()>` function takes so-called payload buffer as argument.
Consequently, the data to be transmitted has to be converted into such a {cpp:class}`PayloadBuffer <constellation::message::PayloadBuffer>`.
For the most common C++ ranges like `std::vector` or `std::array`, moving the object into the payload buffer with `std::move()` is sufficient.

```{seealso}
Since the data transmission protocol as well as the event metadata come with additional overhead, the largest data throughput
depends on the frame size as well as on the number of frames transmitted by a single message. For performance considerations,
it is advised to read [Increase Data Rate in C++](../howtos/data_transmission_speed.md).
```

:::
:::{tab-item} Python
:sync: python

Constellation makes no assumption on the data stored in message frames. All data
is stored in frames, handled as binary blob and transmitted as such. The message
frames of data messages are designed for minimum data copy and maximum speed. A
data message can contain any number of frames.

When storing data in numpy arrays, the `to_bytes` method can be used to convert the array into `bytes`.

If the data should be sent as multiple frames, a list of bytes has to be added into the `data_queue`.

```{caution}
Sending many small packets over the network often suffers from a performance penalty due to the additional overhead created
by the protocol as well as the TCP/IP communication. It should be consider to use multiple frames or to concatenate the data
into larger binary blobs. A future version of the CDTP protocol is currently under development that will automatically
assemble multiple small messages into larger ones to optimize performance.
```


:::
::::

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

  In addition to these user-provided tags, the payload of the BOR message contains the full satellite configuration.

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
  auto msg = newDataMessage();

  // Add timestamps in picoseconds
  msg.addTag("timestamp_begin", ts_start_pico);
  msg.addTag("timestamp_end", ts_end_pico);
  ```

:::
:::{tab-item} Python
:sync: python

Constellation provides the option to attach metadata in the form of dictionaries
to each message sent by the satellite. There are three possibilities:

* Metadata available at the beginning of the run such as additional hardware
  information or firmware revisions can be attached to the begin-of-run (BOR)
  message. This can be done at any time but latest in the `do_starting` method:

  ```python
      def do_starting(self, run_identifier:str) -> str:
        self.BOR = {"something": "interesting", "more_important": "stuff"}
        return "Prepared!"

  ```

  In addition to these user-provided tags, the payload of the BOR message contains the full satellite configuration.

* Similarly, for metadata only available at the end of the run such as aggregate
  statistics, end-of-run (EOR) tags can be set latest in the `do_stopping` method:

  ```python
      def do_stoping(self) -> str:
        self.BOR = {"something": "interesting", "more_important": "stuff"}
        return "Prepared!"

  ```

* Finally, metadata can be attached to each individual data message sent during the run:

  ```python
       def do_run(self, run_identifier: str) -> str:
        while not self._state_thread_evt.is_set():
            data = np.linspace(0, 2 * np.pi, 1024, endpoint=False)
            meta = {"dtype": f"{data_load.dtype}", "other_info": 12345}
            self.data_queue.put((data.tobytes(), meta))
        return "Finished acquisition"

   ```

:::
::::
