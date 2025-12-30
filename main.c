#include <raylib.h>
#include <stdbool.h>
#include <time.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

#define MONSTER_COUNT 1
#define PLAYER_COUNT 4
#define JUICE 0.2
#define CENTER 0.02

#define WIDTH 1200
#define HEIGHT 800
#define SPRITE_DIM 20
#define NUM_ROWS HEIGHT/SPRITE_DIM
#define NUM_COLUMNS WIDTH/SPRITE_DIM

#define MAX_NODES (NUM_ROWS * NUM_COLUMNS)

#define MOVE_UP    (1 << 0)  // 0001
#define MOVE_DOWN  (1 << 1)  // 0010
#define MOVE_LEFT  (1 << 2)  // 0100
#define MOVE_RIGHT (1 << 3)  // 1000


enum Control {
  WASD,
  TFGH,
  PL,
  ARROWS
};

struct Tile;

struct Sprite {
  char* name;
  int powerups;
  bool powerup_active;
  int juice;
  int points;
  enum Control control;

  bool dead;
  bool monster;
  int lives;

  struct Tile* path[MAX_NODES];
  Texture2D texture;
  struct Tile* location;
  float move_timer;      // counts time since last movement
  float move_delay;      // time required between moves in seconds
};

struct Tile {
  int x;
  int y;

  int row;
  int column;

  Color color;
  int juice;
  bool center;
  bool top, right, left, bottom;
  bool visited; // maze gen 
  bool in_closed_set; // A*
  struct Tile* parent; // A*
  struct Tile* previous; // maze gen
  int g;// a*
  int f; // A*
};

struct PriorityQueue {
  int size;
  struct Tile* queue[MAX_NODES];
};

struct Tile tiles[NUM_ROWS][NUM_COLUMNS];
struct Sprite players[PLAYER_COUNT];
struct Sprite monsters[MONSTER_COUNT];

int global_text_offset = 0;

void pq_init(struct PriorityQueue* pq) {
  pq->size = 0;
}
bool pq_is_empty(struct PriorityQueue* pq) {
  return pq->size == 0;
}
void pq_push(struct PriorityQueue* pq, struct Tile* tile) {
  if (pq->size >= MAX_NODES) {
    return; // or assert
  }

  pq->queue[pq->size++] = tile;
}
struct Tile* pq_pop_min(struct PriorityQueue* pq) {
  while (pq->size > 0) {
    int best_index = 0;
    int best_f = pq->queue[0]->f;

    for (int i = 1; i < pq->size; i++) {
      int f = pq->queue[i]->f;
      if (f < best_f) {
        best_f = f;
        best_index = i;
      }
    }

    struct Tile* best = pq->queue[best_index];

    // Remove by swapping with last
    pq->queue[best_index] = pq->queue[pq->size - 1];
    pq->size--;

    // Skip duplicates / already closed tiles
    if (!best->in_closed_set) {
      best->in_closed_set = true; // mark as closed
      return best;
    }
  }

  return NULL; // queue empty or all duplicates
}
bool pq_contains(struct PriorityQueue* pq, struct Tile* tile) {
  for (int i = 0; i < pq->size; i++) {
    if (pq->queue[i] == tile) {
      return true;
    }
  }
  return false;
}

void die(struct Sprite* sprite) {
  sprite->dead = true;
  sprite->texture = LoadTexture("dead.png");
  if (!sprite->location->center) {
    sprite->location->juice += sprite->juice;
    sprite->juice = 0;
  }
}

void pathfind(struct Sprite* sprite, struct Sprite* target) {
  printf("=== PATHFINDING START ===\n");
  printf("From: (%d, %d) To: (%d, %d)\n", 
         sprite->location->row, sprite->location->column,
         target->location->row, target->location->column);

  // Reset A* state
  for (int i = 0; i < NUM_ROWS; i++) {
    for (int j = 0; j < NUM_COLUMNS; j++) {
      tiles[i][j].in_closed_set = false;
      tiles[i][j].parent = NULL;
      tiles[i][j].g = 0;
      tiles[i][j].f = 0;
    }
  }
  printf("Reset all tiles\n");

  struct Tile* begin_tile = sprite->location;
  struct Tile* target_tile = target->location;

  struct PriorityQueue pq;
  pq_init(&pq);
  
  // Initialize starting tile
  begin_tile->g = 0;
  begin_tile->f = abs(begin_tile->row - target_tile->row) + abs(begin_tile->column - target_tile->column);
  pq_push(&pq, begin_tile);
  printf("Pushed start tile to queue, f=%d\n", begin_tile->f);

  int iterations = 0;
  while (!pq_is_empty(&pq)) {
    iterations++;
    struct Tile* current_tile = pq_pop_min(&pq);
    
    if (current_tile == NULL) {
      printf("ERROR: pq_pop_min returned NULL\n");
      break;
    }

    printf("Iteration %d: Visiting (%d, %d), g=%d, f=%d\n", 
           iterations, current_tile->row, current_tile->column, 
           current_tile->g, current_tile->f);
    
    if (current_tile == target_tile) {
      printf("FOUND TARGET!\n");
      break;
    }

    int current_g = current_tile->g;

    // Check all four neighbors
    struct Tile* neighbors[4] = {NULL};
    int neighbor_count = 0;

    // Up
    if (current_tile->row > 0 && !current_tile->top) {
      neighbors[neighbor_count++] = &tiles[current_tile->row-1][current_tile->column];
      printf("  Can go UP to (%d, %d)\n", current_tile->row-1, current_tile->column);
    }
    // Down
    if (current_tile->row < NUM_ROWS - 1 && !current_tile->bottom) {
      neighbors[neighbor_count++] = &tiles[current_tile->row+1][current_tile->column];
      printf("  Can go DOWN to (%d, %d)\n", current_tile->row+1, current_tile->column);
    }
    // Left
    if (current_tile->column > 0 && !current_tile->left) {
      neighbors[neighbor_count++] = &tiles[current_tile->row][current_tile->column-1];
      printf("  Can go LEFT to (%d, %d)\n", current_tile->row, current_tile->column-1);
    }
    // Right
    if (current_tile->column < NUM_COLUMNS - 1 && !current_tile->right) {
      neighbors[neighbor_count++] = &tiles[current_tile->row][current_tile->column+1];
      printf("  Can go RIGHT to (%d, %d)\n", current_tile->row, current_tile->column+1);
    }

    printf("  Found %d neighbors\n", neighbor_count);

    for (int i = 0; i < neighbor_count; i++) {
      struct Tile* neighbor = neighbors[i];
      if (neighbor->in_closed_set) {
        printf("    Neighbor (%d, %d) already in closed set, skipping\n", 
               neighbor->row, neighbor->column);
        continue;
      }

      int tentative_g = current_g + 1;
      if (neighbor->parent == NULL || tentative_g < neighbor->g) {
        printf("    Updating neighbor (%d, %d): g=%d->%d\n", 
               neighbor->row, neighbor->column, neighbor->g, tentative_g);
        neighbor->parent = current_tile;
        neighbor->g = tentative_g;
        neighbor->f = tentative_g + abs(neighbor->row - target_tile->row) + abs(neighbor->column - target_tile->column);
        pq_push(&pq, neighbor);
      }
    }
  }

  printf("Total iterations: %d\n", iterations);

  // Build path from target back to start
  printf("\n=== BUILDING PATH ===\n");
  int size = 0;
  struct Tile* tile = target_tile;
  
  while (tile != NULL && size < MAX_NODES) {
    printf("Path[%d]: (%d, %d)\n", size, tile->row, tile->column);
    sprite->path[size++] = tile;
    if (tile == begin_tile) {
      printf("Reached start tile, stopping\n");
      break;
    }
    tile = tile->parent;
  }

  printf("Path size before reversal: %d\n", size);

  // Reverse the path so it goes from start to target
  for (int i = 0; i < size / 2; i++) {
    struct Tile* tmp = sprite->path[i];
    sprite->path[i] = sprite->path[size - 1 - i];
    sprite->path[size - 1 - i] = tmp;
  }

  // Mark end of path
  if (size < MAX_NODES) sprite->path[size] = NULL;

  printf("\n=== FINAL PATH (start to target) ===\n");
  for (int i = 0; i < size; i++) {
    printf("Step %d: (%d, %d)\n", i, sprite->path[i]->row, sprite->path[i]->column);
  }
  printf("=== PATHFINDING COMPLETE ===\n\n");
  
}
int check_player_input(enum Control control) {
  int mask = 0;

  switch (control) {
    case WASD:
      if (IsKeyDown(KEY_W)) mask |= MOVE_UP;
      if (IsKeyDown(KEY_S)) mask |= MOVE_DOWN;
      if (IsKeyDown(KEY_A)) mask |= MOVE_LEFT;
      if (IsKeyDown(KEY_D)) mask |= MOVE_RIGHT;
      break;

    case TFGH:
      if (IsKeyDown(KEY_T)) mask |= MOVE_UP;
      if (IsKeyDown(KEY_G)) mask |= MOVE_DOWN;
      if (IsKeyDown(KEY_F)) mask |= MOVE_LEFT;
      if (IsKeyDown(KEY_H)) mask |= MOVE_RIGHT;
      break;

    case PL:
      if (IsKeyDown(KEY_P)) mask |= MOVE_UP;
      if (IsKeyDown(KEY_SEMICOLON)) mask |= MOVE_DOWN;  // ; key
      if (IsKeyDown(KEY_L)) mask |= MOVE_LEFT;
      if (IsKeyDown(KEY_APOSTROPHE)) mask |= MOVE_RIGHT;      // ' key
      break;

    case ARROWS:
      if (IsKeyDown(KEY_UP)) mask |= MOVE_UP;
      if (IsKeyDown(KEY_DOWN)) mask |= MOVE_DOWN;
      if (IsKeyDown(KEY_LEFT)) mask |= MOVE_LEFT;
      if (IsKeyDown(KEY_RIGHT)) mask |= MOVE_RIGHT;
      break;
  }

  return mask;
}

void update_player_movement(struct Sprite* sprite) {
  int inputs_bitmap = check_player_input(sprite->control);
  // Move Up
  if ((inputs_bitmap & MOVE_UP) &&
    sprite->location->row > 0 &&
    (!sprite->location->top || sprite->powerup_active))
  {
    sprite->location = &tiles[sprite->location->row-1][sprite->location->column];
  }

  // Move Down
  if ((inputs_bitmap & MOVE_DOWN) &&
    sprite->location->row < NUM_ROWS - 1 &&
    (!sprite->location->bottom || sprite->powerup_active))
  {
    sprite->location = &tiles[sprite->location->row+1][sprite->location->column];
  }

  // Move Left
  if ((inputs_bitmap & MOVE_LEFT) &&
    sprite->location->column > 0 &&
    (!sprite->location->left || sprite->powerup_active))
  {
    sprite->location = &tiles[sprite->location->row][sprite->location->column-1];
  }

  // Move Right
  if ((inputs_bitmap & MOVE_RIGHT) &&
    sprite->location->column < NUM_COLUMNS - 1 &&
    (!sprite->location->right || sprite->powerup_active))
  {
    sprite->location = &tiles[sprite->location->row][sprite->location->column+1];
  }

  if (sprite->location->juice > 0) {
    // TODO: floating point juice?
    sprite->juice += (int)(0.5 * sprite->location->juice);
    sprite->location->juice = 0;
  } else if (sprite->location->center) {
    sprite->points += sprite->juice;
    sprite->juice = 0;
  }
  for (int i = 0; i < MONSTER_COUNT; i++) {
    if (sprite->location ==  monsters[i].location) {
      sprite->lives -= 1;
      if (sprite->lives == 0)  {
        die(sprite);
      }
    }
  } 
  // TODO: check if the player is on the same location as one of the AI's. Implement a sprite death function with a texture change 
}
void update_monster_movement(struct Sprite* monster) {
  if (monster->path[0] == NULL) {
    // Need to generate a new path
    struct Sprite* player = &players[GetRandomValue(0, PLAYER_COUNT-1)];
    pathfind(monster, player);
  }
  
  // Skip tiles in the path that match current location
  while (monster->path[0] != NULL && monster->path[0] == monster->location) {
    printf("Skipping current location in path\n");
    for (int i = 0; i < MAX_NODES - 1; i++) {
      monster->path[i] = monster->path[i + 1];
      if (monster->path[i] == NULL) break;
    }
  }
  
  // Follow the path
  if (monster->path[0] != NULL) {
    monster->location = monster->path[0];
    printf("Monster moved to (%d, %d)\n", monster->location->row, monster->location->column);
    
    // Shift the path array
    for (int i = 0; i < MAX_NODES - 1; i++) {
      monster->path[i] = monster->path[i + 1];
      if (monster->path[i] == NULL) break;
    }
  }
}
void draw_sprite(struct Sprite* sprite) {
  Rectangle src = { 0, 0, sprite->texture.width, sprite->texture.height };
  Rectangle dst = { sprite->location->x, sprite->location->y, SPRITE_DIM*(0.89), SPRITE_DIM*(0.89)}; // scale 50%
  Vector2 origin = { 0, 0 };
  DrawTexturePro(sprite->texture, src, dst, origin, 0, WHITE);
  if (sprite->monster) return;
  char juicebuffer[1024] =  {0};
  snprintf(juicebuffer, 1023, "%s JUICE: %d & SCORE: %d & LIVES: %d", sprite->name, sprite->juice, sprite->points, sprite->lives);


  DrawText(juicebuffer, WIDTH-700, 20+global_text_offset, 27, (Color){255, 0, 0, 255});
  global_text_offset += 50;
  // We use this variable because we need to make sure that each time we add to this scoreboard,
  // it shifts down a little.
}

void generate_maze() {
  for (int i=0; i<(NUM_ROWS);i++) {
    for (int j=0; j<(NUM_COLUMNS);j++)  {
      struct Tile* this_one = &tiles[i][j];
      this_one->x = SPRITE_DIM * j;
      this_one->y = SPRITE_DIM * i;
      this_one->row = i;
      this_one->column = j;
      this_one->top = true;
      this_one->bottom = true;
      this_one->left = true;
      this_one->right = true;
      this_one->visited = false;
      this_one->juice = 0;
      // Where JUICE is like 0.2 ->  20% of juice on each square
      if (GetRandomValue(1,100) < (JUICE*100))  {
        this_one->juice = GetRandomValue(0, 255);
      } else if (GetRandomValue(1, 100) < (CENTER*100)) {
        this_one->center = true;
      }


    }
  }
  

  int first_index_row = GetRandomValue(0, NUM_ROWS-1);
  int first_index_column = GetRandomValue(0, NUM_COLUMNS-1);

  struct Tile* current_tile = &tiles[first_index_row][first_index_column];
  current_tile->previous = NULL;
  current_tile->visited = true;

  do {
    struct Tile* neighbors[4] = {0};
    int neighbors_index = 0;

    if (current_tile->row > 0 && !tiles[current_tile->row-1][current_tile->column].visited)  {
      neighbors[neighbors_index++] = &tiles[current_tile->row-1][current_tile->column];
    }

    if (current_tile->column > 0 && !tiles[current_tile->row][current_tile->column-1].visited)  {
      neighbors[neighbors_index++] = &tiles[current_tile->row][current_tile->column-1];
    }
    
    if (current_tile->row < NUM_ROWS-1 && !tiles[current_tile->row+1][current_tile->column].visited) {
      neighbors[neighbors_index++] =  &tiles[current_tile->row+1][current_tile->column];
    }
     
    if (current_tile->column < NUM_COLUMNS-1 && !tiles[current_tile->row][current_tile->column+1].visited) {
      neighbors[neighbors_index++] = &tiles[current_tile->row][current_tile->column+1];
    }

    // printf("We have a list of %d neighbors for tile row %d col %d\n", neighbors_index, current_tile->row, current_tile->column);


    if (neighbors_index == 0) {
      // printf("There were no neighbors :(\n");
      current_tile = current_tile->previous;
      continue;
    }

    struct Tile* neighbor_choice = neighbors[GetRandomValue(0, neighbors_index-1)];
    // printf("A neighbor has been chosen! He is at row %d column %d\n", neighbor_choice->row, neighbor_choice->column);

    if (neighbor_choice->row > current_tile->row) {
      current_tile->bottom = false;
      neighbor_choice->top = false;
    } else if (neighbor_choice->row < current_tile->row) {
      current_tile->top = false;
      neighbor_choice->bottom = false;
    } else if (neighbor_choice->column > current_tile->column) {
      current_tile->right = false;
      neighbor_choice->left = false;
    } else if (neighbor_choice->column < current_tile->column) {
      current_tile->left = false;
      neighbor_choice->right = false;
    }

    neighbor_choice->previous = current_tile;
    neighbor_choice->visited = true;

    current_tile = neighbor_choice;
    
  } while (current_tile->previous != NULL);
}

int setup() {

    // SetRandomSeed((unsigned long)time(NULL));
    SetRandomSeed(0);
    InitWindow(WIDTH, HEIGHT, "raylib [core] example - basic window");
    InitAudioDevice();
    if (!IsAudioDeviceReady()) {
      exit(-1);
    }

      Sound libet = LoadSoundFromWave(LoadWave("libet.wav"));
    SetSoundVolume(libet, 0.3);
      PlaySound(libet);
  
  generate_maze();
    
}

void update() {
  BeginDrawing();
      ClearBackground(RAYWHITE);
      for (int i = 0; i < NUM_ROWS; i++) {
        for (int j = 0; j < NUM_COLUMNS; j++) {
          struct Tile* current_tile = &tiles[i][j];
          if (current_tile->juice > 0) {
            DrawCircle(current_tile->x+(0.5*SPRITE_DIM), current_tile->y+(0.5*SPRITE_DIM), (SPRITE_DIM/200.0)*(100*fmin(current_tile->juice, 255)/255.0), (Color){0, fmin(current_tile->juice, 255), 0, fmin(current_tile->juice, 255)});
          } else if (current_tile->center) {
            DrawRectangle(current_tile->x, current_tile->y, SPRITE_DIM, SPRITE_DIM, (Color){0, 0, 255, 100});
          }

          if (current_tile->bottom) {
            DrawLine(current_tile->x, current_tile->y+SPRITE_DIM, current_tile->x+SPRITE_DIM, current_tile->y+SPRITE_DIM, (Color){255, 0, 0, 255});
          } 
          
          if (current_tile->top) {
            DrawLine(current_tile->x, current_tile->y, current_tile->x+SPRITE_DIM, current_tile->y, (Color){255, 0, 0, 255});
          }

          if (current_tile->left) {
            DrawLine(current_tile->x, current_tile->y, current_tile->x, current_tile->y+SPRITE_DIM, (Color){255, 0, 0, 255});
          } 

          if (current_tile->right) {
            DrawLine(current_tile->x+SPRITE_DIM, current_tile->y, current_tile->x+SPRITE_DIM, current_tile->y+SPRITE_DIM, (Color){255, 0, 0, 255});
          }
        }
      }
  
      for (int i = 0; i < PLAYER_COUNT; i++) {
        players[i].move_timer += GetFrameTime();  // delta time since last frame
        if (players[i].move_timer >= players[i].move_delay && !players[i].dead) {
          update_player_movement(&players[i]);
          players[i].move_timer = 0;  // reset timer after moving
        }
        draw_sprite(&players[i]);
      }
      global_text_offset = 0;
      for (int i = 0; i < MONSTER_COUNT; i++) {
        monsters[i].move_timer += GetFrameTime();  // delta time since last frame
        if (monsters[i].move_timer >= monsters[i].move_delay) {
          update_monster_movement(&monsters[i]);
          monsters[i].move_timer = 0;  // reset timer after moving
        }
        draw_sprite(&monsters[i]);
      }


  
  EndDrawing();

}

int main(void)
{
     
    setup();
    struct Sprite arthur = {
      name: "posydon",
      texture:  LoadTexture("posydon.png"),
      powerups: 5,
      powerup_active: false,
      juice: 0,
      location: &tiles[0][0],
      control: WASD,
      move_delay: 0.1,
      dead: false,
      lives: 5
    };

    struct Sprite thomas = {
      name: "Thomas",
      texture:  LoadTexture("thomas.png"),
      powerups: 5,
      powerup_active: false,
      juice: 0,
      location: &tiles[0][1],
      control: PL,
      move_delay: 0.1,
      dead: false,
      lives: 10
    };

  struct Sprite rich = {
      name: "Rich",
      texture:  LoadTexture("rich.png"),
      powerups: 5,
      powerup_active: false,
      juice: 0,
      location: &tiles[0][1],
      control: TFGH,
      move_delay: 0.1,
      dead: false,
      lives: 10
    };
      struct Sprite mom = {
      name: "mom",
      texture:  LoadTexture("mom.png"),
      powerups: 5,
      powerup_active: false,
      juice: 0,
      location: &tiles[0][1],
      control: ARROWS,
      move_delay: 0.1,
      dead: false,
      lives: 10
    };

  struct Sprite monster = {0}; // zero-initialize everything
  monster.name = "monster";
  monster.texture = LoadTexture("monsteer.png");
  monster.location = &tiles[20][20];
  monster.move_delay = 0.5;
  monster.monster = true;

  players[0] = arthur;
  players[1] = thomas;
  players[2] = rich;
  players[3] = mom;

  monsters[0] = monster;



    while (!WindowShouldClose())
    {
      update();
    }

    CloseWindow();

    return 0;
}
