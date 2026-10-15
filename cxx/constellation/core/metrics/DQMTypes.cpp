/**
 * @file
 * @brief DQM extension types
 *
 * @copyright Copyright (c) 2026 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "DQMTypes.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#include <msgpack.hpp>

#include "constellation/core/utils/enum.hpp"

using namespace constellation::metrics;

std::size_t constellation::metrics::dtype_size(DType dtype) {
    switch(dtype) {
    case DType::UINT32: return 4;
    case DType::UINT64: return 8;
    case DType::FLOAT32: return 4;
    case DType::FLOAT64: return 8;
    default: throw std::invalid_argument("Unknown DType");
    }
}

std::string Matrix::to_string() const {
    return "Matrix(" + std::to_string(rows_) + "x" + std::to_string(cols_) + ", " + utils::enum_name(dtype_) + ")";
}

void Matrix::msgpack_pack(msgpack::packer<msgpack::sbuffer>& packer) const {
    const auto payload_size = 1 + 2 + 2 + data_.size();
    packer.pack_ext(payload_size, EXT_TYPE_MATRIX);

    const auto dtype_byte = static_cast<std::uint8_t>(dtype_);
    packer.pack_ext_body(reinterpret_cast<const char*>(&dtype_byte), 1);

    packer.pack_ext_body(reinterpret_cast<const char*>(&rows_), 2);
    packer.pack_ext_body(reinterpret_cast<const char*>(&cols_), 2);

    if(!data_.empty()) {
        packer.pack_ext_body(reinterpret_cast<const char*>(data_.data()), data_.size());
    }
}

Matrix Matrix::msgpack_unpack(const msgpack::object& obj) {
    if(obj.type != msgpack::type::EXT) {
        throw std::invalid_argument("Expected MsgPack ext type for Matrix");
    }
    if(obj.via.ext.type() != EXT_TYPE_MATRIX) {
        throw std::invalid_argument("Wrong ext type code for Matrix");
    }

    const auto* ptr = reinterpret_cast<const std::uint8_t*>(obj.via.ext.data());
    const auto ext_size = obj.via.ext.size;

    if(ext_size < 5) {
        throw std::invalid_argument("Matrix ext payload too small");
    }

    const auto dtype = static_cast<DType>(ptr[0]);

    std::uint16_t rows = 0;
    std::uint16_t cols = 0;
    std::memcpy(&rows, ptr + 1, 2);
    std::memcpy(&cols, ptr + 3, 2);

    const auto data_size = ext_size - 5;
    const auto expected_size = static_cast<std::size_t>(rows) * cols * dtype_size(dtype);
    if(data_size != expected_size) {
        throw std::invalid_argument("Matrix data size mismatch");
    }

    std::vector<std::byte> data(data_size);
    if(data_size > 0) {
        std::memcpy(data.data(), ptr + 5, data_size);
    }

    return {rows, cols, dtype, std::move(data)};
}

std::string Histogram1D::to_string() const {
    return "Histogram1D(" + std::to_string(nbins_) + " bins, " + utils::enum_name(dtype_) + ", [" + std::to_string(start_) +
           ", " + std::to_string(end_) + "])";
}

void Histogram1D::msgpack_pack(msgpack::packer<msgpack::sbuffer>& packer) const {
    const auto payload_size = 8 + 8 + 1 + 2 + data_.size();
    packer.pack_ext(payload_size, EXT_TYPE_HISTOGRAM1D);

    packer.pack_ext_body(reinterpret_cast<const char*>(&start_), 8);
    packer.pack_ext_body(reinterpret_cast<const char*>(&end_), 8);

    const auto dtype_byte = static_cast<std::uint8_t>(dtype_);
    packer.pack_ext_body(reinterpret_cast<const char*>(&dtype_byte), 1);
    packer.pack_ext_body(reinterpret_cast<const char*>(&nbins_), 2);

    if(!data_.empty()) {
        packer.pack_ext_body(reinterpret_cast<const char*>(data_.data()), data_.size());
    }
}

Histogram1D Histogram1D::msgpack_unpack(const msgpack::object& obj) {
    if(obj.type != msgpack::type::EXT) {
        throw std::invalid_argument("Expected MsgPack ext type for Histogram1D");
    }
    if(obj.via.ext.type() != EXT_TYPE_HISTOGRAM1D) {
        throw std::invalid_argument("Wrong ext type code for Histogram1D");
    }

    const auto* ptr = reinterpret_cast<const std::uint8_t*>(obj.via.ext.data());
    const auto ext_size = obj.via.ext.size;

    if(ext_size < 19) {
        throw std::invalid_argument("Histogram1D ext payload too small");
    }

    double start = 0.0;
    double end = 0.0;
    std::memcpy(&start, ptr, 8);
    std::memcpy(&end, ptr + 8, 8);

    const auto dtype = static_cast<DType>(ptr[16]);
    std::uint16_t nbins = 0;
    std::memcpy(&nbins, ptr + 17, 2);

    const auto data_size = ext_size - 19;
    const auto expected_size = static_cast<std::size_t>(nbins) * dtype_size(dtype);
    if(data_size != expected_size) {
        throw std::invalid_argument("Histogram1D bin data size mismatch");
    }

    std::vector<std::byte> data(data_size);
    if(data_size > 0) {
        std::memcpy(data.data(), ptr + 19, data_size);
    }

    return {start, end, dtype, nbins, std::move(data)};
}

std::string Histogram2D::to_string() const {
    return "Histogram2D(" + std::to_string(matrix_.rows()) + "x" + std::to_string(matrix_.cols()) + ", x=[" +
           std::to_string(x_start_) + ", " + std::to_string(x_end_) + "], y=[" + std::to_string(y_start_) + ", " +
           std::to_string(y_end_) + "])";
}

void Histogram2D::msgpack_pack(msgpack::packer<msgpack::sbuffer>& packer) const {
    msgpack::sbuffer matrix_buf {};
    msgpack::packer<msgpack::sbuffer> matrix_packer {matrix_buf};
    matrix_.msgpack_pack(matrix_packer);

    const auto payload_size = 8 + 8 + 8 + 8 + matrix_buf.size();
    packer.pack_ext(payload_size, EXT_TYPE_HISTOGRAM2D);

    packer.pack_ext_body(reinterpret_cast<const char*>(&x_start_), 8);
    packer.pack_ext_body(reinterpret_cast<const char*>(&x_end_), 8);
    packer.pack_ext_body(reinterpret_cast<const char*>(&y_start_), 8);
    packer.pack_ext_body(reinterpret_cast<const char*>(&y_end_), 8);

    packer.pack_ext_body(matrix_buf.data(), matrix_buf.size());
}

Histogram2D Histogram2D::msgpack_unpack(const msgpack::object& obj) {
    if(obj.type != msgpack::type::EXT) {
        throw std::invalid_argument("Expected MsgPack ext type for Histogram2D");
    }
    if(obj.via.ext.type() != EXT_TYPE_HISTOGRAM2D) {
        throw std::invalid_argument("Wrong ext type code for Histogram2D");
    }

    const auto* ptr = reinterpret_cast<const std::uint8_t*>(obj.via.ext.data());
    const auto ext_size = obj.via.ext.size;

    if(ext_size < 32) {
        throw std::invalid_argument("Histogram2D ext payload too small");
    }

    double x_start = 0.0;
    double x_end = 0.0;
    double y_start = 0.0;
    double y_end = 0.0;
    std::memcpy(&x_start, ptr, 8);
    std::memcpy(&x_end, ptr + 8, 8);
    std::memcpy(&y_start, ptr + 16, 8);
    std::memcpy(&y_end, ptr + 24, 8);

    const auto matrix_data = reinterpret_cast<const char*>(ptr + 32);
    const auto matrix_size = ext_size - 32;
    auto matrix_obj = msgpack::unpack(matrix_data, matrix_size);
    auto matrix = Matrix::msgpack_unpack(matrix_obj.get());

    return {x_start, x_end, y_start, y_end, std::move(matrix)};
}
