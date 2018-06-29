#pragma once

#include "Common.h"
#include "Squad.h"
#include "SquadData.h"
#include <queue>
#include <tuple>

class CCBot;

class CombatCommander
{
	CCBot &		 m_bot;

	SquadData       m_squadData;
	CUnits			m_combatUnits;
	bool            m_initialized;
	bool            m_attackStarted;

	bool			m_needGuards;
	bool			m_underAttack;
	bool			m_attack;
	std::vector<std::tuple<uint32_t, sc2::UNIT_TYPEID, sc2::Point2D> > m_cloakedUnits;

	void			updateScoutDefenseSquad();
	void			updateDefenseSquads();
	void			updateAttackSquads();
	void			updateGuardSquads();
	void			updateIdleSquad();
	bool			isSquadUpdateFrame();

	const CUnit_ptr findClosestDefender(const Squad & defenseSquad, const sc2::Point2D & pos);
	const CUnit_ptr findClosestWorkerTo(const CUnits & unitsToAssign, const sc2::Point2D & target);


	sc2::Point2D	getMainAttackLocation();

	void			updateDefenseSquadUnits(Squad & defenseSquad, const size_t & flyingDefendersNeeded, const size_t & groundDefendersNeeded);
	bool			shouldWeStartAttacking();
	void			checkForProxyOrCheese();

	void checkDepots() const;

	void checkForLostWorkers();

	void handlePlanetaries() const;
	void handleCloakedUnits();


 public:
	CombatCommander(CCBot & bot);


	void onStart();
	void onFrame(const CUnits & combatUnits);

	void drawSquadInformation();
	const bool underAttack() const;
	void attack(const bool attack);
	void requestGuards(const bool req);
	void addCloakedUnit(const sc2::UNIT_TYPEID & type, const sc2::Point2D & pos);
};
