#pragma once

struct SimulationParametersSpotValues
{
    float friction = 0.001f;
    float radiationFactor = 0.0002f;
    float cellMaxForce = 0.8f;
    float cellMinEnergy = 50.0f;

    float cellBindingForce = 1.0f;
    float cellFusionVelocity = 0.4f;
    float cellMaxBindingEnergy = 500000.0f;

    float tokenMutationRate = 0.0005f;
    float cellFunctionWeaponEnergyCost = 0.2f;
    float cellFunctionWeaponColorPenalty = 0.0f;
    float cellFunctionWeaponGeometryDeviationExponent = 0.0f;

    bool operator==(SimulationParametersSpotValues const& other) const
    {
        return friction == other.friction && radiationFactor == other.radiationFactor
            && cellMaxForce == other.cellMaxForce && cellMinEnergy == other.cellMinEnergy
            && cellBindingForce == other.cellBindingForce && cellFusionVelocity == other.cellFusionVelocity
            && tokenMutationRate == other.tokenMutationRate
            && cellFunctionWeaponEnergyCost == other.cellFunctionWeaponEnergyCost
            && cellFunctionWeaponColorPenalty == other.cellFunctionWeaponColorPenalty
            && cellFunctionWeaponGeometryDeviationExponent == other.cellFunctionWeaponGeometryDeviationExponent
            && cellMaxBindingEnergy == other.cellMaxBindingEnergy;
    }
};
