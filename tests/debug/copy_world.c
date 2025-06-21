#include "../../include/gamecore.h"
#include "collision.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Helper function to initialize a world with sample data
void setup_test_world(SWorldCore *pWorld, SConfig *pConfig, SCollision *pCollision) {
  wc_init(pWorld, pCollision, pConfig);

  // Add a character
  SCharacterCore *pChar = wc_add_character(pWorld);
  pChar->m_Pos = vec2_init(100, 200);
  pChar->m_Vel = vec2_init(5, -3);
  pChar->m_ActiveWeapon = WEAPON_SHOTGUN;
  pChar->m_aWeaponGot[WEAPON_HAMMER] = true;
  pChar->m_Jumped = 2;
  pChar->m_FreezeTime = 100;
  pChar->m_HookedPlayer = 42;

  // Add a projectile entity
  SProjectile *pProj = (SProjectile *)malloc(sizeof(SProjectile));
  memset(pProj, 0, sizeof(SProjectile));
  pProj->m_Base.m_pWorld = pWorld;
  pProj->m_Base.m_Pos = vec2_init(300, 400);
  pProj->m_Base.m_ObjType = WORLD_ENTTYPE_PROJECTILE;
  pProj->m_Base.m_Number = 1;
  pProj->m_Direction = vec2_init(1, 0);
  pProj->m_LifeSpan = 50;
  pProj->m_Owner = 1;
  pProj->m_Explosive = true;
  wc_insert_entity(pWorld, &pProj->m_Base);

  // Add a laser entity
  SLaser *pLaser = (SLaser *)malloc(sizeof(SLaser));
  memset(pLaser, 0, sizeof(SLaser));
  pLaser->m_Base.m_pWorld = pWorld;
  pLaser->m_Base.m_Pos = vec2_init(500, 600);
  pLaser->m_Base.m_ObjType = WORLD_ENTTYPE_LASER;
  pLaser->m_Base.m_Number = 2;
  pLaser->m_From = vec2_init(450, 550);
  pLaser->m_Dir = vec2_init(0, 1);
  pLaser->m_Energy = 10.0f;
  pLaser->m_Bounces = 3;
  pLaser->m_Owner = 1;
  pLaser->m_IsBlueTeleport = true;
  wc_insert_entity(pWorld, &pLaser->m_Base);

  // Set up switches
  for (int i = 0; i < pWorld->m_NumSwitches; ++i) {
    pWorld->m_pSwitches[i].m_Status = true;
    pWorld->m_pSwitches[i].m_EndTick = 1000;
    pWorld->m_pSwitches[i].m_Type = 1;
  }
}

// Helper function to compare two worlds
void compare_worlds(SWorldCore *pWorld1, SWorldCore *pWorld2) {
  assert(pWorld1->m_pCollision == pWorld2->m_pCollision);
  assert(pWorld1->m_pConfig == pWorld2->m_pConfig);
  assert(pWorld1->m_pTunings == pWorld2->m_pTunings);
  assert(pWorld1->m_NumCharacters == pWorld2->m_NumCharacters);
  assert(pWorld1->m_GameTick == pWorld2->m_GameTick);
  assert(pWorld1->m_NoWeakHook == pWorld2->m_NoWeakHook);
  assert(pWorld1->m_NoWeakHookAndBounce == pWorld2->m_NoWeakHookAndBounce);

  // Compare characters
  for (int i = 0; i < pWorld1->m_NumCharacters; i++) {
    SCharacterCore *c1 = &pWorld1->m_pCharacters[i];
    SCharacterCore *c2 = &pWorld2->m_pCharacters[i];
    assert(vvcmp(c1->m_Pos, c2->m_Pos));
    assert(vvcmp(c1->m_Vel, c2->m_Vel));
    assert(c1->m_ActiveWeapon == c2->m_ActiveWeapon);
    for (int j = 0; j < NUM_WEAPONS; j++) {
      assert(c1->m_aWeaponGot[j] == c2->m_aWeaponGot[j]);
    }
    assert(c1->m_Jumped == c2->m_Jumped);
    assert(c1->m_FreezeTime == c2->m_FreezeTime);
    assert(c1->m_HookedPlayer == c2->m_HookedPlayer);
  }

  // Compare entities
  for (int i = 0; i < NUM_WORLD_ENTTYPES; i++) {
    SEntity *e1 = pWorld1->m_apFirstEntityTypes[i];
    SEntity *e2 = pWorld2->m_apFirstEntityTypes[i];
    while (e1 && e2) {
      assert(e1->m_ObjType == e2->m_ObjType);
      assert(e1->m_Number == e2->m_Number);
      assert(vvcmp(e1->m_Pos, e2->m_Pos));
      if (e1->m_ObjType == WORLD_ENTTYPE_PROJECTILE) {
        SProjectile *p1 = (SProjectile *)e1;
        SProjectile *p2 = (SProjectile *)e2;
        assert(vvcmp(p1->m_Direction, p2->m_Direction));
        assert(p1->m_LifeSpan == p2->m_LifeSpan);
        assert(p1->m_Owner == p2->m_Owner);
        assert(p1->m_Explosive == p2->m_Explosive);
      } else if (e1->m_ObjType == WORLD_ENTTYPE_LASER) {
        SLaser *l1 = (SLaser *)e1;
        SLaser *l2 = (SLaser *)e2;
        assert(vvcmp(l1->m_From, l2->m_From));
        assert(vvcmp(l1->m_Dir, l2->m_Dir));
        assert(l1->m_Energy == l2->m_Energy);
        assert(l1->m_Bounces == l2->m_Bounces);
        assert(l1->m_Owner == l2->m_Owner);
        assert(l1->m_IsBlueTeleport == l2->m_IsBlueTeleport);
      }
      e1 = e1->m_pNextTypeEntity;
      e2 = e2->m_pNextTypeEntity;
    }
    assert(e1 == NULL && e2 == NULL); // Ensure same number of entities
  }

  // Compare switches
  for (int i = 0; i < pWorld1->m_NumSwitches; i++) {
    assert(pWorld1->m_pSwitches[i].m_Status == pWorld2->m_pSwitches[i].m_Status);
    assert(pWorld1->m_pSwitches[i].m_EndTick == pWorld2->m_pSwitches[i].m_EndTick);
    assert(pWorld1->m_pSwitches[i].m_Type == pWorld2->m_pSwitches[i].m_Type);
  }
}

int main() {
  // Initialize config and collision
  SConfig config;
  init_config(&config);
  SCollision collision;
  init_collision(&collision, "maps/ctf0.map");

  // Create source and destination worlds
  SWorldCore srcWorld = wc_empty();
  SWorldCore dstWorld = wc_empty();

  // Set up source world with sample data
  setup_test_world(&srcWorld, &config, &collision);

  // Copy source to destination
  wc_copy_world(&dstWorld, &srcWorld);

  // Compare worlds
  compare_worlds(&srcWorld, &dstWorld);

  // Clean up
  wc_free(&srcWorld);
  wc_free(&dstWorld);

  free_collision(&collision);

  printf("All tests passed!\n");
  return 0;
}
