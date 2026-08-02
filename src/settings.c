#include "settings.h"

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

typedef struct Token {
  const char *m_pData;
  size_t m_Length;
} SToken;

typedef struct ConfigSetting {
  const char *m_pName;
  size_t m_Offset;
  int m_Min;
  int m_Max;
} SConfigSetting;

typedef struct TuningSetting {
  const char *m_pMemberName;
  size_t m_Offset;
} STuningSetting;

#define CONFIG_SETTING(Name, ScriptName, Min, Max) {ScriptName, offsetof(SConfig, m_##Name), Min, Max}
static const SConfigSetting s_aConfigSettings[] = {
    CONFIG_SETTING(SvTeleportHoldHook, "sv_teleport_hold_hook", 0, 1),
    CONFIG_SETTING(SvTeleportLoseWeapons, "sv_teleport_lose_weapons", 0, 1),
    CONFIG_SETTING(SvDeepfly, "sv_deepfly", 0, 1),
    CONFIG_SETTING(SvDestroyBulletsOnDeath, "sv_destroy_bullets_on_death", 0, 1),
    CONFIG_SETTING(SvDestroyLasersOnDeath, "sv_destroy_lasers_on_death", 0, 1),
    CONFIG_SETTING(SvFreezeDelay, "sv_freeze_delay", 1, 30),
    CONFIG_SETTING(SvHit, "sv_hit", 0, 1),
    CONFIG_SETTING(SvEndlessDrag, "sv_endless_drag", 0, 1),
    CONFIG_SETTING(SvSoloServer, "sv_solo_server", 0, 1),
    CONFIG_SETTING(SvFastcap, "sv_fastcap", 0, 1),
    CONFIG_SETTING(SvKillGrenades, "sv_kill_grenades", 0, 1),
    CONFIG_SETTING(SvHealthAndAmmo, "sv_health_and_ammo", 0, 1),
    CONFIG_SETTING(SvNoWeapons, "sv_no_weapons", 0, 1),
};
#undef CONFIG_SETTING

#define MACRO_TUNING_PARAM(Name, Value) {#Name, offsetof(STuningParams, m_##Name)},
static const STuningSetting s_aTuningSettings[] = {
#include <ddnet_physics/tuning.h>
};
#undef MACRO_TUNING_PARAM

static char ascii_to_lower(char Character) {
  if (Character >= 'A' && Character <= 'Z')
    return (char)(Character - 'A' + 'a');
  return Character;
}

static bool token_equals(const SToken *pToken, const char *pString) {
  size_t Index = 0;
  while (Index < pToken->m_Length && pString[Index]) {
    if (ascii_to_lower(pToken->m_pData[Index]) != ascii_to_lower(pString[Index]))
      return false;
    ++Index;
  }
  return Index == pToken->m_Length && pString[Index] == '\0';
}

static bool token_equals_tuning_name(const SToken *pToken, const char *pMemberName) {
  size_t TokenIndex = 0;
  for (size_t MemberIndex = 0; pMemberName[MemberIndex]; ++MemberIndex) {
    const char MemberCharacter = pMemberName[MemberIndex];
    if (MemberCharacter >= 'A' && MemberCharacter <= 'Z' && MemberIndex != 0) {
      if (TokenIndex >= pToken->m_Length || pToken->m_pData[TokenIndex] != '_')
        return false;
      ++TokenIndex;
    }
    if (TokenIndex >= pToken->m_Length || ascii_to_lower(pToken->m_pData[TokenIndex]) != ascii_to_lower(MemberCharacter))
      return false;
    ++TokenIndex;
  }
  return TokenIndex == pToken->m_Length;
}

static bool token_to_int(const SToken *pToken, int *pValue) {
  if (pToken->m_Length == 0 || pToken->m_Length >= 64)
    return false;

  char aBuffer[64];
  memcpy(aBuffer, pToken->m_pData, pToken->m_Length);
  aBuffer[pToken->m_Length] = '\0';

  char *pEnd;
  errno = 0;
  const long Value = strtol(aBuffer, &pEnd, 10);
  if (errno == ERANGE || pEnd != aBuffer + pToken->m_Length || Value < INT_MIN || Value > INT_MAX)
    return false;
  *pValue = (int)Value;
  return true;
}

static bool token_to_tuning_value(const SToken *pToken, float *pValue) {
  if (pToken->m_Length == 0 || pToken->m_Length >= 64)
    return false;

  char aBuffer[64];
  memcpy(aBuffer, pToken->m_pData, pToken->m_Length);
  aBuffer[pToken->m_Length] = '\0';

  // Reject spellings such as "nan" and "inf" before parsing. Besides keeping
  // invalid map settings out of the physics state, this remains reliable when
  // the library is built with finite-math optimizations.
  size_t Index = aBuffer[0] == '+' || aBuffer[0] == '-' ? 1 : 0;
  bool HasDigit = false;
  bool HasDecimalPoint = false;
  bool HasExponent = false;
  for (; aBuffer[Index]; ++Index) {
    if (aBuffer[Index] >= '0' && aBuffer[Index] <= '9') {
      HasDigit = true;
      continue;
    }
    if (aBuffer[Index] == '.' && !HasDecimalPoint && !HasExponent) {
      HasDecimalPoint = true;
      continue;
    }
    if ((aBuffer[Index] == 'e' || aBuffer[Index] == 'E') && HasDigit && !HasExponent) {
      HasExponent = true;
      HasDigit = false;
      if (aBuffer[Index + 1] == '+' || aBuffer[Index + 1] == '-')
        ++Index;
      continue;
    }
    return false;
  }
  if (!HasDigit)
    return false;

  char *pEnd;
  errno = 0;
  const float Value = strtof(aBuffer, &pEnd);
  if (errno == ERANGE || pEnd != aBuffer + pToken->m_Length)
    return false;

  const double CheckedScaledValue = (double)Value * 100.0;
  if (CheckedScaledValue < (double)INT_MIN || CheckedScaledValue > (double)INT_MAX)
    return false;
  const float ScaledValue = Value * 100.0f;
  *pValue = (float)(int)ScaledValue / 100.0f;
  return true;
}

static int tokenize_segment(const char *pStart, const char *pEnd, SToken *pTokens, int MaxTokens) {
  int NumTokens = 0;
  const char *pCursor = pStart;
  while (pCursor < pEnd) {
    while (pCursor < pEnd && (*pCursor == ' ' || *pCursor == '\t' || *pCursor == '\r' || *pCursor == '\n'))
      ++pCursor;
    if (pCursor == pEnd)
      break;

    const bool Quoted = *pCursor == '"';
    if (Quoted)
      ++pCursor;
    const char *pTokenStart = pCursor;
    if (Quoted) {
      while (pCursor < pEnd && *pCursor != '"') {
        if (*pCursor == '\\' && pCursor + 1 < pEnd)
          ++pCursor;
        ++pCursor;
      }
    } else {
      while (pCursor < pEnd && *pCursor != ' ' && *pCursor != '\t' && *pCursor != '\r' && *pCursor != '\n')
        ++pCursor;
    }

    if (NumTokens < MaxTokens)
      pTokens[NumTokens] = (SToken){pTokenStart, (size_t)(pCursor - pTokenStart)};
    ++NumTokens;
    if (Quoted && pCursor < pEnd)
      ++pCursor;
  }
  return NumTokens;
}

static const char *find_segment_end(const char *pStart, bool *pComment) {
  bool Quoted = false;
  bool Escaped = false;
  for (const char *pCursor = pStart; *pCursor; ++pCursor) {
    if (Escaped) {
      Escaped = false;
      continue;
    }
    if (Quoted && *pCursor == '\\') {
      Escaped = true;
      continue;
    }
    if (*pCursor == '"') {
      Quoted = !Quoted;
      continue;
    }
    if (!Quoted && *pCursor == ';')
      return pCursor;
    if (!Quoted && *pCursor == '#') {
      *pComment = true;
      return pCursor;
    }
  }
  return pStart + strlen(pStart);
}

static void set_tuning(STuningParams *pTunings, int Zone, const SToken *pName, float Value) {
  if (!pTunings || Zone < 0 || Zone >= NUM_TUNE_ZONES)
    return;

  for (size_t Index = 0; Index < sizeof(s_aTuningSettings) / sizeof(s_aTuningSettings[0]); ++Index) {
    const STuningSetting *pSetting = &s_aTuningSettings[Index];
    if (strcmp(pSetting->m_pMemberName, "VelrampValue") == 0)
      continue;
    if (!token_equals_tuning_name(pName, pSetting->m_pMemberName))
      continue;
    *(float *)((char *)&pTunings[Zone] + pSetting->m_Offset) = Value;
    return;
  }
}

static void apply_setting(const SToken *pTokens, int NumTokens, SMapSettingsTarget *pTarget) {
  if (NumTokens == 3 && token_equals(&pTokens[0], "tune")) {
    float Value;
    if (token_to_tuning_value(&pTokens[2], &Value))
      set_tuning(pTarget->m_pTunings, 0, &pTokens[1], Value);
    return;
  }

  if (NumTokens == 4 && token_equals(&pTokens[0], "tune_zone")) {
    int Zone;
    float Value;
    if (token_to_int(&pTokens[1], &Zone) && token_to_tuning_value(&pTokens[3], &Value))
      set_tuning(pTarget->m_pTunings, Zone, &pTokens[2], Value);
    return;
  }

  if (NumTokens == 2 && token_equals(&pTokens[0], "switch_open")) {
    int Switch;
    if (pTarget->m_pSwitches && token_to_int(&pTokens[1], &Switch) && Switch >= 0 && Switch < pTarget->m_NumSwitches) {
      pTarget->m_pSwitches[Switch].m_Initial = false;
      pTarget->m_pSwitches[Switch].m_Status = false;
    }
    return;
  }

  if (!pTarget->m_pConfig || NumTokens != 2)
    return;

  if (token_equals(&pTokens[0], "sv_gametype")) {
    if (pTarget->m_pUniqueRace && (token_equals(&pTokens[1], "unique") || token_equals(&pTokens[1], "race") || token_equals(&pTokens[1], "fastcap") ||
                                   token_equals(&pTokens[1], "shorts"))) {
      *pTarget->m_pUniqueRace = true;
      if (token_equals(&pTokens[1], "fastcap"))
        pTarget->m_FastcapGameType = true;
    }
    return;
  }

  for (size_t Index = 0; Index < sizeof(s_aConfigSettings) / sizeof(s_aConfigSettings[0]); ++Index) {
    const SConfigSetting *pSetting = &s_aConfigSettings[Index];
    if (!token_equals(&pTokens[0], pSetting->m_pName))
      continue;

    int Value;
    if (!token_to_int(&pTokens[1], &Value))
      return;
    if (Value < pSetting->m_Min)
      Value = pSetting->m_Min;
    if (Value > pSetting->m_Max)
      Value = pSetting->m_Max;
    *(int *)((char *)pTarget->m_pConfig + pSetting->m_Offset) = Value;
    if (pTarget->m_pUniqueRace &&
        (token_equals(&pTokens[0], "sv_fastcap") || token_equals(&pTokens[0], "sv_kill_grenades") || token_equals(&pTokens[0], "sv_health_and_ammo")))
      *pTarget->m_pUniqueRace = true;
    return;
  }
}

static void apply_setting_line(const char *pLine, SMapSettingsTarget *pTarget) {
  const char *pCursor = pLine;
  while (*pCursor) {
    while (*pCursor == ' ' || *pCursor == '\t' || *pCursor == '\r' || *pCursor == '\n' || *pCursor == ';')
      ++pCursor;
    if (!*pCursor || *pCursor == '#')
      return;

    bool Comment = false;
    const char *pEnd = find_segment_end(pCursor, &Comment);
    SToken aTokens[5];
    const int NumTokens = tokenize_segment(pCursor, pEnd, aTokens, 5);
    if (NumTokens <= 5)
      apply_setting(aTokens, NumTokens, pTarget);
    if (Comment || !*pEnd)
      return;
    pCursor = pEnd + 1;
  }
}

void apply_map_settings(const map_data_t *pMap, SMapSettingsTarget *pTarget) {
  if (!pMap || !pTarget)
    return;

  pTarget->m_FastcapGameType = false;
  for (int Index = 0; Index < pMap->num_settings; ++Index) {
    if (pMap->settings[Index])
      apply_setting_line(pMap->settings[Index], pTarget);
  }

  if (pTarget->m_pTunings) {
    for (int Zone = 0; Zone < NUM_TUNE_ZONES; ++Zone) {
      STuningParams *pTuning = &pTarget->m_pTunings[Zone];
      pTuning->m_VelrampValue = logf(pTuning->m_VelrampCurvature) / pTuning->m_VelrampRange;
    }
  }

  if (pTarget->m_pConfig && pTarget->m_pUniqueRace && *pTarget->m_pUniqueRace) {
    // Unique Race exposes sv_kill_grenades as the legacy spelling of
    // sv_destroy_bullets_on_death.
    pTarget->m_pConfig->m_SvSoloServer = 1;
    pTarget->m_pConfig->m_SvDestroyBulletsOnDeath = pTarget->m_pConfig->m_SvKillGrenades;
    if (pTarget->m_FastcapGameType)
      pTarget->m_pConfig->m_SvFastcap = 1;
    if (pTarget->m_pConfig->m_SvFastcap) {
      pTarget->m_pConfig->m_SvHealthAndAmmo = 1;
      pTarget->m_pConfig->m_SvDestroyBulletsOnDeath = 1;
    }
  }

  if (pTarget->m_pConfig && pTarget->m_pTunings && pTarget->m_pConfig->m_SvSoloServer) {
    for (int Zone = 0; Zone < NUM_TUNE_ZONES; ++Zone) {
      pTarget->m_pTunings[Zone].m_PlayerCollision = 0.0f;
      pTarget->m_pTunings[Zone].m_PlayerHooking = 0.0f;
    }
  }
}
