#pragma once

#include "Common.h"
#include "MicroManager.h"

class CCBot;

class SiegeManager : public MicroManager
{

public:

	SiegeManager(CCBot & bot);
	void    executeMicro(const std::vector<const sc2::Unit *> & targets);
	void    assignTargets(const std::vector<const sc2::Unit *> & targets);
	int     getAttackPriority(const sc2::Unit * attacker, const sc2::Unit * unit);
	const sc2::Unit * getTarget(const sc2::Unit * siegeUnit, const std::vector<const sc2::Unit *> & targets);
};
