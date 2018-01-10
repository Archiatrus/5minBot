#pragma once

#include "sc2api/sc2_api.h"
#include "CCBot.h"

extern bool useDebug;

class CCBot;

namespace Drawing
{
	void    drawLine(CCBot & bot, float x1, float y1, float x2, float y2, const sc2::Color & color = sc2::Colors::White);
	void    drawLine(CCBot & bot, const sc2::Point2D & min, const sc2::Point2D max, const sc2::Color & color = sc2::Colors::White);
	void    drawSquare(CCBot & bot, float x1, float y1, float x2, float y2, const sc2::Color & color = sc2::Colors::White);
	void    drawBox(CCBot & bot, float x1, float y1, float x2, float y2, const sc2::Color & color = sc2::Colors::White);
	void    drawBox(CCBot & bot, const sc2::Point2D & min, const sc2::Point2D max, const sc2::Color & color = sc2::Colors::White);
	void    drawSphere(CCBot & bot, float x1, float x2, float radius, const sc2::Color & color = sc2::Colors::White);
	void    drawSphere(CCBot & bot, const sc2::Point2D & pos, float radius, const sc2::Color & color = sc2::Colors::White);
	void    drawText(CCBot & bot, const sc2::Point2D & pos, const std::string & str, const sc2::Color & color = sc2::Colors::White);
	void    drawTextScreen(CCBot & bot, const sc2::Point2D & pos, const std::string & str, const sc2::Color & color = sc2::Colors::White, uint32_t size = 8U);
	void    drawBoxAroundUnit(CCBot & bot, const UnitTag & uinit, sc2::Color color = sc2::Colors::White);
	void    drawSphereAroundUnit(CCBot & bot, const UnitTag & uinit, sc2::Color color = sc2::Colors::White);
};