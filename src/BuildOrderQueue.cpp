#include "BuildOrderQueue.h"
#include "CCBot.h"

BuildOrderItem::BuildOrderItem(const BuildType & t, int p, bool b)
    : type(t)
    , priority(p)
    , blocking(b)
{
}

bool BuildOrderItem::operator < (const BuildOrderItem & x) const
{
    return priority < x.priority;
}