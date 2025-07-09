/**
 * @file
 * @brief Observatory main function
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <exception>
#include <iostream>
#include <memory>

#include <QApplication>
#include <QCoreApplication>
#include <QException>
#include <QInputDialog>
#include <QLineEdit>
#include <QString>

#include "constellation/core/log/log.hpp"
#include "constellation/exec/cli.hpp"
#include "constellation/exec/cpp.hpp"
#include "constellation/gui/qt_utils.hpp"

#include "Observatory.hpp"

using namespace constellation::exec;
using namespace constellation::gui;

int main(int argc, char** argv) {
    try {
        // Initialize Qt application
        initResources();
        auto qapp = std::make_shared<QApplication>(argc, argv);

        try {
            QCoreApplication::setOrganizationName("Constellation");
            QCoreApplication::setOrganizationDomain("constellation.pages.desy.de");
            QCoreApplication::setApplicationName("Observatory");
        } catch(const QException&) {
            LOG(CRITICAL) << "Failed to set up UI application";
            return 1;
        }

        // Get parser and setup
        auto parser = GUIParser("Observatory");
        parser.setup();

        // Parse options
        GUIParser::GUIOptions options {};
        try {
            options = parser.parse(to_span(argc, argv));
        } catch(const std::exception& error) {
            LOG(CRITICAL) << "Argument parsing failed: " << error.what() << "\n\n" << parser.help();
            return 1;
        }

        // Set log level
        constellation_setup_logging(options.log_level, "Observatory");

        // Get Constellation group
        std::string group_name;
        if(options.group.has_value()) {
            group_name = options.group.value();
        } else {
            const QString text =
                QInputDialog::getText(nullptr, "Constellation", "Constellation group to connect to:", QLineEdit::Normal);
            if(!text.isEmpty()) {
                group_name = text.toStdString();
            } else {
                LOG(CRITICAL) << "Invalid or empty constellation group name";
                return 1;
            }
        }

        // Get instance and controller name
        const auto instance_name = options.instance_name.has_value() ? ("." + options.instance_name.value()) : "";
        const auto listener_name = "Observatory" + instance_name;

        // Setup CHIRP
        constellation_setup_chirp(group_name, listener_name, options.interfaces);

        // Start Observatory
        try {
            Observatory gui {group_name};
            gui.show();
            return QCoreApplication::exec();
        } catch(const QException&) {
            LOG(CRITICAL) << "Failed to start UI application";
        }

    } catch(const std::exception& error) {
        std::cerr << "Critical failure: " << error.what() << "\n" << std::flush;
    } catch(...) {
        std::cerr << "Critical failure: <unknown exception>\n" << std::flush;
    }
    return 1;
}
