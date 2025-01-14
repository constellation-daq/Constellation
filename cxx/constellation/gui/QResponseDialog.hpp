/**
 * @file
 * @brief Command Dialog
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <memory>
#include <string_view>

#include <QDialog>
#include <QStandardItemModel>

#include "constellation/build.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/message/CSCP1Message.hpp"

// Expose Qt class auto-generated from the user interface XML:
namespace Ui { // NOLINT(readability-identifier-naming)
    class QResponseDialog;
} // namespace Ui

namespace constellation::gui {

    /**
     * @class QResponseDialog
     * @brief Dialog window to show a satellite response in a coherent way
     */
    class CNSTLN_API QResponseDialog : public QDialog {
        Q_OBJECT

    public:
        /**
         * @brief Constructor of the QResponseDialog
         *
         * @param parent Parent widget of the dialog
         * @param message CSCP message returned by the satellite
         */
        explicit QResponseDialog(QWidget* parent, const message::CSCP1Message& message);

    private:
        /** Helper to fill the UI with a tabular response from a dictionary */
        void show_as_dictionary(const config::Dictionary& dict);

        /** Helper to fill the UI with a list of returned values */
        void show_as_list(const config::List& list);

        /** Helper to fill the UI with a text from the response or a verbatim display of the payload */
        void show_as_string(std::string_view str);

    private:
        std::shared_ptr<Ui::QResponseDialog> ui_;
    };

} // namespace constellation::gui
