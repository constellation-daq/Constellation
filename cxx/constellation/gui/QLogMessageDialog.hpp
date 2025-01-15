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

#include <QDialog>

#include "constellation/build.hpp"
#include "constellation/gui/QLogMessage.hpp"

// Expose Qt class auto-generated from the user interface XML:
namespace Ui { // NOLINT(readability-identifier-naming)
    class QLogMessageDialog;
} // namespace Ui

namespace constellation::gui {

    /**
     * @class QLogMessageDialog
     * @brief Dialog window to show details of individual log messages
     */
    class CNSTLN_API QLogMessageDialog : public QDialog {
        Q_OBJECT

    public:
        QLogMessageDialog(QWidget* parent, const QLogMessage& msg);

    private:
        std::shared_ptr<Ui::QLogMessageDialog> ui_;
    };

} // namespace constellation::gui
