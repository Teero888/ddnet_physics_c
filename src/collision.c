#include "../include/collision.h"
#include "../include/gamecore.h"
#include "../include/vmath.h"
#include "collision_tables.h"
#include "limits.h"
#include <assert.h>
#include <ddnet_map_loader.h>
#include <float.h>
#include <immintrin.h>
#include <mm_malloc.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum { MR_DIR_HERE = 0, MR_DIR_RIGHT, MR_DIR_DOWN, MR_DIR_LEFT, MR_DIR_UP, NUM_MR_DIRS };

static bool tile_exists_next(SCollision *pCollision, int Index) {
  const unsigned char *pTileIdx = pCollision->m_MapData.game_layer.data;
  const unsigned char *pTileFlgs = pCollision->m_MapData.game_layer.flags;
  const unsigned char *pFrontIdx = pCollision->m_MapData.front_layer.data;
  const unsigned char *pFrontFlgs = pCollision->m_MapData.front_layer.flags;
  const unsigned char *pDoorIdx = pCollision->m_MapData.door_layer.index;
  const unsigned char *pDoorFlgs = pCollision->m_MapData.door_layer.flags;
  int TileOnTheLeft = (Index - 1 > 0) ? Index - 1 : Index;
  int TileOnTheRight = (Index + 1 < pCollision->m_MapData.width * pCollision->m_MapData.height) ? Index + 1 : Index;
  int TileBelow = (Index + pCollision->m_MapData.width < pCollision->m_MapData.width * pCollision->m_MapData.height)
                      ? Index + pCollision->m_MapData.width
                      : Index;
  int TileAbove = (Index - pCollision->m_MapData.width > 0) ? Index - pCollision->m_MapData.width : Index;

  if ((pTileIdx[TileOnTheRight] == TILE_STOP && pTileFlgs[TileOnTheRight] == ROTATION_270) ||
      (pTileIdx[TileOnTheLeft] == TILE_STOP && pTileFlgs[TileOnTheLeft] == ROTATION_90))
    return true;
  if ((pTileIdx[TileBelow] == TILE_STOP && pTileFlgs[TileBelow] == ROTATION_0) ||
      (pTileIdx[TileAbove] == TILE_STOP && pTileFlgs[TileAbove] == ROTATION_180))
    return true;
  if (pTileIdx[TileOnTheRight] == TILE_STOPA || pTileIdx[TileOnTheLeft] == TILE_STOPA ||
      ((pTileIdx[TileOnTheRight] == TILE_STOPS || pTileIdx[TileOnTheLeft] == TILE_STOPS)))
    return true;
  if (pTileIdx[TileBelow] == TILE_STOPA || pTileIdx[TileAbove] == TILE_STOPA ||
      ((pTileIdx[TileBelow] == TILE_STOPS || pTileIdx[TileAbove] == TILE_STOPS) && pTileFlgs[TileBelow] | ROTATION_180 | ROTATION_0))
    return true;

  if (pFrontIdx) {
    if (pFrontIdx[TileOnTheRight] == TILE_STOPA || pFrontIdx[TileOnTheLeft] == TILE_STOPA ||
        ((pFrontIdx[TileOnTheRight] == TILE_STOPS || pFrontIdx[TileOnTheLeft] == TILE_STOPS)))
      return true;
    if (pFrontIdx[TileBelow] == TILE_STOPA || pFrontIdx[TileAbove] == TILE_STOPA ||
        ((pFrontIdx[TileBelow] == TILE_STOPS || pFrontIdx[TileAbove] == TILE_STOPS) && pFrontFlgs[TileBelow] | ROTATION_180 | ROTATION_0))
      return true;
    if ((pFrontIdx[TileOnTheRight] == TILE_STOP && pFrontFlgs[TileOnTheRight] == ROTATION_270) ||
        (pFrontIdx[TileOnTheLeft] == TILE_STOP && pFrontFlgs[TileOnTheLeft] == ROTATION_90))
      return true;
    if ((pFrontIdx[TileBelow] == TILE_STOP && pFrontFlgs[TileBelow] == ROTATION_0) ||
        (pFrontIdx[TileAbove] == TILE_STOP && pFrontFlgs[TileAbove] == ROTATION_180))
      return true;
  }
  if (pDoorIdx) {
    if (pDoorIdx[TileOnTheRight] == TILE_STOPA || pDoorIdx[TileOnTheLeft] == TILE_STOPA ||
        ((pDoorIdx[TileOnTheRight] == TILE_STOPS || pDoorIdx[TileOnTheLeft] == TILE_STOPS)))
      return true;
    if (pDoorIdx[TileBelow] == TILE_STOPA || pDoorIdx[TileAbove] == TILE_STOPA ||
        ((pDoorIdx[TileBelow] == TILE_STOPS || pDoorIdx[TileAbove] == TILE_STOPS) && pDoorFlgs[TileBelow] | ROTATION_180 | ROTATION_0))
      return true;
    if ((pDoorIdx[TileOnTheRight] == TILE_STOP && pDoorFlgs[TileOnTheRight] == ROTATION_270) ||
        (pDoorIdx[TileOnTheLeft] == TILE_STOP && pDoorFlgs[TileOnTheLeft] == ROTATION_90))
      return true;
    if ((pDoorIdx[TileBelow] == TILE_STOP && pDoorFlgs[TileBelow] == ROTATION_0) ||
        (pDoorIdx[TileAbove] == TILE_STOP && pDoorFlgs[TileAbove] == ROTATION_180))
      return true;
  }
  return false;
}

static bool tile_exists(SCollision *pCollision, int Index) {
  const unsigned char *pTiles = pCollision->m_MapData.game_layer.data;
  const unsigned char *pFront = pCollision->m_MapData.front_layer.data;
  const unsigned char *pDoor = pCollision->m_MapData.door_layer.index;
  const unsigned char *pTele = pCollision->m_MapData.tele_layer.type;
  const unsigned char *pSpeedup = pCollision->m_MapData.speedup_layer.force;
  const unsigned char *pSwitch = pCollision->m_MapData.switch_layer.type;
  const unsigned char *pTune = pCollision->m_MapData.tune_layer.type;

  if ((pTiles[Index] >= TILE_FREEZE && pTiles[Index] <= TILE_TELE_LASER_DISABLE) ||
      (pTiles[Index] >= TILE_LFREEZE && pTiles[Index] <= TILE_LUNFREEZE))
    return true;
  if (pFront && ((pFront[Index] >= TILE_FREEZE && pFront[Index] <= TILE_TELE_LASER_DISABLE) ||
                 (pFront[Index] >= TILE_LFREEZE && pFront[Index] <= TILE_LUNFREEZE)))
    return true;
  if (pTele && (pTele[Index] == TILE_TELEIN || pTele[Index] == TILE_TELEINEVIL || pTele[Index] == TILE_TELECHECKINEVIL ||
                pTele[Index] == TILE_TELECHECK || pTele[Index] == TILE_TELECHECKIN))
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

#define SQRT2 1.4142135623730951
static void init_distance_field(SCollision *pCollision) {
  const size_t orig_width = pCollision->m_MapData.width;
  const size_t orig_height = pCollision->m_MapData.height;
  const unsigned char *pInfos = pCollision->m_pTileInfos;

  const size_t hr_width = orig_width * DISTANCE_FIELD_RESOLUTION;
  const size_t hr_height = orig_height * DISTANCE_FIELD_RESOLUTION;
  float *hr_field = _mm_malloc(hr_width * hr_height * sizeof(float), 32);
  if (!hr_field) {
    printf("Error: could not allocate %lu bytes of memory\n", hr_width * hr_height * sizeof(float));
    return;
  }
  for (size_t y = 0; y < hr_height; ++y) {
    for (size_t x = 0; x < hr_width; ++x) {
      const size_t orig_x = x / DISTANCE_FIELD_RESOLUTION;
      const size_t orig_y = y / DISTANCE_FIELD_RESOLUTION;
      const size_t orig_idx = orig_y * orig_width + orig_x;
      const size_t tele = pCollision->m_MapData.tele_layer.type ? pCollision->m_MapData.tele_layer.type[orig_idx] : 0;
      hr_field[y * hr_width + x] = (pInfos[orig_idx] & INFO_ISSOLID || tele == TILE_TELEINHOOK || tele == TILE_TELEINWEAPON) ? 0.0f : FLT_MAX;
    }
  }

  // First pass: left-top to right-bottom
  for (size_t y = 1; y < hr_height - 1; ++y) {
    for (size_t x = 1; x < hr_width - 1; ++x) {
      const size_t idx = y * hr_width + x;
      if (hr_field[idx] == 0.0f)
        continue;

      float min_dist = FLT_MAX;
      min_dist = fminf(min_dist, hr_field[idx - 1] + 1.0f);
      min_dist = fminf(min_dist, hr_field[idx - hr_width] + 1.0f);
      min_dist = fminf(min_dist, hr_field[idx - hr_width - 1] + (float)SQRT2);
      min_dist = fminf(min_dist, hr_field[idx - hr_width + 1] + (float)SQRT2);

      hr_field[idx] = fminf(hr_field[idx], min_dist);
    }
  }

  // Second pass: right-bottom to left-top
  for (size_t y = hr_height - 2; y > 0; --y) {
    for (size_t x = hr_width - 2; x > 0; --x) {
      const size_t idx = y * hr_width + x;
      if (hr_field[idx] == 0.0f)
        continue;

      float min_dist = FLT_MAX;
      min_dist = fminf(min_dist, hr_field[idx + 1] + 1.0f);
      min_dist = fminf(min_dist, hr_field[idx + hr_width] + 1.0f);
      min_dist = fminf(min_dist, hr_field[idx + hr_width + 1] + (float)SQRT2);
      min_dist = fminf(min_dist, hr_field[idx + hr_width - 1] + (float)SQRT2);
      hr_field[idx] = fminf(hr_field[idx], min_dist);
    }
  }

  pCollision->m_pSolidTeleDistanceField = _mm_malloc(hr_width * hr_height, 64);
  if (!pCollision->m_pSolidTeleDistanceField) {
    printf("Could not allocated %zu bytes for m_pSolidTeleDistanceField\n", hr_width * hr_height);
  }

  const float scale_to_world = 32.f / (float)DISTANCE_FIELD_RESOLUTION;
  for (size_t i = 0; i < hr_width * hr_height; ++i) {
    hr_field[i] -= 1.5f;
    hr_field[i] *= scale_to_world;
    hr_field[i] = fclamp(hr_field[i], 0, 255);
    pCollision->m_pSolidTeleDistanceField[i] = (uint8_t)imax((int)hr_field[i] - 1, 0);
  }
  free(hr_field);
}

static void init_tuning_params(STuningParams *pTunings) {
#define MACRO_TUNING_PARAM(Name, Value) pTunings->m_##Name = Value;
#include "tuning.h"
#undef MACRO_TUNING_PARAM
}

bool init_collision(SCollision *__restrict__ pCollision, const char *__restrict__ pMap) {
  pCollision->m_MapData = load_map(pMap);
  if (!pCollision->m_MapData.game_layer.data)
    return false;

  map_data_t *pMapData = &pCollision->m_MapData;
  const int Width = pMapData->width;
  const int Height = pMapData->height;
  const int MapSize = Width * Height;

  pCollision->m_pTileInfos = _mm_malloc(MapSize * sizeof(char), 64);
  memset(pCollision->m_pTileInfos, 0, MapSize * sizeof(char));

  pCollision->m_pTileBroadCheck = _mm_malloc(MapSize * sizeof(char), 64);
  memset(pCollision->m_pTileBroadCheck, 0, MapSize * sizeof(char));

  pCollision->m_pMoveRestrictions = _mm_malloc(MapSize * NUM_MR_DIRS * sizeof(char), 64);
  memset(pCollision->m_pMoveRestrictions, 0, MapSize * NUM_MR_DIRS * sizeof(char));

  pCollision->m_pPickups = _mm_malloc(MapSize * sizeof(SPickup), 64);
  memset(pCollision->m_pPickups, 0, MapSize * sizeof(SPickup));
  pCollision->m_pFrontPickups = _mm_malloc(MapSize * sizeof(SPickup), 64);
  memset(pCollision->m_pFrontPickups, 0, MapSize * sizeof(SPickup));

  pCollision->m_pBroadSolidBitField = _mm_malloc(MapSize * sizeof(uint64_t), 64);
  memset(pCollision->m_pBroadSolidBitField, 0, MapSize * sizeof(uint64_t));

  pCollision->m_pBroadTeleInBitField = pCollision->m_MapData.tele_layer.type ? _mm_malloc(MapSize * sizeof(uint64_t), 64) : NULL;
  if (pCollision->m_pBroadTeleInBitField)
    memset(pCollision->m_pBroadTeleInBitField, 0, MapSize * sizeof(uint64_t));

  pCollision->m_pBroadIndicesBitField = _mm_malloc(MapSize * sizeof(uint64_t), 64);
  memset(pCollision->m_pBroadIndicesBitField, 0, MapSize * sizeof(uint64_t));
  pCollision->m_MoveRestrictionsFound = false;

  pCollision->m_pWidthLookup = _mm_malloc(Height * sizeof(unsigned int), 64);

  for (int i = 0; i < Height; ++i)
    pCollision->m_pWidthLookup[i] = i * Width;

  /*   if (pCollision->m_MapData.tele_layer.type) {
      for (int y = 0; y < Height; ++y) {
        for (int x = 0; x < Width; ++x) {
          if (pCollision->m_MapData.tele_layer.type[y * Width + x])
            printf("%d\n", pCollision->m_MapData.tele_layer.type[y * Width + x]);
        }
      }
    } */

  for (int i = 0; i < NUM_TUNE_ZONES; ++i)
    init_tuning_params(&pCollision->m_aTuningList[i]);
  // Figure out important things
  // Make lists of spawn points, tele outs and tele checkpoints outs
  // figure out highest switch number
  pCollision->m_HighestSwitchNumber = 0;
  if (pCollision->m_MapData.switch_layer.number)
    for (int i = 0; i < MapSize; ++i)
      pCollision->m_HighestSwitchNumber = imax(pCollision->m_HighestSwitchNumber, pCollision->m_MapData.switch_layer.number[i]);

  for (int i = 0; i < MapSize; ++i) {
    if (tile_exists(pCollision, i))
      pCollision->m_pTileInfos[i] |= INFO_TILENEXT;
    const int Tile = pCollision->m_MapData.game_layer.data[i];
    const int FTile = pCollision->m_MapData.front_layer.data ? pCollision->m_MapData.front_layer.data[i] : 0;
    if (Tile == TILE_SOLID || Tile == TILE_NOHOOK)
      pCollision->m_pTileInfos[i] |= INFO_ISSOLID;

    if ((Tile >= 192 && Tile <= 194) || (FTile >= 192 && FTile <= 194))
      ++pCollision->m_NumSpawnPoints;
    if (pMapData->tele_layer.type) {
      if (pMapData->tele_layer.type[i] == TILE_TELEOUT)
        ++pCollision->m_aNumTeleOuts[pMapData->tele_layer.number[i]];
      if (pMapData->tele_layer.type[i] == TILE_TELECHECKOUT)
        ++pCollision->m_aNumTeleCheckOuts[pMapData->tele_layer.number[i]];
    }

    for (int d = 0; d < NUM_MR_DIRS; d++) {
      int Tile;
      int Flags;
      if (pCollision->m_MapData.front_layer.data) {
        Tile = get_front_tile_index(pCollision, i);
        Flags = get_front_tile_flags(pCollision, i);
        pCollision->m_pMoveRestrictions[i][d] |= move_restrictions(d, Tile, Flags);
      }
      Tile = get_tile_index(pCollision, i);
      Flags = get_tile_flags(pCollision, i);
      pCollision->m_pMoveRestrictions[i][d] |= move_restrictions(d, Tile, Flags);

      if (pCollision->m_pMoveRestrictions[i][d])
        pCollision->m_MoveRestrictionsFound = true;
    }

    pCollision->m_pPickups[i].m_Type = -1;
    int EntIdx = pMapData->game_layer.data[i] - ENTITY_OFFSET;
    if ((EntIdx >= ENTITY_ARMOR_SHOTGUN && EntIdx <= ENTITY_ARMOR_LASER) || (EntIdx >= ENTITY_ARMOR_1 && EntIdx <= ENTITY_WEAPON_LASER)) {
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
      if (pCollision->m_MapData.switch_layer.type) {
        const int SwitchType = pCollision->m_MapData.switch_layer.type[i];
        if (SwitchType) {
          pCollision->m_pPickups[i].m_Type = SwitchType;
          pCollision->m_pPickups[i].m_Number = pCollision->m_MapData.switch_layer.number[i];
        }
      }
    }

    pCollision->m_pFrontPickups[i].m_Type = -1;
    if (pMapData->front_layer.data) {
      EntIdx = pMapData->front_layer.data[i] - ENTITY_OFFSET;
      if ((EntIdx >= ENTITY_ARMOR_SHOTGUN && EntIdx <= ENTITY_ARMOR_LASER) || (EntIdx >= ENTITY_ARMOR_1 && EntIdx <= ENTITY_WEAPON_LASER)) {
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
        pCollision->m_pFrontPickups[i].m_Type = Type;
        pCollision->m_pFrontPickups[i].m_Subtype = SubType;
        if (pCollision->m_MapData.switch_layer.type) {
          const int SwitchType = pCollision->m_MapData.switch_layer.type[i];
          if (SwitchType) {
            pCollision->m_pFrontPickups[i].m_Type = SwitchType;
            pCollision->m_pFrontPickups[i].m_Number = pCollision->m_MapData.switch_layer.number[i];
          }
        }
      }
    }
  }

  for (int y = 0; y < Height; ++y) {
    for (int x = 0; x < Width; ++x) {
      const int Idx = pCollision->m_pWidthLookup[y] + x;
      for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
          const int dIdx = pCollision->m_pWidthLookup[iclamp(y + dy, 0, Height - 1)] + iclamp(x + dx, 0, Width - 1);
          if (pCollision->m_pPickups[dIdx].m_Type > 0)
            pCollision->m_pTileInfos[Idx] |= INFO_PICKUPNEXT;
          if (pCollision->m_pFrontPickups[dIdx].m_Type > 0)
            pCollision->m_pTileInfos[Idx] |= INFO_PICKUPNEXT;
          if (pCollision->m_MapData.game_layer.data[dIdx] == TILE_DEATH ||
              (pCollision->m_MapData.front_layer.data && pCollision->m_MapData.front_layer.data[dIdx]))
            pCollision->m_pTileInfos[Idx] |= INFO_CANHITKILL;
        }
      }

      for (int i = -1; i <= 1; ++i) {
        if (pCollision->m_pTileInfos[pCollision->m_pWidthLookup[iclamp(y + 1, 0, Height - 1)] + iclamp(x + i, 0, Width - 1)] & INFO_ISSOLID)
          pCollision->m_pTileInfos[Idx] |= INFO_CANGROUND;
      }
    }
  }

  if (pCollision->m_NumSpawnPoints > 0)
    pCollision->m_pSpawnPoints = malloc(pCollision->m_NumSpawnPoints * sizeof(mvec2));
  if (pMapData->tele_layer.type) {
    for (int i = 0; i < 256; ++i) {
      if (pCollision->m_aNumTeleOuts[i] > 0)
        pCollision->m_apTeleOuts[i] = malloc(pCollision->m_aNumTeleOuts[i] * sizeof(mvec2));
      if (pCollision->m_aNumTeleCheckOuts[i] > 0)
        pCollision->m_apTeleCheckOuts[i] = malloc(pCollision->m_aNumTeleCheckOuts[i] * sizeof(mvec2));
    }
  }

  for (int i = 0; i < MapSize; ++i) {
    if (pCollision->m_MapData.game_layer.data[i])
      pCollision->m_pTileBroadCheck[i] = true;
    else if (pCollision->m_MapData.front_layer.data && pCollision->m_MapData.front_layer.data[i])
      pCollision->m_pTileBroadCheck[i] = true;
    else if (pCollision->m_MapData.tele_layer.type && pCollision->m_MapData.tele_layer.type[i])
      pCollision->m_pTileBroadCheck[i] = true;
    else if (pCollision->m_MapData.switch_layer.type && pCollision->m_MapData.switch_layer.type[i])
      pCollision->m_pTileBroadCheck[i] = true;
    else if (pCollision->m_pTileInfos[i] & INFO_TILENEXT)
      pCollision->m_pTileBroadCheck[i] = true;
  }

  for (int y = 0; y < Height; ++y) {
    for (int x = 0; x < Width; ++x) {
      static const mvec2 DIRECTIONS[NUM_MR_DIRS] = {CTVEC2(0, 0), CTVEC2(18, 0), CTVEC2(0, 18), CTVEC2(-18, 0), CTVEC2(0, -18)};
      unsigned char Restrictions = 0;
#pragma clang loop unroll(full)
      for (int d = 0; d < NUM_MR_DIRS; d++) {
        mvec2 NewPos = vvclamp(vvadd(vec2_init(x * 32 + 16, y * 32 + 16), DIRECTIONS[d]), vec2_init(0, 0),
                               vec2_init((pCollision->m_MapData.width * 32) - 16, (pCollision->m_MapData.height * 32) - 16));
        int ModMapIndex = get_pure_map_index(pCollision, NewPos);

        Restrictions |= pCollision->m_pMoveRestrictions[ModMapIndex][d];

        if (pCollision->m_MapData.door_layer.index && pCollision->m_MapData.door_layer.index[ModMapIndex]) {
          Restrictions |=
              move_restrictions(d, pCollision->m_MapData.door_layer.index[ModMapIndex], pCollision->m_MapData.door_layer.flags[ModMapIndex]);
        }
        if (Restrictions) {
          pCollision->m_pTileInfos[y * Width + x] |= INFO_CANHITSTOPPER;
          break;
        }
      }
    }
  }

  init_distance_field(pCollision);

  for (int y = 0; y < Height; ++y) {
    for (int x = 0; x < Width; ++x) {
      const int Idx = pCollision->m_pWidthLookup[y] + x;
      const int maxX = imin((Width - 1) - x, 7);
      const int maxY = imin((Height - 1) - y, 7);

      // Set bitfield for all sub-rectangles
      for (int dy = 0; dy <= maxY; ++dy) {
        for (int dx = 0; dx <= maxX; ++dx) {
          const uint64_t BitIdx = (uint64_t)1 << (dy * 8 + dx);
          for (int iy = y; iy <= y + dy; ++iy) {
            const unsigned char *pRowBroad = pCollision->m_pTileBroadCheck + pCollision->m_pWidthLookup[iy];
            const unsigned char *pRowInfos = pCollision->m_pTileInfos + pCollision->m_pWidthLookup[iy];
            const unsigned char *pRowTele =
                pCollision->m_MapData.tele_layer.type ? pCollision->m_MapData.tele_layer.type + pCollision->m_pWidthLookup[y] : NULL;
            for (int ix = x; ix <= x + dx; ++ix) {
              if (pRowBroad[ix])
                pCollision->m_pBroadIndicesBitField[Idx] |= BitIdx;
              if (pRowInfos[ix] & INFO_ISSOLID)
                pCollision->m_pBroadSolidBitField[Idx] |= BitIdx;
              if (pRowTele && (pRowTele[x] == TILE_TELEINHOOK || pRowTele[x] == TILE_TELEINWEAPON))
                pCollision->m_pBroadTeleInBitField[Idx] |= BitIdx;
            }
          }
        }
      }

// This works, validation not necessary currently
#if 0
      for (int dy = 0; dy <= maxY; ++dy) {
        for (int dx = 0; dx <= maxX; ++dx) {
          bool Hit = false;
          for (int ay = y; ay <= y + dy; ++ay) {
            const unsigned char *rowStart = pCollision->m_pTileInfos + pCollision->m_pWidthLookup[ay];
            for (int ax = x; ax <= x + dx; ++ax) {
              if (rowStart[ax] & INFO_ISSOLID) {
                Hit = true;
                break;
              }
            }
            if (Hit)
              break;
          }

          // Check the corresponding bit in the bitfield
          const uint64_t BitIdx = (uint64_t)1 << (dy * 8 + dx);
          uint64_t Opt = pCollision->m_pBroadSolidBitField[Idx] & BitIdx;
          if (Hit != (bool)Opt) {
            printf("ERROR at (%d, %d) for size (%d, %d): Hit: %d, Opt: %lu\n", x, y, dx, dy, Hit, Opt);
          }
        }
      }
#endif
    }
  }

  // for (int i = 0; i < MapSize; ++i) {
  //   if (i % Width == 0)
  //     printf("\n");
  //   printf(pCollision->m_pTileInfos[i] & INFO_ISSOLID ? "@" : "'");
  // }

  if (!pMapData->tele_layer.type && !pCollision->m_NumSpawnPoints)
    return true;

  int aTeleIdx[256] = {0}, aTeleCheckIdx[256] = {0}, SpawnPointIdx = 0;
  for (int y = 0; y < Height; ++y) {
    for (int x = 0; x < Width; ++x) {
      const int Idx = pCollision->m_pWidthLookup[y] + x;
      if (pCollision->m_NumSpawnPoints) {
        if ((pMapData->game_layer.data[Idx] >= 192 && pMapData->game_layer.data[Idx] <= 194) ||
            (pMapData->front_layer.data && pMapData->front_layer.data[Idx] >= 192 && pMapData->front_layer.data[Idx] <= 194))
          pCollision->m_pSpawnPoints[SpawnPointIdx++] = vec2_init(x, y);
      }
      if (pMapData->tele_layer.type) {
        if (pMapData->tele_layer.type[Idx] == TILE_TELEOUT)
          pCollision->m_apTeleOuts[pMapData->tele_layer.number[Idx]][aTeleIdx[pMapData->tele_layer.number[Idx]]++] =
              vec2_init((x * 32.f) + 16.f, (y * 32.f) + 16.f);
        if (pMapData->tele_layer.type[Idx] == TILE_TELECHECKOUT)
          pCollision->m_apTeleCheckOuts[pMapData->tele_layer.number[Idx]][aTeleCheckIdx[pMapData->tele_layer.number[Idx]]++] =
              vec2_init((x * 32.f) + 16.f, (y * 32.f) + 16.f);
      }
    }
  }

  return true;
}

void free_collision(SCollision *pCollision) {
  if (!pCollision)
    return;

  free_map_data(&pCollision->m_MapData);
  if (pCollision->m_pPickups)
    _mm_free(pCollision->m_pPickups);
  if (pCollision->m_pFrontPickups)
    _mm_free(pCollision->m_pFrontPickups);
  if (pCollision->m_pMoveRestrictions)
    _mm_free(pCollision->m_pMoveRestrictions);
  if (pCollision->m_pSolidTeleDistanceField)
    _mm_free(pCollision->m_pSolidTeleDistanceField);
  if (pCollision->m_pTileBroadCheck)
    _mm_free(pCollision->m_pTileBroadCheck);
  if (pCollision->m_pWidthLookup)
    _mm_free(pCollision->m_pWidthLookup);
  if (pCollision->m_pBroadSolidBitField)
    _mm_free(pCollision->m_pBroadSolidBitField);
  if (pCollision->m_pBroadTeleInBitField)
    _mm_free(pCollision->m_pBroadTeleInBitField);
  if (pCollision->m_pBroadIndicesBitField)
    _mm_free(pCollision->m_pBroadIndicesBitField);
  if (pCollision->m_pTileInfos)
    _mm_free(pCollision->m_pTileInfos);

  // Free spawn points
  if (pCollision->m_NumSpawnPoints)
    free(pCollision->m_pSpawnPoints);

  // Free tele layer arrays
  for (int i = 0; i < 256; ++i) {
    free(pCollision->m_apTeleOuts[i]);
    free(pCollision->m_apTeleCheckOuts[i]);
  }

  // Zero everything
  memset(pCollision, 0, sizeof(SCollision));
}

inline int get_pure_map_index(SCollision *pCollision, mvec2 Pos) {
  const int nx = (int)(vgetx(Pos) + 0.5f) >> 5;
  const int ny = (int)(vgety(Pos) + 0.5f) >> 5;
  return pCollision->m_pWidthLookup[ny] + nx;
}

static inline unsigned char get_move_restrictions_raw(unsigned char Tile, unsigned char Flags) {
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
    return (Flags & TILEFLAG_ROTATE) ? (CANTMOVE_LEFT | CANTMOVE_RIGHT) : (CANTMOVE_DOWN | CANTMOVE_UP);
  case TILE_STOPA:
    return CANTMOVE_LEFT | CANTMOVE_RIGHT | CANTMOVE_UP | CANTMOVE_DOWN;
  default:
    return 0;
  }
}

inline unsigned char move_restrictions(unsigned char Direction, unsigned char Tile, unsigned char Flags) {
  unsigned char Result = get_move_restrictions_raw(Tile, Flags);
  if (Direction == MR_DIR_HERE && Tile == TILE_STOP)
    return Result;
  static const unsigned char aDirections[NUM_MR_DIRS] = {0, CANTMOVE_RIGHT, CANTMOVE_DOWN, CANTMOVE_LEFT, CANTMOVE_UP};
  return Result & aDirections[Direction];
}

inline unsigned char get_tile_index(SCollision *pCollision, int Index) { return pCollision->m_MapData.game_layer.data[Index]; }
inline unsigned char get_front_tile_index(SCollision *pCollision, int Index) { return pCollision->m_MapData.front_layer.data[Index]; }
inline unsigned char get_tile_flags(SCollision *pCollision, int Index) { return pCollision->m_MapData.game_layer.flags[Index]; }
inline unsigned char get_front_tile_flags(SCollision *pCollision, int Index) { return pCollision->m_MapData.front_layer.flags[Index]; }
inline unsigned char get_switch_number(SCollision *pCollision, int Index) { return pCollision->m_MapData.switch_layer.number[Index]; }
inline unsigned char get_switch_type(SCollision *pCollision, int Index) { return pCollision->m_MapData.switch_layer.type[Index]; }
inline unsigned char get_switch_delay(SCollision *pCollision, int Index) { return pCollision->m_MapData.switch_layer.delay[Index]; }
inline unsigned char is_teleport(SCollision *pCollision, int Index) {
  return pCollision->m_MapData.tele_layer.type[Index] == TILE_TELEIN ? pCollision->m_MapData.tele_layer.number[Index] : 0;
}
inline unsigned char is_teleport_hook(SCollision *pCollision, int Index) {
  return pCollision->m_MapData.tele_layer.type[Index] == TILE_TELEINHOOK ? pCollision->m_MapData.tele_layer.number[Index] : 0;
}
inline unsigned char is_teleport_weapon(SCollision *pCollision, int Index) {
  return pCollision->m_MapData.tele_layer.type[Index] == TILE_TELEINWEAPON ? pCollision->m_MapData.tele_layer.number[Index] : 0;
}
inline unsigned char is_evil_teleport(SCollision *pCollision, int Index) {
  return pCollision->m_MapData.tele_layer.type[Index] == TILE_TELEINEVIL ? pCollision->m_MapData.tele_layer.number[Index] : 0;
}
inline unsigned char is_check_teleport(SCollision *pCollision, int Index) {
  return pCollision->m_MapData.tele_layer.type[Index] == TILE_TELECHECKIN ? pCollision->m_MapData.tele_layer.number[Index] : 0;
}
inline unsigned char is_check_evil_teleport(SCollision *pCollision, int Index) {
  return pCollision->m_MapData.tele_layer.type[Index] == TILE_TELECHECKINEVIL ? pCollision->m_MapData.tele_layer.number[Index] : 0;
}
inline unsigned char is_tele_checkpoint(SCollision *pCollision, int Index) {
  return pCollision->m_MapData.tele_layer.type[Index] == TILE_TELECHECK ? pCollision->m_MapData.tele_layer.number[Index] : 0;
}

inline unsigned char get_collision_at(SCollision *pCollision, mvec2 Pos) {
  const int Nx = (int)vgetx(Pos) >> 5;
  const int Ny = (int)vgety(Pos) >> 5;
  const unsigned char Idx = pCollision->m_MapData.game_layer.data[pCollision->m_pWidthLookup[Ny] + Nx];
  return Idx * (Idx - 1 <= TILE_NOLASER - 1);
}
static inline unsigned char get_collision_at_idx(SCollision *pCollision, int Idx) {
  const unsigned char Tile = pCollision->m_MapData.game_layer.data[Idx];
  return Tile * (Tile - 1 <= TILE_NOLASER - 1);
}

inline unsigned char get_front_collision_at(SCollision *pCollision, mvec2 Pos) {
  const int Nx = (int)vgetx(Pos) >> 5;
  const int Ny = (int)vgety(Pos) >> 5;
  const int pos = pCollision->m_pWidthLookup[Ny] + Nx;
  const unsigned char Idx = pCollision->m_MapData.front_layer.data[pos];
  return Idx * (Idx - 1 <= TILE_NOLASER - 1);
}

inline unsigned char get_move_restrictions(SCollision *__restrict__ pCollision, void *__restrict__ pUser, mvec2 Pos) {

  if (!pCollision->m_MoveRestrictionsFound && !pCollision->m_MapData.door_layer.index)
    return 0;
  if (!(pCollision->m_pTileInfos[((int)vgety(Pos) >> 5) * pCollision->m_MapData.width + ((int)vgetx(Pos) >> 5)] & INFO_CANHITSTOPPER))
    return 0;

  static const mvec2 DIRECTIONS[NUM_MR_DIRS] = {CTVEC2(0, 0), CTVEC2(18, 0), CTVEC2(0, 18), CTVEC2(-18, 0), CTVEC2(0, -18)};
  unsigned char Restrictions = 0;
#pragma clang loop unroll(full)
  for (int d = 0; d < NUM_MR_DIRS; d++) {
    mvec2 NewPos = vvclamp(vvadd(Pos, DIRECTIONS[d]), vec2_init(0, 0),
                           vec2_init((pCollision->m_MapData.width * 32) - 16, (pCollision->m_MapData.height * 32) - 16));
    int ModMapIndex = get_pure_map_index(pCollision, NewPos);

    Restrictions |= pCollision->m_pMoveRestrictions[ModMapIndex][d];

    if (pCollision->m_MapData.door_layer.index && pCollision->m_MapData.door_layer.index[ModMapIndex]) {
      if (is_switch_active_cb(pCollision->m_MapData.door_layer.number[ModMapIndex], pUser)) {
        Restrictions |=
            move_restrictions(d, pCollision->m_MapData.door_layer.index[ModMapIndex], pCollision->m_MapData.door_layer.flags[ModMapIndex]);
      }
    }
  }
  return Restrictions;
}

int get_map_index(SCollision *pCollision, mvec2 Pos) {
  const int Nx = (int)vgetx(Pos) >> 5;
  const int Ny = (int)vgety(Pos) >> 5;
  const int Index = pCollision->m_pWidthLookup[Ny] + Nx;
  if (pCollision->m_pTileInfos[Index] & INFO_TILENEXT)
    return Index;
  return -1;
}

inline bool check_point(SCollision *pCollision, mvec2 Pos) {
  const int Nx = (int)(vgetx(Pos) + 0.5f) >> 5;
  const int Ny = (int)(vgety(Pos) + 0.5f) >> 5;
  return pCollision->m_pTileInfos[pCollision->m_pWidthLookup[Ny] + Nx] & INFO_ISSOLID;
}

static inline bool check_point_idx(SCollision *pCollision, int Idx) { return pCollision->m_pTileInfos[Idx] & INFO_ISSOLID; }

static inline void through_offset(mvec2 Pos0, mvec2 Pos1, int *__restrict__ pOffsetX, int *__restrict__ pOffsetY) {
  static const int offsets[8][2] = {{32, 0}, {0, 32}, {-32, 0}, {0, 32}, {32, 0}, {0, -32}, {-32, 0}, {0, -32}};
  const float dx = vgetx(Pos0) - vgetx(Pos1);
  const float dy = vgety(Pos0) - vgety(Pos1);
  int index = ((dx < 0.0f) << 2) | ((dy < 0.0f) << 1) | ((dx >= 0.0f ? dx : -dx) > (dy >= 0.0f ? dy : -dy));
  *pOffsetX = offsets[index][0];
  *pOffsetY = offsets[index][1];
}

static inline bool is_through(SCollision *pCollision, int x, int y, int OffsetX, int OffsetY, mvec2 Pos0, mvec2 Pos1) {
  if (x < 0 || y < 0 || x >= pCollision->m_MapData.width * 32 || y >= pCollision->m_MapData.width * 32)
    return false;
  int pos = get_pure_map_index(pCollision, vec2_init(x, y));
  unsigned char *pFrontIdx = pCollision->m_MapData.front_layer.data;
  unsigned char *pFrontFlgs = pCollision->m_MapData.front_layer.flags;

  if (pFrontIdx) {
    unsigned char FrontTile = pFrontIdx[pos];
    if (FrontTile == TILE_THROUGH_ALL || FrontTile == TILE_THROUGH_CUT)
      return true;
    if (FrontTile == TILE_THROUGH_DIR) {
      unsigned char Flags = pFrontFlgs[pos];
      if ((Flags == ROTATION_0 && vgety(Pos0) > vgety(Pos1)) || (Flags == ROTATION_90 && vgetx(Pos0) < vgetx(Pos1)) ||
          (Flags == ROTATION_180 && vgety(Pos0) < vgety(Pos1)) || (Flags == ROTATION_270 && vgetx(Pos0) > vgetx(Pos1))) {
        return true;
      }
    }
  }
  int offpos = get_pure_map_index(pCollision, vec2_init(x + OffsetX, y + OffsetY));
  if (offpos < 0)
    return false;

  unsigned char *pTileIdx = pCollision->m_MapData.game_layer.data;
  return pTileIdx[offpos] == TILE_THROUGH || (pFrontIdx && pFrontIdx[offpos] == TILE_THROUGH);
}

void move_point(SCollision *pCollision, mvec2 *pInoutPos, mvec2 *pInoutVel, float Elasticity) {
  mvec2 Pos = *pInoutPos;
  mvec2 Vel = *pInoutVel;
  if (check_point(pCollision, vvadd(Pos, Vel))) {
    int Affected = 0;
    if (check_point(pCollision, vec2_init(vgetx(Pos) + vgetx(Vel), vgety(Pos)))) {
      *pInoutVel = vsetx(*pInoutVel, vgetx(*pInoutVel) * -Elasticity);
      Affected++;
    }

    if (check_point(pCollision, vec2_init(vgetx(Pos), vgety(Pos) + vgety(Vel)))) {
      *pInoutVel = vsety(*pInoutVel, vgety(*pInoutVel) * -Elasticity);
      Affected++;
    }

    if (Affected == 0)
      *pInoutVel = vfmul(*pInoutVel, -Elasticity);
    return;
  }
  *pInoutPos = vvadd(Pos, Vel);
}

bool is_hook_blocker(SCollision *pCollision, int Index, mvec2 Pos0, mvec2 Pos1) {
  unsigned char *pTileIdx = pCollision->m_MapData.game_layer.data;
  unsigned char *pTileFlgs = pCollision->m_MapData.game_layer.flags;

  unsigned char *pFrontIdx = pCollision->m_MapData.front_layer.data;
  unsigned char *pFrontFlgs = pCollision->m_MapData.front_layer.flags;
  if (pTileIdx[Index] == TILE_THROUGH_ALL || (pFrontIdx && pFrontIdx[Index] == TILE_THROUGH_ALL))
    return true;
  if (pTileIdx[Index] == TILE_THROUGH_DIR &&
      ((pTileFlgs[Index] == ROTATION_0 && vgety(Pos0) < vgety(Pos1)) || (pTileFlgs[Index] == ROTATION_90 && vgetx(Pos0) > vgetx(Pos1)) ||
       (pTileFlgs[Index] == ROTATION_180 && vgety(Pos0) > vgety(Pos1)) || (pTileFlgs[Index] == ROTATION_270 && vgetx(Pos0) < vgetx(Pos1))))
    return true;
  if (pFrontIdx && pFrontIdx[Index] == TILE_THROUGH_DIR &&
      ((pFrontFlgs[Index] == ROTATION_0 && vgety(Pos0) < vgety(Pos1)) || (pFrontFlgs[Index] == ROTATION_90 && vgetx(Pos0) > vgetx(Pos1)) ||
       (pFrontFlgs[Index] == ROTATION_180 && vgety(Pos0) > vgety(Pos1)) || (pFrontFlgs[Index] == ROTATION_270 && vgetx(Pos0) < vgetx(Pos1))))
    return true;
  return false;
}

static inline uint8_t broad_check(const SCollision *__restrict__ pCollision, mvec2 Start, mvec2 End) {
  const mvec2 MinVec = _mm_min_ps(Start, End);
  const mvec2 MaxVec = _mm_max_ps(Start, End);
  const int MinX = (int)vgetx(MinVec) >> 5;
  const int MinY = (int)vgety(MinVec) >> 5;
  const int MaxX = ((int)vgetx(MaxVec) + 1) >> 5;
  const int MaxY = ((int)vgety(MaxVec) + 1) >> 5;
  const int DiffY = (MaxY - MinY);
  const int DiffX = (MaxX - MinX);
  if (MinY < 0 || MaxY >= pCollision->m_MapData.height || MinX < 0 || MaxX >= pCollision->m_MapData.width)
    return 2;

  return (bool)(pCollision->m_pBroadSolidBitField[(MinY * pCollision->m_MapData.width) + MinX] & (uint64_t)1 << ((DiffY << 3) + DiffX));
}

static inline uint8_t broad_check_tele(const SCollision *__restrict__ pCollision, mvec2 Start, mvec2 End) {
  const mvec2 MinVec = _mm_min_ps(Start, End);
  const mvec2 MaxVec = _mm_max_ps(Start, End);
  const int MinX = (int)vgetx(MinVec) >> 5;
  const int MinY = (int)vgety(MinVec) >> 5;
  const int MaxX = ((int)vgetx(MaxVec) + 1) >> 5;
  const int MaxY = ((int)vgety(MaxVec) + 1) >> 5;
  const int DiffY = (MaxY - MinY);
  const int DiffX = (MaxX - MinX);
  if (MinY < 0 || MaxY >= pCollision->m_MapData.height || MinX < 0 || MaxX >= pCollision->m_MapData.width)
    return 2;

  return (bool)(pCollision->m_pBroadTeleInBitField[pCollision->m_pWidthLookup[MinY] + MinX] & (uint64_t)1 << ((DiffY << 3) + DiffX));
}

#define TILE_SHIFT 5
#define TILE_SIZE (1 << TILE_SHIFT)
unsigned char intersect_line_tele_hook(SCollision *__restrict__ pCollision, mvec2 Pos0, mvec2 Pos1, mvec2 *__restrict__ pOutCollision,
                                       unsigned char *__restrict__ pTeleNr) {
  // broadphase checks
  uint8_t Check[2] = {broad_check(pCollision, Pos0, Pos1), pTeleNr ? broad_check_tele(pCollision, Pos0, Pos1) : 0};
  if (!Check[0] && !Check[1]) {
    *pOutCollision = Pos1;
    return 0;
  }

  const int Width = pCollision->m_MapData.width;

  float x0 = vgetx(Pos0);
  float y0 = vgety(Pos0);
  const float x1 = vgetx(Pos1);
  const float y1 = vgety(Pos1);

  const float dx = x1 - x0;
  const float dy = y1 - y0;

  const float segLen = sqrtf(dx * dx + dy * dy);

  if (segLen == 0.0f) {
    int ix = ((int)(x0 + 0.5f)) >> TILE_SHIFT;
    int iy = ((int)(y0 + 0.5f)) >> TILE_SHIFT;
    int idx = iy * Width + ix;

    if (pTeleNr) {
      unsigned char tele = is_teleport_hook(pCollision, idx);
      if (tele) {
        *pTeleNr = tele;
        *pOutCollision = Pos0;
        return TILE_TELEINHOOK;
      }
    }
    if (check_point_idx(pCollision, idx)) {
      if (!is_through(pCollision, (int)(x0 + 0.5f), (int)(y0 + 0.5f), 0, 0, Pos0, Pos1)) {
        *pOutCollision = Pos0;
        return pCollision->m_MapData.game_layer.data[idx];
      }
    } else if (is_hook_blocker(pCollision, idx, Pos0, Pos1)) {
      *pOutCollision = Pos0;
      return TILE_NOHOOK;
    }
    *pOutCollision = Pos1;
    return 0;
  }

  int mapX = ((int)(x0 + 0.5f)) >> TILE_SHIFT;
  int mapY = ((int)(y0 + 0.5f)) >> TILE_SHIFT;
  const int endX = ((int)(x1 + 0.5f)) >> TILE_SHIFT;
  const int endY = ((int)(y1 + 0.5f)) >> TILE_SHIFT;

  const int stepX = (dx > 0.0f) ? 1 : ((dx < 0.0f) ? -1 : 0);
  const int stepY = (dy > 0.0f) ? 1 : ((dy < 0.0f) ? -1 : 0);

  float tMaxX, tMaxY;
  float tDeltaX, tDeltaY;

  if (stepX != 0) {
    int nextBoundaryX = (stepX > 0) ? ((mapX + 1) << TILE_SHIFT) : (mapX << TILE_SHIFT);
    tMaxX = fabsf((nextBoundaryX - x0) / dx);
    tDeltaX = fabsf((float)TILE_SIZE / dx);
  } else {
    tMaxX = INFINITY;
    tDeltaX = INFINITY;
  }

  if (stepY != 0) {
    int nextBoundaryY = (stepY > 0) ? ((mapY + 1) << TILE_SHIFT) : (mapY << TILE_SHIFT);
    tMaxY = fabsf((nextBoundaryY - y0) / dy);
    tDeltaY = fabsf((float)TILE_SIZE / dy);
  } else {
    tMaxY = INFINITY;
    tDeltaY = INFINITY;
  }

  int off_dx = 0, off_dy = 0;
  through_offset(Pos0, Pos1, &off_dx, &off_dy);

  float t = 0.0f;

  for (;;) {
    const int idx = mapY * Width + mapX;

    if (pTeleNr) {
      unsigned char tele = is_teleport_hook(pCollision, idx);
      if (tele) {
        *pTeleNr = tele;
        const float param = (t <= 0.0f) ? 0.0f : ((t >= segLen) ? 1.0f : (t / segLen));
        *pOutCollision = vvfmix(Pos0, Pos1, param);
        return TILE_TELEINHOOK;
      }
    }

    if (check_point_idx(pCollision, idx)) {
      const float param = (t <= 0.0f) ? 0.0f : ((t >= segLen) ? 1.0f : (t / segLen));
      const mvec2 Pos = vvfmix(Pos0, Pos1, param);
      if (!is_through(pCollision, (int)(vgetx(Pos) + 0.5f), (int)(vgety(Pos) + 0.5f), off_dx, off_dy, Pos0, Pos1)) {
        *pOutCollision = Pos;
        return pCollision->m_MapData.game_layer.data[idx];
      }
    } else if (is_hook_blocker(pCollision, idx, Pos0, Pos1)) {
      const float param = (t <= 0.0f) ? 0.0f : ((t >= segLen) ? 1.0f : (t / segLen));
      *pOutCollision = vvfmix(Pos0, Pos1, param);
      return TILE_NOHOOK;
    }

    if (mapX == endX && mapY == endY)
      break;

    if (tMaxX < tMaxY) {
      mapX += stepX;
      t = tMaxX * segLen;
      tMaxX += tDeltaX;
    } else {
      mapY += stepY;
      t = tMaxY * segLen;
      tMaxY += tDeltaY;
    }

    if (t > segLen) {
      t = segLen;
      mapX = endX;
      mapY = endY;
    }
  }

  *pOutCollision = Pos1;
  return 0;
}

unsigned char intersect_line_tele_weapon(SCollision *__restrict__ pCollision, mvec2 Pos0, mvec2 Pos1, mvec2 *__restrict__ pOutCollision,
                                         mvec2 *__restrict__ pOutBeforeCollision, unsigned char *__restrict__ pTeleNr) {
  uint8_t Check[2] = {broad_check(pCollision, Pos0, Pos1), pTeleNr ? broad_check_tele(pCollision, Pos0, Pos1) : 0};
  if (!Check[0] && !Check[1]) {
    *pOutCollision = Pos1;
    return 0;
  }

  const int Width = pCollision->m_MapData.width;
  const int Height = pCollision->m_MapData.height;
  int Idx = (((int)vgety(Pos0)) * Width * DISTANCE_FIELD_RESOLUTION) + ((int)vgetx(Pos0));
  unsigned char Start = Check[0] > 1 ? 0 : pCollision->m_pSolidTeleDistanceField[Idx];

  int End = (int)vdistance(Pos0, Pos1) + 1;
  Start = iclamp(Start, 0, End);
  Start -= Start % 8;
  const float fEnd = End;
  int dx = 0, dy = 0;
  through_offset(Pos0, Pos1, &dx, &dy);
  int LastIndex = -1;

  int *aIndices = malloc(sizeof(int) * (End + 8));

  const float inv_fEnd = 1.f / fEnd;
  const float Pos0_x = vgetx(Pos0);
  const float Pos0_y = vgety(Pos0);
  const float diff_x = vgetx(Pos1) - Pos0_x;
  const float diff_y = vgety(Pos1) - Pos0_y;

  const __m256 Pos0_x_vec = _mm256_set1_ps(Pos0_x);
  const __m256 Pos0_y_vec = _mm256_set1_ps(Pos0_y);
  const __m256 diff_x_vec = _mm256_set1_ps(diff_x);
  const __m256 diff_y_vec = _mm256_set1_ps(diff_y);
  const __m256 inv_fEnd_vec = _mm256_set1_ps(inv_fEnd);
  const __m256 half_vec = _mm256_set1_ps(0.5f);
  const __m256i width_vec = _mm256_set1_epi32(Width);

  for (int k = Start; k <= End; k += 8) {
    __m256i i_vec = _mm256_set_epi32(k + 7, k + 6, k + 5, k + 4, k + 3, k + 2, k + 1, k);
    __m256 a_vec = _mm256_mul_ps(_mm256_cvtepi32_ps(i_vec), inv_fEnd_vec);
    __m256 Pos_x_vec = _mm256_add_ps(Pos0_x_vec, _mm256_mul_ps(a_vec, diff_x_vec));
    __m256 Pos_y_vec = _mm256_add_ps(Pos0_y_vec, _mm256_mul_ps(a_vec, diff_y_vec));
    __m256 Pos_x_plus_half = _mm256_add_ps(Pos_x_vec, half_vec);
    __m256 Pos_y_plus_half = _mm256_add_ps(Pos_y_vec, half_vec);
    __m256i ix_vec = _mm256_srai_epi32(_mm256_cvttps_epi32(Pos_x_plus_half), 5);
    __m256i iy_vec = _mm256_srai_epi32(_mm256_cvttps_epi32(Pos_y_plus_half), 5);
    __m256i index_vec = _mm256_add_epi32(_mm256_mullo_epi32(iy_vec, width_vec), ix_vec);
    _mm256_storeu_si256((__m256i *)&aIndices[k], index_vec);
  }

  for (int i = Start; i <= End; i++) {
    const int Index = aIndices[i];
    if (Index < 0 || Index >= Width * Height)
      break;
    if (Index == LastIndex)
      continue;
    LastIndex = Index;
    if (pTeleNr) {
      *pTeleNr = is_teleport_weapon(pCollision, Index);
      if (*pTeleNr) {
        *pOutCollision = vvfmix(Pos0, Pos1, i / fEnd);
        *pOutBeforeCollision = vvfmix(Pos0, Pos1, imax(i - 1, 0) / fEnd);
        free(aIndices);
        return TILE_TELEINWEAPON;
      }
    }

    if (check_point_idx(pCollision, Index)) {
      *pOutCollision = vvfmix(Pos0, Pos1, i / fEnd);
      *pOutBeforeCollision = vvfmix(Pos0, Pos1, imax(i - 1, 0) / fEnd);
      free(aIndices);
      return pCollision->m_MapData.game_layer.data[Index];
    }
  }
  *pOutCollision = Pos1;
  *pOutBeforeCollision = Pos1;
  free(aIndices);
  return 0;
}

bool test_box(SCollision *pCollision, mvec2 Pos, mvec2 Size) {
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
  if (!pCollision->m_MapData.tune_layer.type)
    return 0;
  if (pCollision->m_MapData.tune_layer.type[Index])
    return pCollision->m_MapData.tune_layer.number[Index];
  return 0;
}

bool is_speedup(SCollision *pCollision, int Index) {
  return pCollision->m_MapData.speedup_layer.type && pCollision->m_MapData.speedup_layer.force[Index] > 0;
}

void get_speedup(SCollision *__restrict__ pCollision, int Index, mvec2 *__restrict__ pDir, int *__restrict__ pForce, int *__restrict__ pMaxSpeed,
                 int *__restrict__ pType) {
  float Angle = pCollision->m_MapData.speedup_layer.angle[Index] * (PI / 180.0f);
  *pForce = pCollision->m_MapData.speedup_layer.force[Index];
  *pType = pCollision->m_MapData.speedup_layer.type[Index];
  *pDir = vdirection(Angle);
  if (pMaxSpeed)
    *pMaxSpeed = pCollision->m_MapData.speedup_layer.max_speed[Index];
}

// TODO: do the same optimization as in intersect_line_tele_hook
bool intersect_line(SCollision *__restrict__ pCollision, mvec2 Pos0, mvec2 Pos1, mvec2 *__restrict__ pOutCollision,
                    mvec2 *__restrict__ pOutBeforeCollision) {
  if (!broad_check(pCollision, Pos0, Pos1)) {
    *pOutCollision = Pos1;
    *pOutBeforeCollision = Pos1;
    return 0;
  }

  float Distance = vdistance(Pos0, Pos1);
  int End = Distance + 1;
  mvec2 Last = Pos0;
  int LastIdx = -1;
  for (int i = 0; i <= End; i++) {
    float a = i / (float)End;
    mvec2 Pos = vvfmix(Pos0, Pos1, a);
    int Nx = (int)(vgetx(Pos) + 0.5f) >> 5;
    int Ny = (int)(vgety(Pos) + 0.5f) >> 5;
    int Idx = pCollision->m_pWidthLookup[Ny] + Nx;
    if (LastIdx == Idx)
      continue;
    LastIdx = Idx;
    if (check_point_idx(pCollision, Idx)) {
      *pOutCollision = Pos;
      *pOutBeforeCollision = Last;
      return true;
    }

    Last = Pos;
  }
  *pOutCollision = Pos1;
  *pOutBeforeCollision = Pos1;
  return false;
}

static inline bool check_point_int(const SCollision *__restrict__ pCollision, int x, int y) {
  return pCollision->m_pTileInfos[(y >> 5) * pCollision->m_MapData.width + (x >> 5)] & INFO_ISSOLID;
}

static inline bool test_box_character(const SCollision *__restrict__ pCollision, int x, int y) {
  // NOTE: doesn't work out of bounds
  const int frac_x = x & 31;
  const int frac_y = y & 31;
  const uint32_t mask = (1U << 13) | (1U << 18);
  uint32_t check = (1U << frac_x) | (1U << frac_y);
  if ((mask & check) == 0)
    return false;

  if (check_point_int(pCollision, x - HALFPHYSICALSIZE, y + HALFPHYSICALSIZE))
    return true;
  if (check_point_int(pCollision, x + HALFPHYSICALSIZE, y + HALFPHYSICALSIZE))
    return true;
  if (check_point_int(pCollision, x - HALFPHYSICALSIZE, y - HALFPHYSICALSIZE))
    return true;
  if (check_point_int(pCollision, x + HALFPHYSICALSIZE, y - HALFPHYSICALSIZE))
    return true;

  return false;
}

void move_box(const SCollision *__restrict__ pCollision, mvec2 Pos, mvec2 Vel, mvec2 *__restrict__ pOutPos, mvec2 *__restrict__ pOutVel,
              mvec2 Elasticity, bool *__restrict__ pGrounded) {
  float Distance = vsqlength(Vel);
  if (Distance <= 0.00001f * 0.00001f)
    return;

  mvec2 NewPos = vvadd(Pos, Vel);
  const mvec2 minVec = _mm_min_ps(Pos, NewPos);
  const mvec2 maxVec = _mm_max_ps(Pos, NewPos);
  const mvec2 offset = _mm_set1_ps(HALFPHYSICALSIZE + 1.0f);
  const mvec2 minAdj = _mm_sub_ps(minVec, offset);
  const mvec2 maxAdj = _mm_add_ps(maxVec, offset);
  const int MinX = (int)vgetx(minAdj) >> 5;
  const int MinY = (int)vgety(minAdj) >> 5;
  const int MaxX = (int)vgetx(maxAdj) >> 5;
  const int MaxY = (int)vgety(maxAdj) >> 5;
  // bitshift by the index in the 8x8 block (max 63)
  const uint64_t Mask = (uint64_t)1 << (((MaxY - MinY) << 3) + (MaxX - MinX));
  const uint64_t IsSolid = pCollision->m_pBroadSolidBitField[(MinY * pCollision->m_MapData.width) + MinX] & Mask;
  if (!IsSolid) {
    *pOutPos = vvadd(Pos, Vel);
    return;
  }
  const unsigned short Max = s_aMaxTable[(int)Distance];
  const float Fraction = s_aFractionTable[Max];
  uivec2 IPos = (uivec2){(int)(vgetx(Pos) + 0.5f), (int)(vgety(Pos) + 0.5f)};
  uivec2 INewPos;
  for (int i = 0; i <= Max; i++) {
    NewPos = vvadd(Pos, vfmul(Vel, Fraction));
    INewPos = (uivec2){(int)(vgetx(NewPos) + 0.5f), (int)(vgety(NewPos) + 0.5f)};
    if (test_box_character(pCollision, INewPos.x, INewPos.y)) {
      bool Hit = false;
      if (test_box_character(pCollision, IPos.x, INewPos.y)) {
        if (vgety(Vel) > 0)
          *pGrounded = true;
        NewPos = vsety(NewPos, vgety(Pos));
        Vel = vsety(Vel, 0);
        Hit = true;
      }
      if (test_box_character(pCollision, INewPos.x, IPos.y)) {
        NewPos = vsetx(NewPos, vgetx(Pos));
        Vel = vsetx(Vel, 0);
        Hit = true;
      }
      if (!Hit) {
        NewPos = Pos;
        Vel = vfmul(Vel, -1.0f);
        Vel = vvmul(Vel, Elasticity);
      }
    }
    IPos = INewPos;
    Pos = NewPos;
  }

  *pOutPos = Pos;
  *pOutVel = Vel;
}

bool get_nearest_air_pos_player(SCollision *__restrict__ pCollision, mvec2 PlayerPos, mvec2 *__restrict__ pOutPos) {
  for (int dist = 5; dist >= -1; dist--) {
    *pOutPos = vec2_init(vgetx(PlayerPos), vgety(PlayerPos) - dist);
    if (!test_box(pCollision, *pOutPos, PHYSICALSIZEVEC))
      return true;
  }
  return false;
}

bool get_nearest_air_pos(SCollision *__restrict__ pCollision, mvec2 Pos, mvec2 PrevPos, mvec2 *__restrict__ pOutPos) {
  for (int k = 0; k < 16 && check_point(pCollision, Pos); k++) {
    Pos = vvsub(Pos, vnormalize(vvsub(PrevPos, Pos)));
  }

  mvec2 PosInBlock = vec2_init((int)(vgetx(Pos) + 0.5f) % 32, (int)(vgety(Pos) + 0.5f) % 32);
  mvec2 BlockCenter = vfadd(vvsub(vec2_init((int)(vgetx(Pos) + 0.5f), (int)(vgety(Pos) + 0.5f)), PosInBlock), 16.0f);

  *pOutPos = vec2_init(vgetx(BlockCenter) + (vgetx(PosInBlock) < 16 ? -2.0f : 1.0f), vgety(Pos));
  if (!test_box(pCollision, *pOutPos, PHYSICALSIZEVEC))
    return true;

  *pOutPos = vec2_init(vgetx(Pos), vgety(BlockCenter) + (vgety(PosInBlock) < 16 ? -2.0f : 1.0f));
  if (!test_box(pCollision, *pOutPos, PHYSICALSIZEVEC))
    return true;

  *pOutPos = vec2_init(vgetx(BlockCenter) + (vgetx(PosInBlock) < 16 ? -2.0f : 1.0f), vgety(BlockCenter) + (vgety(PosInBlock) < 16 ? -2.0f : 1.0f));
  return !test_box(pCollision, *pOutPos, PHYSICALSIZEVEC);
}

int get_index(SCollision *pCollision, mvec2 PrevPos, mvec2 Pos) {
  float Distance = vdistance(PrevPos, Pos);

  if (!Distance) {
    int Nx = (int)vgetx(Pos) >> 5;
    int Ny = (int)vgety(Pos) >> 5;
    return pCollision->m_pWidthLookup[Ny] + Nx;
  }

  for (int i = 0, id = ceil(Distance); i < id; i++) {
    float a = (float)i / Distance;
    mvec2 Tmp = vvfmix(PrevPos, Pos, a);
    int Nx = (int)vgetx(Tmp) >> 5;
    int Ny = (int)vgety(Tmp) >> 5;
    return pCollision->m_pWidthLookup[Ny] + Nx;
  }

  return -1;
}

int entity(SCollision *pCollision, int x, int y, int Layer) {
  if ((unsigned char)x >= pCollision->m_MapData.width || (unsigned char)y >= pCollision->m_MapData.height)
    return 0;

  const int Index = pCollision->m_pWidthLookup[y] + x;
  switch (Layer) {
  case LAYER_GAME:
    return pCollision->m_MapData.game_layer.data[Index] - ENTITY_OFFSET;
  case LAYER_FRONT:
    return pCollision->m_MapData.front_layer.data[Index] - ENTITY_OFFSET;
  case LAYER_SWITCH:
    return pCollision->m_MapData.switch_layer.type[Index] - ENTITY_OFFSET;
  case LAYER_TELE:
    return pCollision->m_MapData.tele_layer.type[Index] - ENTITY_OFFSET;
  case LAYER_SPEEDUP:
    return pCollision->m_MapData.speedup_layer.type[Index] - ENTITY_OFFSET;
  case LAYER_TUNE:
    return pCollision->m_MapData.tune_layer.type[Index] - ENTITY_OFFSET;
  default:
    printf("Error while initializing gameworld: invalid layer found\n");
  }
  return 0;
}
