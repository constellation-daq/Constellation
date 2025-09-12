/**
 * @file
 * @brief MissionControl GUI definition
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "MissionControl.hpp"

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <QCloseEvent>
#include <QDateTime>
#include <QFileDialog>
#include <QFileInfo>
#include <QLinearGradient>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QMetaType>
#include <QPainter>
#include <QRegularExpression>
#include <QSpinBox>
#include <QString>
#include <QStyleOptionViewItem>
#include <QTextDocument>
#include <QtGlobal>
#include <QTimer>
#include <QTimeZone>

#include "constellation/build.hpp"
#include "constellation/controller/Controller.hpp"
#include "constellation/controller/ControllerConfiguration.hpp"
#include "constellation/controller/exceptions.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/log/Level.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"
#include "constellation/core/utils/enum.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/gui/QCommandDialog.hpp"
#include "constellation/gui/QConnectionDialog.hpp"
#include "constellation/gui/QResponseDialog.hpp"
#include "constellation/gui/qt_utils.hpp"

using namespace constellation::config;
using namespace constellation::controller;
using namespace constellation::gui;
using namespace constellation::log;
using namespace constellation::protocol;
using namespace constellation::utils;

FileSystemModel::FileSystemModel(QObject* parent) : QFileSystemModel(parent) {}

QVariant FileSystemModel::data(const QModelIndex& index, int role) const {
    if(role == Qt::DisplayRole && index.column() == 0) {
        return QDir::toNativeSeparators(filePath(index));
    }
    return QFileSystemModel::data(index, role);
}

MissionControl::MissionControl(std::string controller_name, std::string_view group_name)
    : runcontrol_(std::move(controller_name)), logger_("UI"), user_logger_("OP"),
      run_id_validator_(QRegularExpression("^[\\w-]+$"), this), config_file_fs_(&config_file_completer_) {

    // Register types used in signals & slots:
    qRegisterMetaType<QModelIndex>("QModelIndex");
    qRegisterMetaType<CSCP::State>("protocol::CSCP::State");
    qRegisterMetaType<std::size_t>("std::size_t");

    // Set up the user interface
    setupUi(this);

    // Set initial values for header bar
    const auto state = runcontrol_.getLowestState();
    cnstlnName->setText(QString::fromStdString("<font color=gray><b>" + std::string(group_name) + "</b></font>"));
    labelState->setText(get_styled_state(state, runcontrol_.isInGlobalState()));
    labelNrSatellites->setText("<font color='gray'><b>" + QString::number(runcontrol_.getConnections().size()) +
                               "</b></font>");

    sorting_proxy_.setSourceModel(&runcontrol_);
    viewConn->setModel(&sorting_proxy_);
    viewConn->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(viewConn, &QTreeView::customContextMenuRequested, this, &MissionControl::custom_context_menu);

    // Enable uniform row height to allow for optimizations on Qt end:
    viewConn->setUniformRowHeights(true);

    // Set default column width of main connection view
    viewConn->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    viewConn->header()->resizeSection(0, 150);
    viewConn->header()->setSectionResizeMode(1, QHeaderView::Interactive);
    viewConn->header()->resizeSection(1, 150);
    viewConn->header()->setSectionResizeMode(2, QHeaderView::Fixed);
    viewConn->header()->resizeSection(2, 120);
    viewConn->header()->setSectionResizeMode(3, QHeaderView::Stretch);
    viewConn->header()->setSectionResizeMode(4, QHeaderView::Interactive);
    viewConn->header()->resizeSection(4, 80);
    viewConn->header()->setSectionResizeMode(5, QHeaderView::Fixed);
    viewConn->header()->resizeSection(5, 40);

    const auto cfg_file = gui_settings_.value("run/configfile", "").toString();
    if(QFile::exists(cfg_file)) {
        txtConfigFileName->setText(cfg_file);
    }

    // Restore window geometry:
    restoreGeometry(gui_settings_.value("window/geometry", saveGeometry()).toByteArray());
    restoreState(gui_settings_.value("window/savestate", saveState()).toByteArray());
    move(gui_settings_.value("window/pos", pos()).toPoint());
    resize(gui_settings_.value("window/size", size()).toSize());
    if(gui_settings_.value("window/maximized", isMaximized()).toBool()) {
        showMaximized();
    }

    // Restore log level:
    const auto qslevel = gui_settings_.value("log_level").toString();
    const auto slevel = enum_cast<Level>(qslevel.toStdString());
    comboBoxLogLevel->setCurrentLevel(slevel.value_or(Level::INFO));

    // Restore last run identifier from configuration:
    update_run_identifier(gui_settings_.value("run/identifier", "run").toString(),
                          gui_settings_.value("run/sequence", 0).toInt());

    setWindowTitle("Constellation MissionControl " CNSTLN_VERSION_FULL);

    // Connect timer to method for run timer update
    connect(&display_timer_, &QTimer::timeout, this, &MissionControl::update_run_infos);
    display_timer_.start(300); // internal update time of GUI

    // Connect run identifier edit boxes:
    connect(runIdentifier, &QLineEdit::editingFinished, this, [&]() {
        update_run_identifier(runIdentifier->text(), runSequence->value());
    });
    connect(runSequence, QOverload<int>::of(&QSpinBox::valueChanged), this, [&](int i) {
        update_run_identifier(runIdentifier->text(), i);
    });

    // Connect connection update signal:
    connect(&runcontrol_, &QController::connectionsChanged, this, [&](std::size_t num) {
        labelNrSatellites->setText("<font color='gray'><b>" + QString::number(num) + "</b></font>");
    });

    connect(&runcontrol_, &QController::connectionsChanged, this, &MissionControl::startup);

    // Connect state update signal:
    connect(&runcontrol_, &QController::reachedState, this, [&](CSCP::State state, bool global) {
        update_button_states(state);
        labelState->setText(get_styled_state(state, global));

        if(state == CSCP::State::RUN && global) {
            // Set start time for this run
            const auto run_time = runcontrol_.getRunStartTime();
            if(run_time.has_value()) {
                run_start_time_ = from_timepoint(run_time.value());
            } else {
                run_start_time_ = QDateTime::currentDateTimeUtc();
            }
        }
    });

    connect(&runcontrol_, &QController::leavingState, this, [&](CSCP::State state, bool global) {
        // If previous state was global RUN, increment sequence:
        if(state == CSCP::State::RUN && global) {
            runSequence->setValue(runSequence->value() + 1);
        }
    });

    // Attach validators & completers:
    runIdentifier->setValidator(&run_id_validator_);
    config_file_fs_.setRootPath({});
    config_file_completer_.setMaxVisibleItems(10);
    config_file_completer_.setModel(&config_file_fs_);
    config_file_completer_.setCompletionMode(QCompleter::InlineCompletion);
    txtConfigFileName->setCompleter(&config_file_completer_);

    // Start the controller
    runcontrol_.start();
}

void MissionControl::startup(std::size_t num) {

    // For the very first connection, try to obtain run time and run identifier
    if(num == 1) {
        const auto is_running = runcontrol_.isInState(CSCP::State::RUN);

        // Read last run identifier from the connection:
        const auto run_id = std::string(runcontrol_.getRunIdentifier());
        if(!run_id.empty()) {
            // attempt to find a sequence number:
            const std::size_t pos = run_id.find_last_of('_');
            const auto& identifier = (pos != std::string::npos ? run_id.substr(0, pos) : run_id);
            std::size_t sequence = 0;
            try {
                sequence = (pos != std::string::npos ? std::stoi(run_id.substr(pos + 1)) : 0);
            } catch(const std::invalid_argument&) {
                LOG(logger_, DEBUG) << "Could not detect a sequence number in run identifier, appending 0 instead";
            }

            // This is an old run identifier, increment the sequence:
            if(!is_running) {
                sequence++;
            }
            update_run_identifier(QString::fromStdString(identifier), static_cast<int>(sequence));
        }
    }
}

void MissionControl::update_run_identifier(const QString& text, int number) {

    runIdentifier->setText(text);
    runSequence->setValue(number);

    if(!text.isEmpty()) {
        current_run_ = text + "_";
    } else {
        current_run_.clear();
    }
    current_run_ += QString::number(number);

    gui_settings_.setValue("run/identifier", text);
    gui_settings_.setValue("run/sequence", number);

    LOG(logger_, DEBUG) << "Updated run identifier to " << current_run_.toStdString();
}

void MissionControl::on_viewConn_activated(const QModelIndex& i) {
    // Use the sorting proxy to obtain the correct model index of the source:
    const QModelIndex index = sorting_proxy_.mapToSource(i);
    if(!index.isValid()) {
        return;
    }

    const auto name = runcontrol_.getQName(index);
    const auto details = runcontrol_.getQDetails(index);
    const auto cmds = runcontrol_.getQCommands(index);
    new QConnectionDialog(this, name, details, cmds);
}

void MissionControl::on_btnInit_clicked() {
    // Read config file from UI
    auto configs = parse_config_file(txtConfigFileName->text());

    if(!configs.has_value()) {
        return;
    }

    LOG(user_logger_, INFO) << "Sending transition command " << "initialize"_quote
                            << ", configuration file: " << txtConfigFileName->text().toStdString();
    for(auto& response : runcontrol_.sendQCommands("initialize", configs.value())) {
        LOG(logger_, DEBUG) << "Initialize: " << response.first << ": " << response.second.getVerb().first;
    }
}

void MissionControl::on_txtConfigFileName_textChanged() {
    // Updated config file name, trigger button update:
    const auto state = runcontrol_.getLowestState();
    update_button_states(state);
}

void MissionControl::on_btnShutdown_clicked() {
    if(runcontrol_.getConnectionCount() == 0) {
        return;
    }

    // We don't close the GUI but shutdown satellites instead:
    if(QMessageBox::question(this, "Quitting", "Shutdown all satellites?", QMessageBox::Ok | QMessageBox::Cancel) ==
       QMessageBox::Cancel) {
        LOG(logger_, DEBUG) << "Aborted satellite shutdown";
    } else {
        LOG(user_logger_, INFO) << "Sending command " << "shutdown"_quote;
        for(auto& response : runcontrol_.sendQCommands("shutdown")) {
            LOG(logger_, DEBUG) << "Shutdown: " << response.first << ": " << response.second.getVerb().first;
        }
    }
}

void MissionControl::on_btnConfig_clicked() {
    LOG(user_logger_, INFO) << "Sending transition command " << "launch"_quote;
    for(auto& response : runcontrol_.sendQCommands("launch")) {
        LOG(logger_, DEBUG) << "Launch: " << response.first << ": " << response.second.getVerb().first;
    }
}

void MissionControl::on_btnLand_clicked() {
    LOG(user_logger_, INFO) << "Sending transition command " << "land"_quote;
    for(auto& response : runcontrol_.sendQCommands("land")) {
        LOG(logger_, DEBUG) << "Land: " << response.first << ": " << response.second.getVerb().first;
    }
}

void MissionControl::on_btnStart_clicked() {
    LOG(user_logger_, INFO) << "Sending transition command " << "start"_quote
                            << ", run identifier: " << current_run_.toStdString();
    for(auto& response : runcontrol_.sendQCommands("start", current_run_.toStdString())) {
        LOG(logger_, DEBUG) << "Start: " << response.first << ": " << response.second.getVerb().first;
    }
}

void MissionControl::on_btnStop_clicked() {
    LOG(user_logger_, INFO) << "Sending transition command " << "stop"_quote;
    for(auto& response : runcontrol_.sendQCommands("stop")) {
        LOG(logger_, DEBUG) << "Stop: " << response.first << ": " << response.second.getVerb().first;
    }
}

void MissionControl::on_btnLog_clicked() {
    const auto msg = txtLogmsg->text().toStdString();
    if(msg.empty()) {
        return;
    }

    const auto level = enum_cast<Level>(comboBoxLogLevel->currentText().toStdString());
    LOG(user_logger_, level.value_or(Level::INFO)) << msg;
    txtLogmsg->clear();
}

void MissionControl::on_btnLoadConf_clicked() {
    const QString usedpath = QFileInfo(txtConfigFileName->text()).path();
    const QString filename =
        QFileDialog::getOpenFileName(this, tr("Open File"), usedpath, tr("Configurations (*.conf *.toml *.ini)"));
    if(!filename.isNull()) {
        LOG(user_logger_, INFO) << "Loaded configuration file " << filename.toStdString();
        txtConfigFileName->setText(filename);
    }
}

void MissionControl::on_btnGenConf_clicked() {

    ControllerConfiguration new_cfg;

    // Round-call to collect configurations from the satellites:
    for(const auto& config : runcontrol_.sendQCommands("get_config")) {
        const auto cfg = Dictionary::disassemble(config.second.getPayload());
        new_cfg.addSatelliteConfiguration(config.first, cfg);
    }

    const QString filename = QFileDialog::getSaveFileName(
        this, tr("Save File"), QFileInfo(txtConfigFileName->text()).path(), tr("Configurations (*.conf *.toml *.ini)"));

    if(filename.isNull()) {
        return;
    }

    // Store to file:
    std::ofstream file {filename.toStdString()};
    file << new_cfg.getAsTOML();
    file.close();

    LOG(user_logger_, INFO) << "Stored configuration from running Constellation to file " << filename.toStdString();

    // Set selected config to this one:
    txtConfigFileName->setText(filename);
}

void MissionControl::update_button_states(CSCP::State state) {

    const QRegularExpression rx_conf(R"(.+(\.conf$|\.ini$|\.toml$))");
    auto m = rx_conf.match(txtConfigFileName->text());

    using enum CSCP::State;
    btnInit->setEnabled(CSCP::is_one_of_states<NEW, INIT, SAFE, ERROR>(state) && m.hasMatch());
    btnLand->setEnabled(state == ORBIT);
    btnConfig->setEnabled(state == INIT);
    btnLoadConf->setEnabled(CSCP::is_one_of_states<NEW, initializing, INIT, SAFE, ERROR>(state));
    btnGenConf->setEnabled(CSCP::is_not_one_of_states<NEW, initializing, ERROR>(state) &&
                           runcontrol_.getConnectionCount() > 0);
    txtConfigFileName->setEnabled(CSCP::is_one_of_states<NEW, initializing, INIT, SAFE, ERROR>(state));
    btnStart->setEnabled(state == ORBIT);
    btnStop->setEnabled(state == RUN);
    btnShutdown->setEnabled(CSCP::is_shutdown_allowed(state));

    // Deactivate run identifier fields during run:
    runIdentifier->setEnabled(CSCP::is_not_one_of_states<RUN, starting, stopping, interrupting>(state));
    runSequence->setEnabled(CSCP::is_not_one_of_states<RUN, starting, stopping, interrupting>(state));
}

void MissionControl::update_run_infos() {

    // Update run timer:
    if(runcontrol_.getLowestState() == CSCP::State::RUN) {
        auto duration = duration_string(std::chrono::seconds(run_start_time_.secsTo(QDateTime::currentDateTime())));
        runDuration->setStyleSheet("QLabel { font-weight: bold; }");
        runDuration->setText(duration);
        runID->setText("<b>" + current_run_ + "</b>");
    } else {
        runDuration->setStyleSheet("QLabel { font-weight: normal; color: gray; }");
        runID->setText("<font color=gray><b>" + current_run_ + "</b> (next)</font>");
    }
}

void MissionControl::closeEvent(QCloseEvent* event) {
    LOG(user_logger_, INFO) << "Closing controller";

    // Stop the controller:
    runcontrol_.stop();

    // Store window geometry:
    gui_settings_.setValue("window/geometry", saveGeometry());
    gui_settings_.setValue("window/savestate", saveState());
    gui_settings_.setValue("window/maximized", isMaximized());
    if(!isMaximized()) {
        gui_settings_.setValue("window/pos", pos());
        gui_settings_.setValue("window/size", size());
    }
    gui_settings_.setValue("log_level", comboBoxLogLevel->currentText());

    gui_settings_.setValue("run/configfile", txtConfigFileName->text());

    // Terminate the application
    event->accept();
}

void MissionControl::custom_context_menu(const QPoint& point) {
    // Use the sorting proxy to obtain the correct model index of the source:
    const QModelIndex index = sorting_proxy_.mapToSource(viewConn->indexAt(point));
    if(!index.isValid()) {
        return;
    }

    auto contextMenu = QMenu(viewConn);

    auto* initialiseAction = new QAction(QIcon(":/command/init"), "Initialize", this);
    connect(initialiseAction, &QAction::triggered, this, [this, index]() {
        auto config = parse_config_file(txtConfigFileName->text(), index);
        if(!config.has_value()) {
            return;
        }
        runcontrol_.sendQCommand(index, "initialize", config.value());
    });
    contextMenu.addAction(initialiseAction);

    auto* launchAction = new QAction(QIcon(":/command/launch"), "Launch", this);
    connect(launchAction, &QAction::triggered, this, [this, index]() { runcontrol_.sendQCommand(index, "launch"); });
    contextMenu.addAction(launchAction);

    auto* landAction = new QAction(QIcon(":/command/land"), "Land", this);
    connect(landAction, &QAction::triggered, this, [this, index]() { runcontrol_.sendQCommand(index, "land"); });
    contextMenu.addAction(landAction);

    auto* startAction = new QAction(QIcon(":/command/start"), "Start", this);
    connect(startAction, &QAction::triggered, this, [this, index]() {
        runcontrol_.sendQCommand(index, "start", current_run_.toStdString());
    });
    contextMenu.addAction(startAction);

    auto* stopAction = new QAction(QIcon(":/command/stop"), "Stop", this);
    connect(stopAction, &QAction::triggered, this, [this, index]() { runcontrol_.sendQCommand(index, "stop"); });
    contextMenu.addAction(stopAction);

    auto* terminateAction = new QAction(QIcon(":/command/shutdown"), "Shutdown", this);
    connect(terminateAction, &QAction::triggered, this, [this, index]() { runcontrol_.sendQCommand(index, "shutdown"); });
    contextMenu.addAction(terminateAction);

    // Draw separator
    contextMenu.addSeparator();

    // Add standard commands
    for(const auto command : enum_names<CSCP::StandardCommand>()) {
        if(command == "shutdown" || command.starts_with("_")) {
            // Already added above || hidden command
            continue;
        }
        const auto command_str = to_string(command);
        auto* action = new QAction(QString::fromStdString(command_str), this);
        connect(action, &QAction::triggered, this, [this, index, command_str]() {
            const auto& response = runcontrol_.sendQCommand(index, command_str);
            if(response.hasPayload()) {
                QResponseDialog dialog(this, response);
                dialog.exec();
            }
        });
        contextMenu.addAction(action);
    }

    // Draw separator
    contextMenu.addSeparator();

    // Request possible commands from remote:
    auto dict = runcontrol_.getQCommands(index);
    for(const auto& [key, value] : dict) {
        // Filter out transition and standard commands to not list them twice
        if(enum_cast<CSCP::TransitionCommand>(key).has_value() || enum_cast<CSCP::StandardCommand>(key).has_value()) {
            continue;
        }

        auto* action = new QAction(QString::fromStdString(key), this);
        connect(action, &QAction::triggered, this, [this, index, key, value]() {
            QCommandDialog cmd_dialog {this, runcontrol_.getQName(index), key, value.str()};
            if(cmd_dialog.exec() == QDialog::Accepted && !cmd_dialog.getCommand().empty()) {
                const auto& response = runcontrol_.sendQCommand(index, cmd_dialog.getCommand(), cmd_dialog.getPayload());
                if(response.hasPayload()) {
                    QResponseDialog re_dialog {this, response};
                    re_dialog.exec();
                }
            }
        });
        contextMenu.addAction(action);
    }

    // Draw separator
    contextMenu.addSeparator();

    // Add possibility to run custom command
    auto* customAction = new QAction("Custom...", this);
    connect(customAction, &QAction::triggered, this, [this, index]() {
        QCommandDialog dialog(this, runcontrol_.getQName(index));
        if(dialog.exec() == QDialog::Accepted && !dialog.getCommand().empty()) {
            const auto& response = runcontrol_.sendQCommand(index, dialog.getCommand(), dialog.getPayload());
            if(response.hasPayload()) {
                QResponseDialog dialog(this, response);
                dialog.exec();
            }
        }
    });
    contextMenu.addAction(customAction);

    contextMenu.exec(viewConn->viewport()->mapToGlobal(point));
}

std::optional<std::map<std::string, Controller::CommandPayload>> MissionControl::parse_config_file(const QString& file) {
    try {
        const auto configs = ControllerConfiguration(std::filesystem::path(file.toStdString()));
        // Convert to CommandPayloads:
        std::map<std::string, Controller::CommandPayload> payloads {};
        std::vector<std::string> sats_without_config {};
        for(const auto& satellite : runcontrol_.getConnections()) {
            if(!configs.hasSatelliteConfiguration(satellite)) {
                sats_without_config.push_back(satellite);
            }
            payloads.emplace(satellite, configs.getSatelliteConfiguration(satellite));
        }

        if(!sats_without_config.empty() &&
           QMessageBox::question(this,
                                 "Warning",
                                 "The following satellites do not have explicit configuration sections in the selected "
                                 "configuration file:\n" +
                                     QString::fromStdString(range_to_string(sats_without_config, "\n")) +
                                     "\n\nContinue anyway?",
                                 QMessageBox::Ok | QMessageBox::Cancel) == QMessageBox::Cancel) {
            return {};
        }
        return payloads;
    } catch(const ControllerError& error) {
        QMessageBox::warning(nullptr, "ERROR", QString::fromStdString(std::string("Parsing failed: ") + error.what()));
        return {};
    }
}

std::optional<Controller::CommandPayload> MissionControl::parse_config_file(const QString& file, const QModelIndex& index) {
    const auto name = runcontrol_.getQName(index);
    try {
        const auto configs = ControllerConfiguration(std::filesystem::path(file.toStdString()));

        if(!configs.hasSatelliteConfiguration(name) &&
           QMessageBox::question(
               this,
               "Warning",
               QString::fromStdString(name) +
                   " has no explicit configuration section in the selected configuration file\n\nContinue anyway?",
               QMessageBox::Ok | QMessageBox::Cancel) == QMessageBox::Cancel) {
            return {};
        }
        return configs.getSatelliteConfiguration(name);
    } catch(ControllerError& error) {
        QMessageBox::warning(nullptr, "ERROR", QString::fromStdString(std::string("Parsing failed: ") + error.what()));
    }
    return {};
}
