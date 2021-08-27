﻿#include <sstream>

#include <QFileDialog>
#include <QMessageBox>
#include <QAction>
#include <QInputDialog>
#include <QClipboard>
#include <QTextStream>

#include "Base/ServiceLocator.h"
#include "Base/GlobalFactory.h"
#include "Base/NumberGenerator.h"
#include "Base/LoggingService.h"

#include "EngineInterface/Descriptions.h"
#include "EngineInterface/SimulationController.h"
#include "EngineInterface/Serializer.h"
#include "EngineInterface/SymbolTable.h"
#include "EngineInterface/Physics.h"
#include "EngineInterface/SerializationHelper.h"
#include "EngineInterface/DescriptionFactory.h"
#include "EngineInterface/ZoomLevels.h"

#include "Web/WebAccess.h"

#include "ToolbarController.h"
#include "ToolbarContext.h"
#include "SimulationViewController.h"
#include "DataEditController.h"
#include "DataEditContext.h"
#include "NewSimulationDialog.h"
#include "SimulationParametersDialog.h"
#include "SymbolTableDialog.h"
#include "ComputationSettingsDialog.h"
#include "NewRectangleDialog.h"
#include "NewHexagonDialog.h"
#include "NewParticlesDialog.h"
#include "RandomMultiplierDialog.h"
#include "GridMultiplierDialog.h"
#include "MonitorController.h"
#include "Settings.h"
#include "GeneralInfoController.h"
#include "MainController.h"
#include "MainModel.h"
#include "MainView.h"
#include "Notifier.h"
#include "ActionModel.h"
#include "ActionController.h"
#include "ActionHolder.h"
#include "SimulationConfig.h"
#include "WebSimulationController.h"
#include "NewDiscDialog.h"
#include "ColorizeDialogController.h"
#include "ZoomActionController.h"

ActionController::ActionController(QObject * parent)
	: QObject(parent)
{
	_model = new ActionModel(this);
    _zoomController = new ZoomActionController(this);
}

void ActionController::init(
    MainController* mainController,
    MainModel* mainModel,
    MainView* mainView,
    SimulationViewController* simulationViewController,
    Serializer* serializer,
    GeneralInfoController* infoController,
    DataEditController* dataEditor,
    ToolbarController* toolbar,
    MonitorController* monitor,
    DataRepository* repository,
    Notifier* notifier,
    WebSimulationController* webSimController)
{
    auto factory = ServiceLocator::getInstance().getService<GlobalFactory>();
    auto numberGenerator = factory->buildRandomNumberGenerator();
	numberGenerator->init();
    _zoomController->init(_model->getActionHolder(), simulationViewController);

	_mainController = mainController;
	_mainModel = mainModel;
	_mainView = mainView;
    _simulationViewController = simulationViewController;
	_serializer = serializer;
	_infoController = infoController;
	_dataEditor = dataEditor;
	_toolbar = toolbar;
	_monitor = monitor;
	_repository = repository;
	_notifier = notifier;
    _webSimController = webSimController;
	SET_CHILD(_numberGenerator, numberGenerator);

	connect(_notifier, &Notifier::notifyDataRepositoryChanged, this, &ActionController::receivedNotifications);

	auto actions = _model->getActionHolder();
	connect(actions->actionNewSimulation, &QAction::triggered, this, &ActionController::onNewSimulation);
    connect(actions->actionWebSimulation, &QAction::toggled, this, &ActionController::onWebSimulation);
    connect(actions->actionSaveSimulation, &QAction::triggered, this, &ActionController::onSaveSimulation);
	connect(actions->actionLoadSimulation, &QAction::triggered, this, &ActionController::onLoadSimulation);
	connect(actions->actionComputationSettings, &QAction::triggered, this, &ActionController::onConfigureGrid);
	connect(actions->actionRunSimulation, &QAction::toggled, this, &ActionController::onRunClicked);
	connect(actions->actionRunStepForward, &QAction::triggered, this, &ActionController::onStepForward);
	connect(actions->actionRunStepBackward, &QAction::triggered, this, &ActionController::onStepBackward);
	connect(actions->actionSnapshot, &QAction::triggered, this, &ActionController::onMakeSnapshot);
	connect(actions->actionRestore, &QAction::triggered, this, &ActionController::onRestoreSnapshot);
    connect(actions->actionAcceleration, &QAction::triggered, this, &ActionController::onAcceleration);
    connect(actions->actionSimulationChanger, &QAction::triggered, this, &ActionController::onSimulationChanger);
    connect(actions->actionExit, &QAction::triggered, _mainView, &MainView::close);

	connect(actions->actionZoomIn, &QAction::triggered, _zoomController, &ZoomActionController::onZoomInClicked);
    connect(actions->actionZoomOut, &QAction::triggered, _zoomController, &ZoomActionController::onZoomOutClicked);
    connect(
        _simulationViewController,
        &SimulationViewController::continuousZoomIn,
        _zoomController,
        &ZoomActionController::onContinuousZoomIn);
    connect(
        _simulationViewController,
        &SimulationViewController::continuousZoomOut,
        _zoomController,
        &ZoomActionController::onContinuousZoomOut);
    connect(
        _simulationViewController,
        &SimulationViewController::endContinuousZoom,
        _zoomController,
        &ZoomActionController::onEndContinuousZoom);
    connect(
        _zoomController,
        &ZoomActionController::updateActionsState,
        this,
        &ActionController::updateActionsEnableState);
    connect(actions->actionDisplayLink, &QAction::triggered, this, &ActionController::onToggleDisplayLink);
    connect(actions->actionGlowEffect, &QAction::toggled, this, &ActionController::onToggleGlowEffect);
    connect(actions->actionMotionEffect, &QAction::toggled, this, &ActionController::onToggleMotionEffect);

    connect(actions->actionItemView, &QAction::toggled, this, &ActionController::onToggleEditorMode);
    connect(actions->actionActionMode, &QAction::toggled, this, &ActionController::onToggleActionMode);

	connect(actions->actionMonitor, &QAction::toggled, this, &ActionController::onToggleInfobar);
	connect(actions->actionEditSimParameters, &QAction::triggered, this, &ActionController::onEditSimulationParameters);
	connect(actions->actionEditSymbols, &QAction::triggered, this, &ActionController::onEditSymbolMap);

	connect(actions->actionNewCell, &QAction::triggered, this, &ActionController::onNewCell);
	connect(actions->actionNewParticle, &QAction::triggered, this, &ActionController::onNewParticle);
	connect(actions->actionCopyEntity, &QAction::triggered, this, &ActionController::onCopyEntity);
	connect(actions->actionPasteEntity, &QAction::triggered, this, &ActionController::onPasteEntity);
	connect(actions->actionDeleteEntity, &QAction::triggered, this, &ActionController::onDeleteEntity);
	connect(actions->actionNewToken, &QAction::triggered, this, &ActionController::onNewToken);
	connect(actions->actionCopyToken, &QAction::triggered, this, &ActionController::onCopyToken);
	connect(actions->actionDeleteToken, &QAction::triggered, this, &ActionController::onDeleteToken);
	connect(actions->actionPasteToken, &QAction::triggered, this, &ActionController::onPasteToken);
	connect(actions->actionShowCellInfo, &QAction::toggled, this, &ActionController::onToggleCellInfo);
	connect(actions->actionCenterSelection, &QAction::toggled, this, &ActionController::onCenterSelection);
    connect(actions->actionCopyToClipboard, &QAction::triggered, this, &ActionController::onCopyToClipboard);
    connect(actions->actionPasteFromClipboard, &QAction::triggered, this, &ActionController::onPasteFromClipboard);

	connect(actions->actionNewRectangle, &QAction::triggered, this, &ActionController::onNewRectangle);
	connect(actions->actionNewHexagon, &QAction::triggered, this, &ActionController::onNewHexagon);
    connect(actions->actionNewDisc, &QAction::triggered, this, &ActionController::onNewDisc);
    connect(actions->actionNewParticles, &QAction::triggered, this, &ActionController::onNewParticles);
    connect(actions->actionLoadCol, &QAction::triggered, this, &ActionController::onLoadCollection);
	connect(actions->actionSaveCol, &QAction::triggered, this, &ActionController::onSaveCollection);
	connect(actions->actionCopyCol, &QAction::triggered, this, &ActionController::onCopyCollection);
	connect(actions->actionPasteCol, &QAction::triggered, this, &ActionController::onPasteCollection);
	connect(actions->actionDeleteSel, &QAction::triggered, this, &ActionController::onDeleteSelection);
	connect(actions->actionDeleteCol, &QAction::triggered, this, &ActionController::onDeleteExtendedSelection);
    connect(actions->actionColorizeSel, &QAction::triggered, this, &ActionController::onColorizeSelection);
    connect(
        actions->actionGenerateBranchNumbers, &QAction::triggered, this, &ActionController::onGenerateBranchNumbers);
    connect(
        actions->actionRandomizeCellFunctions, &QAction::triggered, this, &ActionController::onRandomizeCellFunctions);
    connect(
        actions->actionRemoveFreeCellConnections, &QAction::triggered, this, &ActionController::onRemoveFreeCellConnections);
    connect(actions->actionRandomMultiplier, &QAction::triggered, this, &ActionController::onRandomMultiplier);
	connect(actions->actionGridMultiplier, &QAction::triggered, this, &ActionController::onGridMultiplier);

    connect(actions->actionRepetitiveActiveClusterAnalysis, &QAction::triggered, this, &ActionController::onRepetitiveActiveClusterAnalysis);
    connect(actions->actionWriteMonitorLog, &QAction::toggled, this, &ActionController::onToggleWriteMonitorLog);

	connect(actions->actionAbout, &QAction::triggered, this, &ActionController::onShowAbout);
    connect(actions->actionGettingStarted, &QAction::triggered, this, &ActionController::onToggleGettingStarted);
    connect(actions->actionDocumentation, &QAction::triggered, this, &ActionController::onShowDocumentation);

	connect(actions->actionRestrictTPS, &QAction::triggered, this, &ActionController::onToggleRestrictTPS);
}

void ActionController::close()
{
    auto const actions = _model->getActionHolder();
    actions->actionWebSimulation->setChecked(false);
}

ActionHolder * ActionController::getActionHolder()
{
	return _model->getActionHolder();
}

void ActionController::onRunClicked(bool toggled)
{
    auto loggingService = ServiceLocator::getInstance().getService<LoggingService>();
	auto actions = _model->getActionHolder();
	if (toggled) {
        loggingService->logMessage(Priority::Important, "run simulation");
        actions->actionRunStepForward->setEnabled(false);
	}
	else {
        loggingService->logMessage(Priority::Important, "stop simulation");
        actions->actionRunStepForward->setEnabled(true);
	}
	actions->actionRunStepBackward->setEnabled(false);

	_mainController->onRunSimulation(toggled);

    loggingService->logMessage(Priority::Unimportant, "run/stop simulation finished");
}

void ActionController::onStepForward()
{
    auto loggingService = ServiceLocator::getInstance().getService<LoggingService>();
    loggingService->logMessage(Priority::Important, "calculate one time step forward");

    _mainController->onStepForward();
	_model->getActionHolder()->actionRunStepBackward->setEnabled(true);

    loggingService->logMessage(Priority::Unimportant, "calculate one time step forward finished");
}

void ActionController::onStepBackward()
{
    auto loggingService = ServiceLocator::getInstance().getService<LoggingService>();
    loggingService->logMessage(Priority::Important, "calculate one time step backward");

    bool emptyStack = false;
	_mainController->onStepBackward(emptyStack);
	if (emptyStack) {
		_model->getActionHolder()->actionRunStepBackward->setEnabled(false);
	}
	_simulationViewController->refresh();

    loggingService->logMessage(Priority::Unimportant, "calculate one time step backward finished");
}

void ActionController::onMakeSnapshot()
{
    auto loggingService = ServiceLocator::getInstance().getService<LoggingService>();
    loggingService->logMessage(Priority::Important, "make snapshot");

    _mainController->onMakeSnapshot();
	_model->getActionHolder()->actionRestore->setEnabled(true);

    loggingService->logMessage(Priority::Unimportant, "make snapshot finished");
}

void ActionController::onRestoreSnapshot()
{
    auto loggingService = ServiceLocator::getInstance().getService<LoggingService>();
    loggingService->logMessage(Priority::Important, "load snapshot");

	_mainController->onRestoreSnapshot();
	_simulationViewController->refresh();

	loggingService->logMessage(Priority::Unimportant, "load snapshot finished");
}

void ActionController::onAcceleration(bool toggled)
{
    auto loggingService = ServiceLocator::getInstance().getService<LoggingService>();

	auto parameters = _mainModel->getExecutionParameters();
    parameters.activateFreezing = toggled;
    if (toggled) {
        bool ok;
        auto const accelerationTimesteps = QInputDialog::getInt(
            _mainView, "Acceleration active clusters", "Enter the number of time steps of acceleration", parameters.freezingTimesteps, 1, 100, 1, &ok);
        if (!ok) {
            auto const actionHolder = _model->getActionHolder();
            actionHolder->actionAcceleration->setChecked(false);
            return;
        }
        parameters.freezingTimesteps = accelerationTimesteps;

        std::stringstream stream;
		stream << "activate accelerating active clusters by " << accelerationTimesteps << " time steps";
        loggingService->logMessage(Priority::Important, stream.str());
    } else {
        loggingService->logMessage(Priority::Important, "deactivate accelerating active clusters");
	}
    _mainModel->setExecutionParameters(parameters);
    _mainController->onUpdateExecutionParameters(parameters);

	loggingService->logMessage(Priority::Unimportant, "toggle accelerating active clusters finished");	
}

void ActionController::onSimulationChanger(bool toggled)
{
	auto loggingService = ServiceLocator::getInstance().getService < LoggingService>();
    if (toggled) {
        loggingService->logMessage(Priority::Important, "activate parameter changer");
    } else {
        loggingService->logMessage(Priority::Important, "deactivate parameter changer");
    }
    _mainController->onSimulationChanger(toggled);

    loggingService->logMessage(Priority::Unimportant, "toggle parameter changer finished");
}

void ActionController::onToggleDisplayLink(bool toggled)
{
    auto loggingService = ServiceLocator::getInstance().getService<LoggingService>();
    if (toggled) {
        loggingService->logMessage(Priority::Important, "activating display link");
    } else {
        loggingService->logMessage(Priority::Important, "deactivating display link");
    }
    _mainController->onDisplayLink(toggled);

	loggingService->logMessage(Priority::Unimportant, "toggle display link finished");
}

void ActionController::onToggleGlowEffect(bool toggled)
{
    auto loggingService = ServiceLocator::getInstance().getService<LoggingService>();
    if (toggled) {
        loggingService->logMessage(Priority::Important, "activate glow effect");
    } else {
        loggingService->logMessage(Priority::Important, "deactivate glow effect");
    }

	auto viewSettings = _model->getSimulationViewSettings();
    viewSettings.glowEffect = toggled;
    _simulationViewController->setSettings(viewSettings);
    _mainView->refresh();

    loggingService->logMessage(Priority::Unimportant, "toggle glow effect finished");
}

void ActionController::onToggleMotionEffect(bool toggled)
{
    auto loggingService = ServiceLocator::getInstance().getService<LoggingService>();
    if (toggled) {
        loggingService->logMessage(Priority::Important, "activate motion effect");
    } else {
        loggingService->logMessage(Priority::Important, "deactivate motion effect");
    }

    auto viewSettings = _model->getSimulationViewSettings();
    viewSettings.motionEffect = toggled;
    _simulationViewController->setSettings(viewSettings);
    _mainView->refresh();

    loggingService->logMessage(Priority::Unimportant, "toggle motion effect finished");
}

void ActionController::onToggleEditorMode(bool toggled)
{
    auto loggingService = ServiceLocator::getInstance().getService<LoggingService>();

	_model->setEditMode(toggled);
    if (toggled) {
        loggingService->logMessage(Priority::Unimportant, "activate editor mode");

		_infoController->setRendering(GeneralInfoController::Rendering::Item);
        _simulationViewController->disconnectView();
        _simulationViewController->setActiveScene(ActiveView::ItemScene);
        _simulationViewController->connectView();
    }
	else {
        loggingService->logMessage(Priority::Unimportant, "deactivate editor mode");

		setPixelOrVectorView();
	}
    _simulationViewController->refresh();
    updateActionsEnableState();

	Q_EMIT _toolbar->getContext()->show(toggled);
	Q_EMIT _dataEditor->getContext()->show(toggled);

    loggingService->logMessage(Priority::Unimportant, "toggle editor mode finished");
}

void ActionController::onToggleActionMode(bool toggled)
{
    auto loggingService = ServiceLocator::getInstance().getService<LoggingService>();
    if (toggled) {
        loggingService->logMessage(Priority::Important, "activate action mode");
    } else {
        loggingService->logMessage(Priority::Important, "deactivate action mode");
    }

    auto viewSettings = _model->getSimulationViewSettings();
    viewSettings.mode = toggled ? SimulationViewSettings::Mode::ActionMode : SimulationViewSettings::Mode::NavigationMode;
    _simulationViewController->setSettings(viewSettings);
    _mainView->refresh();

    loggingService->logMessage(Priority::Unimportant, "toggle action mode finished");
}

void ActionController::onToggleInfobar(bool toggled)
{
    auto loggingService = ServiceLocator::getInstance().getService<LoggingService>();
    if (toggled) {
        loggingService->logMessage(Priority::Unimportant, "activate infobar");
    } else {
        loggingService->logMessage(Priority::Unimportant, "deactivate infobar");
    }

    _mainView->toggleInfobar(toggled);

    loggingService->logMessage(Priority::Unimportant, "toggle infobar finished");
}

void ActionController::onNewSimulation()
{
	NewSimulationDialog dialog(_mainModel->getSimulationParameters(), _mainModel->getSymbolMap(), _serializer, _mainView);
	if (dialog.exec()) {
        auto loggingService = ServiceLocator::getInstance().getService<LoggingService>();
        loggingService->logMessage(Priority::Important, "create simulation");

		_mainController->onNewSimulation(*dialog.getConfig(), *dialog.getEnergy());

		settingUpNewSimulation(_mainController->getSimulationConfig());

		loggingService->logMessage(Priority::Unimportant, "create simulation finished");
    }
}

void ActionController::onWebSimulation(bool toogled)
{
    if (toogled) {
        if (!_webSimController->onConnectToSimulation()) {
            auto actions = _model->getActionHolder();
            actions->actionWebSimulation->setChecked(false);
        }
    }
    else {
        auto const currentSimulationId = _webSimController->getCurrentSimulationId();
        auto const currentToken = _webSimController->getCurrentToken();
        if (currentSimulationId && currentToken) {
            _webSimController->onDisconnectToSimulation(*currentSimulationId, *currentToken);
        }
    }
}

void ActionController::onSaveSimulation()
{
	QString filename = QFileDialog::getSaveFileName(_mainView, "Save Simulation", "", "Alien Simulation(*.sim)");
	if (!filename.isEmpty()) {
        auto loggingService = ServiceLocator::getInstance().getService<LoggingService>();

        std::stringstream stream;
        QFileInfo info(filename);
        stream << "save simulation '" << info.fileName().toStdString() << "'";
        loggingService->logMessage(Priority::Important, stream.str());

		_mainController->onSaveSimulation(filename.toStdString());

		loggingService->logMessage(Priority::Unimportant, "save simulation finished");
    }
}

void ActionController::onLoadSimulation()
{
	QString filename = QFileDialog::getOpenFileName(_mainView, "Load Simulation", "", "Alien Simulation (*.sim)");
	if (!filename.isEmpty()) {
        auto loggingService = ServiceLocator::getInstance().getService<LoggingService>();

        std::stringstream stream;
        QFileInfo info(filename);
        stream << "load simulation '" << info.fileName().toStdString() << "'";
        loggingService->logMessage(Priority::Important, stream.str());

		if (_mainController->onLoadSimulation(filename.toStdString(), MainController::LoadOption::SaveOldSim)) {
			settingUpNewSimulation(_mainController->getSimulationConfig());

			loggingService->logMessage(Priority::Unimportant, "load simulation finished");
        }
		else {
            loggingService->logMessage(Priority::Important, "load simulation failed");

			QMessageBox msgBox(QMessageBox::Critical, "Error", Const::ErrorLoadSimulation);
			msgBox.exec();
		}
	}
}

void ActionController::onConfigureGrid()
{
	ComputationSettingsDialog dialog(_mainController->getSimulationConfig(), _mainView);
    if (dialog.exec()) {

		auto loggingService = ServiceLocator::getInstance().getService<LoggingService>();
        loggingService->logMessage(Priority::Important, "change general settings");

        auto config = _mainController->getSimulationConfig();
        config->universeSize = *dialog.getUniverseSize();
        config->cudaConstants = *dialog.getCudaConstants();

        auto const extrapolateContent = *dialog.isExtrapolateContent();
        _mainController->onRecreateUniverse(config, extrapolateContent);
        settingUpNewSimulation(config);

		loggingService->logMessage(Priority::Unimportant, "change general settings finished");
    }
}

void ActionController::onEditSimulationParameters()
{
    auto const config = _mainController->getSimulationConfig();
    SimulationParametersDialog dialog(config->parameters, _serializer, _mainView);
	if (dialog.exec()) {
        auto loggingService = ServiceLocator::getInstance().getService<LoggingService>();
        loggingService->logMessage(Priority::Important, "change simulation parameters");

		auto const parameters = dialog.getSimulationParameters();
		_mainModel->setSimulationParameters(parameters);
		_mainController->onUpdateSimulationParameters(parameters);

		loggingService->logMessage(Priority::Unimportant, "change simulation parameters finished");
    }
}

void ActionController::onEditSymbolMap()
{
	auto origSymbols = _mainModel->getSymbolMap();
	SymbolTableDialog dialog(origSymbols->clone(), _serializer, _mainView);
	if (dialog.exec()) {
        auto loggingService = ServiceLocator::getInstance().getService<LoggingService>();
        loggingService->logMessage(Priority::Important, "change symbol map");

		origSymbols->getSymbolsFrom(dialog.getSymbolTable());
		Q_EMIT _dataEditor->getContext()->refresh();

        loggingService->logMessage(Priority::Unimportant, "change symbol map finished");
    }
}

void ActionController::onNewCell()
{
    auto loggingService = ServiceLocator::getInstance().getService<LoggingService>();
    loggingService->logMessage(Priority::Important, "add cell");

	_repository->addAndSelectCell(_model->getPositionDeltaForNewEntity());
	_repository->reconnectSelectedCells();
	Q_EMIT _notifier->notifyDataRepositoryChanged({
		Receiver::DataEditor,
		Receiver::Simulation,
		Receiver::VisualEditor,
		Receiver::ActionController
	}, UpdateDescription::All);

    loggingService->logMessage(Priority::Unimportant, "add cell finished");
}

void ActionController::onNewParticle()
{
    auto loggingService = ServiceLocator::getInstance().getService<LoggingService>();
    loggingService->logMessage(Priority::Important, "add particle");

	_repository->addAndSelectParticle(_model->getPositionDeltaForNewEntity());
	Q_EMIT _notifier->notifyDataRepositoryChanged({
		Receiver::DataEditor,
		Receiver::Simulation,
		Receiver::VisualEditor,
		Receiver::ActionController
	}, UpdateDescription::All);

    loggingService->logMessage(Priority::Unimportant, "add particle finished");
}

void ActionController::onCopyEntity()
{
    auto loggingService = ServiceLocator::getInstance().getService<LoggingService>();
    loggingService->logMessage(Priority::Important, "copy entity");

	auto const& selectedCellIds = _repository->getSelectedCellIds();
	auto const& selectedParticleIds = _repository->getSelectedParticleIds();
	if (!selectedCellIds.empty()) {
		CHECK(selectedParticleIds.empty());
		auto const& cell = _repository->getCellDescRef(*selectedCellIds.begin());
		auto const& cluster = _repository->getClusterDescRef(*selectedCellIds.begin());
        QVector2D vel = *cell.vel;
		_model->setCellCopied(cell, vel);
	}
	if (!selectedParticleIds.empty()) {
		CHECK(selectedCellIds.empty());
		auto const& particle = _repository->getParticleDescRef(*selectedParticleIds.begin());
		_model->setParticleCopied(particle);
	}
	updateActionsEnableState();

    loggingService->logMessage(Priority::Unimportant, "copy entity finished");
}

void ActionController::onLoadCollection()
{
	QString filename = QFileDialog::getOpenFileName(_mainView, "Load Collection", "", "Alien Collection (*.col)");
	if (!filename.isEmpty()) {

		auto loggingService = ServiceLocator::getInstance().getService<LoggingService>();
        std::stringstream stream;
        QFileInfo info(filename);
        stream << "load collection '" << info.fileName().toStdString() << "'";
        loggingService->logMessage(Priority::Important, stream.str());

		DataDescription desc;
		if (SerializationHelper::loadFromFile<DataDescription>(filename.toStdString(), [&](string const& data) { return _serializer->deserializeDataDescription(data); }, desc)) {
			_repository->addAndSelectData(desc, _model->getPositionDeltaForNewEntity());
			Q_EMIT _notifier->notifyDataRepositoryChanged({
				Receiver::DataEditor,
				Receiver::Simulation,
				Receiver::VisualEditor,
				Receiver::ActionController
			}, UpdateDescription::All);

			loggingService->logMessage(Priority::Unimportant, "load collection finished");
        }
		else {
			QMessageBox msgBox(QMessageBox::Critical, "Error", Const::ErrorLoadCollection);
			msgBox.exec();

			loggingService->logMessage(Priority::Important, "load collection failed");
        }
	}
}

void ActionController::onSaveCollection()
{
	QString filename = QFileDialog::getSaveFileName(_mainView, "Save Collection", "", "Alien Collection (*.col)");
	if (!filename.isEmpty()) {

		auto loggingService = ServiceLocator::getInstance().getService<LoggingService>();
        std::stringstream stream;
        QFileInfo info(filename);
        stream << "save collection '" << info.fileName().toStdString() << "'";
        loggingService->logMessage(Priority::Important, stream.str());

		if (!SerializationHelper::saveToFile(filename.toStdString(), [&]() { return _serializer->serializeDataDescription(_repository->getExtendedSelection()); })) {
			QMessageBox msgBox(QMessageBox::Critical, "Error", Const::ErrorSaveCollection);
			msgBox.exec();

			loggingService->logMessage(Priority::Important, "save collection failed");
            return;
        } else {
            loggingService->logMessage(Priority::Unimportant, "save collection finished");
        }
	}
}

void ActionController::onCopyCollection()
{
    auto loggingService = ServiceLocator::getInstance().getService<LoggingService>();
    loggingService->logMessage(Priority::Important, "copy collection");

	DataDescription copiedData = _repository->getExtendedSelection();
	_model->setCopiedCollection(copiedData);
	updateActionsEnableState();

    loggingService->logMessage(Priority::Unimportant, "copy collection finished");
}

void ActionController::onPasteCollection()
{
    auto loggingService = ServiceLocator::getInstance().getService<LoggingService>();
    loggingService->logMessage(Priority::Important, "paste collection");

	DataDescription copiedData = _model->getCopiedCollection();
	_repository->addAndSelectData(copiedData, _model->getPositionDeltaForNewEntity());
	Q_EMIT _notifier->notifyDataRepositoryChanged({
		Receiver::DataEditor, Receiver::Simulation, Receiver::VisualEditor,Receiver::ActionController
	}, UpdateDescription::All);

    loggingService->logMessage(Priority::Unimportant, "paste collection finished");
}

void ActionController::onDeleteSelection()
{
    auto loggingService = ServiceLocator::getInstance().getService<LoggingService>();
    loggingService->logMessage(Priority::Important, "delete selection");

	_repository->deleteSelection();
	Q_EMIT _notifier->notifyDataRepositoryChanged({
		Receiver::DataEditor, Receiver::Simulation, Receiver::VisualEditor, Receiver::ActionController
	}, UpdateDescription::All);

    loggingService->logMessage(Priority::Unimportant, "delete selection finished");
}

void ActionController::onDeleteExtendedSelection()
{
    auto loggingService = ServiceLocator::getInstance().getService<LoggingService>();
    loggingService->logMessage(Priority::Important, "delete extended selection");

	_repository->deleteExtendedSelection();
	Q_EMIT _notifier->notifyDataRepositoryChanged({
		Receiver::DataEditor, Receiver::Simulation, Receiver::VisualEditor, Receiver::ActionController
	}, UpdateDescription::All);

    loggingService->logMessage(Priority::Unimportant, "delete extended selection finished");
}

namespace
{
	void modifyDescription(DataDescription& data, QVector2D const& posDelta, boost::optional<double> const& velocityXDelta
		, boost::optional<double> const& velocityYDelta, boost::optional<double> const& angularVelocityDelta)
	{
		if (data.clusters) {
			for (auto& cluster : data.clusters.get()) {
/*
				*cluster.pos += posDelta;
				if (velocityXDelta) {
					cluster.vel->setX(cluster.vel->x() + *velocityXDelta);
				}
				if (velocityYDelta) {
					cluster.vel->setY(cluster.vel->y() + *velocityYDelta);
				}
				if (angularVelocityDelta) {
					*cluster.angularVel += *angularVelocityDelta;
				}
*/
				if (cluster.cells) {
					for (auto& cell : cluster.cells.get()) {
						*cell.pos += posDelta;
					}
				}
			}
		}
		if (data.particles) {
			for (auto& particle : data.particles.get()) {
				*particle.pos += posDelta;
				if (velocityXDelta) {
					particle.vel->setX(particle.vel->x() + *velocityXDelta);
				}
				if (velocityYDelta) {
					particle.vel->setY(particle.vel->y() + *velocityYDelta);
				}
			}
		}
	}
}

void ActionController::onRandomMultiplier()
{
	RandomMultiplierDialog dialog;
	if (dialog.exec()) {

		auto loggingService = ServiceLocator::getInstance().getService<LoggingService>();
        loggingService->logMessage(Priority::Important, "execute random multiplier");

		DataDescription data = _repository->getExtendedSelection();
		IntVector2D universeSize = _mainController->getSimulationConfig()->universeSize;
		vector<DataRepository::DataAndAngle> addedData;
		for (int i = 0; i < dialog.getNumberOfCopies(); ++i) {
			DataDescription dataCopied = data;
			QVector2D posDelta(_numberGenerator->getRandomReal(0.0, universeSize.x), _numberGenerator->getRandomReal(0.0, universeSize.y));
			boost::optional<double> velocityX;
			boost::optional<double> velocityY;
			boost::optional<double> angle;
			boost::optional<double> angularVelocity;
			if (dialog.isChangeVelX()) {
				velocityX = _numberGenerator->getRandomReal(dialog.getVelXMin(), dialog.getVelXMax());
			}
			if (dialog.isChangeVelY()) {
				velocityY = _numberGenerator->getRandomReal(dialog.getVelYMin(), dialog.getVelYMax());
			}
			if (dialog.isChangeAngle()) {
				angle = _numberGenerator->getRandomReal(dialog.getAngleMin(), dialog.getAngleMax());
			}
			if (dialog.isChangeAngVel()) {
				angularVelocity = _numberGenerator->getRandomReal(dialog.getAngVelMin(), dialog.getAngVelMax());
			}
			modifyDescription(dataCopied, posDelta, velocityX, velocityY, angularVelocity);
			addedData.emplace_back(DataRepository::DataAndAngle{ dataCopied, angle });
		}
		_repository->addDataAtFixedPosition(addedData);
		Q_EMIT _notifier->notifyDataRepositoryChanged({
			Receiver::DataEditor,
			Receiver::Simulation,
			Receiver::VisualEditor,
			Receiver::ActionController
		}, UpdateDescription::All);

	    loggingService->logMessage(Priority::Unimportant, "execute random multiplier finished");
    }
}

void ActionController::onGridMultiplier()
{
	DataDescription data = _repository->getExtendedSelection();
	QVector2D center = data.calcCenter();
	GridMultiplierDialog dialog(center);
	if (dialog.exec()) {
        auto loggingService = ServiceLocator::getInstance().getService<LoggingService>();
        loggingService->logMessage(Priority::Important, "execute grid multiplier");

		QVector2D initialDelta(dialog.getInitialPosX(), dialog.getInitialPosY());
		initialDelta -= center;
		vector<DataRepository::DataAndAngle> addedData;
		for (int i = 0; i < dialog.getHorizontalNumber(); ++i) {
			for (int j = 0; j < dialog.getVerticalNumber(); ++j) {
				if (i == 0 && j == 0 && initialDelta.lengthSquared() < FLOATINGPOINT_MEDIUM_PRECISION) {
					continue;
				}
				DataDescription dataCopied = data;
				boost::optional<double> velocityX;
				boost::optional<double> velocityY;
				boost::optional<double> angle;
				boost::optional<double> angularVelocity;
				if (dialog.isChangeAngle()) {
					angle = dialog.getInitialAngle() + i*dialog.getHorizontalAngleIncrement() + j*dialog.getVerticalAngleIncrement();
				}
				if (dialog.isChangeVelocityX()) {
					velocityX = dialog.getInitialVelX() + i*dialog.getHorizontalVelocityXIncrement() + j*dialog.getVerticalVelocityXIncrement();
				}
				if (dialog.isChangeVelocityY()) {
					velocityY = dialog.getInitialVelY() + j*dialog.getHorizontalVelocityYIncrement() + j*dialog.getVerticalVelocityYIncrement();
				}
				if (dialog.isChangeAngularVelocity()) {
					angularVelocity = dialog.getInitialAngVel() + i*dialog.getHorizontalAngularVelocityIncrement() + j*dialog.getVerticalAngularVelocityIncrement();
				}

				QVector2D posDelta(i*dialog.getHorizontalInterval(), j*dialog.getVerticalInterval());
				posDelta += initialDelta;

				modifyDescription(dataCopied, posDelta, velocityX, velocityY, angularVelocity);
				addedData.emplace_back(DataRepository::DataAndAngle{ dataCopied, angle });
			}
		}
		_repository->addDataAtFixedPosition(addedData);
		Q_EMIT _notifier->notifyDataRepositoryChanged({
			Receiver::DataEditor,
			Receiver::Simulation,
			Receiver::VisualEditor,
			Receiver::ActionController
		}, UpdateDescription::All);

		loggingService->logMessage(Priority::Unimportant, "execute grid multiplier finished");
    }
}

void ActionController::onRepetitiveActiveClusterAnalysis()
{
    QString folder = QFileDialog::getExistingDirectory(_mainView, "Select folder for repetitive pattern analysis");
    if (!folder.isEmpty()) {

        auto loggingService = ServiceLocator::getInstance().getService<LoggingService>();
        loggingService->logMessage(Priority::Important, "repetitive pattern analysis");

        _mainController->onSaveRepetitiveActiveClusterAnalysis(folder.toStdString());

        loggingService->logMessage(Priority::Unimportant, "repetitive pattern analysis finished");
    }
}

void ActionController::onToggleWriteMonitorLog(bool toggled)
{
    if (toggled) {
        QString filename = QFileDialog::getSaveFileName(_mainView, "Stream monitor data to file", "", "monitor data (*.csv)");
        if (filename.isEmpty()) {
            _model->getActionHolder()->actionWriteMonitorLog->setChecked(false);
        }
        _monitor->startWritingToFile(filename.toStdString());
    } else {
        _monitor->stopWritingToFile();
    }
}

void ActionController::onDeleteEntity()
{
    onDeleteSelection();
}

void ActionController::onPasteEntity()
{
    auto loggingService = ServiceLocator::getInstance().getService<LoggingService>();
    loggingService->logMessage(Priority::Important, "paste entity");

	DataDescription copiedData = _model->getCopiedEntity();
	_repository->addAndSelectData(copiedData, _model->getPositionDeltaForNewEntity());
	Q_EMIT _notifier->notifyDataRepositoryChanged({
		Receiver::DataEditor, Receiver::Simulation, Receiver::VisualEditor,Receiver::ActionController
	}, UpdateDescription::All);

	loggingService->logMessage(Priority::Unimportant, "paste entity finished");
}

void ActionController::onNewToken()
{
    auto loggingService = ServiceLocator::getInstance().getService<LoggingService>();
    loggingService->logMessage(Priority::Important, "create token");

	_repository->addToken();
	Q_EMIT _notifier->notifyDataRepositoryChanged({
		Receiver::DataEditor, Receiver::Simulation, Receiver::VisualEditor, Receiver::ActionController
	}, UpdateDescription::All);

	loggingService->logMessage(Priority::Unimportant, "create token finished");
}

void ActionController::onCopyToken()
{
    auto loggingService = ServiceLocator::getInstance().getService<LoggingService>();
    loggingService->logMessage(Priority::Important, "copy token");

	auto cellIds = _repository->getSelectedCellIds();
	CHECK(cellIds.size() == 1);
	auto tokenIndex = _repository->getSelectedTokenIndex();
	CHECK(tokenIndex);
	auto const& cell = _repository->getCellDescRef(*cellIds.begin());
	auto const& token = cell.tokens->at(*tokenIndex);

	_model->setCopiedToken(token);
	updateActionsEnableState();

    loggingService->logMessage(Priority::Unimportant, "copy token finished");
}

void ActionController::onPasteToken()
{ 
    auto loggingService = ServiceLocator::getInstance().getService<LoggingService>();
    loggingService->logMessage(Priority::Important, "paste token");

	auto const& token = _model->getCopiedToken();
	_repository->addToken(token);
	Q_EMIT _notifier->notifyDataRepositoryChanged({
		Receiver::DataEditor, Receiver::Simulation, Receiver::VisualEditor, Receiver::ActionController
	}, UpdateDescription::All);

    loggingService->logMessage(Priority::Unimportant, "paste token finished");
}

void ActionController::onDeleteToken()
{
    auto loggingService = ServiceLocator::getInstance().getService<LoggingService>();
    loggingService->logMessage(Priority::Important, "delete token");

	_repository->deleteToken();
	Q_EMIT _notifier->notifyDataRepositoryChanged({
		Receiver::DataEditor, Receiver::Simulation, Receiver::VisualEditor, Receiver::ActionController
	}, UpdateDescription::All);

    loggingService->logMessage(Priority::Unimportant, "delete token finished");
}

void ActionController::onToggleCellInfo(bool show)
{
    auto loggingService = ServiceLocator::getInstance().getService<LoggingService>();
    if (show) {
        loggingService->logMessage(Priority::Unimportant, "activate cell info");
    } else {
        loggingService->logMessage(Priority::Unimportant, "deactivate cell info");
    }

    Q_EMIT _notifier->toggleCellInfo(show);

	loggingService->logMessage(Priority::Unimportant, "toggle cell info finished");
}

void ActionController::onCenterSelection(bool centerSelection)
{
    auto loggingService = ServiceLocator::getInstance().getService<LoggingService>();
    if (centerSelection) {
        loggingService->logMessage(Priority::Unimportant, "activate centering selection");
    } else {
        loggingService->logMessage(Priority::Unimportant, "deactivate centering selection");
    }

	_simulationViewController->toggleCenterSelection(centerSelection);
    _simulationViewController->refresh();

    loggingService->logMessage(Priority::Unimportant, "toggle centering selection finished");
}

void ActionController::onCopyToClipboard()
{
    auto loggingService = ServiceLocator::getInstance().getService<LoggingService>();
    loggingService->logMessage(Priority::Important, "copy token memory to clipboard");

	auto const cellIds = _repository->getSelectedCellIds();
    CHECK(cellIds.size() == 1);
    auto const tokenIndex = _repository->getSelectedTokenIndex();
    CHECK(tokenIndex);
    auto const& cell = _repository->getCellDescRef(*cellIds.begin());
    auto const& token = cell.tokens->at(*tokenIndex);

    auto const& tokenMemory = *token.data;
    auto tokenMemoryInHex = tokenMemory.toHex();

    for (int index = 255 * 2; index > 0; index -= 2) {
        tokenMemoryInHex.insert(index, char(' '));
    }
    
    auto clipboard = QApplication::clipboard();
    clipboard->setText(QString::fromStdString(tokenMemoryInHex.toStdString()));

    loggingService->logMessage(Priority::Unimportant, "copy token memory to clipboard finished");
}

void ActionController::onPasteFromClipboard()
{
    auto loggingService = ServiceLocator::getInstance().getService<LoggingService>();
    loggingService->logMessage(Priority::Important, "paste token memory from clipboard");

    auto const cellIds = _repository->getSelectedCellIds();
    CHECK(cellIds.size() == 1);
    auto const tokenIndex = _repository->getSelectedTokenIndex();
    CHECK(tokenIndex);
    auto& cell = _repository->getCellDescRef(*cellIds.begin());
    auto& token = cell.tokens->at(*tokenIndex);

    auto& tokenMemory = *token.data;
    auto clipboard = QApplication::clipboard();
    QString newTokenMemoryInHex = clipboard->text();
    newTokenMemoryInHex.remove(QChar(' '));
    tokenMemory = QByteArray::fromHex(newTokenMemoryInHex.toUtf8());

    auto const tokenMemorySize = _mainModel->getSimulationParameters().tokenMemorySize;
    if (tokenMemorySize != tokenMemory.size()) {
        QMessageBox msgBox(QMessageBox::Critical, "Error", Const::ErrorPasteFromClipboard);
        msgBox.exec();
        loggingService->logMessage(Priority::Important, "paste token memory from clipboard failed");
        return;
    }
    Q_EMIT _notifier->notifyDataRepositoryChanged({
        Receiver::DataEditor, Receiver::Simulation, Receiver::VisualEditor, Receiver::ActionController
    }, UpdateDescription::All);

    loggingService->logMessage(Priority::Unimportant, "paste token memory from clipboard finished");
}

void ActionController::onNewRectangle()
{
	NewRectangleDialog dialog(_mainModel->getSimulationParameters());
	if (dialog.exec()) {
        auto loggingService = ServiceLocator::getInstance().getService<LoggingService>();
        loggingService->logMessage(Priority::Important, "create rectangle");

        auto const factory = ServiceLocator::getInstance().getService<DescriptionFactory>();
        auto rect = factory->createRect(
            DescriptionFactory::CreateRectParameters()
                .size(dialog.getBlockSize())
                .cellDistance(dialog.getDistance())
                .cellEnergy(dialog.getInternalEnergy())
                .colorCode(dialog.getColorCode()),
            _numberGenerator);

        _repository->addAndSelectData(DataDescription().addCluster(rect), {0, 0});

		Q_EMIT _notifier->notifyDataRepositoryChanged({
			Receiver::DataEditor,
			Receiver::Simulation,
			Receiver::VisualEditor,
			Receiver::ActionController
		}, UpdateDescription::All);

	    loggingService->logMessage(Priority::Unimportant, "create rectangle finished");
    }
}

namespace
{
}

void ActionController::onNewHexagon()
{
	NewHexagonDialog dialog(_mainModel->getSimulationParameters());
	if (dialog.exec()) {
        auto loggingService = ServiceLocator::getInstance().getService<LoggingService>();
        loggingService->logMessage(Priority::Important, "create hexagon");

		int layers = dialog.getLayers();
		double dist = dialog.getDistance();
		double energy = dialog.getCellEnergy();
        int colorCode = dialog.getColorCode();

        auto const factory = ServiceLocator::getInstance().getService<DescriptionFactory>();
        auto hexagon = factory->createHexagon(
            DescriptionFactory::CreateHexagonParameters().layers(layers).cellDistance(dist).cellEnergy(energy).colorCode(
                colorCode),
            _numberGenerator);

		_repository->addAndSelectData(DataDescription().addCluster(hexagon), { 0, 0 });
		Q_EMIT _notifier->notifyDataRepositoryChanged({
			Receiver::DataEditor,
			Receiver::Simulation,
			Receiver::VisualEditor,
			Receiver::ActionController
		}, UpdateDescription::All);

	    loggingService->logMessage(Priority::Unimportant, "create hexagon finished");
    }
}

void ActionController::onNewDisc()
{
    NewDiscDialog dialog;
    if (dialog.exec()) {

        auto loggingService = ServiceLocator::getInstance().getService<LoggingService>();
        loggingService->logMessage(Priority::Important, "create circle");

        auto factory = ServiceLocator::getInstance().getService<DescriptionFactory>();
        auto circle =
            factory->createUnconnectedDisc(DescriptionFactory::CreateDiscParameters()
                                                 .outerRadius(dialog.getOuterRadius())
                                                 .innerRadius(dialog.getInnerRadius())
                                                 .cellEnergy(dialog.getCellEnergy())
                                                 .colorCode(dialog.getColorCode())
                                                 .cellDistance(dialog.getDistance())
                                                 .maxConnections(_mainModel->getSimulationParameters().cellMaxBonds));
        _repository->addAndSelectData(DataDescription().addCluster(circle), {0, 0}, DataRepository::Reconnect::Yes);

        Q_EMIT _notifier->notifyDataRepositoryChanged(
            {Receiver::DataEditor, Receiver::Simulation, Receiver::VisualEditor, Receiver::ActionController},
            UpdateDescription::All);

        loggingService->logMessage(Priority::Unimportant, "create circle finished");
    }
}

void ActionController::onNewParticles()
{
	NewParticlesDialog dialog;
	if (dialog.exec()) {
        auto loggingService = ServiceLocator::getInstance().getService<LoggingService>();
        loggingService->logMessage(Priority::Important, "create particles");

		double totalEnergy = dialog.getTotalEnergy();
		double maxEnergyPerParticle = dialog.getMaxEnergyPerParticle();

		_repository->addRandomParticles(totalEnergy, maxEnergyPerParticle);
		Q_EMIT _notifier->notifyDataRepositoryChanged({
			Receiver::DataEditor,
			Receiver::Simulation,
			Receiver::VisualEditor,
			Receiver::ActionController
		}, UpdateDescription::All);

	    loggingService->logMessage(Priority::Unimportant, "create particles finished");
    }
}

void ActionController::onColorizeSelection()
{
    if (auto colorCode = ColorizeDialogController::executeDialogAndReturnColorCode()) {
        auto loggingService = ServiceLocator::getInstance().getService<LoggingService>();
        loggingService->logMessage(Priority::Important, "colorize selection");

        _repository->colorizeSelection(*colorCode);
        Q_EMIT _notifier->notifyDataRepositoryChanged(
            {Receiver::DataEditor, Receiver::Simulation, Receiver::VisualEditor, Receiver::ActionController},
            UpdateDescription::All);

        loggingService->logMessage(Priority::Unimportant, "colorize selection finished");
    }
}

void ActionController::onGenerateBranchNumbers()
{
    auto loggingService = ServiceLocator::getInstance().getService<LoggingService>();
    loggingService->logMessage(Priority::Important, "generate branch numbers");

	auto extendedSelection = _repository->getExtendedSelection();
    auto selectedCellIds = _repository->getSelectedCellIds();

    auto factory = ServiceLocator::getInstance().getService<DescriptionFactory>();
    factory->generateBranchNumbers(_mainModel->getSimulationParameters(), extendedSelection, selectedCellIds);

	_repository->updateData(extendedSelection);

    Q_EMIT _notifier->notifyDataRepositoryChanged(
        {Receiver::DataEditor, Receiver::Simulation, Receiver::VisualEditor, Receiver::ActionController},
        UpdateDescription::All);

    loggingService->logMessage(Priority::Unimportant, "generate branch numbers finished");
}

void ActionController::onRandomizeCellFunctions()
{
    auto loggingService = ServiceLocator::getInstance().getService<LoggingService>();
    loggingService->logMessage(Priority::Important, "randomize cell functions");

	auto extendedSelection = _repository->getExtendedSelection();
    auto selectedCellIds = _repository->getSelectedCellIds();

	auto factory = ServiceLocator::getInstance().getService<DescriptionFactory>();
    factory->randomizeCellFunctions(_mainModel->getSimulationParameters(), extendedSelection, selectedCellIds);

	_repository->updateData(extendedSelection);

    Q_EMIT _notifier->notifyDataRepositoryChanged(
        {Receiver::DataEditor, Receiver::Simulation, Receiver::VisualEditor, Receiver::ActionController},
        UpdateDescription::All);

    loggingService->logMessage(Priority::Unimportant, "randomize cell functions finished");
}

void ActionController::onRemoveFreeCellConnections()
{
    auto loggingService = ServiceLocator::getInstance().getService<LoggingService>();
    loggingService->logMessage(Priority::Important, "remove free cell connections");

    auto extendedSelection = _repository->getExtendedSelection();
    auto selectedCellIds = _repository->getSelectedCellIds();

    auto factory = ServiceLocator::getInstance().getService<DescriptionFactory>();
    factory->removeFreeCellConnections(_mainModel->getSimulationParameters(), extendedSelection, selectedCellIds);

    _repository->updateData(extendedSelection);

    Q_EMIT _notifier->notifyDataRepositoryChanged(
        {Receiver::DataEditor, Receiver::Simulation, Receiver::VisualEditor, Receiver::ActionController},
        UpdateDescription::All);

    loggingService->logMessage(Priority::Unimportant, "remove free cell connections finished");
}

void ActionController::onShowAbout()
{
    QFile file("://Version.txt");
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::information(nullptr, "error", file.errorString());
    }
    else {
        QTextStream in(&file);
        auto version = in.readLine();
        QMessageBox msgBox(QMessageBox::Information, "About", QString(Const::InfoAbout).arg(version));
        msgBox.exec();
    }
}

void ActionController::onToggleGettingStarted(bool toggled)
{
    _mainView->toggleGettingStarted(toggled);
}

void ActionController::onShowDocumentation()
{
	_mainView->showDocumentation();
}

void ActionController::onToggleRestrictTPS(bool toggled)
{
    auto loggingService = ServiceLocator::getInstance().getService<LoggingService>();

	if (toggled) {
        bool ok;
        auto restrictedTPS = QInputDialog::getInt(
            _mainView,
            "Restrict TPS",
            "Enter the maximum number time steps per seconds",
            50,
            1,
            1000,
            1,
            &ok);
        if (!ok) {
			auto const actionHolder = _model->getActionHolder();
            actionHolder->actionRestrictTPS->setChecked(false);
            return;
        }
        loggingService->logMessage(Priority::Unimportant, "activate restrict time steps per seconds");

		_mainController->onRestrictTPS(restrictedTPS);
        _infoController->setRestrictedTPS(restrictedTPS);
	}
	else {
        loggingService->logMessage(Priority::Unimportant, "deactivate restrict time steps per seconds");
        _mainController->onRestrictTPS(boost::none);
        _infoController->setRestrictedTPS(boost::none);
    }

    loggingService->logMessage(Priority::Unimportant, "toggle restrict time steps per seconds finished");
}

void ActionController::receivedNotifications(set<Receiver> const & targets)
{
	if (targets.find(Receiver::ActionController) == targets.end()) {
		return;
	}

	int selectedCells = _repository->getSelectedCellIds().size();
	int selectedParticles = _repository->getSelectedParticleIds().size();
	int tokenOfSelectedCell = 0;
	int freeTokenOfSelectedCell = 0;

	if (selectedCells == 1 && selectedParticles == 0) {
		uint64_t selectedCellId = *_repository->getSelectedCellIds().begin();
		if (auto tokens = _repository->getCellDescRef(selectedCellId).tokens) {
			tokenOfSelectedCell = tokens->size();
			freeTokenOfSelectedCell = _mainModel->getSimulationParameters().cellMaxToken - tokenOfSelectedCell;
		}
	}

	_model->setEntitySelected(selectedCells == 1 || selectedParticles == 1);
	_model->setCellWithTokenSelected(tokenOfSelectedCell > 0);
	_model->setCellWithFreeTokenSelected(freeTokenOfSelectedCell > 0);
	_model->setCollectionSelected(selectedCells > 0 || selectedParticles > 0);
    _model->setCellsSelected(selectedCells > 0);

	updateActionsEnableState();
}

void ActionController::settingUpNewSimulation(SimulationConfig const& config)
{
    _infoController->setRendering(GeneralInfoController::Rendering::OpenGL);

    auto actions = _model->getActionHolder();
    actions->actionRunSimulation->setChecked(false);
    actions->actionRestore->setEnabled(false);
    actions->actionRunStepBackward->setEnabled(false);
    actions->actionDisplayLink->setEnabled(true);
    actions->actionDisplayLink->setChecked(true);
    actions->actionGlowEffect->setEnabled(true);
    actions->actionSimulationChanger->setChecked(false);
    actions->actionWebSimulation->setChecked(false);
    onRunClicked(false);
    onToggleCellInfo(true);

    {
        bool oldState = actions->actionRestrictTPS->blockSignals(true);
        actions->actionRestrictTPS->setChecked(false);
        onToggleRestrictTPS(false);
        actions->actionRestrictTPS->blockSignals(oldState);
    }
    actions->actionOpenGLView->setChecked(true);
}

void ActionController::updateActionsEnableState()
{
	bool editMode = _model->isEditMode();
	bool entitySelected = _model->isEntitySelected();
	bool entityCopied = _model->isEntityCopied();
	bool cellWithTokenSelected = _model->isCellWithTokenSelected();
	bool cellWithFreeTokenSelected = _model->isCellWithFreeTokenSelected();
	bool tokenCopied = _model->isTokenCopied();
	bool collectionSelected = _model->isCollectionSelected();
	bool collectionCopied = _model->isCollectionCopied();
    bool cellsSelected = _model->areCellsSelected();

	auto actions = _model->getActionHolder();
    actions->actionItemView->setEnabled(
        _simulationViewController->getZoomFactor() + FLOATINGPOINT_MEDIUM_PRECISION > Const::MinZoomLevelForEditor);
    actions->actionGlowEffect->setEnabled(!editMode);
	actions->actionShowCellInfo->setEnabled(editMode);
    actions->actionCenterSelection->setEnabled(editMode);

	actions->actionNewCell->setEnabled(true);
	actions->actionNewParticle->setEnabled(true);
	actions->actionCopyEntity->setEnabled(editMode && entitySelected);
	actions->actionPasteEntity->setEnabled(editMode && entityCopied);
	actions->actionDeleteEntity->setEnabled(editMode && entitySelected);
    actions->actionNewToken->setEnabled(editMode && cellWithFreeTokenSelected);
	actions->actionCopyToken->setEnabled(editMode && cellWithTokenSelected);
	actions->actionPasteToken->setEnabled(editMode && entitySelected && tokenCopied);
	actions->actionDeleteToken->setEnabled(editMode && cellWithTokenSelected);
    actions->actionCopyToClipboard ->setEnabled(editMode && cellWithTokenSelected);
    actions->actionPasteFromClipboard->setEnabled(editMode && cellWithTokenSelected);

	actions->actionNewRectangle->setEnabled(true);
	actions->actionNewHexagon->setEnabled(true);
	actions->actionNewParticles->setEnabled(true);
    actions->actionLoadCol->setEnabled(true);
	actions->actionSaveCol->setEnabled(editMode && collectionSelected);
	actions->actionCopyCol->setEnabled(editMode && collectionSelected);
	actions->actionPasteCol->setEnabled(collectionCopied);
	actions->actionDeleteSel->setEnabled(editMode && collectionSelected);
	actions->actionDeleteCol->setEnabled(editMode && collectionSelected);
    actions->actionColorizeSel->setEnabled(collectionSelected);
    actions->actionGenerateBranchNumbers->setEnabled(cellsSelected);
    actions->actionRandomizeCellFunctions->setEnabled(cellsSelected);
    actions->actionRemoveFreeCellConnections->setEnabled(cellsSelected);
    actions->actionRandomMultiplier->setEnabled(collectionSelected);
	actions->actionGridMultiplier->setEnabled(collectionSelected);
}

void ActionController::setPixelOrVectorView()
{
	auto loggingService = ServiceLocator::getInstance().getService<LoggingService>();
    loggingService->logMessage(Priority::Unimportant, "toggle to vector rendering");

    _infoController->setRendering(GeneralInfoController::Rendering::OpenGL);
    _simulationViewController->disconnectView();
    _simulationViewController->setActiveScene(ActiveView::OpenGLScene);
    _simulationViewController->connectView();

	loggingService->logMessage(Priority::Unimportant, "toggle to vector rendering finished");
}
