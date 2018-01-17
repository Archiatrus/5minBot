#pragma once

#include "sc2api/sc2_api.h"
#include "BotAssert.h"
#include <iostream>
#include <vector>
#include <cassert>
#include <set>
#include <fstream>
#include <streambuf>
#include <string>

typedef uint64_t UnitTag;

namespace Players
{
    enum {Self = 0, Enemy = 1};
}

namespace sc2
{

	enum class EFFECT_ID;
	typedef SC2Type<EFFECT_ID>  EffectID;
	enum class EFFECT_ID
	{
		INVALID = 0,
		PSISTORM = 1,
		GUARDIANSHIELD = 2,
		TEMPORALFIELDGROWING = 3,
		TEMPORALFIELD = 4,
		THERMALLANCES = 5,
		SCANNERSWEEP = 6,
		NUKEDOT = 7,
		LIBERATORMORPHING = 8,
		LIBERATORMORPHED = 9,
		BLINDINGCLOUD = 10,
		CORROSIVEBILE = 11,
		LURKERATTACK = 12
	};
}