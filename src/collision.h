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

enum { INFO_ISSOLID = 1 << 0, INFO_TILENEXT = 1 << 1 };

typedef struct TuningParams {
#define MACRO_TUNING_PARAM(Name, Value) float m_##Name;
#include "tuning.h"
#undef MACRO_TUNING_PARAM
} STuningParams;

enum {
  POWERUP_HEALTH,
  POWERUP_ARMOR,
  POWERUP_WEAPON,
  POWERUP_NINJA,
  POWERUP_ARMOR_SHOTGUN,
  POWERUP_ARMOR_GRENADE,
  POWERUP_ARMOR_NINJA,
  POWERUP_ARMOR_LASER,
  NUM_POWERUPS
};
typedef struct Pickup {
  char m_Type;
  unsigned char m_Number;
  unsigned char m_Subtype;
} SPickup;

enum { NUM_TUNE_ZONES = 256 };

typedef struct Collision {
  SMapData m_MapData;

  int m_NumSpawnPoints;
  vec2 *m_pSpawnPoints;
  int m_aNumTeleOuts[256];
  vec2 *m_apTeleOuts[256];
  int m_aNumTeleCheckOuts[256];
  vec2 *m_apTeleCheckOuts[256];

  float *m_pSolidDistanceField;
  unsigned char *m_pTileInfos;
  SPickup *m_pPickups;

  bool m_MoveRestrictionsFound;
  unsigned char (*m_pMoveRestrictions)[5];

  bool *m_pTileBroadCheck;

  // Could be made into a dynamic list based on the server settings so only tune
  // zones that actually get modified get loaded
  // this is 48KB xd
  // TODO: do this better lol
  STuningParams m_aTuningList[NUM_TUNE_ZONES];
} SCollision;

bool init_collision(SCollision *restrict pCollision, const char *restrict pMap);
void free_collision(SCollision *pCollision);
int get_pure_map_index(SCollision *pCollision, vec2 Pos);
unsigned char move_restrictions(unsigned char Direction, unsigned char Tile,
                                unsigned char Flags);
unsigned char get_tile_index(SCollision *pCollision, int Index);
unsigned char get_front_tile_index(SCollision *pCollision, int Index);
unsigned char get_tile_flags(SCollision *pCollision, int Index);
unsigned char get_front_tile_flags(SCollision *pCollision, int Index);
unsigned char get_switch_number(SCollision *pCollision, int Index);
unsigned char get_switch_type(SCollision *pCollision, int Index);
unsigned char get_switch_delay(SCollision *pCollision, int Index);
unsigned char is_teleport(SCollision *pCollision, int Index);
unsigned char is_teleport_hook(SCollision *pCollision, int Index);
unsigned char is_teleport_weapon(SCollision *pCollision, int Index);
unsigned char is_evil_teleport(SCollision *pCollision, int Index);
unsigned char is_check_teleport(SCollision *pCollision, int Index);
unsigned char is_check_evil_teleport(SCollision *pCollision, int Index);
unsigned char is_tele_checkpoint(SCollision *pCollision, int Index);
unsigned char get_collision_at(SCollision *pCollision, vec2 Pos);
unsigned char get_front_collision_at(SCollision *pCollision, vec2 Pos);
unsigned char get_move_restrictions(SCollision *pCollision, void *pUser,
                                    vec2 Pos, int OverrideCenterTileIndex);
int get_map_index(SCollision *pCollision, vec2 Pos);
bool check_point(SCollision *pCollision, vec2 Pos);
void ThroughOffset(vec2 Pos0, vec2 Pos1, int *restrict pOffsetX,
                   int *restrict pOffsetY);
bool is_through(SCollision *pCollision, int x, int y, int OffsetX, int OffsetY,
                vec2 Pos0, vec2 Pos1);
bool is_hook_blocker(SCollision *pCollision, int Index, vec2 Pos0, vec2 Pos1);
unsigned char intersect_line_tele_hook(SCollision *restrict pCollision,
                                       vec2 Pos0, vec2 Pos1,
                                       vec2 *restrict pOutCollision,
                                       int *restrict pTeleNr);

bool test_box(SCollision *pCollision, vec2 Pos, vec2 Size);
unsigned char is_tune(SCollision *pCollision, int Index);
bool is_speedup(SCollision *pCollision, int Index);
void get_speedup(SCollision *restrict pCollision, int Index,
                 vec2 *restrict pDir, int *restrict pForce,
                 int *restrict pMaxSpeed, int *restrict pType);
const vec2 *spawn_points(SCollision *restrict pCollision,
                         int *restrict pOutNum);
const vec2 *tele_outs(SCollision *restrict pCollision, int Number,
                      int *restrict pOutNum);
const vec2 *tele_check_outs(SCollision *restrict pCollision, int Number,
                            int *restrict pOutNum);
unsigned char intersect_line(SCollision *restrict pCollision, vec2 Pos0,
                             vec2 Pos1, vec2 *restrict pOutCollision,
                             vec2 *restrict pOutBeforeCollision);
void move_box(SCollision *restrict pCollision, vec2 *restrict pInoutPos,
              vec2 *restrict pInoutVel, vec2 Elasticity,
              bool *restrict pGrounded);
bool get_nearest_air_pos_player(SCollision *pCollision, vec2 PlayerPos,
                                vec2 *pOutPos);
bool get_nearest_air_pos(SCollision *pCollision, vec2 Pos, vec2 PrevPos,
                         vec2 *pOutPos);
int get_index(SCollision *pCollision, vec2 PrevPos, vec2 Pos);
unsigned char mover_speed(SCollision *pCollision, int x, int y, vec2 *pSpeed);
int entity(SCollision *pCollision, int x, int y, int Layer);
#endif // LIB_COLLISION_H
