#include "collision.h"
#include "vmath.h"

extern inline int get_pure_map_index(SCollision *pCollision, vec2 Pos);

extern inline int get_move_restrictions_mask(int Direction);

extern inline int get_move_restrictions_raw(int Tile, int Flags);

extern inline int move_restrictions(int Direction, int Tile, int Flags);

extern inline int get_tile_index(SCollision *pCollision, int Index);
extern inline int get_front_tile_index(SCollision *pCollision, int Index);
extern inline int get_tile_flags(SCollision *pCollision, int Index);
extern inline int get_front_tile_flags(SCollision *pCollision, int Index);
extern inline int get_switch_number(SCollision *pCollision, int Index);
extern inline int get_switch_type(SCollision *pCollision, int Index);
extern inline int get_switch_delay(SCollision *pCollision, int Index);
extern inline int is_teleport(SCollision *pCollision, int Index);
extern inline int is_teleport_hook(SCollision *pCollision, int Index);
extern inline int is_teleport_weapon(SCollision *pCollision, int Index);
extern inline int is_evil_teleport(SCollision *pCollision, int Index);
extern inline bool is_check_teleport(SCollision *pCollision, int Index);
extern inline bool is_check_evil_teleport(SCollision *pCollision, int Index);
extern inline int is_tele_checkpoint(SCollision *pCollision, int Index);
extern inline int get_collision_at(SCollision *pCollision, float x, float y);
extern inline int get_front_collision_at(SCollision *pCollision, float x,
                                         float y);
extern inline bool tile_exists_next(SCollision *pCollision, int Index);
extern inline bool tile_exists(SCollision *pCollision, int Index);
extern inline int get_move_restrictions(SCollision *pCollision,
                                        CALLBACK_SWITCHACTIVE pfnSwitchActive,
                                        void *pUser, vec2 Pos, float Distance,
                                        int OverrideCenterTileIndex);
extern inline int get_map_index(SCollision *pCollision, vec2 Pos);
extern inline bool check_point(SCollision *pCollision, vec2 Pos);
extern inline void ThroughOffset(vec2 Pos0, vec2 Pos1, int *pOffsetX,
                                 int *pOffsetY);
extern inline bool is_through(SCollision *pCollision, int x, int y, int OffsetX,
                              int OffsetY, vec2 Pos0, vec2 Pos1);
extern inline bool is_hook_blocker(SCollision *pCollision, int x, int y,
                                   vec2 Pos0, vec2 Pos1);
extern inline int intersect_line_tele_hook(SCollision *pCollision, vec2 Pos0,
                                           vec2 Pos1, vec2 *pOutCollision,
                                           vec2 *pOutBeforeCollision,
                                           int *pTeleNr, bool OldTeleHook);
extern inline bool test_box(SCollision *pCollision, vec2 Pos, vec2 Size);

extern inline int is_tune(SCollision *pCollision, int Index);
extern inline bool is_speedup(SCollision *pCollision, int Index);
extern inline void get_speedup(SCollision *pCollision, int Index, vec2 *pDir,
                               int *pForce, int *pMaxSpeed, int *pType);
extern inline const vec2 *spawn_points(SCollision *pCollision, int *pOutNum);
extern inline const vec2 *tele_outs(SCollision *pCollision, int Number,
                                    int *pOutNum);
extern inline const vec2 *tele_check_outs(SCollision *pCollision, int Number,
                                          int *pOutNum);

extern inline int intersect_line(SCollision *pCollision, vec2 Pos0, vec2 Pos1,
                                 vec2 *pOutCollision,
                                 vec2 *pOutBeforeCollision);

extern inline void move_box(SCollision *pCollision, vec2 *pInoutPos,
                            vec2 *pInoutVel, vec2 Size, vec2 Elasticity,
                            bool *pGrounded);

extern inline bool get_nearest_air_pos_player(SCollision *pCollision,
                                              vec2 PlayerPos, vec2 *pOutPos);

extern inline bool get_nearest_air_pos(SCollision *pCollision, vec2 Pos,
                                       vec2 PrevPos, vec2 *pOutPos);

extern inline int get_index(SCollision *pCollision, vec2 PrevPos, vec2 Pos);
