/**
 * @file
 * @brief Collection of all exec library errors
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <string>
#include <string_view>
#include <utility>

#include "constellation/build.hpp"
#include "constellation/core/utils/exceptions.hpp"
#include "constellation/core/utils/string.hpp"

namespace constellation::exec {
    /**
     * @ingroup Exceptions
     * @brief Generic error for CLI functions
     */
    class CNSTLN_API CommandLineInterfaceError : public utils::RuntimeError {
    public:
        explicit CommandLineInterfaceError(std::string what_arg) : utils::RuntimeError(std::move(what_arg)) {}
    };

    /**
     * @ingroup Exceptions
     * @brief Error while interacting with a Dynamic Shared Object (DSO)
     */
    class CNSTLN_API DSOLoaderError : public utils::RuntimeError {
    protected:
        DSOLoaderError() = default;
    };

    /**
     * @ingroup Exceptions
     * @brief Error while loading a Dynamic Shared Object (DSO)
     */
    class CNSTLN_API DSOLoadingError : public DSOLoaderError {
    public:
        explicit DSOLoadingError(std::string_view dso_name, std::string_view reason) {
            error_message_ = "Error while loading ";
            error_message_ += dso_name;
            error_message_ += ": ";
            error_message_ += reason;
        }
    };

    /**
     * @ingroup Exceptions
     * @brief Requested function not found in Dynamic Shared Object (DSO)
     */
    class CNSTLN_API DSOFunctionLoadingError : public DSOLoaderError {
    public:
        explicit DSOFunctionLoadingError(std::string_view function, std::string_view dso_name, std::string_view reason) {
            error_message_ = "Error while loading function ";
            error_message_ += utils::quote(function);
            error_message_ += " from ";
            error_message_ += dso_name;
            error_message_ += ": ";
            error_message_ += reason;
        }
    };

    /**
     * @ingroup Exceptions
     * @brief Error while interacting with the Python loader
     */
    class CNSTLN_API PyLoaderError : public utils::RuntimeError {
    public:
        explicit PyLoaderError(std::string_view what_arg) { error_message_ = what_arg; }

    protected:
        PyLoaderError() = default;
    };

    /**
     * @ingroup Exceptions
     * @brief Python loader not built
     */
    class CNSTLN_API PyLoaderNotBuildError : public DSOLoaderError {
    public:
        explicit PyLoaderNotBuildError() { error_message_ = "Python integration has not been built"; }
    };

    /**
     * @ingroup Exceptions
     * @brief Error while loading from Python
     */
    class CNSTLN_API PyLoadingError : public DSOLoaderError {
    public:
        explicit PyLoadingError(std::string_view what, std::string_view reason) {
            error_message_ = "Error while loading ";
            error_message_ += what;
            error_message_ += ": ";
            error_message_ += reason;
        }
    };

    /**
     * @ingroup Exceptions
     * @brief Exception during Python execution
     */
    class CNSTLN_API PyLoaderPythonException : public DSOLoaderError {
    public:
        explicit PyLoaderPythonException(std::string_view message) {
            error_message_ = "Exception during Python execution: ";
            error_message_ += message;
        }
    };

} // namespace constellation::exec
