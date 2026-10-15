/**
 * @file
 * @brief Implementation of metric classes
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "Metric.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>

#include <msgpack.hpp>

#include "constellation/core/config/value_types.hpp"
#include "constellation/core/message/PayloadBuffer.hpp"
#include "constellation/core/metrics/DQMTypes.hpp"
#include "constellation/core/utils/casts.hpp"
#include "constellation/core/utils/exceptions.hpp"
#include "constellation/core/utils/msgpack.hpp"

using namespace constellation::metrics;
using namespace constellation::message;
using namespace constellation::utils;

PayloadBuffer MetricValue::assemble() const {
    msgpack::sbuffer sbuf {};

    // Pack value
    std::visit(
        [&sbuf](const auto& val) {
            using T = std::decay_t<decltype(val)>;
            if constexpr(std::is_same_v<T, config::Scalar>) {
                msgpack_pack(sbuf, val);
            } else {
                // Pack as MsgPack ext object
                msgpack::packer<msgpack::sbuffer> packer {sbuf};
                val.msgpack_pack(packer);
            }
        },
        value_);

    msgpack_pack(sbuf, static_cast<std::uint8_t>(0x0));
    msgpack_pack(sbuf, metric_->unit());
    return {std::move(sbuf)};
}

MetricValue MetricValue::disassemble(std::string name, const message::PayloadBuffer& message) {
    // Offset since we decode separate msgpack objects
    std::size_t offset = 0;

    try {
        // Unpack value
        auto obj = msgpack::unpack(to_char_ptr(message.span().data()), message.span().size(), offset);

        MetricData value {};
        if(obj->type == msgpack::type::EXT) {
            // Dispatch on ext type code
            switch(obj->via.ext.type()) {
            case EXT_TYPE_MATRIX: value = Matrix::msgpack_unpack(obj.get()); break;
            case EXT_TYPE_HISTOGRAM1D: value = Histogram1D::msgpack_unpack(obj.get()); break;
            case EXT_TYPE_HISTOGRAM2D: value = Histogram2D::msgpack_unpack(obj.get()); break;
            default: throw std::invalid_argument("Unknown DQM ext type code");
            }
        } else {
            // Scalar value
            value = obj->as<config::Scalar>();
        }

        // Unpack flags
        msgpack_unpack_to<std::uint8_t>(to_char_ptr(message.span().data()), message.span().size(), offset);

        // Unpack unit
        const auto unit = msgpack_unpack_to<std::string>(to_char_ptr(message.span().data()), message.span().size(), offset);

        // Create metric with empty description
        return {std::make_shared<Metric>(std::move(name), unit), std::move(value)};
    } catch(const MsgpackUnpackError& e) {
        throw std::invalid_argument(e.what());
    }
}
