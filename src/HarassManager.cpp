#pragma once
#include "HarassManager.h"
#include "Util.h"
#include "CCBot.h"
#include "pathPlaning.h"
#include "Drawing.h"

const int updateRatePathplaning = 10;


Hitsquad::Hitsquad(CCBot & bot, const sc2::Unit * medivac) : m_bot(bot), m_status(HarassStatus::Idle), m_medivac(medivac), m_pathPlanCounter(updateRatePathplaning+1)
{
}

void Hitsquad::escapePathPlaning()
{
	const BaseLocation * saveBase = getSavePosition();
	sc2::Point2D targetPos = saveBase->getBasePosition();
	
	pathPlaning escapePlan(m_bot, m_medivac->pos, targetPos, m_bot.Map().width(), m_bot.Map().height(), 1.0f);
	
	std::vector<sc2::Point2D> escapePath=escapePlan.planPath();
	for (sc2::Point2D pos : escapePath)
	{
		if (m_wayPoints.size() > 0)
		{
			Drawing::drawLine(m_bot,m_wayPoints.back(), pos);
		}
		m_wayPoints.push(pos);
	}
}


const bool Hitsquad::addMedivac(const sc2::Unit * medivac)
{
	if (!m_medivac)
	{
		m_medivac=medivac;
		m_status = HarassStatus::Idle;
		return true;
	}
	return false;
}

const bool Hitsquad::addMarine(const sc2::Unit * marine)
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

sc2::Units Hitsquad::getMarines() const
{
	return m_marines;
}

const sc2::Unit * Hitsquad::getMedivac() const
{
	return m_medivac;
}

const int Hitsquad::getStatus() const
{
	return m_status;
}

bool isDead(const sc2::Unit * unit)
{
	//!unit->buffs.empty() && unit->buffs.front().ToType()==sc2::BUFF_ID::
	return !(unit->is_alive);
}

void Hitsquad::checkForCasualties()
{
	//Last game loop check prevents units in bunker... but this is also true for in medivac
	//m_marines.erase(std::remove_if(m_marines.begin(), m_marines.end(), [this](const sc2::Unit * unit) {return !(unit->is_alive && unit->last_seen_game_loop == m_bot.Observation()->GetGameLoop()); }), m_marines.end());
	m_marines.erase(std::remove_if(m_marines.begin(), m_marines.end(), &isDead), m_marines.end());
	if (m_medivac && !m_medivac->is_alive)
	{
		m_medivac = nullptr;
		m_status = HarassStatus::Doomed;
		std::copy(m_marines.begin(), m_marines.end(), std::back_inserter(m_doomedMarines));
		m_marines.erase(m_marines.begin(), m_marines.end());
	}
}

void Hitsquad::harass(const BaseLocation * target)
{
	checkForCasualties();
	switch (m_status)
	{
	case HarassStatus::Idle:
		//We get here if we have a brand new hit squad or want to restart from home
		if (m_medivac)
		{
			if (m_medivac->health == m_medivac->health_max)
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
			else if (m_medivac->health != m_medivac->health_max)
			{
				m_bot.Workers().setRepairWorker(m_medivac);
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
			if (m_medivac->cargo_space_taken < m_marines.size())
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
		if (m_medivac->health < 0.5*m_medivac->health_max || shouldWeFlee(getNearbyEnemyUnits()))
		{
			while (!m_wayPoints.empty())
			{
				m_wayPoints.pop();
			}
			m_status = HarassStatus::Fleeing;
		}
		break;
	case HarassStatus::Harass:
	{
		sc2::Units targetUnits = getNearbyEnemyUnits();
		if (m_medivac->health < 0.5*m_medivac->health_max || shouldWeFlee(targetUnits))
		{
			m_status = HarassStatus::Fleeing;
			return;
		}
		if (m_medivac->cargo_space_taken > 0)
		{
			Micro::SmartAbility(m_medivac, sc2::ABILITY_ID::UNLOADALLAT, m_medivac, m_bot);
		}
		const sc2::Unit * targetUnit = getTargetMarines(targetUnits);
		if (targetUnit)
		{
			//Really ugly here
			sc2::Tag oldTargetTag = 0;
			if (m_marines.front() && m_marines.front()->orders.size() > 0)
			{
				oldTargetTag = m_marines.front()->orders.back().target_unit_tag;
			}
			if (oldTargetTag && m_bot.GetUnit(oldTargetTag) && Util::IsWorkerType(m_bot.GetUnit(oldTargetTag)->unit_type))
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
		for (const auto & m : m_marines)
		{
			//This is still buggy since we can not drop individual marines
			if (m->health <= 5)
			{
				Micro::SmartRightClick(m, m_medivac, m_bot);
				Micro::SmartRightClick(m_medivac, m, m_bot);
			}
		}
		break;
	}
	case HarassStatus::Fleeing:
	{
		//Pick everybody up
		if (m_medivac->cargo_space_taken != m_marines.size())
		{
			Micro::SmartRightClick(m_marines, m_medivac, m_bot);
			Micro::SmartRightClick(m_medivac, m_marines, m_bot);
			Micro::SmartCDAbility(m_medivac, sc2::ABILITY_ID::EFFECT_MEDIVACIGNITEAFTERBURNERS, m_bot);
			return;
		}
		//nearby enemies
		sc2::Units targetUnits = getNearbyEnemyUnits();
		// The ones that can hit our medivac
		sc2::Units targetUnitsCanHitMedivac;
		for (const auto & unit : targetUnits)
		{
			if (Util::canHitMe(m_medivac, unit, m_bot))
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
			}
		}
		m_pathPlanCounter++;
		if (Util::Dist(m_medivac->pos, m_wayPoints.front()) < 0.95f)
		{
			m_wayPoints.pop();
		}
		else
		{
			//Micro::SmartMove(m_medivac, m_wayPoints.front(), m_bot);
			Micro::SmartCDAbility(m_medivac, sc2::ABILITY_ID::EFFECT_MEDIVACIGNITEAFTERBURNERS, m_bot);
			m_bot.Actions()->UnitCommand(m_medivac, sc2::ABILITY_ID::MOVE, m_wayPoints.front());
		}
		//I probably could also just see if m_wayPoints is empty?!
		const BaseLocation * savePos = Hitsquad::getSavePosition();
		if (Util::Dist(m_medivac->pos, savePos->getBasePosition()) < 0.95f)
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
		if (m_medivac->cargo_space_taken > 0)
		{
			Micro::SmartAbility(m_medivac, sc2::ABILITY_ID::UNLOADALLAT, m_medivac, m_bot);
		}
		else
		{
			for (const auto & marine : m_marines)
			{
				if (marine->health != marine->health_max)
				{
					if (m_medivac->orders.empty())
					{
						m_bot.Actions()->UnitCommand(m_medivac, sc2::ABILITY_ID::EFFECT_HEAL, marine);
					}
					return;
				}
			}
			if (m_medivac->health < 0.5*m_medivac->health_max || m_marines.size() < 6)
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
		if (m_medivac->cargo_space_taken != m_marines.size())
		{
			Micro::SmartRightClick(m_marines, m_medivac, m_bot);
			Micro::SmartRightClick(m_medivac, m_marines, m_bot);
			Micro::SmartCDAbility(m_medivac, sc2::ABILITY_ID::EFFECT_MEDIVACIGNITEAFTERBURNERS, m_bot);
			return;
		}
		const sc2::Unit * worker = m_bot.Workers().getClosestMineralWorkerTo(m_medivac->pos);
		if (worker)
		{
			if (m_bot.Bases().getBaseLocation(worker->pos))
			{
				if (manhattenMove(m_bot.Bases().getBaseLocation(worker->pos)))
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
}

const BaseLocation * Hitsquad::getSavePosition() const
{
	//
	sc2::Point2D pos = m_medivac->pos;
	std::vector<const BaseLocation *> bases = m_bot.Bases().getBaseLocations();
	//We use that it is ordered
	std::map<float, const BaseLocation *> allTargetBases;
	sc2::Units enemyUnits = m_bot.UnitInfo().getUnits(Players::Enemy);
	for (const auto & base : bases)
	{
		if (!(base->isOccupiedByPlayer(Players::Enemy)))
		{
			bool actuallySafe = true;
			for (const auto & enemy:enemyUnits)
			{
				if (Util::Dist(enemy->pos, base->getBasePosition()) < 15.0f)
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
	return allTargetBases.begin()->second;
	/*
	//If we are damaged or lost too many marines
	else
	{
		const sc2::Unit * worker = m_bot.Workers().getClosestMineralWorkerTo(m_medivac->pos);
		if (worker)
		{
			if (m_bot.Bases().getBaseLocation(worker->pos))
			{
				return m_bot.Bases().getBaseLocation(worker->pos);
			}
		}
		//If there are no mineral workers left, we are probably dead anyway
		return m_bot.Bases().getPlayerStartingBaseLocation(Players::Self);
	}*/
}

const bool Hitsquad::shouldWeFlee(sc2::Units targets) const
{
	int opponents = 0;
	for (const auto & t : targets)
	{
		if (Util::IsCombatUnitType(t->unit_type.ToType(), m_bot))
		{
			opponents++;
		}
	}
	if (opponents >= m_marines.size())
	{
		return true;
	}
	return false;
}

sc2::Units Hitsquad::getNearbyEnemyUnits() const
{
	//sc2::Units enemyUnits = m_bot.Observation()->GetUnits(sc2::Unit::Alliance::Enemy);
	sc2::Units enemyUnits = m_bot.UnitInfo().getUnits(Players::Enemy);
	sc2::Units nearbyEnemyUnits;
	sc2::Point2D pos = m_medivac->pos;
	float range = Util::GetUnitTypeSight(m_medivac->unit_type.ToType(), m_bot);
	for (const auto & t : enemyUnits)
	{
		if (Util::Dist(pos, t->pos) <= range)
		{
			nearbyEnemyUnits.push_back(t);
		}
	}
	return nearbyEnemyUnits;
}

const sc2::Unit * Hitsquad::getTargetMarines(sc2::Units targets) const
{
	const sc2::Unit * target = nullptr;
	int maxPriority = 0;
	float minHealth = std::numeric_limits<float>::max();
	for (const auto & t : targets)
	{
		int prio = 0;
		if (t->unit_type.ToType() == sc2::UNIT_TYPEID::ZERG_SPORECRAWLER || t->unit_type.ToType() == sc2::UNIT_TYPEID::TERRAN_MISSILETURRET)
		{
			prio = 10;
		}
		else if (t->unit_type.ToType() == sc2::UNIT_TYPEID::ZERG_QUEEN || t->unit_type.ToType() == sc2::UNIT_TYPEID::ZERG_SPINECRAWLER || t->unit_type.ToType() == sc2::UNIT_TYPEID::PROTOSS_PHOTONCANNON)
		{
			prio = 9;
		}
		else if (Util::IsWorkerType(t->unit_type.ToType()))
		{
			prio = 8;
		}
		else if (Util::IsCombatUnitType(t->unit_type.ToType(),m_bot))
		{
			prio = 5;
		}
		else if (Util::IsTownHallType(t->unit_type.ToType()))
		{
			prio = 2;
		}
		else if (t->unit_type.ToType() == sc2::UNIT_TYPEID::ZERG_EGG || t->unit_type.ToType() == sc2::UNIT_TYPEID::ZERG_LARVA)
		{
			prio = 0;
		}
		else
		{
			prio = 1;
		}
		if (!target || maxPriority < prio || (prio == maxPriority && minHealth>t->health+t->shield))
		{
			target = t;
			maxPriority = prio;
			minHealth = t->health + t->shield;
		}
	}
	return target;
}

const bool Hitsquad::manhattenMove(const BaseLocation * target)
{
	if (!target || m_bot.Workers().isBeingRepairedNum(m_medivac)>0)
	{
		return false;
	}
	sc2::Point2D posEnd = target->getPosition() + 1.2f*(target->getPosition() - target->getBasePosition());
	float dist;
	if (m_wayPoints.empty())
	{
		dist = Util::Dist(posEnd, m_medivac->pos);
	}
	else
	{
		dist = Util::Dist(m_wayPoints.back(), m_medivac->pos);
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
			//WAYPOINTS QUEUE
			sc2::Point2D posStart = m_medivac->pos;
			int margin = 5;
			sc2::Point2D posA = m_bot.Map().getClosestBorderPoint(posStart,margin);
			sc2::Point2D posB = m_bot.Map().getClosestBorderPoint(posEnd,margin);


			float x_min = m_bot.Observation()->GetGameInfo().playable_min.x + margin;
			float x_max = m_bot.Observation()->GetGameInfo().playable_max.x - margin;
			float y_min = m_bot.Observation()->GetGameInfo().playable_min.y + margin;
			float y_max = m_bot.Observation()->GetGameInfo().playable_max.y - margin;

			//we are at the same side
			if (posA.x == posB.x || posA.y == posB.y)
			{
				m_wayPoints.push(posA);
			}
			//other side
			else if (posA.x == x_min && posB.x == x_max || posA.x == x_max && posB.x == x_min || posA.y == y_min && posB.y == y_max || posA.y == y_max && posB.y == y_min)
			{
				if (posA.x == x_min)
				{
					m_wayPoints.push(sc2::Point2D(x_max, posStart.y));
				}
				else if (posA.x == x_max)
				{
					m_wayPoints.push(sc2::Point2D(x_min, posStart.y));
				}
				else if (posA.y == y_min)
				{
					m_wayPoints.push(sc2::Point2D(posStart.x, y_max));
				}
				else if (posA.y == y_max)
				{
					m_wayPoints.push(sc2::Point2D(posStart.x, y_min));
				}
			}
			else
			{
				m_wayPoints.push(posA);
				//Over an Edge
				if (posA.x == x_min || posA.x == x_max)
				{
					m_wayPoints.push(sc2::Point2D(posA.x, posB.y));
				}
				else
				{
					m_wayPoints.push(sc2::Point2D(posB.x, posA.y));
				}

			}
			m_wayPoints.push(posB);
			m_wayPoints.push(posEnd);
		}
		//If we found a new base and it is closer to us then our current target
		else if (Util::Dist(m_wayPoints.back(),posEnd)>10.0f && Util::Dist(m_medivac->pos, posEnd)<Util::Dist(m_medivac->pos, m_wayPoints.back()))
		{
			while (!m_wayPoints.empty())
			{
				m_wayPoints.pop();
			}
		}
		//If this is too close then the medivac sometime stops
		else if (Util::Dist(m_medivac->pos, m_wayPoints.front()) < 0.1f)
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

HarassManager::HarassManager(CCBot & bot)
	: m_bot(bot), m_liberator(nullptr)
{

}

void HarassManager::onStart()
{

}
void HarassManager::onFrame()
{
	handleHitSquads();
	handleLiberator();
}

void HarassManager::handleHitSquads()
{
	std::vector<const BaseLocation *> targetBases=getPotentialTargets(m_hitSquads.size());
	if (targetBases.size() < 1)
	{
		return;
	}
	for (int i = 0; i < m_hitSquads.size() && i < targetBases.size(); ++i)
	{
		m_hitSquads[i].harass(targetBases[i]);
	}
}


std::vector<const BaseLocation *> HarassManager::getPotentialTargets(size_t n) const
{
	std::vector<const BaseLocation *> targetBases;
	std::set<const BaseLocation *> bases = m_bot.Bases().getOccupiedBaseLocations(Players::Enemy);
	if (bases.empty())
	{
		return targetBases;
	}
	const BaseLocation * homeBase = m_bot.Bases().getPlayerStartingBaseLocation(Players::Enemy);
	sc2::Point2D homePos;
	if (homeBase)
	{
		homePos = homeBase->getBasePosition();
	}
	else
	{
		homePos = (*(bases.begin()))->getBasePosition();
	}
	std::map<float, const BaseLocation *> targetsWithDistance;
	for (const auto & base : bases)
	{
		float dist = static_cast<float>(base->getGroundDistance(homePos));
		targetsWithDistance[-dist] = base;
	}
	for (const auto & tb : targetsWithDistance)
	{
		targetBases.push_back(tb.second);
	}
	return targetBases;
}


const bool HarassManager::needMedivac() const
{
	//if there are not enough
	if (m_hitSquads.size() < m_numHitSquads)
	{
		return true;
	}
	//if one is missing a medivac
	for (const auto & hs : m_hitSquads)
	{
		if (!hs.getMedivac())
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
		if (hs.getMedivac() && hs.getStatus() == HarassStatus::Idle && hs.getNumMarines()<8)
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

const bool HarassManager::setMedivac(const sc2::Unit * medivac)
{
	for (auto & hs : m_hitSquads)
	{
		if (!hs.getMedivac())
		{
			hs.addMedivac(medivac);
			return true;
		}
	}
	if (m_hitSquads.size() < m_numHitSquads)
	{
		m_hitSquads.push_back(Hitsquad(m_bot, medivac));
		return true;
	}
	return false;
}

const bool HarassManager::setMarine(const sc2::Unit * marine)
{
	for (auto & hs : m_hitSquads)
	{
		if (hs.addMarine(marine))
		{
			return true;
		}
	}
	return false;
}

const bool HarassManager::setLiberator(const sc2::Unit * liberator)
{
	return false;
}

void HarassManager::handleLiberator()
{

}

const sc2::Unit * HarassManager::getMedivac()
{
	if (m_hitSquads.empty())
	{
		return nullptr;
	}
	return m_hitSquads.front().getMedivac();
}

const sc2::Units HarassManager::getMarines()
{
	if (m_hitSquads.empty())
	{
		return sc2::Units();
	}
	return m_hitSquads.front().getMarines();
}
const sc2::Unit * HarassManager::getLiberator()
{
	return m_liberator;
}