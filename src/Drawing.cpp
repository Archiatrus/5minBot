#include "sc2api/sc2_api.h"
#include "CCBot.h"
#include "Drawing.h"



void Drawing::drawLine(CCBot & bot, float x1, float y1, float x2, float y2, const sc2::Color & color)
{
	if (!useDebug)
	{
		return;
	}
	const float maxZ = bot.Map().getHeight(x1, y1);
	bot.Debug()->DebugLineOut(sc2::Point3D(x1, y1, maxZ + 0.2f), sc2::Point3D(x2, y2, maxZ + 0.2f), color);
}

void Drawing::drawLine(CCBot & bot, const sc2::Point2D & min, const sc2::Point2D max, const sc2::Color & color)
{
	if (!useDebug)
	{
		return;
	}
	const float maxZ = bot.Map().getHeight(min);
	bot.Debug()->DebugLineOut(sc2::Point3D(min.x, min.y, maxZ + 0.2f), sc2::Point3D(max.x, max.y, maxZ + 0.2f), color);
}

void Drawing::drawSquare(CCBot & bot, float x1, float y1, float x2, float y2, const sc2::Color & color)
{
	if (!useDebug)
	{
		return;
	}
	const float maxZ = bot.Map().getHeight(x1, y1);
	bot.Debug()->DebugLineOut(sc2::Point3D(x1, y1, maxZ), sc2::Point3D(x1 + 1, y1, maxZ), color);
	bot.Debug()->DebugLineOut(sc2::Point3D(x1, y1, maxZ), sc2::Point3D(x1, y1 + 1, maxZ), color);
	bot.Debug()->DebugLineOut(sc2::Point3D(x1 + 1, y1 + 1, maxZ), sc2::Point3D(x1 + 1, y1, maxZ), color);
	bot.Debug()->DebugLineOut(sc2::Point3D(x1 + 1, y1 + 1, maxZ), sc2::Point3D(x1, y1 + 1, maxZ), color);
}

void Drawing::drawBox(CCBot & bot, float x1, float y1, float x2, float y2, const sc2::Color & color)
{
	if (!useDebug)
	{
		return;
	}
	bot.Debug()->DebugBoxOut(sc2::Point3D(x1, y1, 2.0f + bot.Map().getHeight(x1,y1)), sc2::Point3D(x2, y2, 5.0f - bot.Map().getHeight(x1, y1) ), color);
}

void Drawing::drawBox(CCBot & bot, const sc2::Point2D & min, const sc2::Point2D max, const sc2::Color & color)
{
	if (!useDebug)
	{
		return;
	}
	bot.Debug()->DebugBoxOut(sc2::Point3D(min.x, min.y, bot.Map().getHeight(min)+ 2.0f), sc2::Point3D(max.x, max.y, bot.Map().getHeight(min) - 5.0f), color);
}

void Drawing::drawSphere(CCBot & bot, const sc2::Point2D & pos, float radius, const sc2::Color & color)
{
	if (!useDebug)
	{
		return;
	}
	bot.Debug()->DebugSphereOut(sc2::Point3D(pos.x, pos.y, bot.Map().getHeight(pos)), radius, color);
}

void Drawing::drawSphere(CCBot & bot, float x, float y, float radius, const sc2::Color & color)
{
	if (!useDebug)
	{
		return;
	}
	bot.Debug()->DebugSphereOut(sc2::Point3D(x, y, bot.Map().getHeight(x,y)), radius, color);
}

void Drawing::drawText(CCBot & bot, const sc2::Point2D & pos, const std::string & str, const sc2::Color & color)
{
	if (!useDebug)
	{
		return;
	}
	bot.Debug()->DebugTextOut(str, sc2::Point3D(pos.x, pos.y, bot.Map().getHeight(pos)), color);
}

void Drawing::drawTextScreen(CCBot & bot, const sc2::Point2D & pos, const std::string & str, const sc2::Color & color,uint32_t size)
{
	if (!useDebug)
	{
		return;
	}
	bot.Debug()->DebugTextOut(str, pos, color,size);
}


void Drawing::drawBoxAroundUnit(CCBot & bot, const UnitTag & unitTag, sc2::Color color)
{
	const sc2::Unit * unit = bot.GetUnit(unitTag);

	if (!unit) { return; }

	sc2::Point3D p_min = unit->pos;
	p_min.x -= 2.0f;
	p_min.y -= 2.0f;
	p_min.z -= 2.0f;

	sc2::Point3D p_max = unit->pos;
	p_max.x += 2.0f;
	p_max.y += 2.0f;
	p_max.z += 2.0f;

	drawSquare(bot,unit->pos.x - 2.0f, unit->pos.y - 2.0f, unit->pos.x + 2.0f, unit->pos.y + 2.0f, color);
}

void Drawing::drawSphereAroundUnit(CCBot & bot, const UnitTag & unitTag, sc2::Color color)
{
	const sc2::Unit * unit = bot.GetUnit(unitTag);

	if (!unit) { return; }

	drawSphere(bot,unit->pos, 1, color);
}