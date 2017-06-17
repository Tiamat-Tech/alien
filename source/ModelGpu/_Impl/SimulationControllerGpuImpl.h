#pragma once

#include <QTime>

#include "Model/SimulationController.h"
#include "DefinitionsImpl.h"

class SimulationControllerGpuImpl
	: public SimulationController
{
	Q_OBJECT
public:
	SimulationControllerGpuImpl(QObject* parent = nullptr);
	virtual ~SimulationControllerGpuImpl() = default;

	virtual void init(SimulationContextApi* context) override;
	virtual void setRun(bool run) override;
	virtual void calculateSingleTimestep() override;
	virtual SimulationContextApi* getContext() const override;

private:
	Q_SLOT void oneSecondTimerTimeout();
	Q_SLOT void frameTimerTimeout();

	SimulationContextGpuImpl *_context = nullptr;

	RunningMode _mode = RunningMode::DoNothing;
	QTime _timeSinceLastStart;
	int _timestepsPerSecond = 0;
	int _displayedFramesSinceLastStart = 0;
	QTimer* _frameTimer = nullptr;
	QTimer* _oneSecondTimer = nullptr;
};