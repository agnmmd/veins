//
// Copyright (C) 2006-2018 Christoph Sommer <sommer@ccs-labs.org>
//
// Documentation for these modules is at http://veins.car2x.org/
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//

#include "veins/modules/analogueModel/SimpleObstacleShadowing.h"

using namespace Veins;

using Veins::AirFrame;

SimpleObstacleShadowing::SimpleObstacleShadowing(cComponent* owner, ObstacleControl& obstacleControl, bool useTorus, const Coord& playgroundSize)
    : AnalogueModel(owner)
    , obstacleControl(obstacleControl)
    , useTorus(useTorus)
    , playgroundSize(playgroundSize)
{
    if (useTorus) throw cRuntimeError("SimpleObstacleShadowing does not work on torus-shaped playgrounds");
    annotations = AnnotationManagerAccess().getIfExists();

    // subscribe this Analogue Model to the signal emitted by the Obstacle Control
    getSimulation()->getSystemModule()->subscribe(ObstacleControl::clearAnalogueModuleCacheSignal, this);
}

SimpleObstacleShadowing::~SimpleObstacleShadowing()
{
    // unsubscribe this Analogue Model from the signal emitted by the Obstacle Control
    getSimulation()->getSystemModule()->unsubscribe(ObstacleControl::clearAnalogueModuleCacheSignal, this);
}

void SimpleObstacleShadowing::filterSignal(Signal* signal)
{
    auto senderPos = signal->getSenderPoa().pos.getPositionAt();
    auto receiverPos = signal->getReceiverPoa().pos.getPositionAt();

    // sanity checks for obstacle definitions
    if (!obstacleControl.isAnyObstacleDefined()) {
        throw cRuntimeError("Unable to use SimpleObstacleShadowing: No obstacle types have been configured, or no obstacles have been added");
    }

    double factor = 1;
    const double EXTREME_ATTENUATION = 1e-30;

    // return cached result, if available
    CacheKey cacheKey{senderPos, receiverPos};
    CacheEntries::const_iterator cacheEntryIter = cacheEntries.find(cacheKey);
    if (cacheEntryIter != cacheEntries.end()) {
        factor = cacheEntryIter->second;
    }
    else {
        // calculate attenuation for given obstacles
        std::vector<Obstacle const*> potentialObstacles = obstacleControl.getPotentialObstacles(senderPos, receiverPos);
        for (auto o : potentialObstacles) {

            double factorOld = factor;

            factor = obstacleControl.calculateAttenuation(senderPos, receiverPos, *o);

            // draw a "hit!" bubble
            if (annotations && (factor != factorOld)) annotations->drawBubble(o->getBboxP1(), "hit");

            // bail if attenuation is already extremely high
            if (factor < EXTREME_ATTENUATION) break;
        }
    }

    // cache result
    if (cacheEntries.size() >= 1000) cacheEntries.clear();
    cacheEntries[cacheKey] = factor;

    EV_TRACE << "value is: " << factor << endl;

    *signal *= factor;
}

void SimpleObstacleShadowing::receiveSignal(cComponent* source, simsignal_t signalID, bool b, cObject* details)
{
    if (signalID == ObstacleControl::clearAnalogueModuleCacheSignal) cacheEntries.clear();
}
