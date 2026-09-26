# CMDP Extension Types

* Status: draft
* Editor: The Constellation authors

The CMDP Extension Types specification defines structured metric value types for the Constellation Monitoring Distribution Protocol (CMDP).

## Preamble

The key words “MUST”, “MUST NOT”, “REQUIRED”, “SHALL”, “SHALL NOT”, “SHOULD”, “SHOULD NOT”, “RECOMMENDED”, “MAY”, and “OPTIONAL” in this document are to be interpreted as described in [RFC 2119](http://tools.ietf.org/html/rfc2119).

## Goals

This specification defines [MessagePack](https://github.com/msgpack/msgpack/blob/master/spec.md) Extension types for transmitting structured metric data via CMDP.
It extends the metrics data payload defined in [CMDP](https://gitlab.desy.de/constellation/constellation/-/blob/main/docs/protocols/cmdp.md) with typed 2D matrices and histograms for data quality monitoring.

### Related Specifications

* [CMDP](https://gitlab.desy.de/constellation/constellation/-/blob/main/docs/protocols/cmdp.md) defines the monitoring distribution protocol.
* [MessagePack](https://github.com/msgpack/msgpack/blob/master/spec.md) defines the encoding for data structures.

## Implementation

### Extension Type Registry

The following MsgPack Extension type codes are defined:

| Type        | Code   | Description                               |
|-------------|--------|-------------------------------------------|
| Matrix      | `0x01` | Dense 2D array of typed numeric values    |
| Histogram1D | `0x02` | 1D histogram with uniform binning         |
| Histogram2D | `0x03` | 2D histogram with uniform x and y binning |

### Element Data Types

The dtype tag identifies the element type and byte size for matrix and histogram payloads.

| Tag | Type    | Size (bytes) |
|-----|---------|--------------|
| 0   | uint32  | 4            |
| 1   | uint64  | 8            |
| 2   | float32 | 4            |
| 3   | float64 | 8            |

Implementations MUST reject payloads with unrecognized dtype tags.

### Byte Order

All multi-byte numeric fields inside extension payloads MUST use little-endian byte order.
The MsgPack framing itself (extension headers, length fields) remains big-endian as defined by the MsgPack specification.

### Matrix (ext type 0x01)

A Matrix stores a dense 2D array of typed elements in row-major order.
The extension payload SHALL have the following layout:

```text
+--------+--------+--------+--------+--------+--------+--------+--------+--------+========+
| dtype  |             rows (uint32 LE)      |             cols (uint32 LE)      |  data  |
+--------+--------+--------+--------+--------+--------+--------+--------+--------+========+
```

The `dtype` field SHALL be a 1-OCTET element type tag as defined above.
The `rows` and `cols` fields SHALL be uint32 little-endian values representing the matrix dimensions.
The `data` field SHALL contain `rows * cols * sizeof(dtype)` bytes of element data in row-major little-endian order.

The total payload size SHALL be `1 + 4 + 4 + rows * cols * sizeof(dtype)` bytes.
The element at position `(row, col)` is located at byte offset `(row * cols + col) * sizeof(dtype)` within `data`.
An empty matrix (zero rows or zero columns) SHALL have a 9-byte payload with no data section.

### Histogram1D (ext type 0x02)

A Histogram1D stores bin counts with uniform bin edges.
The extension payload SHALL have the following layout:

```text
+--------+--------+--------+--------+--------+--------+--------+--------+
|                          start (float64 LE)                           |
+--------+--------+--------+--------+--------+--------+--------+--------+
|                          end (float64 LE)                             |
+--------+--------+--------+--------+--------+--------+--------+--------+
| dtype  |          nbins (uint32 LE)        |
+--------+--------+--------+--------+--------+
|              bins (nbins * sizeof(dtype) LE)                          |
+========+
```

The `start` field SHALL be a float64 little-endian lower edge of the first bin.
The `end` field SHALL be a float64 little-endian upper edge of the last bin.
The `dtype` field SHALL be a 1-OCTET element type tag as defined above.
The `nbins` field SHALL be a uint32 little-endian bin count.
The `bins` field SHALL contain `nbins * sizeof(dtype)` bytes of little-endian bin counts.

The total payload size SHALL be `8 + 8 + 1 + 4 + nbins * sizeof(dtype)` bytes.
Bin `i` (0-indexed) spans the interval `[start + i * (end - start) / nbins, start + (i+1) * (end - start) / nbins)`.
Underflow and overflow bins are not transmitted.

### Histogram2D (ext type 0x03)

A Histogram2D combines axis metadata with an embedded Matrix extension object.
The extension payload SHALL have the following layout:

```text
+--------+--------+--------+--------+--------+--------+--------+--------+
|                          x_start (float64 LE)                         |
+--------+--------+--------+--------+--------+--------+--------+--------+
|                          x_end (float64 LE)                           |
+--------+--------+--------+--------+--------+--------+--------+--------+
|                          y_start (float64 LE)                         |
+--------+--------+--------+--------+--------+--------+--------+--------+
|                          y_end (float64 LE)                           |
+--------+--------+--------+--------+--------+--------+--------+--------+
|                     matrix (MsgPack ext type 0x01)                    |
+========+
```

The `x_start`, `x_end`, `y_start`, `y_end` fields SHALL be float64 little-endian axis bin edges.
The `matrix` field SHALL be a complete MsgPack extension object of type `0x01` containing the bin count matrix.
The x axis has `matrix.cols` bins and the y axis has `matrix.rows` bins.
The matrix element at `(row, col)` holds the bin count for y bin `row` and x bin `col`.

The total payload size SHALL be `8 + 8 + 8 + 8 + sizeof(matrix ext object)` bytes.

### Metrics Data Payload

The CMDP metrics data payload contains, in order, a metrics value, metric flags, and a unit string.
The metrics value MAY be a MsgPack Extension object with type code `0x01`, `0x02`, or `0x03` as defined above.
Deserializers SHALL distinguish between scalar and structured metrics by checking whether the metrics value is a MsgPack Extension type and dispatching on the type code.
