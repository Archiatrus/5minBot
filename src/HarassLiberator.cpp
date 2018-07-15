#include "HarassLiberator.h"
#include "Util.h"
#include "CCBot.h"
#include "pathPlaning.h"


const int updateRatePathplaning = 10;
// auto test = m_bot.Observation()->GetAbilityData()[sc2::AbilityID(sc2::ABILITY_ID::MORPH_LIBERATORAGMODE)].cast_range;
const float siegeRange = 5.0f;

HarassLiberator::HarassLiberator(CCBot & bot) :m_bot(bot), m_status(Status::Dead), m_liberator(nullptr), m_pathPlanCounter(0), m_target(nullptr), m_stalemateCheck(sc2::Point2D())
{
}

HarassLiberator::~HarassLiberator()
{
	if (m_liberator)
	{
		m_liberator->setOccupation({ CUnit::Occupation::Combat, 0 });
	}
}

const bool HarassLiberator::needLiberator()
{
	if (m_status == Status::Dead && m_target)
	{
		m_status = Status::init;
		return true;
	}
	return false;
}

const HarassLiberator::Status HarassLiberator::getStatus() const
{
	return m_status;
}

const bool HarassLiberator::setLiberator(const CUnit_ptr liberator)
{
	if (m_liberator && m_liberator->isAlive())
	{
		return false;
	}
	m_liberator = liberator;
	m_liberator->setOccupation({ CUnit::Occupation::Harass, 0 });
	m_status = Status::idle;
	return true;
}

bool HarassLiberator::harass(const BaseLocation * target)
{
	if (m_liberator && m_liberator->changedUnitType())
	{
		m_liberator = m_bot.UnitInfo().getUnit(m_liberator->getUnit_ptr());
	}
	if (m_status != Status::init && (m_liberator && !m_liberator->isAlive() || !m_liberator))
	{
		m_status = Status::Dead;
	}

	// New target
	if (target)
	{
		// target = new target
		m_target = target;
	}
	// No new target
	else
	{
		// No old target
		if (!m_target)
		{
			return false;
		}
	}

	switch (m_status)
	{
	case Status::idle: planPath(); break;
	case Status::onMyWay: travelToDestination(); break;
	case Status::Harass: ActivateSiege(); break;
	case Status::Dead: break;
	}
	return true;
}

void HarassLiberator::planPath()
{
	if (m_bot.Observation()->GetGameLoop() - m_pathPlanCounter < 224)
	{
		return;
	}
	m_pathPlanCounter = m_bot.Observation()->GetGameLoop();
	// Where we want to shot
	const sc2::Point2D targetSpot = m_target->getCenterOfMinerals() + Util::normalizeVector(m_target->getCenterOfMinerals() - m_target->getCenterOfBase(), 3);
	Drawing::drawSphere(m_bot, targetSpot, 3, sc2::Colors::Green);
	// Where we want to siege up
	const sc2::Point2D siegeSpot = targetSpot + Util::normalizeVector(targetSpot - m_target->getCenterOfBase(), siegeRange);
	Drawing::drawSphere(m_bot, siegeSpot, 2, sc2::Colors::Yellow);
	// find save target spot
	sc2::Point2D saveSiegeSpot{};
	CUnits staticDs = m_bot.UnitInfo().getUnits(Players::Enemy, std::vector<sc2::UNIT_TYPEID>({ sc2::UNIT_TYPEID::PROTOSS_PHOTONCANNON, sc2::UNIT_TYPEID::ZERG_SPORECRAWLER, sc2::UNIT_TYPEID::TERRAN_MISSILETURRET }));
	staticDs.erase(std::remove_if(staticDs.begin(), staticDs.end(), [&](const auto & staticD) {
		return Util::Dist(staticD->getPos(), siegeSpot) > 20.0f;
	}), staticDs.end());

	const std::vector<sc2::Point2D> & tiles = m_bot.Map().getClosestTilesToAir(siegeSpot);

	for (const auto & tile : tiles)
	{
		if (Util::DistSq(targetSpot, tile) > std::pow(siegeRange, 2) + 0.001f)  // -.-
		{
			continue;
		}
		float threatLvl = 0.0f;
		for (const auto & staticD : staticDs)
		{
			const float range = staticD->getAttackRange();
			const float dist = Util::Dist(staticD->getPos(), tile);
			threatLvl += std::max(3.0f + range - dist, 0.0f);
		}
		if (threatLvl == 0.0f)
		{
			saveSiegeSpot = tile;
			break;
		}
		if (Util::Dist(siegeSpot, tile) > 25.0f)
		{
			break;
		}
	}
	Drawing::drawSphere(m_bot, saveSiegeSpot, 2, sc2::Colors::Purple);
	if (saveSiegeSpot == sc2::Point2D())
	{
		return;
	}
	m_wayPoints = m_bot.Map().getEdgePath(m_liberator->getPos(), saveSiegeSpot);
	m_status = Status::onMyWay;
}


void HarassLiberator::travelToDestination()
{
	if (m_wayPoints.empty())
	{
		const sc2::Point2D targetSpot = m_target->getCenterOfMinerals() + Util::normalizeVector(m_target->getCenterOfMinerals() - m_target->getCenterOfBase(), 3);
		if (Util::DistSq(m_liberator->getPos(), targetSpot) > std::pow(siegeRange, 2) + 5.0f)  // -.-
		{
			m_status = Status::idle;
		}
		else
		{
			m_status = Status::Harass;
		}
	}
	else if (Util::Dist(m_liberator->getPos(), m_wayPoints.front()) < 0.1f)
	{
		m_wayPoints.pop();
	}
	else
	{
		// Airblocker on waypoint -> medivac gets stuck
		if (m_bot.Observation()->GetGameLoop() % 23 == 0)
		{
			if (m_wayPoints.size() > 0 && Util::Dist(m_liberator->getPos(), m_stalemateCheck) < 0.1f)
			{
				m_wayPoints.pop();
			}
			m_stalemateCheck = m_liberator->getPos();
		}
		else
		{
			Micro::SmartMove(m_liberator, m_wayPoints.front(), m_bot);
		}
	}
}

void HarassLiberator::ActivateSiege()
{
	const CUnits enemies = m_liberator->getEnemyUnitsInSight();
	bool enemyInRange = false;
	for (const auto & e : enemies)
	{
		if ((e->isType(sc2::UNIT_TYPEID::PROTOSS_PHOTONCANNON) || e->isType(sc2::UNIT_TYPEID::TERRAN_MISSILETURRET) || e->isType(sc2::UNIT_TYPEID::ZERG_SPORECRAWLER)) && e->isCompleted() && Util::DistSq(e->getPos(), m_liberator->getPos()) < e->getAttackRange())
		{
			enemyInRange = true;
			break;
		}
	}
	if (m_liberator->isType(sc2::UNIT_TYPEID::TERRAN_LIBERATOR))
	{
		if (enemyInRange)
		{
			m_status = Status::idle;
		}
		else
		{
			const sc2::Point2D targetSpot = m_target->getCenterOfMinerals() + Util::normalizeVector(m_target->getCenterOfMinerals() - m_target->getCenterOfBase(), 3);
			Micro::SmartAbility(m_liberator, sc2::ABILITY_ID::MORPH_LIBERATORAGMODE, targetSpot, m_bot);
		}
	}
	else
	{
		if (enemyInRange)
		{
			Micro::SmartAbility(m_liberator, sc2::ABILITY_ID::MORPH_LIBERATORAAMODE, m_bot);
		}
		else
		{
			if (!(m_liberator->getEngagedTargetTag() && m_liberator->canHitMe(m_bot.UnitInfo().getUnit(m_liberator->getEngagedTargetTag()))))
			{
				const std::vector<sc2::Effect> effects = m_bot.Observation()->GetEffects();
				for (const auto & effect : effects)
				{
					if (effect.effect_id == sc2::EffectID(sc2::EFFECT_ID::LIBERATORMORPHED) && Util::DistSq(m_liberator->getPos(), effect.positions.front()) < 26.0f)
					{
						for (const auto & enemy : enemies)
						{
							const float radius = m_bot.Observation()->GetEffectData()[sc2::EffectID(sc2::EFFECT_ID::LIBERATORMORPHED)].radius;
							if (Util::DistSq(effect.positions.front(), enemy->getPos()) < std::pow(radius, 2) && m_liberator->canHitMe(enemy))
							{
								Micro::SmartAttackUnit(m_liberator, enemy, m_bot);
								return;
							}
						}
					}
				}
			}
		}
	}
}
