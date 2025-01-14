#pragma once

#include <cooperative_groups.h>
#include "cuda_runtime_api.h"
#include "sm_60_atomic_functions.h"

#include "AccessTOs.cuh"
#include "Base.cuh"
#include "Map.cuh"
#include "CellProcessor.cuh"
#include "ParticleProcessor.cuh"
#include "TokenProcessor.cuh"
#include "CleanupKernels.cuh"
#include "Operation.cuh"
#include "DebugKernels.cuh"
#include "SimulationResult.cuh"
#include "FlowFieldKernel.cuh"

__global__ void processingStep1(SimulationData data)
{
    CellProcessor cellProcessor;
    cellProcessor.init(data);
    cellProcessor.clearTag(data);
    cellProcessor.updateMap(data);
    cellProcessor.radiation(data);  //do not use ParticleProcessor in this kernel
}

__global__ void processingStep2(SimulationData data)
{
    CellProcessor cellProcessor;
    cellProcessor.collisions(data);

    ParticleProcessor particleProcessor;
    particleProcessor.updateMap(data);
}

__global__ void processingStep3(SimulationData data)
{
    CellProcessor cellProcessor;
    cellProcessor.applyAndCheckForces(data);
    cellProcessor.clearTag(data);

    ParticleProcessor particleProcessor;
    particleProcessor.movement(data);
    particleProcessor.collision(data);
}

__global__ void processingStep4(SimulationData data, int numTokenPointers)
{
    CellProcessor cellProcessor;
    cellProcessor.calcForces(data);

    TokenProcessor tokenProcessor;
    tokenProcessor.movement(data, numTokenPointers);    //changes cell energy without lock
}

__global__ void processingStep5(SimulationData data)
{
    CellProcessor cellProcessor;
    cellProcessor.calcPositionsAndCheckBindings(data);
}

__global__ void processingStep6(SimulationData data, SimulationResult result)
{
    CellProcessor cellProcessor;
    cellProcessor.calcForces(data);

    TokenProcessor tokenProcessor;
    tokenProcessor.executeReadonlyCellFunctions(data, result);
}

__global__ void processingStep7(SimulationData data, int numCellPointers)
{
    CellProcessor cellProcessor;
    cellProcessor.calcVelocities(data, numCellPointers);
}

__global__ void processingStep8(SimulationData data, SimulationResult result, int numTokenPointers)
{
    TokenProcessor tokenProcessor;
    tokenProcessor.executeModifyingCellFunctions(data, result, numTokenPointers);
}

__global__ void processingStep9(SimulationData data)
{
    CellProcessor cellProcessor;
    cellProcessor.calcAveragedVelocities(data);
}

__global__ void processingStep10(SimulationData data)
{
    CellProcessor cellProcessor;
    cellProcessor.applyAveragedVelocities(data);
    cellProcessor.decay(data);
}

__global__ void processingStep11(SimulationData data)
{
    CellConnectionProcessor::processConnectionsOperations(data);
} 

__global__ void processingStep12(SimulationData data, int numParticlePointers)
{
    ParticleProcessor particleProcessor;
    particleProcessor.transformation(data, numParticlePointers);

    CellConnectionProcessor::processDelCellOperations(data);
}

/************************************************************************/
/* Main      															*/
/************************************************************************/

__global__ void calcSimulationTimestepKernel(SimulationData data, SimulationResult result)
{
    data.prepareForSimulation();
    result.resetStatistics();

    KERNEL_CALL_1_1(applyFlowFieldSettingsKernel, data);
    KERNEL_CALL(processingStep1, data);
    KERNEL_CALL(processingStep2, data);
    KERNEL_CALL(processingStep3, data);
    KERNEL_CALL(processingStep4, data, data.entities.tokenPointers.getNumEntries());
    KERNEL_CALL(processingStep5, data);
    KERNEL_CALL(processingStep6, data, result);
    KERNEL_CALL(processingStep7, data, data.entities.cellPointers.getNumEntries());
    KERNEL_CALL(processingStep8, data, result, data.entities.tokenPointers.getNumEntries());
    KERNEL_CALL(processingStep9, data);
    KERNEL_CALL(processingStep10, data);
    KERNEL_CALL(processingStep11, data);
    KERNEL_CALL(processingStep12, data, data.entities.particlePointers.getNumEntries());

    KERNEL_CALL_1_1(cleanupAfterSimulationKernel, data);

    result.setArrayResizeNeeded(data.shouldResize());
}

