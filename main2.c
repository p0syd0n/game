#include <raylib.h>
#include <stdbool.h>
#include <time.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

#define PLAYER_COUNT 2
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
struct Sprite monsters[1];

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


void pathfind(struct Sprite* sprite, struct Sprite* target) {

  for (int i = 0; i < NUM_ROWS; i++) {
    for (int j = 0; j < NUM_COLUMNS; j++) {
      tiles[i][j].in_closed_set = false;
      tiles[i][j].parent = NULL;
    }
  }
  printf("Finished setting things up\n");
  struct Tile* begin_tile =  sprite->location;
  struct Tile* current_tile = sprite->location;
  struct Tile* target_tile = target->location;

  struct PriorityQueue pq;
  pq_init(&pq);
  pq_push(&pq, current_tile);
  printf("PQ set up\n");
  while (!pq_is_empty(&pq)) {
    struct Tile* current_tile = pq_pop_min(&pq);

    if (current_tile == target_tile) break;

    int current_g = current_tile->g;

    // Up
    if (current_tile->row > 0) {
      struct Tile* neighbor = &tiles[current_tile->row-1][current_tile->column];
      if (!current_tile->top && !neighbor->bottom) {
        int tentative_g = current_g + 1;
        if (neighbor->parent == NULL || tentative_g < neighbor->g) {
          neighbor->parent = current_tile;
          neighbor->g = tentative_g;
          neighbor->f = tentative_g + abs(neighbor->row - target_tile->row) + abs(neighbor->column - target_tile->column);
          pq_push(&pq, neighbor);
        }
      }
    }

    // Down
    if (current_tile->row < NUM_ROWS - 1) {
      struct Tile* neighbor = &tiles[current_tile->row+1][current_tile->column];
      if (!current_tile->bottom && !neighbor->top) {
        int tentative_g = current_g + 1;
        if (neighbor->parent == NULL || tentative_g < neighbor->g) {
          neighbor->parent = current_tile;
          neighbor->g = tentative_g;
          neighbor->f = tentative_g + abs(neighbor->row - target_tile->row) + abs(neighbor->column - target_tile->column);
          pq_push(&pq, neighbor);
        }
      }
    }

    // Left
    if (current_tile->column > 0) {
      struct Tile* neighbor = &tiles[current_tile->row][current_tile->column-1];
      if (!current_tile->left && !neighbor->right) {
        int tentative_g = current_g + 1;
        if (neighbor->parent == NULL || tentative_g < neighbor->g) {
          neighbor->parent = current_tile;
          neighbor->g = tentative_g;
          neighbor->f = tentative_g + abs(neighbor->row - target_tile->row) + abs(neighbor->column - target_tile->column);
          pq_push(&pq, neighbor);
        }
      }
    }

    // Right
    if (current_tile->column < NUM_COLUMNS - 1) {
      struct Tile* neighbor = &tiles[current_tile->row][current_tile->column+1];
      if (!current_tile->right && !neighbor->left) {
        int tentative_g = current_g + 1;
        if (neighbor->parent == NULL || tentative_g < neighbor->g) {
          neighbor->parent = current_tile;
          neighbor->g = tentative_g;
          neighbor->f = tentative_g + abs(neighbor->row - target_tile->row) + abs(neighbor->column - target_tile->column);
          pq_push(&pq, neighbor);
        }
      }
    }
  }
  printf("FInished the fat loop\n");
  int size = 0;
  struct Tile* path[MAX_NODES] = {0};
  struct Tile* tile = target_tile;  // start at the target
  printf("Starting to trace path from target tile row=%d col=%d\n",
       target_tile->row, target_tile->column);

while (tile != NULL && size < MAX_NODES) {
    printf("Step %d: at tile row=%d col=%d, parent=%p\n",
           size, tile->row, tile->column, (void*)tile->parent);
    path[size++] = tile;

    if (tile->parent == NULL) {
        printf("Reached the root tile, stopping.\n");
        break;
    }

    printf("Moving up to parent tile row=%d col=%d\n",
           tile->parent->row, tile->parent->column);
    tile = tile->parent;
}

printf("Finished tracing path. Path size = %d\n", size);

  printf("got the size\n");
  exit(-1);
  // Optional: reverse the path to go from start -> target
  for (int i = 0; i < size / 2; i++) {
    struct Tile* tmp = path[i];
    path[i] = path[size - 1 - i];
    path[size - 1 - i] = tmp;
  }

  // Print the path
  for (int i = 0; i < size; i++) {
    printf("%d %d\n", path[i]->row, path[i]->column);
  }


    // Store path in sprite
  for (int i = 0; i < size; i++) {
    sprite->path[i] = path[i];
  }
  // mark the end
  if (size < MAX_NODES) sprite->path[size] = NULL;

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
  
  // TODO: check if the player is on the same location as one of the AI's. Implement a sprite death function with a texture change 
}

void update_monster_movement(struct Sprite* monster) {
  printf("Printing it:\n");
  if (monster->path[0] == NULL) {
    printf("Chosen...\n");
    struct Sprite* player = &players[GetRandomValue(0,PLAYER_COUNT-1)];
    printf("About to pathfind\n");
    pathfind(monster, player);
  } else {
  printf("The path is not null\n");
  } 

}
void draw_sprite(struct Sprite* sprite) {
  Rectangle src = { 0, 0, sprite->texture.width, sprite->texture.height };
  Rectangle dst = { sprite->location->x, sprite->location->y, SPRITE_DIM*(0.89), SPRITE_DIM*(0.89)}; // scale 50%
  Vector2 origin = { 0, 0 };
  DrawTexturePro(sprite->texture, src, dst, origin, 0, WHITE);

  char juicebuffer[1024] =  {0};
  snprintf(juicebuffer, 1023, "%s JUICE: %d", sprite->name, sprite->juice);
  char pointsbuffer[1024] = {0};
  snprintf(pointsbuffer, 1023, "%s SCORE: %d", sprite->name, sprite->points);


  DrawText(juicebuffer, WIDTH-300, 20+global_text_offset, 27, (Color){255, 0, 0, 255});
  global_text_offset += 50;
  // We use this variable because we need to make sure that each time we add to this scoreboard,
  // it shifts down a little.
  DrawText(pointsbuffer, WIDTH-300, 20+global_text_offset, 27, (Color){255, 0, 0, 255});
  global_text_offset += 50;
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
        if (players[i].move_timer >= players[i].move_delay) {
          update_player_movement(&players[i]);
          players[i].move_timer = 0;  // reset timer after moving
        }
        draw_sprite(&players[i]);
      }
      global_text_offset = 0;
      for (int i = 0; i < 1; i++) {
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
    struct Sprite posydon = {
      name: "posydon",
      texture:  LoadTexture("posydon.png"),
      powerups: 5,
      powerup_active: false,
      juice: 0,
      location: &tiles[0][0],
      control: WASD,
      move_delay: 0.1
    };

    struct Sprite zeus = {
      name: "zeus",
      texture:  LoadTexture("zeus.png"),
      powerups: 5,
      powerup_active: false,
      juice: 0,
      location: &tiles[0][1],
      control: PL,
      move_delay: 0.1
    };
  struct Sprite monster = {0}; // zero-initialize everything
  monster.name = "monster";
  monster.texture = LoadTexture("monsteer.png");
  monster.location = &tiles[20][20];
  monster.move_delay = 0.1;

  players[0] = posydon;
  players[1] = zeus;

  monsters[0] = monster;



    while (!WindowShouldClose())
    {
      update();
    }

    CloseWindow();

    return 0;
}
