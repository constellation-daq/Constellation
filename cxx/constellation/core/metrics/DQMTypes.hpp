/**
 * @file
 * @brief DQM extension types
 *
 * @copyright Copyright (c) 2026 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include <msgpack/object_decl.hpp>
#include <msgpack/pack_decl.hpp>
#include <msgpack/sbuffer_decl.hpp>

#include "constellation/build.hpp"

namespace constellation::metrics {

    /// MsgPack extension type codes for DQM types
    constexpr std::int8_t EXT_TYPE_MATRIX = 0x01;
    constexpr std::int8_t EXT_TYPE_HISTOGRAM1D = 0x02;
    constexpr std::int8_t EXT_TYPE_HISTOGRAM2D = 0x03;

    /// Element data type tag for DQM payloads
    enum class DType : std::uint8_t {
        UINT32 = 0,
        UINT64 = 1,
        FLOAT32 = 2,
        FLOAT64 = 3,
    };

    /// Byte size of a single element for a given DType
    CNSTLN_API std::size_t dtype_size(DType dtype);

    /**
     * @brief Dense 2D matrix of typed numeric elements in row-major little-endian layout
     */
    class Matrix {
    public:
        /**
         * @brief Construct a Matrix from typed element data
         *
         * @tparam T Element type (uint32_t, uint64_t, float, double)
         * @param rows Number of rows
         * @param cols Number of columns
         * @param data Span of elements in row-major order
         */
        template <typename T> Matrix(std::uint16_t rows, std::uint16_t cols, std::span<const T> data);

        /**
         * @brief Construct a Matrix from raw bytes with explicit dtype
         *
         * @param rows Number of rows
         * @param cols Number of columns
         * @param dtype Element data type
         * @param data Raw byte buffer in row-major little-endian order
         */
        Matrix(std::uint16_t rows, std::uint16_t cols, DType dtype, std::vector<std::byte> data)
            : rows_(rows), cols_(cols), dtype_(dtype), data_(std::move(data)) {}

        /// Construct an empty matrix
        Matrix() = default;

        std::uint16_t rows() const { return rows_; }
        std::uint16_t cols() const { return cols_; }
        DType dtype() const { return dtype_; }
        std::span<const std::byte> data() const { return data_; }

        /// Total number of elements
        std::size_t size() const { return static_cast<std::size_t>(rows_) * cols_; }

        /// Total byte size of the element data
        std::size_t data_bytes() const { return size() * dtype_size(dtype_); }

        /**
         * @brief Access a single element with bounds checking
         *
         * @tparam T Element type matching the matrix dtype
         * @param row Row index
         * @param col Column index
         * @return Element value
         */
        template <typename T> T element(std::uint16_t row, std::uint16_t col) const;

        /// Summary string for logging
        CNSTLN_API std::string to_string() const;

        /** Assemble as MsgPack ext type */
        CNSTLN_API void msgpack_pack(msgpack::packer<msgpack::sbuffer>& packer) const;

        /** Disassemble from MsgPack ext object */
        CNSTLN_API static Matrix msgpack_unpack(const msgpack::object& obj);

    private:
        std::uint16_t rows_ {0};
        std::uint16_t cols_ {0};
        DType dtype_ {DType::FLOAT64};
        std::vector<std::byte> data_;
    };

    /**
     * @brief 1D histogram with uniform binning and typed bin counts
     */
    class Histogram1D {
    public:
        /**
         * @brief Construct a Histogram1D from typed bin data
         *
         * @tparam T Element type (uint32_t, uint64_t, float, double)
         * @param start Lower edge of the first bin
         * @param end Upper edge of the last bin
         * @param bins Span of bin counts
         */
        template <typename T> Histogram1D(double start, double end, std::span<const T> bins);

        /**
         * @brief Construct a Histogram1D from raw bytes with explicit dtype
         *
         * @param start Lower edge of the first bin
         * @param end Upper edge of the last bin
         * @param dtype Element data type
         * @param nbins Number of bins
         * @param data Raw byte buffer of bin counts in little-endian order
         */
        Histogram1D(double start, double end, DType dtype, std::uint16_t nbins, std::vector<std::byte> data)
            : start_(start), end_(end), dtype_(dtype), nbins_(nbins), data_(std::move(data)) {}

        /// Construct an empty histogram
        Histogram1D() = default;

        double start() const { return start_; }
        double end() const { return end_; }
        DType dtype() const { return dtype_; }
        std::uint16_t nbins() const { return nbins_; }
        std::span<const std::byte> data() const { return data_; }

        /// Total byte size of the bin data
        std::size_t data_bytes() const { return static_cast<std::size_t>(nbins_) * dtype_size(dtype_); }

        /**
         * @brief Access a single bin count with bounds checking
         *
         * @tparam T Element type matching the histogram dtype
         * @param bin Bin index
         * @return Bin count value
         */
        template <typename T> T bin(std::uint16_t bin) const;

        /// Summary string for logging
        CNSTLN_API std::string to_string() const;

        /** Assemble as MsgPack ext type */
        CNSTLN_API void msgpack_pack(msgpack::packer<msgpack::sbuffer>& packer) const;

        /** Disassemble from MsgPack ext object */
        CNSTLN_API static Histogram1D msgpack_unpack(const msgpack::object& obj);

    private:
        double start_ {0.0};
        double end_ {0.0};
        DType dtype_ {DType::FLOAT64};
        std::uint16_t nbins_ {0};
        std::vector<std::byte> data_;
    };

    /**
     * @brief 2D histogram with uniform x and y binning over a Matrix of bin counts
     */
    class Histogram2D {
    public:
        /**
         * @brief Construct a Histogram2D
         *
         * @param x_start Lower edge of the first x bin
         * @param x_end Upper edge of the last x bin
         * @param y_start Lower edge of the first y bin
         * @param y_end Upper edge of the last y bin
         * @param matrix Bin count matrix
         */
        Histogram2D(double x_start, double x_end, double y_start, double y_end, Matrix matrix)
            : x_start_(x_start), x_end_(x_end), y_start_(y_start), y_end_(y_end), matrix_(std::move(matrix)) {}

        /// Construct an empty histogram
        Histogram2D() = default;

        double x_start() const { return x_start_; }
        double x_end() const { return x_end_; }
        double y_start() const { return y_start_; }
        double y_end() const { return y_end_; }
        const Matrix& matrix() const { return matrix_; }

        /// Summary string for logging
        CNSTLN_API std::string to_string() const;

        /** Assemble as MsgPack ext type */
        CNSTLN_API void msgpack_pack(msgpack::packer<msgpack::sbuffer>& packer) const;

        /** Disassemble from MsgPack ext object */
        CNSTLN_API static Histogram2D msgpack_unpack(const msgpack::object& obj);

    private:
        double x_start_ {0.0};
        double x_end_ {0.0};
        double y_start_ {0.0};
        double y_end_ {0.0};
        Matrix matrix_;
    };

} // namespace constellation::metrics

#include "DQMTypes.ipp" // IWYU pragma: keep
