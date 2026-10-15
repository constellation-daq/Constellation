/**
 * @file
 * @brief DQM types template implementation
 *
 * @copyright Copyright (c) 2026 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include "DQMTypes.hpp" // NOLINT(misc-header-include-cycle)

#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace constellation::metrics {

    namespace detail {
        template <typename T> constexpr DType dtype_for() {
            if constexpr(std::is_same_v<T, std::uint32_t>) {
                return DType::UINT32;
            } else if constexpr(std::is_same_v<T, std::uint64_t>) {
                return DType::UINT64;
            } else if constexpr(std::is_same_v<T, float>) {
                return DType::FLOAT32;
            } else if constexpr(std::is_same_v<T, double>) {
                return DType::FLOAT64;
            } else {
                static_assert(!std::is_same_v<T, T>, "Unsupported element type for Matrix");
            }
        }
    } // namespace detail

    template <typename T>
    Matrix::Matrix(std::uint16_t rows, std::uint16_t cols, std::span<const T> data)
        : rows_(rows), cols_(cols), dtype_(detail::dtype_for<T>()) {
        const auto expected = static_cast<std::size_t>(rows) * cols;
        if(data.size() != expected) {
            throw std::invalid_argument("Matrix data size mismatch");
        }
        const auto* raw = reinterpret_cast<const std::byte*>(data.data());
        data_.assign(raw, raw + data.size_bytes());
    }

    template <typename T> T Matrix::element(std::uint16_t row, std::uint16_t col) const {
        if(row >= rows_ || col >= cols_) {
            throw std::out_of_range("Matrix element index out of range");
        }
        const auto offset = (static_cast<std::size_t>(row) * cols_ + col) * sizeof(T);
        T value {};
        std::memcpy(&value, data_.data() + offset, sizeof(T));
        return value;
    }

    template <typename T>
    Histogram1D::Histogram1D(double start, double end, std::span<const T> bins)
        : start_(start), end_(end), dtype_(detail::dtype_for<T>()) {
        nbins_ = static_cast<std::uint16_t>(bins.size());
        data_.resize(bins.size() * sizeof(T));
        if(!bins.empty()) {
            std::memcpy(data_.data(), bins.data(), bins.size() * sizeof(T));
        }
    }

    template <typename T> T Histogram1D::bin(std::uint16_t idx) const {
        if(idx >= nbins_) {
            throw std::out_of_range("Histogram1D bin index out of range");
        }
        T value {};
        std::memcpy(&value, data_.data() + static_cast<std::size_t>(idx) * sizeof(T), sizeof(T));
        return value;
    }
} // namespace constellation::metrics
