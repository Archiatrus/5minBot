#include "sc2api/sc2_api.h"

#include "ShuttleService.h"
#include "Util.h"
#include "CCBot.h"

shuttle::shuttle(CCBot * const bot, sc2::Units passengers, sc2::Point2D targetPos) :
	m_bot(bot),
	m_shuttle(nullptr),
	m_passengers(passengers),
	m_targetPos(targetPos),
	m_status(ShuttleStatus::lookingForShuttle)
{

}

void shuttle::hereItGoes()
{
	switch (m_status)
	{
	case(ShuttleStatus::lookingForShuttle):break;
	case(ShuttleStatus::Loading):loadPassangers(); break;
	case(ShuttleStatus::OnMyWay):travelToDestination(); break;
	case(ShuttleStatus::Unloading):unloadPassangers(); break;
	case(ShuttleStatus::Done):break;
	}
}

void shuttle::loadPassangers()
{
	if (m_shuttle->cargo_space_taken < m_passengers.size())
	{
		Micro::SmartRightClick(m_passengers, m_shuttle, *m_bot);
		Micro::SmartRightClick(m_shuttle, m_passengers, *m_bot);
		Micro::SmartCDAbility(m_shuttle, sc2::ABILITY_ID::EFFECT_MEDIVACIGNITEAFTERBURNERS, *m_bot);
	}
	else
	{
		m_status = ShuttleStatus::OnMyWay;
	}
}

void shuttle::travelToDestination()
{
	if (m_wayPoints.empty())
	{
		if (Util::Dist(m_shuttle->pos, m_targetPos)>1.0f)
		{
			m_wayPoints = m_bot->Map().getEdgePath(m_shuttle->pos, m_targetPos);
		}
		else
		{
			m_status = ShuttleStatus::Unloading;
		}
	}
	else if (Util::Dist(m_wayPoints.back(), m_targetPos)>10.0f)
	{
		while (!m_wayPoints.empty())
		{
			m_wayPoints.pop();
		}
	}
	else if (Util::Dist(m_shuttle->pos, m_wayPoints.front()) < 0.1f)
	{
		m_wayPoints.pop();
	}
	else
	{
		Micro::SmartCDAbility(m_shuttle, sc2::ABILITY_ID::EFFECT_MEDIVACIGNITEAFTERBURNERS, *m_bot);
		Micro::SmartMove(m_shuttle, m_wayPoints.front(), *m_bot);
	}
}

void shuttle::unloadPassangers()
{
	if (m_shuttle->cargo_space_taken > 0)
	{
		Micro::SmartAbility(m_shuttle, sc2::ABILITY_ID::UNLOADALLAT, m_shuttle, *m_bot);
	}
	else
	{
		m_status = ShuttleStatus::Done;
	}
}


void shuttle::updateTargetPos(const sc2::Point2D newTargetPos)
{
	m_targetPos = newTargetPos;
}

const bool shuttle::isShuttle(const sc2::Unit* unit) const
{
	return m_shuttle && unit->tag == m_shuttle->tag;
}

const bool shuttle::needShuttleUnit() const
{
	return m_status == ShuttleStatus::lookingForShuttle;
}

void shuttle::assignShuttleUnit(const sc2::Unit* unit)
{
	m_shuttle = unit;
	m_status = ShuttleStatus::Loading;
}

int shuttle::getShuttleStatus() const
{
	return m_status;
}