#ifndef LIB_COLLISION_H
#define LIB_COLLISION_H

#include "../libs/ddnet_maploader_c/map_loader.h"
#include "stdbool.h"
#include "vmath.h"

enum {
  CANTMOVE_LEFT = 1 << 0,
  CANTMOVE_RIGHT = 1 << 1,
  CANTMOVE_UP = 1 << 2,
  CANTMOVE_DOWN = 1 << 3,
};

#define DEATH 9
#define PICKUPSIZE 14
#define PHYSICALSIZE 28.f
#define HALFPHYSICALSIZE 14
#define PHYSICALSIZEVEC vec2_init(28.f, 28.f)

typedef struct Collision {
  SMapData m_MapData;

  int m_NumPickupsTotal;

  int m_NumSpawnPoints;
  vec2 *m_pSpawnPoints;
  int m_aNumTeleOuts[256];
  vec2 *m_apTeleOuts[256];
  int m_aNumTeleCheckOuts[256];
  vec2 *m_apTeleCheckOuts[256];

  bool *m_pTileExists;

} SCollision;

typedef _Bool (*CALLBACK_SWITCHACTIVE)(int Number, void *pUser);

bool init_collision(SCollision *pCollision, const char *pMap);
void free_collision(SCollision *pCollision);
int get_pure_map_index(SCollision *pCollision, vec2 Pos);
int get_move_restrictions_raw(int Tile, int Flags);
int move_restrictions(int Direction, int Tile, int Flags);
int get_tile_index(SCollision *pCollision, int Index);
int get_front_tile_index(SCollision *pCollision, int Index);
int get_tile_flags(SCollision *pCollision, int Index);
int get_front_tile_flags(SCollision *pCollision, int Index);
int get_switch_number(SCollision *pCollision, int Index);
int get_switch_type(SCollision *pCollision, int Index);
int get_switch_delay(SCollision *pCollision, int Index);
int is_teleport(SCollision *pCollision, int Index);
int is_teleport_hook(SCollision *pCollision, int Index);
int is_teleport_weapon(SCollision *pCollision, int Index);
int is_evil_teleport(SCollision *pCollision, int Index);
bool is_check_teleport(SCollision *pCollision, int Index);
bool is_check_evil_teleport(SCollision *pCollision, int Index);
int is_tele_checkpoint(SCollision *pCollision, int Index);
int get_collision_at(SCollision *pCollision, float x, float y);
int get_front_collision_at(SCollision *pCollision, float x, float y);
int get_move_restrictions(SCollision *pCollision,
                          CALLBACK_SWITCHACTIVE pfnSwitchActive, void *pUser,
                          vec2 Pos, int OverrideCenterTileIndex);
int get_map_index(SCollision *pCollision, vec2 Pos);
bool check_point(SCollision *pCollision, vec2 Pos);
void ThroughOffset(vec2 Pos0, vec2 Pos1, int *pOffsetX, int *pOffsetY);
bool is_through(SCollision *pCollision, int x, int y, int OffsetX, int OffsetY,
                vec2 Pos0, vec2 Pos1);
bool is_hook_blocker(SCollision *pCollision, int x, int y, vec2 Pos0,
                     vec2 Pos1);
int intersect_line_tele_hook(SCollision *pCollision, vec2 Pos0, vec2 Pos1,
                             vec2 *pOutCollision, int *pTeleNr,
                             bool OldTeleHook);
bool test_box(SCollision *pCollision, vec2 Pos, vec2 Size);
int is_tune(SCollision *pCollision, int Index);
bool is_speedup(SCollision *pCollision, int Index);
void get_speedup(SCollision *pCollision, int Index, vec2 *pDir, int *pForce,
                 int *pMaxSpeed, int *pType);
const vec2 *spawn_points(SCollision *pCollision, int *pOutNum);
const vec2 *tele_outs(SCollision *pCollision, int Number, int *pOutNum);
const vec2 *tele_check_outs(SCollision *pCollision, int Number, int *pOutNum);
int intersect_line(SCollision *pCollision, vec2 Pos0, vec2 Pos1,
                   vec2 *pOutCollision, vec2 *pOutBeforeCollision);
void move_box(SCollision *pCollision, vec2 *pInoutPos, vec2 *pInoutVel,
              vec2 Elasticity, bool *pGrounded);
bool get_nearest_air_pos_player(SCollision *pCollision, vec2 PlayerPos,
                                vec2 *pOutPos);
bool get_nearest_air_pos(SCollision *pCollision, vec2 Pos, vec2 PrevPos,
                         vec2 *pOutPos);
int get_index(SCollision *pCollision, vec2 PrevPos, vec2 Pos);
int mover_speed(SCollision *pCollision, int x, int y, vec2 *pSpeed);
int entity(SCollision *pCollision, int x, int y, int Layer);

#endif // LIB_COLLISION_H
