#include "HarassManager.h"
#include "Util.h"
#include "CCBot.h"
#include "pathPlaning.h"
#include "Drawing.h"
#include "ShuttleService.h"

const int updateRatePathplaning = 10;


Hitsquad::Hitsquad(CCBot & bot, CUnit_ptr medivac) : m_bot(bot), m_status(HarassStatus::Idle), m_medivac(medivac),m_pathPlanCounter(updateRatePathplaning+1), m_target(nullptr)
{
}

void Hitsquad::escapePathPlaning()
{
	const BaseLocation * saveBase = getSavePosition();
	sc2::Point2D targetPos = saveBase->getBasePosition();
	
	pathPlaning escapePlan(m_bot, m_medivac->getPos(), targetPos, m_bot.Map().width(), m_bot.Map().height(), 1.0f);
	
	std::vector<sc2::Point2D> escapePath=escapePlan.planPath();
	if (escapePath.empty())
	{
		return;
	}
	m_wayPoints.push(escapePath.front());
	for (size_t i = 1; i<escapePath.size() - 1; ++i)
	{
		const sc2::Point2D directionNext = escapePath[i] - m_wayPoints.back();
		const sc2::Point2D directionNextNext = escapePath[i + 1] - m_wayPoints.back();
		if (directionNext.x*directionNextNext.y != directionNext.y*directionNextNext.x)
		{
			m_wayPoints.push(escapePath[i]);
			if (m_wayPoints.size() > 0)
			{
				Drawing::drawLine(m_bot, m_wayPoints.back(), escapePath[i]);
			}
		}
	}
	m_wayPoints.push(escapePath.back());
}


const bool Hitsquad::addMedivac(CUnit_ptr medivac)
{
	if (!m_medivac)
	{
		m_medivac=medivac;
		m_status = HarassStatus::Idle;
		return true;
	}
	return false;
}

const bool Hitsquad::addMarine(CUnit_ptr marine)
{
	if (m_marines.size() < 8)
	{
		m_marines.push_back(marine);
		return true;
	}
	return false;
}

const int Hitsquad::getNumMarines() const
{
	return static_cast<int>(m_marines.size());
}

CUnits Hitsquad::getMarines() const
{
	return m_marines;
}

CUnit_ptr Hitsquad::getMedivac() const
{
	return m_medivac;
}

const int Hitsquad::getStatus() const
{
	return m_status;
}

bool isDead(const CUnit_ptr unit)
{
	return !(unit->isAlive());
}

void Hitsquad::checkForCasualties()
{
	//Last game loop check prevents units in bunker... but this is also true for in medivac
	m_marines.erase(std::remove_if(m_marines.begin(), m_marines.end(), [this](const CUnit_ptr & unit) {
		if (!unit->isAlive())
		{
			return true;
		}
		if (unit->getLastSeenGameLoop() == m_bot.Observation()->GetGameLoop())
		{
			return false;
		}
		for (const auto & p : m_medivac->getPassengers())
		{
			if (p.tag == unit->getTag())
			{
				return false;
			}
		}
		return true;
	}), m_marines.end());
	
	
	//m_marines.erase(std::remove_if(m_marines.begin(), m_marines.end(), &isDead), m_marines.end());
	if (m_medivac && !m_medivac->isAlive())
	{
		m_medivac = nullptr;
		m_status = HarassStatus::Doomed;
		std::copy(m_marines.begin(), m_marines.end(), std::back_inserter(m_doomedMarines));
		m_marines.erase(m_marines.begin(), m_marines.end());
	}
}

bool Hitsquad::harass(const BaseLocation * target)
{
	//New target
	if (target)
	{
		//target = new target
		m_target = target;
	}
	//No new target
	else
	{
		//new target = previous target
		if (m_target)
		{
			target = m_target;
		}
		else
		{
			return false;
		}
	}
	checkForCasualties();
	switch (m_status)
	{
	case HarassStatus::Idle:
		if (haveNoTarget())
		{
			if (m_medivac && m_medivac->getCargoSpaceTaken() > 0)
			{
				Micro::SmartAbility(m_medivac, sc2::ABILITY_ID::UNLOADALLAT, m_medivac, m_bot);
				return true;
			}
			else
			{
				return false;
			}
		}
		//We get here if we have a brand new hit squad or want to restart from home
		if (m_medivac)
		{
			if (m_medivac->getHealth() == m_medivac->getHealthMax())
			{
				if (m_marines.size() == 8)
				{
					//Everybody there? Start loading!
					while (!m_wayPoints.empty())
					{
						m_wayPoints.pop();
					}
					m_status = HarassStatus::Loading;
				}
			}
			else if (m_medivac->getHealth() != m_medivac->getHealthMax())
			{
				m_bot.Workers().setRepairWorker(m_medivac,3);
			}
		}
		break;
	case HarassStatus::Loading:
		if (m_medivac)
		{
			if (m_marines.size() < 6)
			{
				m_status = HarassStatus::Fleeing;
			}
			if (m_medivac->getCargoSpaceTaken() < m_marines.size())
			{
				Micro::SmartRightClick(m_marines, m_medivac, m_bot);
				Micro::SmartRightClick(m_medivac, m_marines, m_bot);
				Micro::SmartCDAbility(m_medivac, sc2::ABILITY_ID::EFFECT_MEDIVACIGNITEAFTERBURNERS, m_bot);
			}
			else
			{
				m_status = HarassStatus::OnMyWay;
			}
		}
		break;
	case HarassStatus::OnMyWay:
		if (manhattenMove(target))
		{
			m_status = HarassStatus::Harass;
		}
		for (const auto & enemy : getNearbyEnemyUnits())
		{
			if (shouldWeFlee(getNearbyEnemyUnits(), static_cast<int>(m_marines.size()))
				|| ((enemy->getUnitType()==sc2::UNIT_TYPEID::PROTOSS_PHOTONCANNON || enemy->getUnitType() == sc2::UNIT_TYPEID::ZERG_SPORECRAWLER || enemy->getUnitType() == sc2::UNIT_TYPEID::TERRAN_MISSILETURRET)
					&& enemy->isCompleted() && Util::Dist(enemy->getPos(),m_medivac->getPos())<enemy->getAttackRange()))
			{
				while (!m_wayPoints.empty())
				{
					m_wayPoints.pop();
				}
				m_status = HarassStatus::Fleeing;
				break;
			}
		}
		break;
	case HarassStatus::Harass:
	{
		CUnits targetUnits = getNearbyEnemyUnits();
		if (m_medivac->getHealth() < 0.5*m_medivac->getHealthMax() || shouldWeFlee(targetUnits, static_cast<int>(m_marines.size())))
		{
			m_status = HarassStatus::Fleeing;
			return true;
		}
		if (m_medivac->getCargoSpaceTaken() > 0)
		{
			Micro::SmartAbility(m_medivac, sc2::ABILITY_ID::UNLOADALLAT, m_medivac, m_bot);
		}
		const CUnit_ptr targetUnit = getTargetMarines(targetUnits);
		if (targetUnit)
		{
			//Really ugly here
			sc2::Tag oldTargetTag = 0;
			if (m_marines.front() && m_marines.front()->getOrders().size() > 0)
			{
				oldTargetTag = m_marines.front()->getOrders().back().target_unit_tag;
			}
			if (oldTargetTag && m_bot.GetUnit(oldTargetTag) && m_bot.GetUnit(oldTargetTag)->isWorker())
			{
				//If we already attack a probe don't change the target
			}
			else
			{
				Micro::SmartAttackUnit(m_marines, targetUnit, m_bot);
				Micro::SmartStim(m_marines, m_bot);
			}
		}
		else
		{
			m_status = HarassStatus::Refueling;
		}
		int injuredCounter = 0;
		for (const auto & m : m_marines)
		{
			//This is still buggy since we can not drop individual marines
			if (m->getHealth() <= 5.0f)
			{
				for (const auto & enemy : targetUnits)
				{
					if (m->canHitMe(enemy))
					{
						Micro::SmartRightClick(m, m_medivac, m_bot);
						Micro::SmartRightClick(m_medivac, m, m_bot);
						++injuredCounter;
						break;
					}
				}
			}
		}
		if (injuredCounter == m_marines.size())
		{
			m_status = HarassStatus::Fleeing;
		}
		break;
	}
	case HarassStatus::Fleeing:
	{
		//Pick everybody up
		if (m_medivac->getCargoSpaceTaken() != m_marines.size())
		{
			Micro::SmartRightClick(m_marines, m_medivac, m_bot);
			Micro::SmartRightClick(m_medivac, m_marines, m_bot);
			Micro::SmartCDAbility(m_medivac, sc2::ABILITY_ID::EFFECT_MEDIVACIGNITEAFTERBURNERS, m_bot);
			return true;
		}
		//nearby enemies
		CUnits targetUnits = getNearbyEnemyUnits();
		// The ones that can hit our medivac
		CUnits targetUnitsCanHitMedivac;
		for (const auto & unit : targetUnits)
		{
			if (m_medivac->canHitMe(unit))
			{
				targetUnitsCanHitMedivac.push_back(unit);
			}
		}
		if (targetUnitsCanHitMedivac.size() > 0 || m_wayPoints.empty())
		{
			if (m_pathPlanCounter > updateRatePathplaning || m_wayPoints.empty())
			{
				while (!m_wayPoints.empty())
				{
					m_wayPoints.pop();
				}
				escapePathPlaning();
				m_pathPlanCounter = 0;
				return true;
			}
		}
		m_pathPlanCounter++;
		if (Util::Dist(m_medivac->getPos(), m_wayPoints.front()) < 0.95f)
		{
			m_wayPoints.pop();
		}
		else
		{
			//Micro::SmartMove(m_medivac, m_wayPoints.front(), m_bot);
			Micro::SmartCDAbility(m_medivac, sc2::ABILITY_ID::EFFECT_MEDIVACIGNITEAFTERBURNERS, m_bot);
			Micro::SmartMove(m_medivac, m_wayPoints.front(), m_bot);
		}
		//I probably could also just see if m_wayPoints is empty?!
		const BaseLocation * savePos = Hitsquad::getSavePosition();
		if (Util::Dist(m_medivac->getPos(), savePos->getBasePosition()) < 0.95f)
		{
			m_status = HarassStatus::Refueling;
		}
		break;
	}
	case HarassStatus::Refueling:
		while (!m_wayPoints.empty())
		{
			m_wayPoints.pop();
		}
		if (m_medivac->getCargoSpaceTaken() > 0)
		{
			Micro::SmartAbility(m_medivac, sc2::ABILITY_ID::UNLOADALLAT, m_medivac, m_bot);
		}
		else
		{
			for (const auto & marine : m_marines)
			{
				if (marine->getHealth() != marine->getHealthMax())
				{
					if (m_medivac->getOrders().empty() && m_medivac->getEnergy() > 5.0f)
					{
						Micro::SmartAbility(m_medivac, sc2::ABILITY_ID::EFFECT_HEAL, marine,m_bot);
					}
					return true;
				}
			}
			if (m_medivac->getHealth() < 0.5*m_medivac->getHealthMax() || m_marines.size() < 6 || haveNoTarget())
			{
				m_status = HarassStatus::GoingHome;
			}
			else
			{
				m_status = HarassStatus::Loading;
			}
		}
		break;
	case HarassStatus::GoingHome:
	{
		if (m_medivac->getCargoSpaceTaken() != m_marines.size())
		{
			Micro::SmartRightClick(m_marines, m_medivac, m_bot);
			Micro::SmartRightClick(m_medivac, m_marines, m_bot);
			Micro::SmartCDAbility(m_medivac, sc2::ABILITY_ID::EFFECT_MEDIVACIGNITEAFTERBURNERS, m_bot);
			return true;
		}
		const CUnit_ptr worker = m_bot.Workers().getClosestMineralWorkerTo(m_medivac->getPos());
		if (worker)
		{
			if (m_bot.Bases().getBaseLocation(worker->getPos()))
			{
				if (manhattenMove(m_bot.Bases().getBaseLocation(worker->getPos())))
				{
					m_status = HarassStatus::Idle;
				}
			}
		}
		//If there are no mineral workers left, we are probably dead anyway
		break;
	}
	default:
		break;
	}
	return true;
}

const bool Hitsquad::haveNoTarget() const
{
	for (const auto & base : m_bot.Bases().getOccupiedBaseLocations(Players::Enemy))
	{
		if (base->getBaseID() == m_target->getBaseID())
		{
			return false;
		}
	}
	return true;
}

const BaseLocation * Hitsquad::getSavePosition() const
{
	//
	sc2::Point2D pos = m_medivac->getPos();
	std::vector<const BaseLocation *> bases = m_bot.Bases().getBaseLocations();
	//We use that it is ordered
	std::map<float, const BaseLocation *> allTargetBases;
	CUnits enemyUnits = m_bot.UnitInfo().getUnits(Players::Enemy);
	for (const auto & base : bases)
	{
		if (!(base->isOccupiedByPlayer(Players::Enemy)))
		{
			bool actuallySafe = true;
			for (const auto & enemy : enemyUnits)
			{
				if (Util::Dist(enemy->getPos(), base->getBasePosition()) < 15.0f)
				{
					actuallySafe = false;
					break;
				}
			}
			if (actuallySafe)
			{
				allTargetBases[Util::Dist(base->getBasePosition(), pos)] = base;
			}
		}
	}
	return allTargetBases.empty() ? m_bot.Bases().getPlayerStartingBaseLocation(Players::Self) : allTargetBases.begin()->second;
}

const bool Hitsquad::shouldWeFlee(CUnits targets,int threshold) const
{
	int opponents = 0;
	for (const auto & t : targets)
	{
		if (t->isCombatUnit() && t->isVisible() && t->isCompleted())
		{
			opponents++;
		}
	}
	if (opponents >= threshold)
	{
		return true;
	}
	return false;
}

CUnits Hitsquad::getNearbyEnemyUnits() const
{
	CUnits nearbyEnemyUnits;
	const sc2::Point2D pos = m_medivac->getPos();
	const float range = m_medivac->getSightRange();
	const BaseLocation * targetBase = m_bot.Bases().getBaseLocation(pos);
	for (const auto & t : m_bot.UnitInfo().getUnits(Players::Enemy))
	{
		const float dist = Util::Dist(pos, t->getPos());
		if (dist <= range)
		{
			nearbyEnemyUnits.push_back(t);
		}
		else if (targetBase && t->isBuilding() && dist < 30.0f)
		{
			BaseLocation * base = m_bot.Bases().getBaseLocation(t->getPos());
			if (base && base->getBaseID() == targetBase->getBaseID())
			{
				nearbyEnemyUnits.push_back(t);
			}
		}

	}
	return nearbyEnemyUnits;
}

CUnit_ptr Hitsquad::getTargetMarines(CUnits targets) const
{
	CUnit_ptr target = nullptr;
	int maxPriority = 0;
	float minHealth = std::numeric_limits<float>::max();
	for (const auto & t : targets)
	{
		int prio = 0;
		if (t->getUnitType().ToType() == sc2::UNIT_TYPEID::ZERG_SPORECRAWLER || t->getUnitType().ToType() == sc2::UNIT_TYPEID::TERRAN_MISSILETURRET)
		{
			prio = 10;
		}
		else if (t->getUnitType().ToType() == sc2::UNIT_TYPEID::ZERG_QUEEN || t->getUnitType().ToType() == sc2::UNIT_TYPEID::ZERG_SPINECRAWLER || t->getUnitType().ToType() == sc2::UNIT_TYPEID::PROTOSS_PHOTONCANNON)
		{
			prio = 9;
		}
		else if (Util::IsWorkerType(t->getUnitType().ToType()))
		{
			prio = 8;
		}
		else if (Util::IsCombatUnitType(t->getUnitType().ToType(),m_bot))
		{
			prio = 5;
		}
		else if (Util::IsTownHallType(t->getUnitType().ToType()))
		{
			prio = 2;
		}
		else if (t->getUnitType().ToType() == sc2::UNIT_TYPEID::ZERG_EGG || t->getUnitType().ToType() == sc2::UNIT_TYPEID::ZERG_LARVA)
		{
			prio = 0;
		}
		else
		{
			prio = 1;
		}
		if (!target || maxPriority < prio || (prio == maxPriority && minHealth>t->getHealth()))
		{
			target = t;
			maxPriority = prio;
			minHealth = t->getHealth();
		}
	}
	return target;
}

//target = proposed target.
const bool Hitsquad::manhattenMove(const BaseLocation * newTarget)
{
	if (!newTarget || m_bot.Workers().isBeingRepairedNum(m_medivac)>0)
	{
		return false;
	}
	//If we get a new target

	

	sc2::Point2D posEnd = newTarget->getPosition() + 1.2f*(newTarget->getPosition() - newTarget->getBasePosition());
	posEnd = m_bot.Map().findLandingZone(posEnd);
	float dist;
	if (m_wayPoints.empty())
	{
		dist = Util::Dist(posEnd, m_medivac->getPos());
	}
	else
	{
		dist = Util::Dist(m_wayPoints.back(), m_medivac->getPos());
	}
	if (dist>0.75f)
	{
		//Save boost for escape
		if (!(m_status == HarassStatus::OnMyWay && dist < 50.0f))
		{
			Micro::SmartCDAbility(m_medivac, sc2::ABILITY_ID::EFFECT_MEDIVACIGNITEAFTERBURNERS, m_bot);
		}
		if (m_wayPoints.empty())
		{
			m_wayPoints=m_bot.Map().getEdgePath(m_medivac->getPos(), posEnd);

		}
		//If we found a new base and it is closer to us then our current target
		else if (Util::Dist(m_wayPoints.back(),posEnd)>10.0f && Util::Dist(m_medivac->getPos(), posEnd)<Util::Dist(m_medivac->getPos(), m_wayPoints.back()))
		{
			while (!m_wayPoints.empty())
			{
				m_wayPoints.pop();
			}
		}
		//If this is too close then the medivac sometime stops
		else if (Util::Dist(m_medivac->getPos(), m_wayPoints.front()) < 0.1f)
		{
			m_wayPoints.pop();
		}
		else
		{
			Micro::SmartMove(m_medivac, m_wayPoints.front(),m_bot);
		}
		return false;
	}
	else
	{
		return true;
	}
}

//////////////////////////////////////////////////////////////////// WIDOW MINE HARASS /////////////////////////////////////////////
WMHarass::WMHarass(CCBot & bot) :m_bot(bot),m_widowmine(nullptr), m_lastLoopEnemySeen(0),m_status(WMStatus::Dead),m_shuttle(nullptr)
{

}

void WMHarass::getWayPoints(const sc2::Point2D targetPos)
{
	m_wayPoints.push(targetPos);
}

void WMHarass::replanWayPoints(const sc2::Point2D targetPos)
{
	while (m_wayPoints.size() > 0)
	{
		m_wayPoints.pop();
	}
	m_wayPoints.push(targetPos);
}

void WMHarass::harass(const sc2::Point2D pos)
{
	if (m_status != WMStatus::Dead && !m_widowmine->isAlive())
	{
		m_status = WMStatus::Dead;
		m_widowmine = nullptr;
		return;
	}
	//The pointer is outdated whenever it changes the burrow type
	if (m_widowmine->changedUnitType())
	{
		m_widowmine = m_bot.UnitInfo().getUnit(m_widowmine->getUnit_ptr());
	}
	switch (m_status)
	{
		case(WMStatus::Dead):
		{
			if (m_widowmine->isAlive())
			{
				m_status = WMStatus::NewWM;
			}
			return;
		}
		case(WMStatus::NewWM):
		{
			m_shuttle=m_bot.requestShuttleService({ m_widowmine }, pos);
			while (m_wayPoints.size() > 0)
			{
				m_wayPoints.pop();
			}
			m_wayPoints.push(pos);
			m_status = WMStatus::WaitingForShuttle;
			break;
		}
		case(WMStatus::WaitingForShuttle):
		{
			if (m_shuttle->getShuttleStatus() == ShuttleStatus::OnMyWay)
			{
				m_status = WMStatus::ShuttleTransport;
			}
			break;
		}
		case(WMStatus::ShuttleTransport):
		{
			if (m_shuttle->getShuttleStatus() == ShuttleStatus::Done || m_shuttle->getShuttleStatus() == ShuttleStatus::OnMyWayBack)
			{
				m_wayPoints.pop();
				m_shuttle = nullptr;
				m_status = WMStatus::Harass;
			}
			else if (m_wayPoints.back() != pos)
			{
				m_wayPoints.pop();
				m_wayPoints.push(pos);
				m_shuttle->updateTargetPos(pos);
			}
			break;
		}
		case(WMStatus::Harass):
		{
			if (Util::Dist(m_widowmine->getPos(), pos) < 1.0f)
			{
				if (m_widowmine->getUnitType().ToType() != sc2::UNIT_TYPEID::TERRAN_WIDOWMINEBURROWED)
				{
					Micro::SmartAbility(m_widowmine, sc2::ABILITY_ID::BURROWDOWN, m_bot);
				}
			}
			else
			{
				m_status = WMStatus::Relocating;
			}
			break;
		}
		case(WMStatus::Relocating):
		{
			//Still on our way
			CUnits enemies = m_widowmine->getEnemyUnitsInSight();
			//If enemies nearby, better burrow
			for (const auto & enemy : enemies)
			{
				if (enemy->isCombatUnit() || (enemy->isWorker() && Util::Dist(m_widowmine->getPos(), enemy->getPos()) < 2.0f))
				{
					if (m_widowmine->getUnitType().ToType() != sc2::UNIT_TYPEID::TERRAN_WIDOWMINEBURROWED)
					{
						Micro::SmartAbility(m_widowmine, sc2::ABILITY_ID::BURROWDOWN, m_bot);
					}
					m_lastLoopEnemySeen = m_bot.Observation()->GetGameLoop();
					return;
				}
			}
			//Nobody in sight
			if (m_widowmine->getUnitType().ToType() == sc2::UNIT_TYPEID::TERRAN_WIDOWMINEBURROWED)
			{
				const uint32_t currentLoop = m_bot.Observation()->GetGameLoop();
				//wait a bit before unburrowing
				if (currentLoop - m_lastLoopEnemySeen > 448)//20sec
				{
					Micro::SmartAbility(m_widowmine, sc2::ABILITY_ID::BURROWUP, m_bot);
				}
				return;
			}
			//Plan our way
			if (m_wayPoints.empty())
			{
				if (Util::Dist(m_widowmine->getPos(), pos) < 1.0f)
				{
					m_status = WMStatus::Harass;
					return;
				}
				else
				{
					getWayPoints(pos);
				}
			}
			//Replan if destination changed
			if (m_wayPoints.back() != pos)
			{
				replanWayPoints(pos);
			}
			//Walk there
			if (Util::Dist(m_widowmine->getPos(), m_wayPoints.front()) > 1.0f)
			{
				Micro::SmartMove(m_widowmine, m_wayPoints.front(), m_bot);
			}
			else
			{
				m_wayPoints.pop();
			}
		}
	}
	//No widow mine yet
	if (m_status==WMStatus::Dead)
	{
		return;
	}
	
}

const bool WMHarass::addWidowMine(CUnit_ptr widowMine)
{
	if (m_widowmine)
	{
		return false;
	}
	m_widowmine = widowMine;
	m_status = WMStatus::NewWM;
	return true;
}

CUnit_ptr WMHarass::getwidowMine() const
{
	return m_widowmine;
}

///////////////////////////////////////////////////////////////////// HARASS MANAGER ////////////////////////////////////////////////////
HarassManager::HarassManager(CCBot & bot)
	: m_bot(bot), m_WMHarass(WMHarass(bot)), m_liberator(nullptr)
{

}

void HarassManager::onStart()
{

}
void HarassManager::onFrame()
{
	handleHitSquads();
	handleLiberator();
	handleWMHarass();
}

void HarassManager::handleHitSquads()
{
	std::vector<const BaseLocation *> targetBases = getPotentialTargets();
	if (targetBases.empty())
	{
		return;
	}
	//On which side is the target? One 
	if (targetBases[0])
	{
		if (m_hitSquads.find(0) == m_hitSquads.end())
		{
			m_hitSquads.emplace(0, Hitsquad(m_bot, nullptr));
		}
		m_hitSquads.at(0).harass(targetBases[0]);
	}
	else
	{
		if (m_hitSquads.find(0) != m_hitSquads.end())
		{
			if (!m_hitSquads.at(0).harass())
			{
				m_hitSquads.erase(0);
			}
		}
	}
	if (targetBases[1])
	{
		if (m_hitSquads.find(1) == m_hitSquads.end())
		{
			m_hitSquads.emplace(1, Hitsquad(m_bot, nullptr));
		}
		m_hitSquads.at(1).harass(targetBases[1]);
	}
	else
	{
		if (m_hitSquads.find(1) != m_hitSquads.end())
		{

			if (!m_hitSquads.at(1).harass())
			{
				m_hitSquads.erase(1);
			}
		}
	}
}


std::vector<const BaseLocation *> HarassManager::getPotentialTargets() const
{
	std::vector<const BaseLocation *> targetBases;
	std::set<const BaseLocation *> bases = m_bot.Bases().getOccupiedBaseLocations(Players::Enemy);
	if (bases.empty())
	{
		return targetBases;
	}
	const BaseLocation * enemyHomeBase = m_bot.Bases().getPlayerStartingBaseLocation(Players::Enemy);
	sc2::Point2D enemyHomePos;
	if (enemyHomeBase)
	{
		enemyHomePos = enemyHomeBase->getBasePosition();
	}
	else
	{
		enemyHomePos = (*(bases.begin()))->getBasePosition();
	}
	
	const sc2::Point2D homePos = m_bot.Bases().getPlayerStartingBaseLocation(Players::Self)->getPosition();//We always know our location
	const sc2::Point2D diff = enemyHomePos - homePos;
	targetBases = { nullptr,nullptr };
	std::vector<int> maxDist = { 0,0 };
	for (const auto & base : bases)
	{
		const sc2::Point2D targetPos = base->getBasePosition();
		const float side = (targetPos.x - homePos.x)*diff.y - (targetPos.y - homePos.y)*diff.x;
		const int dist = base->getGroundDistance(enemyHomePos);
		if (!(targetBases[side>=0]) || maxDist[side >= 0] < dist)
		{
			targetBases[side >= 0] = base;
			maxDist[side >= 0] = dist;
		}
	}
	return targetBases;
}


const bool HarassManager::needMedivac() const
{
	//if one is missing a medivac
	for (const auto & hs : m_hitSquads)
	{
		if (!hs.second.getMedivac())
		{
			return true;
		}
	}
	return false;
}
const bool HarassManager::needMarine() const
{
	//if one is missing a marine
	for (const auto & hs : m_hitSquads)
	{
		//Only if it already has a Medivac
		if (hs.second.getMedivac() && hs.second.getStatus() == HarassStatus::Idle && hs.second.getNumMarines()<8)
		{
			return true;
		}
	}
	return false;
}

const bool HarassManager::needLiberator() const
{
	return false;
}

const bool HarassManager::needWidowMine() const
{
	return !m_WMHarass.getwidowMine();
}

const bool HarassManager::setMedivac(CUnit_ptr medivac)
{
	for (auto & hs : m_hitSquads)
	{
		if (!hs.second.getMedivac())
		{
			return hs.second.addMedivac(medivac);
		}
	}
	return false;
}

const bool HarassManager::setMarine(CUnit_ptr marine)
{
	for (auto & hs : m_hitSquads)
	{
		if (hs.second.addMarine(marine))
		{
			return true;
		}
	}
	return false;
}

const bool HarassManager::setLiberator(CUnit_ptr liberator)
{
	return false;
}

const bool HarassManager::setWidowMine(CUnit_ptr widowMine)
{
	return m_WMHarass.addWidowMine(widowMine);
}

void HarassManager::handleLiberator()
{

}

void HarassManager::handleWMHarass()
{
	if (m_WMHarass.getwidowMine())
	{
		const sc2::Point2D pos = m_bot.Bases().getNextExpansion(Players::Enemy);
		auto base = m_bot.Bases().getBaseLocation(pos);
		m_WMHarass.harass(base->getBasePosition());
	}
}


CUnits HarassManager::getMedivacs()
{
	CUnits medivacs;
	for (const auto & hs : m_hitSquads)
	{
		if (hs.second.getMedivac())
		{
			medivacs.push_back(hs.second.getMedivac());
		}
	}
	return medivacs;
}

CUnits HarassManager::getMarines()
{
	CUnits marines;
	for (const auto & hs : m_hitSquads)
	{
		for (const auto & marine:hs.second.getMarines() )
		{
			marines.push_back(marine);
		}
	}
	return marines;
}

CUnit_ptr HarassManager::getLiberator()
{
	return m_liberator;
}

CUnit_ptr HarassManager::getWidowMine()
{
	return m_WMHarass.getwidowMine();
}
