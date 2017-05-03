#include <gtest/gtest.h>

#include "global/ServiceLocator.h"
#include "model/BuilderFacade.h"
#include "model/ModelSettings.h"
#include "model/SimulationController.h"
#include "model/context/SimulationContext.h"
#include "model/context/UnitGrid.h"
#include "model/context/Unit.h"
#include "model/context/UnitContext.h"
#include "model/context/MapCompartment.h"

#include "tests/Predicates.h"

class UnitGridTest : public ::testing::Test
{
public:
	UnitGridTest();
	~UnitGridTest();

protected:
	IntVector2D correctUniversePosition(IntVector2D const& pos);

	SimulationController* _controller = nullptr;
	SimulationContext* _context = nullptr;
	UnitGrid* _grid = nullptr;
	IntVector2D _gridSize{ 6, 6 };
	IntVector2D _universeSize{ 1200, 600 };
	IntVector2D _compartmentSize;
};

UnitGridTest::UnitGridTest()
{
	_controller = new SimulationController();
	BuilderFacade* facade = ServiceLocator::getInstance().getService<BuilderFacade>();
	auto metric = facade->buildSpaceMetric(_universeSize);
	auto symbols = ModelSettings::loadDefaultSymbolTable();
	auto parameters = ModelSettings::loadDefaultSimulationParameters();
	_context = facade->buildSimulationContext(4, _gridSize, metric, symbols, parameters, _controller);
	_controller->newUniverse(_context);
	_grid = _context->getUnitGrid();
	_compartmentSize = { _universeSize.x / _gridSize.x, _universeSize.y / _gridSize.y };
}

UnitGridTest::~UnitGridTest()
{
	delete _controller;
}

IntVector2D UnitGridTest::correctUniversePosition(IntVector2D const & pos)
{
	return{ ((pos.x % _universeSize.x) + _universeSize.x) % _universeSize.x, ((pos.y % _universeSize.y) + _universeSize.y) % _universeSize.y };
}

TEST_F(UnitGridTest, testGridSize)
{
	ASSERT_PRED2(predEqualIntVector, _gridSize, _grid->getSize());
}

TEST_F(UnitGridTest, testCompartmentRects)
{
	for (int x = 0; x < _gridSize.x; ++x) {
		for (int y = 0; y < _gridSize.y; ++y) {
			auto rect = _grid->calcCompartmentRect({ x, y });
			IntVector2D expectedRectP1 = { x*_compartmentSize.x, y*_compartmentSize.y };
			IntVector2D expectedRectP2 = { (x + 1)*_compartmentSize.x - 1, (y + 1)*_compartmentSize.y - 1 };
			ASSERT_PRED2(predEqualIntVector, expectedRectP1, rect.p1);
			ASSERT_PRED2(predEqualIntVector, expectedRectP2, rect.p2);
		}
	}
}
