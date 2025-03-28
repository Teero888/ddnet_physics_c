#include "collision.h"
#include "gamecore.h"
#include "limits.h"
#include "map_loader.h"
#include "vmath.h"
#include <stdio.h>
#include <string.h>

enum {
  MR_DIR_HERE = 0,
  MR_DIR_RIGHT,
  MR_DIR_DOWN,
  MR_DIR_LEFT,
  MR_DIR_UP,
  NUM_MR_DIRS
};

static bool tile_exists_next(SCollision *pCollision, int Index) {
  const unsigned char *pTileIdx = pCollision->m_MapData.m_GameLayer.m_pData;
  const unsigned char *pTileFlgs = pCollision->m_MapData.m_GameLayer.m_pFlags;
  const unsigned char *pFrontIdx = pCollision->m_MapData.m_FrontLayer.m_pData;
  const unsigned char *pFrontFlgs = pCollision->m_MapData.m_FrontLayer.m_pFlags;
  const unsigned char *pDoorIdx = pCollision->m_MapData.m_DoorLayer.m_pIndex;
  const unsigned char *pDoorFlgs = pCollision->m_MapData.m_DoorLayer.m_pFlags;
  int TileOnTheLeft = (Index - 1 > 0) ? Index - 1 : Index;
  int TileOnTheRight = (Index + 1 < pCollision->m_MapData.m_Width *
                                        pCollision->m_MapData.m_Height)
                           ? Index + 1
                           : Index;
  int TileBelow =
      (Index + pCollision->m_MapData.m_Width <
       pCollision->m_MapData.m_Width * pCollision->m_MapData.m_Height)
          ? Index + pCollision->m_MapData.m_Width
          : Index;
  int TileAbove = (Index - pCollision->m_MapData.m_Width > 0)
                      ? Index - pCollision->m_MapData.m_Width
                      : Index;

  if ((pTileIdx[TileOnTheRight] == TILE_STOP &&
       pTileFlgs[TileOnTheRight] == ROTATION_270) ||
      (pTileIdx[TileOnTheLeft] == TILE_STOP &&
       pTileFlgs[TileOnTheLeft] == ROTATION_90))
    return true;
  if ((pTileIdx[TileBelow] == TILE_STOP &&
       pTileFlgs[TileBelow] == ROTATION_0) ||
      (pTileIdx[TileAbove] == TILE_STOP &&
       pTileFlgs[TileAbove] == ROTATION_180))
    return true;
  if (pTileIdx[TileOnTheRight] == TILE_STOPA ||
      pTileIdx[TileOnTheLeft] == TILE_STOPA ||
      ((pTileIdx[TileOnTheRight] == TILE_STOPS ||
        pTileIdx[TileOnTheLeft] == TILE_STOPS)))
    return true;
  if (pTileIdx[TileBelow] == TILE_STOPA || pTileIdx[TileAbove] == TILE_STOPA ||
      ((pTileIdx[TileBelow] == TILE_STOPS ||
        pTileIdx[TileAbove] == TILE_STOPS) &&
       pTileFlgs[TileBelow] | ROTATION_180 | ROTATION_0))
    return true;

  if (pFrontIdx) {
    if (pFrontIdx[TileOnTheRight] == TILE_STOPA ||
        pFrontIdx[TileOnTheLeft] == TILE_STOPA ||
        ((pFrontIdx[TileOnTheRight] == TILE_STOPS ||
          pFrontIdx[TileOnTheLeft] == TILE_STOPS)))
      return true;
    if (pFrontIdx[TileBelow] == TILE_STOPA ||
        pFrontIdx[TileAbove] == TILE_STOPA ||
        ((pFrontIdx[TileBelow] == TILE_STOPS ||
          pFrontIdx[TileAbove] == TILE_STOPS) &&
         pFrontFlgs[TileBelow] | ROTATION_180 | ROTATION_0))
      return true;
    if ((pFrontIdx[TileOnTheRight] == TILE_STOP &&
         pFrontFlgs[TileOnTheRight] == ROTATION_270) ||
        (pFrontIdx[TileOnTheLeft] == TILE_STOP &&
         pFrontFlgs[TileOnTheLeft] == ROTATION_90))
      return true;
    if ((pFrontIdx[TileBelow] == TILE_STOP &&
         pFrontFlgs[TileBelow] == ROTATION_0) ||
        (pFrontIdx[TileAbove] == TILE_STOP &&
         pFrontFlgs[TileAbove] == ROTATION_180))
      return true;
  }
  if (pDoorIdx) {
    if (pDoorIdx[TileOnTheRight] == TILE_STOPA ||
        pDoorIdx[TileOnTheLeft] == TILE_STOPA ||
        ((pDoorIdx[TileOnTheRight] == TILE_STOPS ||
          pDoorIdx[TileOnTheLeft] == TILE_STOPS)))
      return true;
    if (pDoorIdx[TileBelow] == TILE_STOPA ||
        pDoorIdx[TileAbove] == TILE_STOPA ||
        ((pDoorIdx[TileBelow] == TILE_STOPS ||
          pDoorIdx[TileAbove] == TILE_STOPS) &&
         pDoorFlgs[TileBelow] | ROTATION_180 | ROTATION_0))
      return true;
    if ((pDoorIdx[TileOnTheRight] == TILE_STOP &&
         pDoorFlgs[TileOnTheRight] == ROTATION_270) ||
        (pDoorIdx[TileOnTheLeft] == TILE_STOP &&
         pDoorFlgs[TileOnTheLeft] == ROTATION_90))
      return true;
    if ((pDoorIdx[TileBelow] == TILE_STOP &&
         pDoorFlgs[TileBelow] == ROTATION_0) ||
        (pDoorIdx[TileAbove] == TILE_STOP &&
         pDoorFlgs[TileAbove] == ROTATION_180))
      return true;
  }
  return false;
}

static bool tile_exists(SCollision *pCollision, int Index) {
  const unsigned char *pTiles = pCollision->m_MapData.m_GameLayer.m_pData;
  const unsigned char *pFront = pCollision->m_MapData.m_FrontLayer.m_pData;
  const unsigned char *pDoor = pCollision->m_MapData.m_DoorLayer.m_pIndex;
  const unsigned char *pTele = pCollision->m_MapData.m_TeleLayer.m_pType;
  const unsigned char *pSpeedup = pCollision->m_MapData.m_SpeedupLayer.m_pForce;
  const unsigned char *pSwitch = pCollision->m_MapData.m_SwitchLayer.m_pType;
  const unsigned char *pTune = pCollision->m_MapData.m_TuneLayer.m_pType;

  if ((pTiles[Index] >= TILE_FREEZE &&
       pTiles[Index] <= TILE_TELE_LASER_DISABLE) ||
      (pTiles[Index] >= TILE_LFREEZE && pTiles[Index] <= TILE_LUNFREEZE))
    return true;
  if (pFront &&
      ((pFront[Index] >= TILE_FREEZE &&
        pFront[Index] <= TILE_TELE_LASER_DISABLE) ||
       (pFront[Index] >= TILE_LFREEZE && pFront[Index] <= TILE_LUNFREEZE)))
    return true;
  if (pTele &&
      (pTele[Index] == TILE_TELEIN || pTele[Index] == TILE_TELEINEVIL ||
       pTele[Index] == TILE_TELECHECKINEVIL || pTele[Index] == TILE_TELECHECK ||
       pTele[Index] == TILE_TELECHECKIN))
    return true;
  if (pSpeedup && pSpeedup[Index] > 0)
    return true;
  if (pDoor && pDoor[Index])
    return true;
  if (pSwitch && pSwitch[Index])
    return true;
  if (pTune && pTune[Index])
    return true;
  return tile_exists_next(pCollision, Index);
}

bool init_collision(SCollision *restrict pCollision,
                    const char *restrict pMap) {
  pCollision->m_MapData = load_map(pMap);
  if (!pCollision->m_MapData.m_GameLayer.m_pData)
    return false;

  SMapData *pMapData = &pCollision->m_MapData;
  int Width = pMapData->m_Width;
  int Height = pMapData->m_Height;

  pCollision->m_pTileInfos = calloc(Width * Height, 1);
  pCollision->m_pMoveRestrictions = calloc(Width * Height, 5);
  pCollision->m_MoveRestrictionsFound = false;

  // Figure out important things
  // Make lists of spawn points, tele outs and tele checkpoints outs
  for (int i = 0; i < Width * Height; ++i) {

    if (tile_exists(pCollision, i))
      pCollision->m_pTileInfos[i] |= INFO_TILENEXT;
    if (pCollision->m_MapData.m_GameLayer.m_pData[i] == TILE_SOLID ||
        pCollision->m_MapData.m_GameLayer.m_pData[i] == TILE_NOHOOK)
      pCollision->m_pTileInfos[i] |= INFO_ISSOLID;

    for (int d = 0; d < NUM_MR_DIRS; d++) {
      int Tile;
      int Flags;
      if (pCollision->m_MapData.m_FrontLayer.m_pData) {
        Tile = get_front_tile_index(pCollision, i);
        Flags = get_front_tile_flags(pCollision, i);
        pCollision->m_pMoveRestrictions[i][d] |=
            move_restrictions(d, Tile, Flags);
      }
      Tile = get_tile_index(pCollision, i);
      Flags = get_tile_flags(pCollision, i);
      pCollision->m_pMoveRestrictions[i][d] |=
          move_restrictions(d, Tile, Flags);
      if (pCollision->m_pMoveRestrictions[i][d])
        pCollision->m_MoveRestrictionsFound = true;
    }

    int EntIdx = pMapData->m_GameLayer.m_pData[i] - ENTITY_OFFSET;
    if ((EntIdx >= ENTITY_ARMOR_SHOTGUN && EntIdx <= ENTITY_ARMOR_LASER) ||
        (EntIdx >= ENTITY_ARMOR_1 && EntIdx <= ENTITY_WEAPON_LASER))
      ++pCollision->m_NumPickupsTotal;
    if (pMapData->m_GameLayer.m_pData[i] >= 192 &&
        pMapData->m_GameLayer.m_pData[i] <= 194)
      ++pCollision->m_NumSpawnPoints;
    if (pMapData->m_TeleLayer.m_pType) {
      if (pMapData->m_TeleLayer.m_pType[i] == TILE_TELEOUT)
        ++pCollision->m_aNumTeleOuts[pMapData->m_TeleLayer.m_pNumber[i]];
      if (pMapData->m_TeleLayer.m_pType[i] == TILE_TELECHECKOUT)
        ++pCollision->m_aNumTeleCheckOuts[pMapData->m_TeleLayer.m_pNumber[i]];
    }
  }

  // apparently freeing this makes program go slow
  // if (!pCollision->m_MoveRestrictionsFound)
  //   free(pCollision->m_pMoveRestrictions);

  if (pCollision->m_NumSpawnPoints > 0)
    pCollision->m_pSpawnPoints =
        malloc(pCollision->m_NumSpawnPoints * sizeof(float[4]));
  if (pMapData->m_TeleLayer.m_pType) {
    for (int i = 0; i < 256; ++i) {
      if (pCollision->m_aNumTeleOuts[i] > 0)
        pCollision->m_apTeleOuts[i] =
            malloc(pCollision->m_aNumTeleOuts[i] * sizeof(float[4]));
      if (pCollision->m_aNumTeleCheckOuts[i] > 0)
        pCollision->m_apTeleCheckOuts[i] =
            malloc(pCollision->m_aNumTeleCheckOuts[i] * sizeof(float[4]));
    }
  }

  if (!pMapData->m_TeleLayer.m_pType && !pCollision->m_NumSpawnPoints)
    return true;

  int TeleIdx = 0, TeleCheckIdx = 0, SpawnPointIdx = 0;
  for (int y = 0; y < Height; ++y) {
    for (int x = 0; x < Width; ++x) {
      int Idx = y * Width + x;
      if (pCollision->m_NumSpawnPoints) {
        if (pMapData->m_GameLayer.m_pData[Idx] >= 192 &&
            pMapData->m_GameLayer.m_pData[Idx] <= 194)
          pCollision->m_pSpawnPoints[SpawnPointIdx++] = vec2_init(x, y);
      }
      if (pMapData->m_TeleLayer.m_pType) {
        if (pMapData->m_TeleLayer.m_pType[Idx] == TILE_TELEOUT)
          pCollision
              ->m_apTeleOuts[pMapData->m_TeleLayer.m_pNumber[Idx]][TeleIdx++] =
              vec2_init(x, y);
        if (pMapData->m_TeleLayer.m_pType[Idx] == TILE_TELECHECKOUT)
          pCollision->m_apTeleCheckOuts[pMapData->m_TeleLayer.m_pNumber[Idx]]
                                       [TeleCheckIdx++] = vec2_init(x, y);
      }
    }
  }

  return true;
}

void free_collision(SCollision *pCollision) {
  free_map_data(&pCollision->m_MapData);
  if (pCollision->m_NumSpawnPoints)
    free(pCollision->m_pSpawnPoints);
  if (pCollision->m_MoveRestrictionsFound)
    free(pCollision->m_pMoveRestrictions);
  if (pCollision->m_pTileInfos)
    free(pCollision->m_pTileInfos);
  if (pCollision->m_MapData.m_TeleLayer.m_pType)
    for (int i = 0; i < 256; ++i) {
      free(pCollision->m_apTeleOuts[i]);
      free(pCollision->m_apTeleCheckOuts[i]);
    }
  memset(pCollision, 0, sizeof(SCollision));
}

inline int get_pure_map_index(SCollision *pCollision, vec2 Pos) {
  const int nx = (int)(Pos.x + 0.5f) >> 5;
  const int ny = (int)(Pos.y + 0.5f) >> 5;
  return ny * pCollision->m_MapData.m_Width + nx;
}

static inline unsigned char get_move_restrictions_raw(unsigned char Tile,
                                                      unsigned char Flags) {
  Flags &= (TILEFLAG_XFLIP | TILEFLAG_YFLIP | TILEFLAG_ROTATE);
  switch (Tile) {
  case TILE_STOP: {
    static const int move_table[] = {
        CANTMOVE_DOWN, // 0: ROTATION_0
        CANTMOVE_DOWN, // 1: TILEFLAG_YFLIP ^ ROTATION_180
        CANTMOVE_UP,   // 2: TILEFLAG_YFLIP ^ ROTATION_0
        CANTMOVE_UP,   // 3: ROTATION_180
        0,
        0,
        0,
        0,              // 4-7: unused
        CANTMOVE_LEFT,  // 8: ROTATION_90
        CANTMOVE_LEFT,  // 9: TILEFLAG_YFLIP ^ ROTATION_270
        CANTMOVE_RIGHT, // 10: TILEFLAG_YFLIP ^ ROTATION_90
        CANTMOVE_RIGHT  // 11: ROTATION_270
    };
    return move_table[Flags];
  }
  case TILE_STOPS:
    return (Flags & TILEFLAG_ROTATE) ? (CANTMOVE_LEFT | CANTMOVE_RIGHT)
                                     : (CANTMOVE_DOWN | CANTMOVE_UP);
  case TILE_STOPA:
    return CANTMOVE_LEFT | CANTMOVE_RIGHT | CANTMOVE_UP | CANTMOVE_DOWN;
  default:
    return 0;
  }
}

inline unsigned char move_restrictions(unsigned char Direction,
                                       unsigned char Tile,
                                       unsigned char Flags) {
  unsigned char Result = get_move_restrictions_raw(Tile, Flags);
  if (Direction == MR_DIR_HERE && Tile == TILE_STOP)
    return Result;
  static const unsigned char aDirections[NUM_MR_DIRS] = {
      0, CANTMOVE_RIGHT, CANTMOVE_DOWN, CANTMOVE_LEFT, CANTMOVE_UP};
  return Result & aDirections[Direction];
}

inline unsigned char get_tile_index(SCollision *pCollision, int Index) {
  return pCollision->m_MapData.m_GameLayer.m_pData[Index];
}
inline unsigned char get_front_tile_index(SCollision *pCollision, int Index) {
  return pCollision->m_MapData.m_FrontLayer.m_pData[Index];
}
inline unsigned char get_tile_flags(SCollision *pCollision, int Index) {
  return pCollision->m_MapData.m_GameLayer.m_pFlags[Index];
}
inline unsigned char get_front_tile_flags(SCollision *pCollision, int Index) {
  return pCollision->m_MapData.m_FrontLayer.m_pFlags[Index];
}
inline unsigned char get_switch_number(SCollision *pCollision, int Index) {
  return pCollision->m_MapData.m_SwitchLayer.m_pNumber[Index];
}
inline unsigned char get_switch_type(SCollision *pCollision, int Index) {
  return pCollision->m_MapData.m_SwitchLayer.m_pType[Index];
}
inline unsigned char get_switch_delay(SCollision *pCollision, int Index) {
  return pCollision->m_MapData.m_SwitchLayer.m_pDelay[Index];
}
inline unsigned char is_teleport(SCollision *pCollision, int Index) {
  return pCollision->m_MapData.m_TeleLayer.m_pType[Index] == TILE_TELEIN
             ? pCollision->m_MapData.m_TeleLayer.m_pNumber[Index]
             : 0;
}
inline unsigned char is_teleport_hook(SCollision *pCollision, int Index) {
  return pCollision->m_MapData.m_TeleLayer.m_pType[Index] == TILE_TELEINHOOK
             ? pCollision->m_MapData.m_TeleLayer.m_pNumber[Index]
             : 0;
}
inline unsigned char is_teleport_weapon(SCollision *pCollision, int Index) {
  return pCollision->m_MapData.m_TeleLayer.m_pType[Index] == TILE_TELEINWEAPON
             ? pCollision->m_MapData.m_TeleLayer.m_pNumber[Index]
             : 0;
}
inline unsigned char is_evil_teleport(SCollision *pCollision, int Index) {
  return pCollision->m_MapData.m_TeleLayer.m_pType[Index] == TILE_TELEINEVIL
             ? pCollision->m_MapData.m_TeleLayer.m_pNumber[Index]
             : 0;
}
inline unsigned char is_check_teleport(SCollision *pCollision, int Index) {
  return pCollision->m_MapData.m_TeleLayer.m_pType[Index] == TILE_TELECHECKIN
             ? pCollision->m_MapData.m_TeleLayer.m_pNumber[Index]
             : 0;
}
inline unsigned char is_check_evil_teleport(SCollision *pCollision, int Index) {
  return pCollision->m_MapData.m_TeleLayer.m_pType[Index] ==
                 TILE_TELECHECKINEVIL
             ? pCollision->m_MapData.m_TeleLayer.m_pNumber[Index]
             : 0;
}
inline unsigned char is_tele_checkpoint(SCollision *pCollision, int Index) {
  return pCollision->m_MapData.m_TeleLayer.m_pType[Index] == TILE_TELECHECK
             ? pCollision->m_MapData.m_TeleLayer.m_pNumber[Index]
             : 0;
}

inline unsigned char get_collision_at(SCollision *pCollision, float x,
                                      float y) {
  int Nx = (int)x >> 5;
  int Ny = (int)y >> 5;
  int pos = Ny * pCollision->m_MapData.m_Width + Nx;
  int Idx = pCollision->m_MapData.m_GameLayer.m_pData[pos];
  if (Idx >= TILE_SOLID && Idx <= TILE_NOLASER)
    return Idx;
  return 0;
}
inline unsigned char get_front_collision_at(SCollision *pCollision, float x,
                                            float y) {
  int Nx = (int)x >> 5;
  int Ny = (int)y >> 5;
  int pos = Ny * pCollision->m_MapData.m_Width + Nx;
  int Idx = pCollision->m_MapData.m_FrontLayer.m_pData[pos];
  if (Idx >= TILE_SOLID && Idx <= TILE_NOLASER)
    return Idx;
  return 0;
}

inline unsigned char get_move_restrictions(SCollision *pCollision, void *pUser,
                                           vec2 Pos,
                                           int OverrideCenterTileIndex) {

  if (!pCollision->m_MoveRestrictionsFound &&
      !pCollision->m_MapData.m_DoorLayer.m_pIndex)
    return 0;
  static const vec2 DIRECTIONS[NUM_MR_DIRS] = {CTVEC2(0, 0), CTVEC2(18, 0),
                                               CTVEC2(0, 18), CTVEC2(-18, 0),
                                               CTVEC2(0, -18)};
  unsigned char Restrictions = 0;
  for (int d = 0; d < NUM_MR_DIRS; d++) {
    int ModMapIndex = get_pure_map_index(pCollision, vvadd(Pos, DIRECTIONS[d]));
    if (d == MR_DIR_HERE && OverrideCenterTileIndex >= 0)
      ModMapIndex = OverrideCenterTileIndex;
    Restrictions |= pCollision->m_pMoveRestrictions[ModMapIndex][d];
    if (pCollision->m_MapData.m_DoorLayer.m_pIndex &&
        pCollision->m_MapData.m_DoorLayer.m_pIndex[ModMapIndex]) {
      if (is_switch_active_cb(
              pCollision->m_MapData.m_DoorLayer.m_pNumber[ModMapIndex],
              pUser)) {
        Restrictions |= move_restrictions(
            d, pCollision->m_MapData.m_DoorLayer.m_pIndex[ModMapIndex],
            pCollision->m_MapData.m_DoorLayer.m_pFlags[ModMapIndex]);
      }
    }
  }
  return Restrictions;
}

int get_map_index(SCollision *pCollision, vec2 Pos) {
  int Nx = (int)Pos.x >> 5;
  int Ny = (int)Pos.y >> 5;
  int Index = Ny * pCollision->m_MapData.m_Width + Nx;

  if (pCollision->m_pTileInfos[Index] & INFO_TILENEXT)
    return Index;
  else
    return -1;
}

inline bool check_point(SCollision *pCollision, vec2 Pos) {
  int Nx = (int)(Pos.x + 0.5f) >> 5;
  int Ny = (int)(Pos.y + 0.5f) >> 5;
  unsigned char Idx = pCollision->m_MapData.m_GameLayer
                          .m_pData[Ny * pCollision->m_MapData.m_Width + Nx];
  return Idx == TILE_SOLID || Idx == TILE_NOHOOK;
}

static inline bool check_point_idx(SCollision *pCollision, int Idx) {
  unsigned char Tile = pCollision->m_MapData.m_GameLayer.m_pData[Idx];
  return Tile == TILE_SOLID || Tile == TILE_NOHOOK;
}

void ThroughOffset(vec2 Pos0, vec2 Pos1, int *restrict pOffsetX,
                   int *restrict pOffsetY) {
  float x = Pos0.x - Pos1.x;
  float y = Pos0.y - Pos1.y;
  if (fabs(x) > fabs(y)) {
    if (x < 0) {
      *pOffsetX = -32;
      *pOffsetY = 0;
    } else {
      *pOffsetX = 32;
      *pOffsetY = 0;
    }
  } else {
    if (y < 0) {
      *pOffsetX = 0;
      *pOffsetY = -32;
    } else {
      *pOffsetX = 0;
      *pOffsetY = 32;
    }
  }
}

bool is_through(SCollision *pCollision, int x, int y, int OffsetX, int OffsetY,
                vec2 Pos0, vec2 Pos1) {
  int pos = get_pure_map_index(pCollision, vec2_init(x, y));

  unsigned char *pFrontIdx = pCollision->m_MapData.m_FrontLayer.m_pData;
  unsigned char *pFrontFlgs = pCollision->m_MapData.m_FrontLayer.m_pFlags;
  if (pFrontIdx && (pFrontIdx[pos] == TILE_THROUGH_ALL ||
                    pFrontIdx[pos] == TILE_THROUGH_CUT))
    return true;
  if (pFrontIdx && pFrontIdx[pos] == TILE_THROUGH_DIR &&
      ((pFrontFlgs[pos] == ROTATION_0 && Pos0.y > Pos1.y) ||
       (pFrontFlgs[pos] == ROTATION_90 && Pos0.x < Pos1.x) ||
       (pFrontFlgs[pos] == ROTATION_180 && Pos0.y < Pos1.y) ||
       (pFrontFlgs[pos] == ROTATION_270 && Pos0.x > Pos1.x)))
    return true;
  int offpos =
      get_pure_map_index(pCollision, vec2_init(x + OffsetX, y + OffsetY));
  unsigned char *pTileIdx = pCollision->m_MapData.m_GameLayer.m_pData;
  return pTileIdx[offpos] == TILE_THROUGH ||
         (pFrontIdx && pFrontIdx[offpos] == TILE_THROUGH);
}

bool is_hook_blocker(SCollision *pCollision, int x, int y, vec2 Pos0,
                     vec2 Pos1) {
  int pos = get_pure_map_index(pCollision, vec2_init(x, y));
  unsigned char *pTileIdx = pCollision->m_MapData.m_GameLayer.m_pData;
  unsigned char *pTileFlgs = pCollision->m_MapData.m_GameLayer.m_pData;

  unsigned char *pFrontIdx = pCollision->m_MapData.m_FrontLayer.m_pData;
  unsigned char *pFrontFlgs = pCollision->m_MapData.m_FrontLayer.m_pFlags;
  if (pTileIdx[pos] == TILE_THROUGH_ALL ||
      (pFrontIdx && pFrontIdx[pos] == TILE_THROUGH_ALL))
    return true;
  if (pTileIdx[pos] == TILE_THROUGH_DIR &&
      ((pTileFlgs[pos] == ROTATION_0 && Pos0.y < Pos1.y) ||
       (pTileFlgs[pos] == ROTATION_90 && Pos0.x > Pos1.x) ||
       (pTileFlgs[pos] == ROTATION_180 && Pos0.y > Pos1.y) ||
       (pTileFlgs[pos] == ROTATION_270 && Pos0.x < Pos1.x)))
    return true;
  if (pFrontIdx && pFrontIdx[pos] == TILE_THROUGH_DIR &&
      ((pFrontFlgs[pos] == ROTATION_0 && Pos0.y < Pos1.y) ||
       (pFrontFlgs[pos] == ROTATION_90 && Pos0.x > Pos1.x) ||
       (pFrontFlgs[pos] == ROTATION_180 && Pos0.y > Pos1.y) ||
       (pFrontFlgs[pos] == ROTATION_270 && Pos0.x < Pos1.x)))
    return true;
  return false;
}

static inline bool broad_check_char(SCollision *restrict pCollision, vec2 Start,
                                    vec2 End) {
  const int MinX = (int)(fmin(Start.x, End.x) - HALFPHYSICALSIZE - 1) >> 5;
  const int MinY = (int)(fmin(Start.y, End.y) - HALFPHYSICALSIZE - 1) >> 5;
  const int MaxX = (int)(fmax(Start.x, End.x) + HALFPHYSICALSIZE + 1) >> 5;
  const int MaxY = (int)(fmax(Start.y, End.y) + HALFPHYSICALSIZE + 1) >> 5;
  for (int y = MinY; y <= MaxY; ++y) {
    for (int x = MinX; x <= MaxX; ++x) {
      if (pCollision->m_pTileInfos[y * pCollision->m_MapData.m_Width + x] &
          INFO_ISSOLID)
        return true;
    }
  }
  return false;
}

static inline bool broad_check(SCollision *restrict pCollision, vec2 Start,
                               vec2 End) {
  const int MinX = (int)fmin(Start.x, End.x) >> 5;
  const int MinY = (int)fmin(Start.y, End.y) >> 5;
  const int MaxX = (int)fmax(Start.x, End.x) >> 5;
  const int MaxY = (int)fmax(Start.y, End.y) >> 5;
  for (int y = MinY; y <= MaxY; ++y) {
    for (int x = MinX; x <= MaxX; ++x) {
      if (pCollision->m_pTileInfos[y * pCollision->m_MapData.m_Width + x] &
          INFO_ISSOLID)
        return true;
    }
  }
  return false;
}

unsigned char intersect_line_tele_hook(SCollision *restrict pCollision,
                                       vec2 Pos0, vec2 Pos1,
                                       vec2 *restrict pOutCollision,
                                       int *restrict pTeleNr,
                                       bool OldTeleHook) {
  // NOTE:another only 10ms save because the hook does many intersect lines
  // which make the broad check overlap. still better than nothing
  if (!broad_check(pCollision, Pos0, Pos1)) {
    if (pOutCollision)
      *pOutCollision = Pos1;
    return 0;
  }
  const int End = vdistance(Pos0, Pos1) + 1;
  int dx = 0, dy = 0;
  ThroughOffset(Pos0, Pos1, &dx, &dy);
  int LastIndex = -1;
  const float Step = 1.f / End;
  const int Width = pCollision->m_MapData.m_Width;
  vec2 Pos;
  int ix;
  int iy;
  int Index;
  for (float a = 0; a <= 1.f; a += Step) {
    Pos = vvfmix(Pos0, Pos1, a);
    ix = (int)(Pos.x + 0.5f) >> 5;
    iy = (int)(Pos.y + 0.5f) >> 5;
    Index = iy * Width + ix;

    // behind this is basically useless to optimize
    if (Index == LastIndex)
      continue;
    LastIndex = Index;
    if (pTeleNr) {
      if (OldTeleHook)
        *pTeleNr = is_teleport(pCollision, Index);
      else
        *pTeleNr = is_teleport_hook(pCollision, Index);
      if (*pTeleNr) {
        if (pOutCollision)
          *pOutCollision = Pos;
        return TILE_TELEINHOOK;
      }
    }

    if (check_point_idx(pCollision, Index)) {
      if (!is_through(pCollision, ix, iy, dx, dy, Pos0, Pos1)) {
        if (pOutCollision)
          *pOutCollision = Pos;

        int Idx = pCollision->m_MapData.m_GameLayer.m_pData[Index];
        if (Idx >= TILE_SOLID && Idx <= TILE_NOLASER)
          return Idx;
        return 0;
      }
    } else if (is_hook_blocker(pCollision, ix, iy, Pos0, Pos1)) {
      if (pOutCollision)
        *pOutCollision = Pos;
      return TILE_NOHOOK;
    }
  }
  if (pOutCollision)
    *pOutCollision = Pos1;
  return 0;
}

bool test_box(SCollision *pCollision, vec2 Pos, vec2 Size) {
  Size = vfmul(Size, 0.5f);
  if (check_point(pCollision, vec2_init(Pos.x - Size.x, Pos.y - Size.y)))
    return true;
  if (check_point(pCollision, vec2_init(Pos.x + Size.x, Pos.y - Size.y)))
    return true;
  if (check_point(pCollision, vec2_init(Pos.x - Size.x, Pos.y + Size.y)))
    return true;
  if (check_point(pCollision, vec2_init(Pos.x + Size.x, Pos.y + Size.y)))
    return true;
  return false;
}

unsigned char is_tune(SCollision *pCollision, int Index) {
  if (!pCollision->m_MapData.m_TuneLayer.m_pType)
    return 0;
  if (pCollision->m_MapData.m_TuneLayer.m_pType[Index])
    return pCollision->m_MapData.m_TuneLayer.m_pNumber[Index];
  return 0;
}

bool is_speedup(SCollision *pCollision, int Index) {
  return pCollision->m_MapData.m_SpeedupLayer.m_pType &&
         pCollision->m_MapData.m_SpeedupLayer.m_pForce[Index] > 0;
}

void get_speedup(SCollision *restrict pCollision, int Index,
                 vec2 *restrict pDir, int *restrict pForce,
                 int *restrict pMaxSpeed, int *restrict pType) {
  float Angle =
      pCollision->m_MapData.m_SpeedupLayer.m_pAngle[Index] * (PI / 180.0f);
  *pForce = pCollision->m_MapData.m_SpeedupLayer.m_pForce[Index];
  *pType = pCollision->m_MapData.m_SpeedupLayer.m_pType[Index];
  *pDir = vdirection(Angle);
  if (pMaxSpeed)
    *pMaxSpeed = pCollision->m_MapData.m_SpeedupLayer.m_pMaxSpeed[Index];
}

const vec2 *spawn_points(SCollision *restrict pCollision,
                         int *restrict pOutNum) {
  *pOutNum = pCollision->m_NumSpawnPoints;
  return pCollision->m_pSpawnPoints;
}

const vec2 *tele_outs(SCollision *restrict pCollision, int Number,
                      int *restrict pOutNum) {
  *pOutNum = pCollision->m_aNumTeleOuts[Number];
  return pCollision->m_apTeleOuts[Number];
}
const vec2 *tele_check_outs(SCollision *restrict pCollision, int Number,
                            int *restrict pOutNum) {
  *pOutNum = pCollision->m_aNumTeleCheckOuts[Number];
  return pCollision->m_apTeleCheckOuts[Number];
}

unsigned char intersect_line(SCollision *restrict pCollision, vec2 Pos0,
                             vec2 Pos1, vec2 *restrict pOutCollision,
                             vec2 *restrict pOutBeforeCollision) {
  float Distance = vdistance(Pos0, Pos1);
  int End = Distance + 1;
  vec2 Last = Pos0;
  for (int i = 0; i <= End; i++) {
    float a = i / (float)End;
    vec2 Pos = vvfmix(Pos0, Pos1, a);
    // Temporary position for checking collision
    int ix = (int)(Pos.x + 0.5f);
    int iy = (int)(Pos.y + 0.5f);

    if (check_point(pCollision, vec2_init(ix, iy))) {
      if (pOutCollision)
        *pOutCollision = Pos;
      if (pOutBeforeCollision)
        *pOutBeforeCollision = Last;
      return get_collision_at(pCollision, ix, iy);
    }

    Last = Pos;
  }
  if (pOutCollision)
    *pOutCollision = Pos1;
  if (pOutBeforeCollision)
    *pOutBeforeCollision = Pos1;
  return 0;
}

static inline bool check_point_int(SCollision *restrict pCollision, ivec2 Pos) {
  int Nx = Pos.x >> 5;
  int Ny = Pos.y >> 5;
  return pCollision->m_pTileInfos[Ny * pCollision->m_MapData.m_Width + Nx] &
         INFO_ISSOLID;
}

static inline bool test_box_character(SCollision *restrict pCollision,
                                      ivec2 Pos) {
  if (check_point_int(pCollision, (ivec2){Pos.x - HALFPHYSICALSIZE,
                                          Pos.y - HALFPHYSICALSIZE}))
    return true;
  if (check_point_int(pCollision, (ivec2){Pos.x + HALFPHYSICALSIZE,
                                          Pos.y - HALFPHYSICALSIZE}))
    return true;
  if (check_point_int(pCollision, (ivec2){Pos.x - HALFPHYSICALSIZE,
                                          Pos.y + HALFPHYSICALSIZE}))
    return true;
  if (check_point_int(pCollision, (ivec2){Pos.x + HALFPHYSICALSIZE,
                                          Pos.y + HALFPHYSICALSIZE}))
    return true;
  return false;
}

void move_box(SCollision *restrict pCollision, vec2 *restrict pInoutPos,
              vec2 *restrict pInoutVel, vec2 Elasticity,
              bool *restrict pGrounded) {
  vec2 Vel = *pInoutVel;
  float Distance = vlength(Vel);
  if (Distance <= 0.00001f) {
    // printf("dist <= 0.00001f movebox\n");
    return;
  }
  vec2 Pos = *pInoutPos;
  vec2 NewPos = vvadd(Pos, Vel);
  int Max = (int)Distance;
  float Fraction = 1.0f / (float)(Max + 1);
  if (!broad_check_char(pCollision, *pInoutPos, NewPos)) {
    // NOTE: this stupid fking loop is needed to simulate accumulated
    // fp-rounding errors
    // thats why this bs only saves 10ms instead of 100
    // if you can find a perfect rounding error approximation please submit a
    // pr
    for (int i = 0; i <= Max; i++)
      Pos = vvadd(Pos, vfmul(Vel, Fraction));
    *pInoutPos = Pos;
    return;
  }

  ivec2 IPos = (ivec2){(int)(Pos.x + 0.5f), (int)(Pos.y + 0.5f)};
  ivec2 INewPos;
  int Hits;
  for (int i = 0; i <= Max; i++) {
    NewPos = vvadd(Pos, vfmul(Vel, Fraction));
    INewPos = (ivec2){(int)(NewPos.x + 0.5f), (int)(NewPos.y + 0.5f)};
    if (test_box_character(pCollision, INewPos)) {
      Hits = 0;
      if (test_box_character(pCollision, (ivec2){IPos.x, INewPos.y})) {
        if (pGrounded && Elasticity.y > 0 && Vel.y > 0)
          *pGrounded = true;
        NewPos.y = Pos.y;
        Vel.y *= -Elasticity.y;
        Hits++;
      }
      if (test_box_character(pCollision, (ivec2){INewPos.x, IPos.y})) {
        NewPos.x = Pos.x;
        Vel.x *= -Elasticity.x;
        Hits++;
      }
      if (Hits == 0) {
        if (pGrounded && Elasticity.y > 0 && Vel.y > 0)
          *pGrounded = true;
        NewPos = Pos;
        Vel.x *= -Elasticity.x;
        Vel.y *= -Elasticity.y;
      }
    }
    IPos = INewPos;
    Pos = NewPos;
  }

  *pInoutPos = Pos;
  *pInoutVel = Vel;
}

bool get_nearest_air_pos_player(SCollision *pCollision, vec2 PlayerPos,
                                vec2 *pOutPos) {
  for (int dist = 5; dist >= -1; dist--) {
    *pOutPos = vec2_init(PlayerPos.x, PlayerPos.y - dist);
    if (!test_box(pCollision, *pOutPos, PHYSICALSIZEVEC)) {
      return true;
    }
  }
  return false;
}

bool get_nearest_air_pos(SCollision *pCollision, vec2 Pos, vec2 PrevPos,
                         vec2 *pOutPos) {
  for (int k = 0; k < 16 && check_point(pCollision, Pos); k++) {
    Pos = vvsub(Pos, vnormalize(vvsub(PrevPos, Pos)));
  }

  vec2 PosInBlock =
      vec2_init((int)(Pos.x + 0.5f) % 32, (int)(Pos.y + 0.5f) % 32);
  vec2 BlockCenter = vfadd(
      vvsub(vec2_init((int)(Pos.x + 0.5f), (int)(Pos.y + 0.5f)), PosInBlock),
      16.0f);

  *pOutPos =
      vec2_init(BlockCenter.x + (PosInBlock.x < 16 ? -2.0f : 1.0f), Pos.y);
  if (!test_box(pCollision, *pOutPos, PHYSICALSIZEVEC))
    return true;

  *pOutPos =
      vec2_init(Pos.x, BlockCenter.y + (PosInBlock.y < 16 ? -2.0f : 1.0f));
  if (!test_box(pCollision, *pOutPos, PHYSICALSIZEVEC))
    return true;

  *pOutPos = vec2_init(BlockCenter.x + (PosInBlock.x < 16 ? -2.0f : 1.0f),
                       BlockCenter.y + (PosInBlock.y < 16 ? -2.0f : 1.0f));
  return !test_box(pCollision, *pOutPos, PHYSICALSIZEVEC);
}

int get_index(SCollision *pCollision, vec2 PrevPos, vec2 Pos) {
  float Distance = vdistance(PrevPos, Pos);

  if (!Distance) {
    int Nx = (int)Pos.x >> 5;
    int Ny = (int)Pos.y >> 5;

    if (pCollision->m_MapData.m_TeleLayer.m_pType ||
        (pCollision->m_MapData.m_SpeedupLayer.m_pForce &&
         pCollision->m_MapData.m_SpeedupLayer
                 .m_pForce[Ny * pCollision->m_MapData.m_Width + Nx] > 0)) {
      return Ny * pCollision->m_MapData.m_Width + Nx;
    }
  }

  for (int i = 0, id = ceil(Distance); i < id; i++) {
    float a = (float)i / Distance;
    vec2 Tmp = vvfmix(PrevPos, Pos, a);
    int Nx = (int)Tmp.x >> 5;
    int Ny = (int)Tmp.y >> 5;
    if (pCollision->m_MapData.m_TeleLayer.m_pType ||
        (pCollision->m_MapData.m_SpeedupLayer.m_pForce &&
         pCollision->m_MapData.m_SpeedupLayer
                 .m_pForce[Ny * pCollision->m_MapData.m_Width + Nx] > 0)) {
      return Ny * pCollision->m_MapData.m_Width + Nx;
    }
  }

  return -1;
}

unsigned char mover_speed(SCollision *pCollision, int x, int y, vec2 *pSpeed) {
  int Nx = x >> 5;
  int Ny = y >> 5;
  int Index = pCollision->m_MapData.m_GameLayer
                  .m_pData[Ny * pCollision->m_MapData.m_Width + Nx];

  if (Index != TILE_CP && Index != TILE_CP_F) {
    return 0;
  }

  vec2 Target;
  switch (pCollision->m_MapData.m_GameLayer
              .m_pFlags[Ny * pCollision->m_MapData.m_Width + Nx]) {
  case ROTATION_0:
    Target.x = 0.0f;
    Target.y = -4.0f;
    break;
  case ROTATION_90:
    Target.x = 4.0f;
    Target.y = 0.0f;
    break;
  case ROTATION_180:
    Target.x = 0.0f;
    Target.y = 4.0f;
    break;
  case ROTATION_270:
    Target.x = -4.0f;
    Target.y = 0.0f;
    break;
  default:
    Target = vec2_init(0.0f, 0.0f);
    break;
  }
  if (Index == TILE_CP_F) {
    Target = vfmul(Target, 4.0f);
  }
  *pSpeed = Target;
  return Index;
}

int entity(SCollision *pCollision, int x, int y, int Layer) {
  if ((unsigned char)x >= pCollision->m_MapData.m_Width ||
      (unsigned char)y >= pCollision->m_MapData.m_Height)
    return 0;

  const int Index = y * pCollision->m_MapData.m_Width + x;
  switch (Layer) {
  case LAYER_GAME:
    return pCollision->m_MapData.m_GameLayer.m_pData[Index] - ENTITY_OFFSET;
  case LAYER_FRONT:
    return pCollision->m_MapData.m_FrontLayer.m_pData[Index] - ENTITY_OFFSET;
  case LAYER_SWITCH:
    return pCollision->m_MapData.m_SwitchLayer.m_pType[Index] - ENTITY_OFFSET;
  case LAYER_TELE:
    return pCollision->m_MapData.m_TeleLayer.m_pType[Index] - ENTITY_OFFSET;
  case LAYER_SPEEDUP:
    return pCollision->m_MapData.m_SpeedupLayer.m_pType[Index] - ENTITY_OFFSET;
  case LAYER_TUNE:
    return pCollision->m_MapData.m_TuneLayer.m_pType[Index] - ENTITY_OFFSET;
  default:
    printf("Error while initializing gameworld: invalid layer found\n");
  }
  return 0;
}
