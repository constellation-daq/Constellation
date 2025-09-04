# Increase Data Rate in C++

Data transmission speed over the network does not only depend on the data throughout, but also on the TCP message throughput.
As the payload for each message gets smaller, the data transmission speed typically decreases as the overhead of sending
a TCP message becomes relevant.

Since data from detectors is often fairly small, Constellation is optimized for small data sizes (1KiB) by grouping data
together to larger messages. This grouping is implemented directly in the framework and the grouped data is split again
before being stored, thus being entirely transparent when implementing a new satellite.

```{figure} Data_Rate_vs_Block_Size.svg
Benchmark Data Rate vs Block Size
```

For all but the most extreme scenarios, Constellation can saturate a 10G network link using an optimized C++ build and a
custom memory allocator like [jemalloc](https://jemalloc.net/) or [mimalloc](https://microsoft.github.io/mimalloc/).
While mimalloc is enabled by default, jemalloc provides slightly better performance but has to be installed separately.

An optimized build using jemalloc can be created via:

```sh
meson setup build_opt -Dbuildtype=release -Db_lto=true -Dc_args=-march=native -Dcpp_args=-march=native -Dmemory_allocator=jemalloc -Dwrap_mode=forcefallback -Dcxx_tests=disabled
```

```{note}
When building external satellites using Meson, it is important to set the `buildtype`, `b_lto`, `c_args` and `cpp_args`
options in that project as well. The custom memory allocator will be used automatically when Constellation is built with
that option.
```
