#pragma once

#include "Cell.cuh"
#include "SimulationData.cuh"
#include "Token.cuh"

class SensorFunction
{
public:
    __inline__ __device__ void init_block(Cluster* cluster, SimulationData* data);
    __inline__ __device__ void processing_block(Token* token);

private:
    __device__ __inline__ void searchVicinity(Token* token, int const& minSize, int const& maxSize, Cell*& result);
    __device__ __inline__ void searchByAngle(Token* token, int const& minSize, int const& maxSize, Cell*& result);
    __device__ __inline__ void searchFromCenter(Token* token, int const& minSize, int const& maxSize, Cell*& result);
    __device__ __inline__ void searchTowardCenter(Token* token, int const& minSize, int const& maxSize, Cell*& result);

    __device__ __inline__ void getNearbyCell(float2 const& pos, int range, int minSize, int maxSize, Cell*& result);

    __device__ __inline__ void
    getNearbyCellAlongBeam(float2 const& pos, float angle, int minSize, int maxSize, Cell*& result);

    struct AngleAndDistance
    {
        float angle;
        float distance;
    };
    __device__ __inline__ AngleAndDistance getAngleAndDistance(Cell* cell, Cell* sourceCell, Cell* scanCell) const;

private:
    SimulationData* _data;
    Cluster* _cluster;
};

/************************************************************************/
/* Implementation                                                       */
/************************************************************************/

__inline__ __device__ void SensorFunction::init_block(Cluster* cluster, SimulationData* data)
{
    _data = data;
    _cluster = cluster;
}

__inline__ __device__ void SensorFunction::processing_block(Token* token)
{
    auto& tokenMem = token->memory;

    __shared__ int command;
    if (0 == threadIdx.x) {
        command = static_cast<unsigned char>(tokenMem[Enums::Sensor_Input]) % Enums::SensorIn::_COUNTER;
    }
    __syncthreads();

    if (Enums::SensorIn_DoNothing == command) {
        tokenMem[Enums::Sensor_Output] = Enums::SensorOut_NothingFound;
        return;
    }

    __shared__ int minMass;
    __shared__ int maxMass;
    if (0 == threadIdx.x) {
        minMass = static_cast<unsigned char>(tokenMem[Enums::Sensor_InMinMass]);
        maxMass = static_cast<unsigned char>(tokenMem[Enums::Sensor_InMaxMass]);
        if (0 == maxMass) {
            maxMass = 16000;  //large value => no max mass check
        }
    }
    __syncthreads();

    __shared__ Cell* scanCell;
    if (Enums::SensorIn_SearchVicinity == command) {
        searchVicinity(token, minMass, maxMass, scanCell);
    }
    else if (Enums::SensorIn_SearchByAngle == command) {
        searchByAngle(token, minMass, maxMass, scanCell);
    }
    else if (Enums::SensorIn_SearchFromCenter == command) {
        searchFromCenter(token, minMass, maxMass, scanCell);
    }
    else {
        searchTowardCenter(token, minMass, maxMass, scanCell);
    }
    __syncthreads();

    if (!scanCell) {
        if (0 == threadIdx.x) {
            tokenMem[Enums::Sensor_Output] = Enums::SensorOut_NothingFound;
        }
        __syncthreads();

        return;
    }

    if (0 == threadIdx.x) {
        auto const& cell = token->cell;
        auto const& sourceCell = token->sourceCell;
        auto const angleAndDistance = getAngleAndDistance(cell, sourceCell, scanCell);
        tokenMem[Enums::Sensor_Output] = Enums::SensorOut_ClusterFound;
        tokenMem[Enums::Sensor_OutDistance] = QuantityConverter::convertDistanceToData(angleAndDistance.distance);
        tokenMem[Enums::Sensor_InOutAngle] = QuantityConverter::convertAngleToData(angleAndDistance.angle);
        tokenMem[Enums::Sensor_OutMass] = QuantityConverter::convertURealToData(scanCell->cluster->numCellPointers);
        scanCell->cluster->unfreeze(30);
    }
    __syncthreads();
}


__device__ __inline__ void
SensorFunction::searchVicinity(Token* token, int const& minSize, int const& maxSize, Cell*& result)
{
    auto const& cell = token->cell;
    getNearbyCell(cell->absPos, cudaSimulationParameters.cellFunctionSensorRange, minSize, maxSize, result);
}

__device__ __inline__ void
SensorFunction::searchByAngle(Token* token, int const& minSize, int const& maxSize, Cell*& result)
{
    auto const& cell = token->cell;
    auto const& sourceCell = token->sourceCell;
    auto& tokenMem = token->memory;

    __shared__ float angle;
    if (0 == threadIdx.x) {
        auto const relAngle = QuantityConverter::convertDataToAngle(tokenMem[Enums::Sensor_InOutAngle]);
        angle = Math::angleOfVector(sourceCell->relPos - cell->relPos) + _cluster->angle + relAngle;
    }
    __syncthreads();

    getNearbyCellAlongBeam(cell->absPos, angle, minSize, maxSize, result);
}

__device__ __inline__ void
SensorFunction::searchFromCenter(Token* token, int const& minSize, int const& maxSize, Cell*& result)
{
    auto const& cell = token->cell;

    __shared__ float angle;

    if (0 == threadIdx.x) {
        angle = Math::angleOfVector(cell->absPos -_cluster->pos);
    }
    __syncthreads();

    getNearbyCellAlongBeam(cell->absPos, angle, minSize, maxSize, result);
}

__device__ __inline__ void
SensorFunction::searchTowardCenter(Token* token, int const& minSize, int const& maxSize, Cell*& result)
{
    auto const& cell = token->cell;

    __shared__ float angle;

    if (0 == threadIdx.x) {
        angle = Math::angleOfVector(_cluster->pos - cell->absPos);
    }
    __syncthreads();

    getNearbyCellAlongBeam(cell->absPos, angle, minSize, maxSize, result);
}

__device__ __inline__ void
SensorFunction::getNearbyCell(float2 const& pos, int range, int minSize, int maxSize, Cell*& result)
{
    __shared__ int stepSize;
    __shared__ int numScanPointsPerAxis;
    __shared__ float distanceToResult;
    __shared__ int resultLock;
    if (0 == threadIdx.x) {
        stepSize = ceil(sqrt(minSize + FP_PRECISION)) + 3;
        numScanPointsPerAxis = (range * 2 + 1) / stepSize;
        result = nullptr;
        distanceToResult = 10000.0;
        resultLock = 0;
    }
    __syncthreads();

    auto const threadPartition = calcPartition(numScanPointsPerAxis * numScanPointsPerAxis, threadIdx.x, blockDim.x);
    for (int index = threadPartition.startIndex; index <= threadPartition.endIndex; ++index) {
        int x = index % numScanPointsPerAxis;
        int y = index / numScanPointsPerAxis;
        auto const posDelta =
            float2{static_cast<float>(x * stepSize - range), static_cast<float>(y * stepSize - range)};
        if (Math::length(posDelta) > range) {
            continue;
        }
        auto const scanCell = _data->cellMap.get(pos + posDelta);
        if (!scanCell) {
            continue;
        }
        auto const scanCluster = scanCell->cluster;
        if (_cluster == scanCluster) {
            continue;
        }
        auto const scanSize = scanCluster->numCellPointers;
        if (scanSize >= minSize && scanSize <= maxSize) {
            auto const distance = _data->cellMap.mapDistance(scanCell->absPos, pos);

            while (1 == atomicExch_block(&resultLock, 1)) {}
            __threadfence_block();
            if (!result || (distance < distanceToResult)) {
                result = scanCell;
                distanceToResult = distance;
            }
            __threadfence_block();
            atomicExch_block(&resultLock, 0);
        }
    }
    __syncthreads();
}

__device__ __inline__ void
SensorFunction::getNearbyCellAlongBeam(float2 const& pos, float angle, int minSize, int maxSize, Cell*& result)
{
    __shared__ float2 direction;

    __shared__ int hitLock;
    __shared__ bool hit;
    __shared__ int hitDistance;

    if (0 == threadIdx.x) {
        direction = Math::unitVectorOfAngle(angle);
        hit = false;
        hitLock = 0;
        result = nullptr;
    }
    __syncthreads();

    auto const threadPartition =
        calcPartition((cudaSimulationParameters.cellFunctionSensorRange - 1) / 2, threadIdx.x, blockDim.x);
    for (int index = threadPartition.startIndex; index <= threadPartition.startIndex; ++index) {
        auto const distance = index * 2 + 1;
        for (int deltaX = -1; deltaX < 2; ++deltaX) {
            for (int deltaY = -1; deltaY < 2; ++deltaY) {
                auto const scanPos =
                    pos + direction * distance + float2{static_cast<float>(deltaX), static_cast<float>(deltaY)};
                auto const scanCell = _data->cellMap.get(scanPos);
                if (!scanCell) {
                    continue;
                }
                if (scanCell->cluster == _cluster) {
                    continue;
                }

                auto const massSize = scanCell->cluster->numCellPointers;
                if (massSize >= minSize && massSize <= maxSize) {
                    while (1 == atomicExch_block(&hitLock, 1)) {}
                    __threadfence_block();
                    if (!hit || distance < hitDistance) {
                        hit = true;
                        hitDistance = distance;
                        result = scanCell;
                    }
                    __threadfence_block();
                    atomicExch_block(&hitLock, 0);
                }
            }
        }
    }
    __syncthreads();
}

__device__ __inline__ auto SensorFunction::getAngleAndDistance(Cell* cell, Cell* sourceCell, Cell* scanCell) const
    -> AngleAndDistance
{
    AngleAndDistance result;
    auto directionOfScanCell = scanCell->absPos - cell->absPos;
    _data->cellMap.mapDisplacementCorrection(directionOfScanCell);
    result.distance = Math::length(directionOfScanCell);

    Math::normalize(directionOfScanCell);
    auto const sourceCellAngle = Math::angleOfVector(sourceCell->relPos - cell->relPos);
    auto const scanCellAngle = Math::angleOfVector(directionOfScanCell);
    result.angle = scanCellAngle - sourceCellAngle - _cluster->angle;

    return result;
}
