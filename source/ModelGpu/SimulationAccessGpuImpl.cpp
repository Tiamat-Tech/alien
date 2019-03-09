#include <QImage>

#include "ModelBasic/SpaceProperties.h"
#include "ModelBasic/EntityRenderer.h"

#include "CudaWorker.h"
#include "ThreadController.h"
#include "SimulationContextGpuImpl.h"
#include "SimulationAccessGpuImpl.h"
#include "SimulationControllerGpu.h"
#include "DataConverter.h"

SimulationAccessGpuImpl::~SimulationAccessGpuImpl()
{
}

void SimulationAccessGpuImpl::init(SimulationControllerGpu* controller)
{
	_context = static_cast<SimulationContextGpuImpl*>(controller->getContext());
	_numberGen = _context->getNumberGenerator();
	auto cudaBridge = _context->getGpuThreadController()->getCudaWorker();
	auto size = _context->getSpaceProperties()->getSize();
	_requiredRect = { {0,0}, size };
	for (auto const& connection : _connections) {
		QObject::disconnect(connection);
	}
	_connections.push_back(connect(cudaBridge, &CudaWorker::dataObtained, this, &SimulationAccessGpuImpl::dataObtainedFromGpu, Qt::QueuedConnection));
}

void SimulationAccessGpuImpl::clear()
{
}

/*
namespace
{
	void enlargeRect(IntVector2D const& p, optional<IntRect>& rect)
	{
		if (rect) {
			if (p.x < rect->p1.x) {
				rect->p1.x = p.x;
			}
			if (p.y < rect->p1.y) {
				rect->p1.y = p.y;
			}
			if (p.x > rect->p2.x) {
				rect->p2.x = p.x;
			}
			if (p.y > rect->p2.y) {
				rect->p2.y = p.y;
			}
		}
		else {
			rect = IntRect{ p, p };
		}
	}
	
	IntRect getRect(DataChangeDescription const & desc)
	{
		optional<IntRect> result;
		for (auto const& cluster : desc.clusters) {
			IntVector2D pos(*cluster->pos);
			enlargeRect(pos, result);
		}
		for (auto const& particle : desc.particles) {
			IntVector2D pos(*particle->pos);
			enlargeRect(pos, result);
		}
		return result.get_value_or(IntRect());
	}
}
*/

void SimulationAccessGpuImpl::updateData(DataChangeDescription const & desc)
{
	_dataToUpdate.clusters.insert(_dataToUpdate.clusters.end(), desc.clusters.begin(), desc.clusters.end());
	_dataToUpdate.particles.insert(_dataToUpdate.particles.end(), desc.particles.begin(), desc.particles.end());

	metricCorrection(_dataToUpdate);

	auto cudaWorker = _context->getGpuThreadController()->getCudaWorker();

	if (cudaWorker->isSimulationRunning()) {
		cudaWorker->requireData({ {0, 0}, _context->getSpaceProperties()->getSize() });
	}
	else {
		cudaWorker->requireData(_requiredRect);
	}
}

void SimulationAccessGpuImpl::requireData(IntRect rect, ResolveDescription const & resolveDesc)
{
	_dataDescRequired = true;
	_requiredRect = rect;
	_resolveDesc = resolveDesc;

	auto cudaWorker = _context->getGpuThreadController()->getCudaWorker();
	cudaWorker->requireData(_requiredRect);
}

void SimulationAccessGpuImpl::requireImage(IntRect rect, QImage * target)
{
	_imageRequired = true;
	_requiredRect = rect;
	_requiredImage = target;

	auto cudaWorker = _context->getGpuThreadController()->getCudaWorker();
	cudaWorker->requireData(_requiredRect);
}

DataDescription const & SimulationAccessGpuImpl::retrieveData()
{
	return _dataCollected;
}

void SimulationAccessGpuImpl::dataObtainedFromGpu()
{
	if (!_dataToUpdate.empty()) {
		updateDataToGpuModel();
		Q_EMIT dataUpdated();
	}

	if (_imageRequired) {
		_imageRequired = false;
		createImageFromGpuModel();
		Q_EMIT imageReady();
	}

	if (_dataDescRequired) {
		_dataDescRequired = false;
		createDataFromGpuModel();
		Q_EMIT dataReadyToRetrieve();
	}
}

void SimulationAccessGpuImpl::updateDataToGpuModel()
{
	auto cudaWorker = _context->getGpuThreadController()->getCudaWorker();

	cudaWorker->lockData();
	SimulationAccessTO* simulationTO = cudaWorker->retrieveData();

	DataConverter converter(simulationTO, _numberGen);
	converter.updateData(_dataToUpdate);

	cudaWorker->updateData();
	cudaWorker->unlockData();
	_dataToUpdate.clear();
}

void colorPixel(QImage* image, IntVector2D const& pos, QRgb const& color, int alpha)
{
	QRgb const& origColor = image->pixel(pos.x, pos.y);

	int red = (qRed(color) * alpha + qRed(origColor) * (255 - alpha)) / 255;
	int green = (qGreen(color) * alpha + qGreen(origColor) * (255 - alpha)) / 255;
	int blue = (qBlue(color) * alpha + qBlue(origColor) * (255 - alpha)) / 255;
	image->setPixel(pos.x, pos.y, qRgb(red, green, blue));
}

void SimulationAccessGpuImpl::createImageFromGpuModel()
{
	auto space = _context->getSpaceProperties();
	auto cudaWorker = _context->getGpuThreadController()->getCudaWorker();

	_requiredImage->fill(QColor(0x00, 0x00, 0x1b));

	cudaWorker->lockData();
	SimulationAccessTO* simulationTO = cudaWorker->retrieveData();

	for (int i = 0; i < *simulationTO->numParticles; ++i) {
		ParticleAccessTO& particle = simulationTO->particles[i];
		float2& pos = particle.pos;
		IntVector2D intPos = { static_cast<int>(pos.x), static_cast<int>(pos.y) };
		space->correctPosition(intPos);
		_requiredImage->setPixel(intPos.x, intPos.y, EntityRenderer::calcParticleColor(particle.energy));
	}

	for (int i = 0; i < *simulationTO->numCells; ++i) {
		CellAccessTO& cell = simulationTO->cells[i];
		float2 const& pos = cell.pos;
		IntVector2D intPos = { static_cast<int>(pos.x), static_cast<int>(pos.y) };
		space->correctPosition(intPos);
		uint32_t color = EntityRenderer::calcCellColor(0, 0, cell.energy);
		_requiredImage->setPixel(intPos.x, intPos.y, color);
		--intPos.x;
		space->correctPosition(intPos);
		colorPixel(_requiredImage, intPos, color, 0x60);
		intPos.x += 2;
		space->correctPosition(intPos);
		colorPixel(_requiredImage, intPos, color, 0x60);
		--intPos.x;
		--intPos.y;
		space->correctPosition(intPos);
		colorPixel(_requiredImage, intPos, color, 0x60);
		intPos.y += 2;
		space->correctPosition(intPos);
		colorPixel(_requiredImage, intPos, color, 0x60);
	}

	cudaWorker->unlockData();
}

void SimulationAccessGpuImpl::createDataFromGpuModel()
{
	_dataCollected.clear();
	auto cudaBridge = _context->getGpuThreadController()->getCudaWorker();

	cudaBridge->lockData();
	SimulationAccessTO* simulationTO = cudaBridge->retrieveData();
	DataConverter converter(simulationTO, _numberGen);
	_dataCollected = converter.getDataDescription(_requiredRect);

	cudaBridge->unlockData();
}

void SimulationAccessGpuImpl::metricCorrection(DataChangeDescription & data) const
{
	SpaceProperties* space = _context->getSpaceProperties();
	for (auto& cluster : data.clusters) {
		QVector2D origPos = cluster->pos.getValue();
		auto pos = origPos;
		space->correctPosition(pos);
		auto correctionDelta = pos - origPos;
		if (!correctionDelta.isNull()) {
			cluster->pos.setValue(pos);
		}
		for (auto& cell : cluster->cells) {
			cell->pos.setValue(cell->pos.getValue() + correctionDelta);
		}
	}
	for (auto& particle : data.particles) {
		QVector2D origPos = particle->pos.getValue();
		auto pos = origPos;
		space->correctPosition(pos);
		if (pos != origPos) {
			particle->pos.setValue(pos);
		}
	}
}
