# Receiving Data in C++

This how-to guide describes the concepts of receiving data from other satellites.
It is recommended to read through the [implementation tutorial](../tutorials/satellite_cxx.md) of satellites in C++
to get a good understanding of implementing a basic satellite.

```{seealso}
For the basic concepts behind data transmission and processing in Constellation
check the [chapter in the operator’s guide](../../operator_guide/concepts/data.md).
```

Receiving data with a satellite written in C++ entails inheriting from the `ReceiverSatellite` class.
This class provides all features necessary for an efficient reception and treatment of data.
The following sections details its functions and provide guidance in implementing data handling methods.

## Preparation: Validating Output Paths

In many cases, the receiving satellite will store data to a file on disk.
The `ReceiverSatellite` class provides a few convenience methods which ease this task and provide additional functionality
to the user:

The `validate_output_directory` should be called in the {bdg-secondary}`initializing` method of the receiver satellite.
It will check for the existence and accessibility of the selected storage directory. A `SatelliteError` exception is thrown
upon any issue detected. In addition, this method will register the `DISKSPACE_FREE` metric for the selected storage
location, which will regularly broadcast the available space on the target storage device. In addition, log messages are
emitted on `WARNING` level when the available storage falls below 10 GB, and on `CRITICAL` level when the available disk
space is less than 3 GB.

An example for using the method is given below:

```cpp
void MyWriterSatellite::initializing(Configuration& config) {
    base_path_ = config.getPath("output_directory");
    validate_output_directory(base_path_);
}
```

During the {bdg-secondary}`starting` transition, also the final file name is known, which likely uses the current run
identifier as part of the file name. In order to ensure that the file can be created properly, no other data is overwritten,
and the storage is writable, the `validate_output_file` should be used. It takes the base path, a file name and an extension
as arguments, making it convenient for assembling the final storage path. The function returns the validated canonical path
to the target file.

A possible application could be the following, where the output file path is validated and a binary file stream is opened:

```cpp
void MyWriterSatellite::starting(std::string_view run_identifier) {
    // Open target file
    auto file_path = validate_output_file(base_path_, "data_" + std::string(run_identifier), "raw");
    file_stream_ = std::ofstream(file_path, std::ios_base::binary);
}
```

Alternatively, the `create_output_file` function can be used to validate the target file in the same manner, but directly
create the output file stream and return it. The additional function argument allows selecting whether the file is opened in
binary mode or not. This would reduce the above example to:

```cpp
void MyWriterSatellite::starting(std::string_view run_identifier) {
    // Open target file
    file_stream_ = create_output_file(base_path_, "data_" + std::string(run_identifier), "raw", true);
}
```

Both functions will also update the registered metric `DISKSPACE_FREE` to the latest target storage.

## Handling Data: Callbacks

Data are received through the callbacks `receive_bor`, `receive_data` and `receive_eor` which are purely virtual in the
`ReceiverSatellite` base class and therefore must be implemented by any receiver implementation.

As the names indicate, these callbacks separate between the three message types known to Constellation for data transfer.
The information provided to the callbacks differs:

* The `receive_bor` callback provides access to the sender's name, a dictionary with user-defined and the configuration of
  the sending satellite.
* The `receive_data` callback provides access to the sender's name and the data record.
* Finally, the `receive_eor` callback provides access to to the sender's name, a dictionary with user-defined and the run
  metadata of the sending satellite collected by the framework.

A simplified example implementation of these callbacks can be found below. Here, the callbacks are merely logging information
taken from the received messages, and discard the data.

```cpp
void MyWriterSatellite::receive_bor(std::string_view sender, const Dictionary& user_tags, const Configuration& config) {
    LOG(INFO) << "Received BOR from " << sender << " with config" << config.getDictionary().to_string();
}

void MyWriterSatellite::receive_data(std::string_view sender, const CDTP2Message::DataRecord& data_record) {
    LOG(DEBUG) << "Received data record from " << sender << " with " << data_record.getBlocks() << " blocks";
}

void MyWriterSatellite::receive_eor(std::string_view sender, const Dictionary& user_tags, const Dictionary& run_metadata) {
    LOG(INFO) << "Received EOR from " << sender << " with metadata" << run_metadata.to_string();
}
```
