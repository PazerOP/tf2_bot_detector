/*
 *prediction.h
 *
 * Created on: Dec 5, 2016
 *     Author: nullifiedcat
 */

#pragma once

#include <enums.hpp>
#include "config.h"
#include "vector"

class CachedEntity;
class Vector;

Vector SimpleLatencyPrediction(CachedEntity *ent, int hb);

bool PerformProjectilePrediction(CachedEntity *target, int hitbox);

Vector BuildingPrediction(CachedEntity *building, Vector vec, float speed, float gravity);
Vector ProjectilePrediction(CachedEntity *ent, int hb, float speed, float gravitymod, float entgmod);
Vector ProjectilePrediction_Engine(CachedEntity *ent, int hb, float speed, float gravitymod, float entgmod /* ignored */);

std::vector<Vector> Predict(Vector pos, float offset, Vector vel, Vector acceleration, std::pair<Vector, Vector> minmax, float time, int count, bool vischeck = true);
float PlayerGravityMod(CachedEntity *player);

Vector EnginePrediction(CachedEntity *player, float time);
#if ENABLE_VISUALS
void Prediction_PaintTraverse();
#endif

float DistanceToGround(CachedEntity *ent);
float DistanceToGround(Vector origin);
float DistanceToGround(Vector origin, Vector mins, Vector maxs);
