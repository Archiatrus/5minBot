#pragma once

#include "Common.h"
#include "MicroManager.h"

class CCBot;

class SiegeManager : public MicroManager
{

public:

	SiegeManager(CCBot & bot);
	void    executeMicro(const CUnits & targets);
	void    assignTargets(const CUnits & targets);
	float SiegeManager::getAttackPriority(const CUnit_ptr attacker, const CUnit_ptr unit) const;
	const CUnit_ptr getTarget(const CUnit_ptr siegeUnit, const CUnits & targets);
};
