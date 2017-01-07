#ifndef SIMULATIONUNIT_H
#define SIMULATIONUNIT_H

#include <QThread>
#include "definitions.h"

class SimulationUnit : public QObject
{
    Q_OBJECT
public:
    SimulationUnit (QObject* parent = 0);
    ~SimulationUnit ();

public slots:
	void init(SimulationContext* context);
	void setRandomSeed(uint seed);

public:
    qreal calcTransEnergy ();
    qreal calcRotEnergy ();
    qreal calcInternalEnergy ();

public slots:
    void calcNextTimestep ();

signals:
    void nextTimestepCalculated ();

protected:
    void debugCluster (CellCluster* c, int s);

    SimulationContext* _context = nullptr;
};

#endif // SIMULATIONUNIT_H
