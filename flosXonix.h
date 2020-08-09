#include <GL/glut.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#define TIMER_ID 0
#define TIMER_INTERVAL 20
#define UNUSED_ARG(x) ((void)(x))

static int win_width, win_height;
static bool animation_ongoing = false;
static int level = 1;

/* Svi moguci pokreti heroja u igri
 */
typedef enum {
	PASSIVE,
	NORTH,
	EAST,
	SOUTH,
	WEST
} hero_motion; 

/* Informacije o heroju
 */
typedef struct {
	float position_x;
	float position_y;
	float actual_position_x;
	float actual_position_y;
	int size;
	hero_motion direction;
	float velocity;
	bool drawing_wall;
	bool last_maybe_matrix_check;
} hero_info;

typedef enum {
	FIELD,				// nema zida, trebalo bi da bude popunjeno zidom
	BALL_FIELD,			// ovo je jedina teritorija koja nije popunjena zidom
	WALL,				// popunjena zidom
	MAYBE				// jos uvek se gradi zid
} territory_state;

/* Informacije o loptama
 */
typedef struct {
	float position_x;
	float position_y;
	float actual_position_x;
	float actual_position_y;
	float velocity_x;
	float velocity_y; // brzina je norma vektora
	int velocity_on_last_collision;
	bool wall_horizontal_collision;
	bool wall_vertical_collision;
} ball_info;

/* Informacije o platformi
 */
typedef struct {
	int size_x;
	int size_y;
} field_info;

static hero_info hero;
static ball_info *ball;
static field_info field;
static territory_state **walls_and_territories;

static void game_info_initialization();
static void deinitialize_game();
static void update_actual_position();
static void change_maybes_and_fill();
static bool collision_detection();
static void ball_collision_detection();
static void set_ball_fields(int x, int y);
static bool is_surrounding(int x, int y, territory_state state);
static bool is_over();

static void draw_hero();
static void draw_field();
static void draw_ball();

static void on_display(void);
static void on_keyboard(unsigned char key, int x, int y);
static void on_reshape(int width, int height);
static void on_timer(int value);
