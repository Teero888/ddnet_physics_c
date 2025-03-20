#ifndef LIB_COLLISION_H
#define LIB_COLLISION_H

#include "vmath.h"
struct Collision {
} typedef SCollision;

typedef bool (*CALLBACK_SWITCHACTIVE)(int Number, void *pUser);
int get_move_restrictions(SCollision *pCollision,
                          CALLBACK_SWITCHACTIVE pfnSwitchActive, void *pUser,
                          vec2 Pos, float Distance,
                          int OverrideCenterTileIndex);
int get_map_index(SCollision *pCollision, vec2 Pos);
int is_tune(SCollision *pCollision, int Index);
bool check_point(SCollision *pCollision, vec2 Pos);
int intersect_line_tele_hook(SCollision *pCollision, vec2 Pos0, vec2 Pos1,
                             vec2 *pOutCollision, vec2 *pOutBeforeCollision,
                             int *pTeleNr);
vec2 *tele_outs(SCollision *pCollision, int Number, int *pOutNum);

#endif // LIB_COLLISION_H
