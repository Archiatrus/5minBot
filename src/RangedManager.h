#pragma once

#include "Common.h"
#include "MicroManager.h"

class CCBot;

class RangedManager: public MicroManager
{
public:

    RangedManager(CCBot & bot);
    void    executeMicro(const CUnits & targets);
    void    assignTargets(const CUnits & targets);
    int     getAttackPriority(const CUnit_ptr & target);
	std::map<float, CUnits, std::greater<float>>	getAttackPriority(const CUnits & unit);
	std::vector<std::pair<float, CUnit_ptr>> getTarget(const CUnit_ptr & unit, const std::map<float, CUnits, std::greater<float>>& sortedEnemies);
    const CUnit_ptr getTarget(const CUnit_ptr rangedUnit, const CUnits & targets);
};
