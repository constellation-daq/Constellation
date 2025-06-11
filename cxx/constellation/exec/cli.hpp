/**
 * @file
 * @brief Command-line interface parser
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <argparse/argparse.hpp>

#include "constellation/build.hpp"
#include "constellation/core/log/Level.hpp"
#include "constellation/core/networking/asio_helpers.hpp"

namespace constellation::exec {

    /**
     * @brief Cast C-style main argument to C++ span
     *
     * @param argc Argument count
     * @param argv Pointer to arguments
     * @return Span of arguments
     */
    inline std::span<const char*> to_span(int argc, char** argv) {
        return {const_cast<const char**>(argv), static_cast<std::size_t>(argc)};
    };

    class CNSTLN_API BaseParser : protected argparse::ArgumentParser {
    public:
        struct BaseOptions {
            /** Console log level */
            log::Level log_level;

            /** List of interfaces */
            std::vector<networking::Interface> interfaces;
        };

    public:
        /**
         * @brief Construct a new parser
         *
         * @param program Name of the program to display in help
         */
        BaseParser(std::string program);

        virtual ~BaseParser() = default;

        /// @cond doxygen_suppress
        // No copy/move constructor/assignment
        BaseParser(const BaseParser& other) = delete;
        BaseParser& operator=(const BaseParser& other) = delete;
        BaseParser(BaseParser&& other) = delete;
        BaseParser& operator=(BaseParser&& other) = delete;
        /// @endcond

        /**
         * @brief Add the CLI options to the parser
         *
         * This add the `--level` and `--interface` options
         *
         * @note Inheriting classes might override this function but have to call `BaseParser::setup()` to add the CLI
         *       options from the base parser.
         */
        virtual void setup();

        /**
         * @brief Parse options from the command line
         *
         * @note Inheriting classes might override this function but have to call `BaseParser::parse()` to add parse the
         *       options from the base parser.
         *
         * @return Parsed options
         */
        BaseOptions parse(std::span<const char*> args);

        /**
         * @brief Get program help
         *
         * @return String containing the program help
         */
        std::string help() const;
    };

    class CNSTLN_API SatelliteParser : public BaseParser {
    public:
        struct SatelliteOptions : BaseOptions {
            /** Constellation group */
            std::string group;

            /** Satellite type */
            std::string satellite_type;

            /** Satellite name */
            std::string satellite_name;
        };

    public:
        /**
         * @brief Construct a new parser
         *
         * @param program Name of the program to display in help
         * @param type Optional predefined satellite type
         */
        SatelliteParser(std::string program, std::optional<std::string> type = std::nullopt);

        virtual ~SatelliteParser() = default;

        /// @cond doxygen_suppress
        // No copy/move constructor/assignment
        SatelliteParser(const SatelliteParser& other) = delete;
        SatelliteParser& operator=(const SatelliteParser& other) = delete;
        SatelliteParser(SatelliteParser&& other) = delete;
        SatelliteParser& operator=(SatelliteParser&& other) = delete;
        /// @endcond

        /**
         * @brief Add the CLI options to the parser
         *
         * This add the `--group` and `--name` options in addition to the options from the `BaseParser`.
         * Additionally the `--type` option is added if no predefined type was specified in the constructor.
         *
         * @note Inheriting classes might override this function but have to call `SatelliteParser::setup()` to add the CLI
         *       options from the satellite parser.
         */
        void setup() override;

        /**
         * @brief Parse options from the command line
         *
         * @note Inheriting classes might override this function but have to call `SatelliteParser::parse()` to add parse the
         *       options from the satellite parser.
         *
         * @return Parsed options
         */
        SatelliteOptions parse(std::span<const char*> args);

    private:
        std::optional<std::string> type_;
    };

    class CNSTLN_API GUIParser : public BaseParser {
    public:
        struct GUIOptions : BaseOptions {
            /** Constellation group */
            std::optional<std::string> group;

            /** Name of the instance */
            std::optional<std::string> instance_name;
        };

    public:
        /**
         * @brief Construct a new parser
         *
         * @param program Name of the program to display in help
         */
        GUIParser(std::string program);

        virtual ~GUIParser() = default;

        /// @cond doxygen_suppress
        // No copy/move constructor/assignment
        GUIParser(const GUIParser& other) = delete;
        GUIParser& operator=(const GUIParser& other) = delete;
        GUIParser(GUIParser&& other) = delete;
        GUIParser& operator=(GUIParser&& other) = delete;
        /// @endcond

        /**
         * @brief Add the CLI options to the parser
         *
         * This add the `--group` and `--name` options in addition to the options from the `BaseParser`.
         *
         * @note Inheriting classes might override this function but have to call `GUIParser::setup()` to add the CLI
         *       options from the GUI parser.
         */
        void setup() override;

        /**
         * @brief Parse options from the command line
         *
         * @note Inheriting classes might override this function but have to call `GUIParser::parse()` to add parse the
         *       options from the GUI parser.
         *
         * @return Parsed options
         */
        GUIOptions parse(std::span<const char*> args);
    };

} // namespace constellation::exec
