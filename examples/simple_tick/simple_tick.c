#include <ddnet_map_loader.h>
#include <ddnet_physics/collision.h>
#include <ddnet_physics/gamecore.h>
#include <stdio.h>

int main()
{
  SConfig config = {};
  SWorldCore world = wc_empty();
  SCollision collision = {};
  STeeGrid tee_grid = {};

  map_data_t map = load_map("tinycave.map");
  if(!map._map_file_data)
  {
    puts("failed to load map");
    return 1;
  }

  if(!init_collision(&collision, &map))
  {
    puts("failed to init collision");
    return 1;
  }

  init_config(&config);
  tg_init(&tee_grid, collision.m_MapData.width, collision.m_MapData.height);
  wc_init(&world, &collision, &tee_grid, &config);

  if(!wc_add_character(&world, 1))
  {
    puts("failed to add character");
    return 1;
  }

  for(int i = 0; i < world.m_NumCharacters; i++)
  {
    SCharacterCore *chr = &world.m_pCharacters[i];
    SPlayerInput input = {
      .m_Direction = 1,
      .m_TargetX = 10,
      .m_TargetY = 10,
      .m_Fire = 1,
    };
    cc_on_input(chr, &input);
  }

  wc_tick(&world);

  for(int i = 0; i < world.m_NumCharacters; i++)
  {
    SCharacterCore *chr = &world.m_pCharacters[i];
    const int x = ((int)vgetx(chr->m_Pos) >> 5) - MAP_EXPAND;
    const int y = ((int)vgety(chr->m_Pos) >> 5) - MAP_EXPAND;
    printf("character at %d %d\n", x, y);
  }

  wc_free(&world);
  tg_destroy(&tee_grid);
  free_collision(&collision);
}
