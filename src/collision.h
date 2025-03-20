#ifndef LIB_COLLISION_H
#define LIB_COLLISION_H

#include "vmath.h"
struct Collision {
} typedef SCollision;

typedef bool (*CALLBACK_SWITCHACTIVE)(int Number, void *pUser);

// NOTE: we can probably inline most of these
int get_move_restrictions(SCollision *pCollision,
                          CALLBACK_SWITCHACTIVE pfnSwitchActive, void *pUser,
                          vec2 Pos, float Distance,
                          int OverrideCenterTileIndex);
int get_map_index(SCollision *pCollision, vec2 Pos);
bool check_point(SCollision *pCollision, vec2 Pos);
int intersect_line_tele_hook(SCollision *pCollision, vec2 Pos0, vec2 Pos1,
                             vec2 *pOutCollision, vec2 *pOutBeforeCollision,
                             int *pTeleNr);
bool test_box(SCollision *pCollision, vec2 Pos, vec2 Size);
int get_collision_at(SCollision *pCollision, float x, float y);
int get_front_collision_at(SCollision *pCollision, float x, float y);
int is_tune(SCollision *pCollision, int Index);
int is_speedup(SCollision *pCollision, int Index);
void get_speedup(SCollision *pCollision, int Index, vec2 *pDir, int *pForce,
                 int *pMaxSpeed, int *pType);
int get_tile_index(SCollision *pCollision, int Index);
int get_front_tile_index(SCollision *pCollision, int Index);

int get_switch_number(SCollision *pCollision, int Index);
int get_switch_type(SCollision *pCollision, int Index);
int get_switch_delay(SCollision *pCollision, int Index);

int is_teleport(SCollision *pCollision, int Index);
int is_evil_teleport(SCollision *pCollision, int Index);

// Checkpoint tiles for tele checkpoints
int is_tele_checkpoint(SCollision *pCollision, int Index);

const vec2 *tele_ins(SCollision *pCollision, int Number, int *pOutNum);
const vec2 *tele_outs(SCollision *pCollision, int Number, int *pOutNum);
const vec2 *tele_check_outs(SCollision *pCollision, int Number, int *pOutNum);

#endif // LIB_COLLISION_H
