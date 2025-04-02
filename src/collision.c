#include "collision.h"
#include "gamecore.h"
#include "limits.h"
#include "map_loader.h"
#include "vmath.h"
#include <float.h>
#include <immintrin.h>
#include <stdio.h>
#include <string.h>

int NumSkips = 0;
int NumIntersects = 0;

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

static void init_tuning_params(STuningParams *pTunings) {
#define MACRO_TUNING_PARAM(Name, Value) pTunings->m_##Name = Value;
#include "tuning.h"
#undef MACRO_TUNING_PARAM
}

static void init_distance_field(SCollision *pCollision) {
  int Width = pCollision->m_MapData.m_Width;
  int Height = pCollision->m_MapData.m_Height;
  unsigned char *pField = pCollision->m_pSolidDistanceField;
  unsigned char *pInfos = pCollision->m_pTileInfos;
  float *tmp = (float *)malloc(Width * Height * sizeof(float));

  for (int y = 0; y < Height; ++y) {
    for (int x = 0; x < Width; ++x) {
      int Idx = y * Width + x;
      if (pInfos[Idx] & INFO_ISSOLID) {
        pField[Idx] = 0;
        tmp[Idx] = 0.0f;
      } else {
        pField[Idx] = 255;
        tmp[Idx] = FLT_MAX;
      }
    }
  }

  for (int y = 0; y < Height; ++y) {
    for (int x = 0; x < Width; ++x) {
      int Idx = y * Width + x;
      if (tmp[Idx] == 0.0f)
        continue;
      float min = FLT_MAX;
      if (x > 0) {
        float dist = tmp[Idx - 1] + 1.0f;
        min = fminf(min, dist);
      }
      if (y > 0) {
        float dist = tmp[Idx - Width] + 1.0f;
        min = fminf(min, dist);
      }
      if (x > 0 && y > 0) {
        float dist = tmp[Idx - Width - 1] + sqrtf(2.0f);
        min = fminf(min, dist);
      }
      if (x < Width - 1 && y > 0) {
        float dist = tmp[Idx - Width + 1] + sqrtf(2.0f);
        min = fminf(min, dist);
      }
      tmp[Idx] = fminf(tmp[Idx], min);
    }
  }
  for (int y = Height - 1; y >= 0; --y) {
    for (int x = Width - 1; x >= 0; --x) {
      int Idx = y * Width + x;
      if (tmp[Idx] == 0.0f)
        continue;
      float min = FLT_MAX;
      if (x < Width - 1) {
        float dist = tmp[Idx + 1] + 1.0f;
        min = fminf(min, dist);
      }
      if (y < Height - 1) {
        float dist = tmp[Idx + Width] + 1.0f;
        min = fminf(min, dist);
      }
      if (x < Width - 1 && y < Height - 1) {
        float dist = tmp[Idx + Width + 1] + sqrtf(2.0f);
        min = fminf(min, dist);
      }
      if (x > 0 && y < Height - 1) {
        float dist = tmp[Idx + Width - 1] + sqrtf(2.0f);
        min = fminf(min, dist);
      }
      tmp[Idx] = fminf(tmp[Idx], min);
      float scaled = tmp[Idx] * 32.0f;
      if (scaled > 255.0f) {
        scaled = 255.0f;
      }
      pField[Idx] = (unsigned char)scaled;
    }
  }

  free(tmp);
}

bool init_collision(SCollision *restrict pCollision,
                    const char *restrict pMap) {
  pCollision->m_MapData = load_map(pMap);
  if (!pCollision->m_MapData.m_GameLayer.m_pData)
    return false;

  SMapData *pMapData = &pCollision->m_MapData;
  int Width = pMapData->m_Width;
  int Height = pMapData->m_Height;

  pCollision->m_pSolidDistanceField = calloc(Width * Height, 1);
  pCollision->m_pTileInfos = calloc(Width * Height, 1);
  pCollision->m_pMoveRestrictions = calloc(Width * Height, 5);
  pCollision->m_pPickups = calloc(Width * Height, sizeof(SPickup));
  pCollision->m_MoveRestrictionsFound = false;

  for (int i = 0; i < 256; ++i)
    init_tuning_params(&pCollision->m_aTuningList[i]);
  // Figure out important things
  // Make lists of spawn points, tele outs and tele checkpoints outs
  for (int i = 0; i < Width * Height; ++i) {
    if (tile_exists(pCollision, i))
      pCollision->m_pTileInfos[i] |= INFO_TILENEXT;
    int Tile = pCollision->m_MapData.m_GameLayer.m_pData[i];
    if (Tile == TILE_SOLID || Tile == TILE_NOHOOK)
      pCollision->m_pTileInfos[i] |= INFO_ISSOLID;

    if (Tile >= 192 && Tile <= 194)
      ++pCollision->m_NumSpawnPoints;
    if (pMapData->m_TeleLayer.m_pType) {
      if (pMapData->m_TeleLayer.m_pType[i] == TILE_TELEOUT)
        ++pCollision->m_aNumTeleOuts[pMapData->m_TeleLayer.m_pNumber[i]];
      if (pMapData->m_TeleLayer.m_pType[i] == TILE_TELECHECKOUT)
        ++pCollision->m_aNumTeleCheckOuts[pMapData->m_TeleLayer.m_pNumber[i]];
    }

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

    pCollision->m_pPickups[i].m_Type = -1;
    int EntIdx = pMapData->m_GameLayer.m_pData[i] - ENTITY_OFFSET;
    if ((EntIdx >= ENTITY_ARMOR_SHOTGUN && EntIdx <= ENTITY_ARMOR_LASER) ||
        (EntIdx >= ENTITY_ARMOR_1 && EntIdx <= ENTITY_WEAPON_LASER)) {
      int Type = -1;
      int SubType = 0;
      if (EntIdx == ENTITY_ARMOR_1)
        Type = POWERUP_ARMOR;
      else if (EntIdx == ENTITY_ARMOR_SHOTGUN)
        Type = POWERUP_ARMOR_SHOTGUN;
      else if (EntIdx == ENTITY_ARMOR_GRENADE)
        Type = POWERUP_ARMOR_GRENADE;
      else if (EntIdx == ENTITY_ARMOR_NINJA)
        Type = POWERUP_ARMOR_NINJA;
      else if (EntIdx == ENTITY_ARMOR_LASER)
        Type = POWERUP_ARMOR_LASER;
      else if (EntIdx == ENTITY_HEALTH_1)
        Type = POWERUP_HEALTH;
      else if (EntIdx == ENTITY_WEAPON_SHOTGUN) {
        Type = POWERUP_WEAPON;
        SubType = WEAPON_SHOTGUN;
      } else if (EntIdx == ENTITY_WEAPON_GRENADE) {
        Type = POWERUP_WEAPON;
        SubType = WEAPON_GRENADE;
      } else if (EntIdx == ENTITY_WEAPON_LASER) {
        Type = POWERUP_WEAPON;
        SubType = WEAPON_LASER;
      } else if (EntIdx == ENTITY_POWERUP_NINJA) {
        Type = POWERUP_NINJA;
        SubType = WEAPON_NINJA;
      }
      pCollision->m_pPickups[i].m_Type = Type;
      pCollision->m_pPickups[i].m_Subtype = SubType;
      if (pCollision->m_MapData.m_SwitchLayer.m_pType) {
        const int SwitchType = pCollision->m_MapData.m_SwitchLayer.m_pType[i];
        if (SwitchType) {
          pCollision->m_pPickups[i].m_Type = SwitchType;
          pCollision->m_pPickups[i].m_Number =
              pCollision->m_MapData.m_SwitchLayer.m_pNumber[i];
        }
      }
    }
  }

  init_distance_field(pCollision);

  if (pCollision->m_NumSpawnPoints > 0)
    pCollision->m_pSpawnPoints =
        malloc(pCollision->m_NumSpawnPoints * sizeof(vec2));
  if (pMapData->m_TeleLayer.m_pType) {
    for (int i = 0; i < 256; ++i) {
      if (pCollision->m_aNumTeleOuts[i] > 0)
        pCollision->m_apTeleOuts[i] =
            malloc(pCollision->m_aNumTeleOuts[i] * sizeof(vec2));
      if (pCollision->m_aNumTeleCheckOuts[i] > 0)
        pCollision->m_apTeleCheckOuts[i] =
            malloc(pCollision->m_aNumTeleCheckOuts[i] * sizeof(vec2));
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
  free(pCollision->m_pPickups);
  free(pCollision->m_pMoveRestrictions);
  free(pCollision->m_pSolidDistanceField);
  if (pCollision->m_NumSpawnPoints)
    free(pCollision->m_pSpawnPoints);
  if (pCollision->m_pTileInfos)
    free(pCollision->m_pTileInfos);
  if (pCollision->m_MapData.m_TeleLayer.m_pType)
    for (int i = 0; i < 256; ++i) {
      free(pCollision->m_apTeleOuts[i]);
      free(pCollision->m_apTeleCheckOuts[i]);
    }
  memset(pCollision, 0, sizeof(SCollision));

  if (NumSkips)
    printf("Skips:%d, Intersects:%d, Ratio:%f\n", NumSkips, NumIntersects,
           (float)NumIntersects / (float)NumSkips);
}

inline int get_pure_map_index(SCollision *pCollision, vec2 Pos) {
  const int nx = (int)(vgetx(Pos) + 0.5f) >> 5;
  const int ny = (int)(vgety(Pos) + 0.5f) >> 5;
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

inline unsigned char get_collision_at(SCollision *pCollision, vec2 Pos) {
  int Nx = (int)vgetx(Pos) >> 5;
  int Ny = (int)vgety(Pos) >> 5;
  int pos = Ny * pCollision->m_MapData.m_Width + Nx;
  int Idx = pCollision->m_MapData.m_GameLayer.m_pData[pos];
  if (Idx >= TILE_SOLID && Idx <= TILE_NOLASER)
    return Idx;
  return 0;
}
static inline unsigned char get_collision_at_idx(SCollision *pCollision,
                                                 int Idx) {
  const int Tile = pCollision->m_MapData.m_GameLayer.m_pData[Idx];
  if (Tile >= TILE_SOLID && Tile <= TILE_NOLASER)
    return Idx;
  return 0;
}

inline unsigned char get_front_collision_at(SCollision *pCollision, vec2 Pos) {
  int Nx = (int)vgetx(Pos) >> 5;
  int Ny = (int)vgety(Pos) >> 5;
  int pos = Ny * pCollision->m_MapData.m_Width + Nx;
  int Idx = pCollision->m_MapData.m_FrontLayer.m_pData[pos];
  if (Idx >= TILE_SOLID && Idx <= TILE_NOLASER)
    return Idx;
  return 0;
}

inline unsigned char get_move_restrictions(SCollision *restrict pCollision,
                                           void *restrict pUser, vec2 Pos,
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
  int Nx = (int)vgetx(Pos) >> 5;
  int Ny = (int)vgety(Pos) >> 5;
  int Index = Ny * pCollision->m_MapData.m_Width + Nx;

  if (pCollision->m_pTileInfos[Index] & INFO_TILENEXT)
    return Index;
  else
    return -1;
}

inline bool check_point(SCollision *pCollision, vec2 Pos) {
  int Nx = (int)(vgetx(Pos) + 0.5f) >> 5;
  int Ny = (int)(vgety(Pos) + 0.5f) >> 5;
  return pCollision->m_pTileInfos[Ny * pCollision->m_MapData.m_Width + Nx] &
         INFO_ISSOLID;
}

static inline bool check_point_idx(SCollision *pCollision, int Idx) {
  return pCollision->m_pTileInfos[Idx] & INFO_ISSOLID;
}

void ThroughOffset(vec2 Pos0, vec2 Pos1, int *restrict pOffsetX,
                   int *restrict pOffsetY) {
  float x = vgetx(Pos0) - vgetx(Pos1);
  float y = vgety(Pos0) - vgety(Pos1);
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
      ((pFrontFlgs[pos] == ROTATION_0 && vgety(Pos0) > vgety(Pos1)) ||
       (pFrontFlgs[pos] == ROTATION_90 && vgetx(Pos0) < vgetx(Pos1)) ||
       (pFrontFlgs[pos] == ROTATION_180 && vgety(Pos0) < vgety(Pos1)) ||
       (pFrontFlgs[pos] == ROTATION_270 && vgetx(Pos0) > vgetx(Pos1))))
    return true;
  int offpos =
      get_pure_map_index(pCollision, vec2_init(x + OffsetX, y + OffsetY));
  unsigned char *pTileIdx = pCollision->m_MapData.m_GameLayer.m_pData;
  if (offpos < 0)
    return false;
  return pTileIdx[offpos] == TILE_THROUGH ||
         (pFrontIdx && pFrontIdx[offpos] == TILE_THROUGH);
}

bool is_hook_blocker(SCollision *pCollision, int Index, vec2 Pos0, vec2 Pos1) {
  unsigned char *pTileIdx = pCollision->m_MapData.m_GameLayer.m_pData;
  unsigned char *pTileFlgs = pCollision->m_MapData.m_GameLayer.m_pFlags;

  unsigned char *pFrontIdx = pCollision->m_MapData.m_FrontLayer.m_pData;
  unsigned char *pFrontFlgs = pCollision->m_MapData.m_FrontLayer.m_pFlags;
  if (pTileIdx[Index] == TILE_THROUGH_ALL ||
      (pFrontIdx && pFrontIdx[Index] == TILE_THROUGH_ALL))
    return true;
  if (pTileIdx[Index] == TILE_THROUGH_DIR &&
      ((pTileFlgs[Index] == ROTATION_0 && vgety(Pos0) < vgety(Pos1)) ||
       (pTileFlgs[Index] == ROTATION_90 && vgetx(Pos0) > vgetx(Pos1)) ||
       (pTileFlgs[Index] == ROTATION_180 && vgety(Pos0) > vgety(Pos1)) ||
       (pTileFlgs[Index] == ROTATION_270 && vgetx(Pos0) < vgetx(Pos1))))
    return true;
  if (pFrontIdx && pFrontIdx[Index] == TILE_THROUGH_DIR &&
      ((pFrontFlgs[Index] == ROTATION_0 && vgety(Pos0) < vgety(Pos1)) ||
       (pFrontFlgs[Index] == ROTATION_90 && vgetx(Pos0) > vgetx(Pos1)) ||
       (pFrontFlgs[Index] == ROTATION_180 && vgety(Pos0) > vgety(Pos1)) ||
       (pFrontFlgs[Index] == ROTATION_270 && vgetx(Pos0) < vgetx(Pos1))))
    return true;
  return false;
}

static inline bool broad_check_char(SCollision *restrict pCollision, vec2 Start,
                                    vec2 End) {
  const float StartX = vgetx(Start);
  const float StartY = vgety(Start);
  const float EndX = vgetx(End);
  const float EndY = vgety(End);
  const int MinX = (int)(fmin(StartX, EndX) - HALFPHYSICALSIZE - 1) >> 5;
  const int MinY = (int)(fmin(StartY, EndY) - HALFPHYSICALSIZE - 1) >> 5;
  const int MaxX = (int)(fmax(StartX, EndX) + HALFPHYSICALSIZE + 1) >> 5;
  const int MaxY = (int)(fmax(StartY, EndY) + HALFPHYSICALSIZE + 1) >> 5;
  for (int y = MinY; y <= MaxY; ++y) {
    for (int x = MinX; x <= MaxX; ++x) {
      if (pCollision->m_pTileInfos[y * pCollision->m_MapData.m_Width + x] &
          INFO_ISSOLID)
        return true;
    }
  }
  return false;
}

static inline bool broad_check_tele(SCollision *restrict pCollision, vec2 Start,
                                    vec2 End) {
  const float StartX = vgetx(Start);
  const float StartY = vgety(Start);
  const float EndX = vgetx(End);
  const float EndY = vgety(End);
  const int MinX = (int)fmin(StartX, EndX) >> 5;
  const int MinY = (int)fmin(StartY, EndY) >> 5;
  const int MaxX = (int)fmax(StartX, EndX) >> 5;
  const int MaxY = (int)fmax(StartY, EndY) >> 5;
  for (int y = MinY; y <= MaxY; ++y) {
    for (int x = MinX; x <= MaxX; ++x) {
      if (is_teleport_hook(pCollision, y * pCollision->m_MapData.m_Width + x))
        return true;
    }
  }
  return false;
}

static inline bool broad_check(SCollision *restrict pCollision, vec2 Start,
                               vec2 End) {
  const float StartX = vgetx(Start);
  const float StartY = vgety(Start);
  const float EndX = vgetx(End);
  const float EndY = vgety(End);
  const int MinX = (int)fmin(StartX, EndX) >> 5;
  const int MinY = (int)fmin(StartY, EndY) >> 5;
  const int MaxX = (int)fmax(StartX, EndX) >> 5;
  const int MaxY = (int)fmax(StartY, EndY) >> 5;
  for (int y = MinY; y <= MaxY; ++y) {
    for (int x = MinX; x <= MaxX; ++x) {
      if ((unsigned char)(pCollision->m_MapData.m_GameLayer
                              .m_pData[y * pCollision->m_MapData.m_Width + x] -
                          1) < TILE_CREDITS_1)
        return true;
    }
  }
  return false;
}

#if 0
// Original intersect code
unsigned char intersect_line_tele_hook(SCollision *restrict pCollision,
                                       vec2 Pos0, vec2 Pos1,
                                       vec2 *restrict pOutCollision,
                                       int *restrict pTeleNr) {
  float Distance = vdistance(Pos0, Pos1);
  int End = (Distance + 1);
  int dx = 0, dy = 0; // Offset for checking the "through" tile
  ThroughOffset(Pos0, Pos1, &dx, &dy);
  for (int i = 0; i <= End; i++) {
    float a = i / (float)End;
    vec2 Pos = vvfmix(Pos0, Pos1, a);
    // Temporary position for checking collision
    int ix = round_to_int(vgetx(Pos));
    int iy = round_to_int(vgety(Pos));

    int Index = get_pure_map_index(pCollision, Pos);
    if (pTeleNr) {
      *pTeleNr = is_teleport_hook(pCollision, Index);
    }
    if (pTeleNr && *pTeleNr) {
      if (pOutCollision)
        *pOutCollision = Pos;
      return TILE_TELEINHOOK;
    }

    int hit = 0;
    if (check_point_idx(pCollision, Index)) {
      if (!is_through(pCollision, ix, iy, dx, dy, Pos0, Pos1))
        hit = get_collision_at_idx(pCollision, Index);
    } else if (is_hook_blocker(pCollision, Index, Pos0, Pos1)) {
      hit = TILE_NOHOOK;
    }
    if (hit) {
      if (pOutCollision)
        *pOutCollision = Pos;
      return hit;
    }
  }
  if (pOutCollision)
    *pOutCollision = Pos1;
  return 0;
}
#else
unsigned char intersect_line_tele_hook(SCollision *restrict pCollision,
                                       vec2 Pos0, vec2 Pos1,
                                       vec2 *restrict pOutCollision,
                                       int *restrict pTeleNr) {
  const int End = vdistance(Pos0, Pos1) + 1;

  // SMH THIS CHECK CHANGES PHYSICS
/*   if (!broad_check(pCollision, Pos0, Pos1)) {
    if (pTeleNr) {
      if (!broad_check_tele(pCollision, Pos0, Pos1)) {
        *pOutCollision = Pos1;
        return 0;
      }
    } else {
      *pOutCollision = Pos1;
      ++NumSkips;
      return 0;
    }
  } */

  ++NumIntersects;
  const float fEnd = End;
  int dx = 0, dy = 0;
  ThroughOffset(Pos0, Pos1, &dx, &dy);
  int LastIndex = -1;
  const int Width = pCollision->m_MapData.m_Width;
  for (int i = 0; i <= End; i++) {
    float a = i / fEnd;
    vec2 Pos = vvfmix(Pos0, Pos1, a);
    int ix = (int)(vgetx(Pos) + 0.5);
    int iy = (int)(vgety(Pos) + 0.5);
    int Index = (iy >> 5) * Width + (ix >> 5);

    if (Index == LastIndex)
      continue;
    LastIndex = Index;
    if (pTeleNr) {
      *pTeleNr = is_teleport_hook(pCollision, Index);
      if (*pTeleNr) {
        if (pOutCollision)
          *pOutCollision = Pos;
        return TILE_TELEINHOOK;
      }
    }

    if (check_point_idx(pCollision, Index)) {
      if (!is_through(pCollision, ix, iy, dx, dy, Pos0, Pos1)) {
        *pOutCollision = Pos;

        int Idx = pCollision->m_MapData.m_GameLayer.m_pData[Index];
        if (Idx >= TILE_SOLID && Idx <= TILE_NOLASER)
          return Idx;
        return 0;
      }
    } else if (is_hook_blocker(pCollision, Index, Pos0, Pos1)) {
      *pOutCollision = Pos;
      return TILE_NOHOOK;
    }
  }
  *pOutCollision = Pos1;
  return 0;
}
#endif

bool test_box(SCollision *pCollision, vec2 Pos, vec2 Size) {
  float SizeX = vgetx(Size) * 0.5f;
  float SizeY = vgety(Size) * 0.5f;
  float PosX = vgetx(Pos);
  float PosY = vgety(Pos);
  if (check_point(pCollision, vec2_init(PosX - SizeX, PosY - SizeY)))
    return true;
  if (check_point(pCollision, vec2_init(PosX + SizeX, PosY - SizeY)))
    return true;
  if (check_point(pCollision, vec2_init(PosX - SizeX, PosY + SizeY)))
    return true;
  if (check_point(pCollision, vec2_init(PosX + SizeX, PosY + SizeY)))
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
  int LastIdx = -1;
  for (int i = 0; i <= End; i++) {
    float a = i / (float)End;
    vec2 Pos = vvfmix(Pos0, Pos1, a);
    int Nx = (int)(vgetx(Pos) + 0.5f) >> 5;
    int Ny = (int)(vgety(Pos) + 0.5f) >> 5;
    int Idx = Ny * pCollision->m_MapData.m_Width + Nx;
    if (LastIdx == Idx)
      continue;
    LastIdx = Idx;
    if (check_point_idx(pCollision, Idx)) {
      *pOutCollision = Pos;
      *pOutBeforeCollision = Last;
      return get_collision_at_idx(pCollision, Idx);
    }

    Last = Pos;
  }
  *pOutCollision = Pos1;
  *pOutBeforeCollision = Pos1;
  return 0;
}

static inline bool check_point_int(SCollision *restrict pCollision, int x,
                                   int y) {
  int Nx = x >> 5;
  int Ny = y >> 5;
  return pCollision->m_pTileInfos[Ny * pCollision->m_MapData.m_Width + Nx] &
         INFO_ISSOLID;
}

static inline bool test_box_character(SCollision *restrict pCollision, int x,
                                      int y) {
  if (check_point_int(pCollision, x - HALFPHYSICALSIZE, y - HALFPHYSICALSIZE))
    return true;
  if (check_point_int(pCollision, x + HALFPHYSICALSIZE, y - HALFPHYSICALSIZE))
    return true;
  if (check_point_int(pCollision, x - HALFPHYSICALSIZE, y + HALFPHYSICALSIZE))
    return true;
  if (check_point_int(pCollision, x + HALFPHYSICALSIZE, y + HALFPHYSICALSIZE))
    return true;
  return false;
}

void move_box(SCollision *restrict pCollision, vec2 *restrict pInoutPos,
              vec2 *restrict pInoutVel, vec2 Elasticity,
              bool *restrict pGrounded) {
  vec2 Vel = *pInoutVel;
  float Distance = vlength(Vel);
  if (Distance <= 0.00001f) {
    return;
  }
  vec2 Pos = *pInoutPos;
  vec2 NewPos = vvadd(Pos, Vel);
  int Max = (int)Distance;
  float Fraction = 1.0f / (float)(Max + 1);
  if (!broad_check_char(pCollision, *pInoutPos, NewPos)) {
    const vec2 Frac = vfmul(Vel, Fraction);
    __m256 accum = _mm256_setr_ps(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                  vgety(Pos), vgetx(Pos));
    __m256 frac = _mm256_setr_ps(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                 vgety(Frac), vgetx(Frac));
    for (int i = 0; i <= Max; i++)
      accum = _mm256_add_ps(accum, frac);
    float result[8];
    _mm256_storeu_ps(result, accum);
    *pInoutPos = vec2_init(result[7], result[6]);
    return;
  }

  ivec2 IPos = (ivec2){(int)(vgetx(Pos) + 0.5f), (int)(vgety(Pos) + 0.5f)};
  ivec2 INewPos;
  int Hits;
  for (int i = 0; i <= Max; i++) {
    NewPos = vvadd(Pos, vfmul(Vel, Fraction));
    INewPos = (ivec2){(int)(vgetx(NewPos) + 0.5f), (int)(vgety(NewPos) + 0.5f)};
    if (test_box_character(pCollision, INewPos.x, INewPos.y)) {
      Hits = 0;
      if (test_box_character(pCollision, IPos.x, INewPos.y)) {
        if (pGrounded && vgety(Elasticity) > 0 && vgety(Vel) > 0)
          *pGrounded = true;
        NewPos = vsety(NewPos, vgety(Pos));
        Vel = vsety(Vel, -vgety(Vel) * vgety(Elasticity));
        Hits++;
      }
      if (test_box_character(pCollision, INewPos.x, IPos.y)) {
        NewPos = vsetx(NewPos, vgetx(Pos));
        Vel = vsetx(Vel, -vgetx(Vel) * vgetx(Elasticity));
        Hits++;
      }
      if (Hits == 0) {
        if (pGrounded && vgety(Elasticity) > 0 && vgety(Vel) > 0)
          *pGrounded = true;
        NewPos = Pos;
        Vel = vfmul(Vel, -1.0f);
        Vel = vvmul(Vel, Elasticity);
      }
    }
    IPos = INewPos;
    Pos = NewPos;
  }

  *pInoutPos = Pos;
  *pInoutVel = Vel;
}

bool get_nearest_air_pos_player(SCollision *restrict pCollision, vec2 PlayerPos,
                                vec2 *restrict pOutPos) {
  for (int dist = 5; dist >= -1; dist--) {
    *pOutPos = vec2_init(vgetx(PlayerPos), vgety(PlayerPos) - dist);
    if (!test_box(pCollision, *pOutPos, PHYSICALSIZEVEC)) {
      return true;
    }
  }
  return false;
}

bool get_nearest_air_pos(SCollision *restrict pCollision, vec2 Pos,
                         vec2 PrevPos, vec2 *restrict pOutPos) {
  for (int k = 0; k < 16 && check_point(pCollision, Pos); k++) {
    Pos = vvsub(Pos, vnormalize(vvsub(PrevPos, Pos)));
  }

  vec2 PosInBlock =
      vec2_init((int)(vgetx(Pos) + 0.5f) % 32, (int)(vgety(Pos) + 0.5f) % 32);
  vec2 BlockCenter =
      vfadd(vvsub(vec2_init((int)(vgetx(Pos) + 0.5f), (int)(vgety(Pos) + 0.5f)),
                  PosInBlock),
            16.0f);

  *pOutPos = vec2_init(
      vgetx(BlockCenter) + (vgetx(PosInBlock) < 16 ? -2.0f : 1.0f), vgety(Pos));
  if (!test_box(pCollision, *pOutPos, PHYSICALSIZEVEC))
    return true;

  *pOutPos = vec2_init(vgetx(Pos), vgety(BlockCenter) +
                                       (vgety(PosInBlock) < 16 ? -2.0f : 1.0f));
  if (!test_box(pCollision, *pOutPos, PHYSICALSIZEVEC))
    return true;

  *pOutPos =
      vec2_init(vgetx(BlockCenter) + (vgetx(PosInBlock) < 16 ? -2.0f : 1.0f),
                vgety(BlockCenter) + (vgety(PosInBlock) < 16 ? -2.0f : 1.0f));
  return !test_box(pCollision, *pOutPos, PHYSICALSIZEVEC);
}

int get_index(SCollision *pCollision, vec2 PrevPos, vec2 Pos) {
  float Distance = vdistance(PrevPos, Pos);

  if (!Distance) {
    int Nx = (int)vgetx(Pos) >> 5;
    int Ny = (int)vgety(Pos) >> 5;

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
    int Nx = (int)vgetx(Tmp) >> 5;
    int Ny = (int)vgety(Tmp) >> 5;
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
    Target = vec2_init(0.0f, -4.0f);
    break;
  case ROTATION_90:
    Target = vec2_init(4.0f, 0.0f);
    break;
  case ROTATION_180:
    Target = vec2_init(0.0f, 4.0f);
    break;
  case ROTATION_270:
    Target = vec2_init(-4.0f, 0.0f);
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
