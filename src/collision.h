#ifndef LIB_COLLISION_H
#define LIB_COLLISION_H

#include "../libs/ddnet_maploader_c/map_loader.h"
#include "stdbool.h"
#include "vmath.h"

typedef SMapData SCollision;
typedef _Bool (*CALLBACK_SWITCHACTIVE)(int Number, void *pUser);

// NOTE: we can probably inline most of these
int get_move_restrictions(SCollision *pCollision,
                          CALLBACK_SWITCHACTIVE pfnSwitchActive, void *pUser,
                          vec2 Pos, float Distance,
                          int OverrideCenterTileIndex);
int get_map_index(SCollision *pCollision, vec2 Pos);
bool check_point(SCollision *pCollision, vec2 Pos);
int intersect_line_tele_hook(SCollision *pCollision, vec2 Pos0, vec2 Pos1,
                             vec2 *pOutCollision, vec2 *pOutBeforeCollision,
                             int *pTeleNr, bool OldTeleHook);
bool test_box(SCollision *pCollision, vec2 Pos, vec2 Size);
int get_collision_at(SCollision *pCollision, float x, float y);
int get_front_collision_at(SCollision *pCollision, float x, float y);
int is_tune(SCollision *pCollision, int Index);
bool is_speedup(SCollision *pCollision, int Index);
void get_speedup(SCollision *pCollision, int Index, vec2 *pDir, int *pForce,
                 int *pMaxSpeed, int *pType);
int get_tile_index(SCollision *pCollision, int Index);
int get_front_tile_index(SCollision *pCollision, int Index);

int get_tile_flags(SCollision *pCollision, int Index);
int get_front_tile_flags(SCollision *pCollision, int Index);

int get_switch_number(SCollision *pCollision, int Index);
int get_switch_type(SCollision *pCollision, int Index);
int get_switch_delay(SCollision *pCollision, int Index);

int is_teleport(SCollision *pCollision, int Index);
int is_teleport_hook(SCollision *pCollision, int Index);
int is_evil_teleport(SCollision *pCollision, int Index);
bool is_check_teleport(SCollision *pCollision, int Index);
bool is_check_evil_teleport(SCollision *pCollision, int Index);

// Checkpoint tiles for tele checkpoints
int is_tele_checkpoint(SCollision *pCollision, int Index);

const vec2 *spawn_points(SCollision *pCollision, int *pOutNum);

const vec2 *tele_ins(SCollision *pCollision, int Number, int *pOutNum);
const vec2 *tele_outs(SCollision *pCollision, int Number, int *pOutNum);
const vec2 *tele_check_outs(SCollision *pCollision, int Number, int *pOutNum);

bool tile_exists(SCollision *pCollision, int Index);

void move_box(SCollision *pCollision, vec2 *pInoutPos, vec2 *pInoutVel,
              vec2 Size, vec2 Elasticity, bool *pGrounded);

#endif // LIB_COLLISION_H
