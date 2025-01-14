#pragma once

#include <cstdint>
#include <map>
#include <string>

#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>

#include "ElementaryTypes.h"

struct SimulationParameters;

struct DataDescription;
struct ClusterDescription;
struct CellDescription;
struct ParticleDescription;

struct DataChangeDescription;
struct CellChangeDescription;
struct ParticleChangeDescription;

struct GpuSettings;

struct GeneralSettings;
struct Settings;

class _Serializer;
using Serializer = boost::shared_ptr<_Serializer>;

struct OverallStatistics;
