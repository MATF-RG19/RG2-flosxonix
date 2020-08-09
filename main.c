#include <GL/glut.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#define TIMER_ID 0
#define TIMER_INTERVAL 20
#define UNUSED_ARG(x) ((void)(x))

static int win_width, win_height;
static bool animation_ongoing = false;

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
	float old_velocity_x;
	float old_velocity_y; // brzina je norma vektora
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
static ball_info ball;
static field_info field;
static territory_state **walls_and_territories;

static void game_info_initialization();
static void deinitialize_game();
static void update_actual_position();
static void change_maybes_and_fill();
static bool collision_detection();
static bool ball_collision_detection();
static void set_ball_fields(int x, int y);

static void draw_hero();
static void draw_field();
static void draw_ball();

static void on_display(void);
static void on_keyboard(unsigned char key, int x, int y);
static void on_reshape(int width, int height);
static void on_timer(int value);

static void test(){
	for(int i = 0; i < field.size_y * 10; i += 10){
		for(int j = 0; j < field.size_x * 10; j += 10){
			switch(walls_and_territories[i][j]){
				case FIELD:
					printf("O ");
					break;
				case WALL:
					printf("1 ");
					break;
				case MAYBE:
					printf("v ");
					break;
				case BALL_FIELD:
					printf("B ");
					break;
				default:
					break;
			}
		}
		puts("");
	}
	puts("**************");
}

static void test_field(){
	int temp_x = hero.position_x * 10;
	int temp_y = hero.position_y * 10;
	for(int i = temp_y; i < temp_y + 10; ++i){	
		for(int j = temp_x; j < temp_x + 10; ++j){
			switch(walls_and_territories[i][j]){
				case FIELD:
					printf("O ");
					break;
				case WALL:
					printf("1 ");
					break;
				case MAYBE:
					printf("v ");
					break;
				case BALL_FIELD:
					printf("B ");
					break;
				default:
					break;
			}
		}
		puts("");
	}
	printf("##################################\n");
}

static void game_info_initialization()
{
	field.size_x = 20;
	field.size_y = 20;

	hero.velocity = 0.5; // (0.1, +inf) nista izmedju .10...01 - 0.199...99
	hero.position_x = (field.size_x - 1) / 2.0;
	hero.position_y = 0;
	hero.size = 1;
	hero.direction = PASSIVE;
	hero.drawing_wall = false;
	hero.last_maybe_matrix_check = false;
	
	ball.position_x = 2;
	ball.position_y = 8;
	ball.velocity_x = 0.1;
	ball.velocity_y = ball.velocity_x;
	ball.velocity_on_last_collision = 1;
	/* Alokacija i inicijalizacija pojedinih polja za pocetni oblik zida*/
	walls_and_territories = malloc(sizeof(*walls_and_territories) * field.size_x * field.size_y * 100);
	if(NULL == walls_and_territories){
		perror("aloc walls_and_territories");
		exit(EXIT_FAILURE);
	}
	for(int i = 0; i < field.size_y * 10; ++i){
		walls_and_territories[i] = malloc(sizeof(*walls_and_territories[i]) * field.size_x * 10);
		if(NULL == walls_and_territories[i]){
			perror("aloc walls_and_territories inside for");
			exit(EXIT_FAILURE);
		}
		for(int j = 0; j < field.size_x * 10; ++j){
			if(i < 10 || j < 10 ||
			   (i >= (field.size_x - 1) * 10 && i < field.size_x * 10) ||
			   (j >= (field.size_y - 1) * 10 && j < field.size_y * 10)){
				walls_and_territories[i][j] = WALL;
			}else{
				walls_and_territories[i][j] = FIELD;
			}
		}
	}
	update_actual_position();
}

static void deinitialize_game()
{
	for(int i = 0; i < field.size_y * 10; ++i){
		free(walls_and_territories[i]);
	}
	free(walls_and_territories);
}

/* Vrsi se izracunavanje koordinata u prostoru.
 * Pozicija za heroja (0, 0) je u donjem levom uglu 
 */
static void update_actual_position()
{
	hero.actual_position_x = (field.size_x - 1) / 2.0 - hero.position_x;
	hero.actual_position_y = -(field.size_y - 1) / 2.0 + hero.position_y;
	ball.actual_position_x = (field.size_x - 1) / 2.0 - ball.position_x + 0.5;
	ball.actual_position_y = -(field.size_y - 1) / 2.0 + ball.position_y - 0.5;
	int temp_x = hero.position_x * 10;
	int temp_y = hero.position_y * 10;
	/* TODO dodati novo stanje maybe pored true/false za "moguci" zid
	 */
	/* Popunjavanje zida na osnovu pozicije heroja */
	if(hero.drawing_wall){
//		animation_ongoing =false;// DEBUGGIN'
		bool whole_maybe_matrix = true;
		for(int i = temp_y; i < temp_y + 10; ++i){	
			for(int j = temp_x; j < temp_x + 10; ++j){
				switch(walls_and_territories[i][j]){
					case FIELD:
						whole_maybe_matrix = false;
						break;
					case WALL:
						whole_maybe_matrix = false;
						break;
					case MAYBE:
						break;
					default:
						break;
				}
			}
		}
		// OKs
		// 000 vvv 
		// vvv vvv
		// vvv 000
		//
		// 0vv vv0
		// 0vv vv0
		// 0vv vv0
		//
		// vvv 000
		// 000 000
		// 000 vvv
		//        
		// v00 00v
		// v00 00v
		// v00 00v
		//
		// i.e. gameover
		// v v v v v v v v v v 
		// v v v v v v v v v v 
		// v v v v v v v v v v 
		// v v v v v v v v v v 
		// v v v v v v v v v v 
		// v v v v v v v v v v 
		// v v v v v v O O O O 
		// v v v v v v O O O O 
		// v v v v v v O O O O 
		// v v v v v v O O O O 
		bool whole_line = true;
		territory_state temp;
		/*
		*/
		for(int i = temp_y; i < temp_y + 10; ++i){	
			temp = walls_and_territories[i][temp_x];
			for(int j = temp_x; j < temp_x + 10; ++j){	
				if(walls_and_territories[i][j] != temp){ // FIXME da radi samo sa MAYBE i FIELD, a WALL ignorise
					whole_line = false;
					break;
				}
			}
			if(!whole_line) break;
		}
		if(!whole_line){
			for(int j = temp_x; j < temp_x + 10; ++j){	
				temp = walls_and_territories[temp_y][j];
				for(int i = temp_y; i < temp_y + 10; ++i){	
					if(walls_and_territories[i][j] != temp){ // FIXME kao gore navedeno
					// GAME OVER case 
					// cosak potencijalnog zida sa nekim drugim se sudara
						deinitialize_game();
						game_info_initialization();
					}
				}
			}
		}
		//test_field();
		//
		// TODO drawing stops
		// v v v v v v O O 1 1 
		// v v v v v v O O 1 1 
		// v v v v v v O O 1 1 
		// v v v v v v O O 1 1 
		// v v v v v v O O 1 1 
		// v v v v v v O O 1 1 
		// v v v v v v O O 1 1 
		// v v v v v v O O 1 1 
		// v v v v v v O O 1 1 
		// v v v v v v O O 1 1 
		//
		

		if(hero.last_maybe_matrix_check && whole_maybe_matrix){
			// GAME OVER CASE 
			// presek dva potencijalna zida
			deinitialize_game();
			game_info_initialization();
			return;
		}else{
			hero.last_maybe_matrix_check = whole_maybe_matrix;
		}
	}
	// FIXME zavrsava se drawing_wall u gornjoj implementaciji, ne u donjoj
	temp_x = hero.position_x * 10;
	temp_y = hero.position_y * 10;
	bool maybe_exists = false;
	for(int i = temp_y; i < temp_y + 10; ++i){	
		for(int j = temp_x; j < temp_x + 10; ++j){
			if(walls_and_territories[i][j] == FIELD){
				if(!hero.drawing_wall) hero.drawing_wall = true;
				maybe_exists = true;
				walls_and_territories[i][j] = MAYBE;
			}else if(walls_and_territories[i][j] == MAYBE){
				maybe_exists = true;
			}
		}
	}
	if(walls_and_territories[temp_y][temp_x] == WALL && !maybe_exists && hero.drawing_wall){
		hero.drawing_wall = false;
		change_maybes_and_fill();
	}
	// test(); // DEBUGGIN'
}

static void change_maybes_and_fill(){
	for(int i = 0; i < field.size_y * 10; ++i){	
		for(int j = 0; j < field.size_x * 10; ++j){
			if(walls_and_territories[i][j] == MAYBE){
				walls_and_territories[i][j] = WALL;
			}
		}
	}
	/* dfs menheten rastojanje po lopti, postavljanje stanja loptine teritorije */
	printf("%f , %f\n", ball.position_x, ball.position_y);
	set_ball_fields((int)ball.position_x, (int)ball.position_y);

	test();
	/* popunjavanje obicnih polja */
	for(int i = 0; i < field.size_y * 10; ++i){	
		for(int j = 0; j < field.size_x * 10; ++j){
			if(walls_and_territories[i][j] == FIELD){
				walls_and_territories[i][j] = WALL; 
				/* TODO Uvodjenje novog stanja za animaciju uzdizanja zida u buducnosti */
			}
		}
	}
	/* reset loptine teritorije u obicna polja*/
	for(int i = 0; i < field.size_y * 10; ++i){	
		for(int j = 0; j < field.size_x * 10; ++j){
			if(walls_and_territories[i][j] == BALL_FIELD){
				walls_and_territories[i][j] = FIELD; 
				/* TODO Uvodjenje novog stanja za animaciju uzdizanja zida u buducnosti */
			}
		}
	}
}

// FIXME Zna da pretera(crta gde ne treba), problem je tip podataka argumenata verovatno(zaokruzivanje)
// NOTE ovde smetaju one vrednosti brzine kretanja heroja sto su izmedju 0.1 i 0.2
static void set_ball_fields(int x, int y)
{
	int temp_x = x * 10;
	int temp_y = y * 10;
	
	if(walls_and_territories[temp_y][temp_x] == WALL){
		return;
	}
	if(walls_and_territories[temp_y][temp_x] == BALL_FIELD){
		return;
	}
	
	for(int i = temp_y; i < temp_y + 10; ++i){	
		for(int j = temp_x; j < temp_x + 10; ++j){
			if(walls_and_territories[i][j] == FIELD){
				walls_and_territories[i][j] = BALL_FIELD;
			}
		}
	}
	set_ball_fields(x + 1, y);
	set_ball_fields(x, y + 1);
	set_ball_fields(x - 1, y);
	set_ball_fields(x, y - 1);
}

/* Provera za one timer */
static bool collision_detection()
{
	if(hero.position_x < 0){
		hero.position_x = 0;
		return true;
	}else if(hero.position_x > field.size_x - 1){
		hero.position_x = field.size_x - 1;
		return true;
	}
	if(hero.position_y < 0){
		hero.position_y = 0;
		return true;
	}else if(hero.position_y > field.size_y - 1){
		hero.position_y = field.size_y - 1;
		return true;
	}
	return false;
}

// TODO 
// Obrada kolizija izmedju vise lopti
static bool ball_collision_detection()
{
	/* prilagodjavanje za matricu koja predstavlja postavljene zidove */
	/* uzima se informacija radi provera buduce pozicije lopte */
	int temp_x = ((ball.position_x) + ball.velocity_x) * 10 ;
	int temp_y = ((ball.position_y) + ball.velocity_y) * 10 ;
	if(walls_and_territories[temp_y][temp_x] == MAYBE || 
	/* GAME OVER CASE TODO
	 * Ovo je za sada ovako, ubaciti mogucnost da se izgubi igra ako se dotakne potencijalni zid
	 * ili da neko vreme dok ne postane potencijalni zid - pravi zid
	 */
	   walls_and_territories[temp_y][temp_x] == WALL){
		/* Resavanje iscrtavanja ulaska lopte u koliziju */
		ball.position_x -=   ball.velocity_x;
		ball.position_y -=   ball.velocity_y;
		/* Gledanje situacije pre nego sto je lopta usla u koliziju*/
		temp_x -= ball.velocity_x * 10;
		temp_y -= ball.velocity_y * 10;

		/****
		 *1
		 *
		 *7
		 ***/

		/* Gledanje slucajeva kada je
		 * lopta udarila u horizontalnu/vertikalnu stranu zida
		 * suzbijanje situacije kada je zid 2 puta
		 * udarena horizontalna/vertikalna strana zida.
		 */
		if((walls_and_territories[temp_y + 10][temp_x + 10]) == WALL &&
		   (walls_and_territories[temp_y - 10][temp_x + 10]) == WALL){
			if(ball.wall_vertical_collision) ball.velocity_y *= -1;
			ball.wall_vertical_collision = true;
			ball.wall_horizontal_collision = false;
		}
		/****
		 *  3
		 *
		 *  9
		 ***/
		else if((walls_and_territories[temp_y + 10][temp_x - 10]) == WALL &&
			(walls_and_territories[temp_y - 10][temp_x - 10]) == WALL){
			if(ball.wall_vertical_collision) ball.velocity_y *= -1;
			ball.wall_vertical_collision = true;
			ball.wall_horizontal_collision = false;
		}
		/****
		 *
		 *
		 *7 9
		 ****/
		else if((walls_and_territories[temp_y - 10][temp_x - 10]) &&
			(walls_and_territories[temp_y - 10][temp_x + 10])){
			if(ball.wall_horizontal_collision)
				ball.velocity_x *= -1;
			ball.wall_vertical_collision = false;
			ball.wall_horizontal_collision = true;
		}
		/***
		 *1 3
		 *
		 ***/
		else if((walls_and_territories[temp_y + 10][temp_x - 10]) == WALL &&
			(walls_and_territories[temp_y + 10][temp_x + 10]) == WALL
		){
			if(ball.wall_horizontal_collision)
				ball.velocity_x *= -1;
			ball.wall_vertical_collision = false;
			ball.wall_horizontal_collision = true;
		}
		
		/*** DEBUG U sluzbi debagovanja
		 * 123
		 * 456
		 * 789
		 ***
		else if((temp_x - 10 > 0)&&(temp_y + 10 < field.size_y * 10)){
			printf("%d", walls_and_territories[temp_y + 10][temp_x - 10]);
		}else{
			putchar('x');
		}
		if((temp_y + 10 < field.size_y * 10)){
			printf("%d", walls_and_territories[temp_y + 10][temp_x]);
		}else{
			putchar('x');
		}
		if((temp_x + 10 < field.size_x * 10)&&(temp_y + 10 < field.size_y * 10)){
			printf("%d", walls_and_territories[temp_y + 10][temp_x + 10]);
		}else{
			putchar('x');
		}
		puts("");
		
		if((temp_x - 10 > 0)){
			printf("%d", walls_and_territories[temp_y][temp_x - 10]);
		}else{
			putchar('x');
		}
		printf("%d", walls_and_territories[temp_y][temp_x]);
		if((temp_x + 10 < field.size_x * 10)){
			printf("%d", walls_and_territories[temp_y][temp_x + 10]);
		}else{
			putchar('x');
		}
		puts("");

		if((temp_x - 10 > 0)&&(temp_y - 10 > 0)){
			printf("%d", walls_and_territories[temp_y - 10][temp_x - 10]);
		}else{
			putchar('x');
		}
		if((temp_y - 10 > 0)){
			printf("%d", walls_and_territories[temp_y - 10][temp_x]);
		}else{
			putchar('x');
		}
		if((temp_x + 10 < field.size_x * 10)&&(temp_y - 10 > 0)){
			printf("%d", walls_and_territories[temp_y - 10][temp_x + 10]);
		}else{
			putchar('x');
		}
		puts("");
****** DEBUG */

		// vx>0 vy>0 velocity_on_last_collision
		//  0    0          0
		//  0    1          1
		//  1    0          2
		//  1    1          3

		// 1 3 2 0 Clockwise motion
		// 2 3 1 0 Counter-Clockwise motion
		if(ball.velocity_x > 0 && ball.velocity_y > 0){ // 3
			if(ball.velocity_on_last_collision == 1){
				ball.velocity_x *= 1;
				ball.velocity_y *= -1;
			}else if(ball.velocity_on_last_collision == 2){
				ball.velocity_x *= -1;
				ball.velocity_y *= 1;
			}
			ball.velocity_on_last_collision = 3;
		}else if(ball.velocity_x > 0 && ball.velocity_y < 0){ // 2
			if(ball.velocity_on_last_collision == 3){
				ball.velocity_x *= -1;
				ball.velocity_y *= 1;
			}else if(ball.velocity_on_last_collision == 0){
				ball.velocity_x *= 1;
				ball.velocity_y *= -1;
			}
			ball.velocity_on_last_collision = 2;
		}else if(ball.velocity_x < 0 && ball.velocity_y < 0){ // 0
			if(ball.velocity_on_last_collision == 2){
				ball.velocity_x *= 1;
				ball.velocity_y *= -1;
			}else if(ball.velocity_on_last_collision == 1){
				ball.velocity_x *= -1;
				ball.velocity_y *= 1;
			}
			ball.velocity_on_last_collision = 0;
		}else if(ball.velocity_x < 0 && ball.velocity_y > 0){ // 1
			if(ball.velocity_on_last_collision == 0){
				ball.velocity_x *= -1;
				ball.velocity_y *= 1;
			}else if(ball.velocity_on_last_collision == 3){
				ball.velocity_x *= 1;
				ball.velocity_y *= -1;
			}
			ball.velocity_on_last_collision = 1;
		}
		return true;
	}
	return false;
}

/* Iscrtavanja komponenti iz igrice, podesavanje refleksija svetlosti */
static void draw_hero()
{
	glMatrixMode(GL_MODELVIEW);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0); 
	GLfloat ambient_coeffs[] = {0.7, 0.1, 0.1, 1};
	GLfloat diffuse_coeffs[] = {1, 0, 0, 1};
	glMaterialfv(GL_FRONT, GL_AMBIENT, ambient_coeffs);
	glMaterialfv(GL_FRONT, GL_DIFFUSE, diffuse_coeffs);
	update_actual_position();
	glPushMatrix();
		glTranslatef(hero.actual_position_x, hero.actual_position_y, 0);
		glutSolidCube(hero.size);
	glPopMatrix();
}

static void draw_ball()
{
	glMatrixMode(GL_MODELVIEW);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0); 
	GLfloat ambient_coeffs[] = {0.1, 0.8, 0.1, 1};
	GLfloat diffuse_coeffs[] = {0, 1, 0, 1};
	glMaterialfv(GL_FRONT, GL_AMBIENT, ambient_coeffs);
	glMaterialfv(GL_FRONT, GL_DIFFUSE, diffuse_coeffs);
	update_actual_position();
	glPushMatrix();
		glTranslatef(ball.actual_position_x, ball.actual_position_y, -1);
		glutSolidSphere(0.5, 20, 20);
	glPopMatrix();
}

void draw_wall_part()
{

	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glScalef(0.1, 0.1, 1);
	glutSolidCube(1);
}

/*TODO
 * popunjavanje povrsine koja je osvojena
 */

static void draw_field()
{
	glTranslatef(0, 0, 1);
	glMatrixMode(GL_MODELVIEW);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0); 
	GLfloat ambient_coeffs1[] = {0.1, 0.2, 0.2, 1};
	GLfloat diffuse_coeffs1[] = {0, 0.1, 0.7, 1};
	glMaterialfv(GL_FRONT, GL_AMBIENT, ambient_coeffs1);
	glMaterialfv(GL_FRONT, GL_DIFFUSE, diffuse_coeffs1);
	/*  Ubaci flood algorithm
	 * 	promenljiva unutrasnjosti zida
	 * 	2 promenljive za pocetak i kraj zida po x-osi
	 *
	 *	****** ****
	 *		^
	 *	   ^- polovina
	 *	glScalef(polovina, 1, 1)  
	 *			      ^- za ovu dole drugu ideju 
	 *  */
	/* TODO Animacija novonastalih zidova (potrebna su nova stanja za walls_and_territories) */
	bool inside_wall = false;
	/* Float zato sto je korisno pri prosledjivanju
	 * argumentu afine transformacije bez odsecanja decimala */
	float wall_x_start;
	float wall_x_stop;
	float wall_x_middle;
	for(int i = 0; i < field.size_y * 10; ++i){
		for(int j = 0; j <= field.size_x * 10; ++j){
			if(walls_and_territories[i][j] == WALL && !(j == field.size_x * 10)){
				if(!inside_wall){
					wall_x_start = j;
				}
				inside_wall = true;
			}else{
				if(inside_wall){
					wall_x_stop = j;
					wall_x_middle = (wall_x_stop - wall_x_start) / 2.0;
// 	printf("%d , %d :: %d\n", wall_x_start, wall_x_stop, wall_x_middle); 
// for debuggin' stuff
					glPushMatrix();
						glTranslatef(- (wall_x_start + wall_x_middle) / 10.0 + (field.size_x - 1) / 2.0 + 0.45, 
							     i / 10.0 + -(field.size_y - 1) / 2.0 - 0.45, 
							     0);
						glScalef(2* wall_x_middle, 1, 1);
						draw_wall_part();
					glPopMatrix();
				}
				inside_wall = false;
			}
		}
	}
	/***
	 * Iscrtavanje "potencijalnih" zidova
	 ***/
	GLfloat ambient_coeffs3[] = {0.2, 0.2, 0.1, 1};
	GLfloat diffuse_coeffs3[] = {0.7, 0.7, 0, 1};
	glMaterialfv(GL_FRONT, GL_AMBIENT, ambient_coeffs3);
	glMaterialfv(GL_FRONT, GL_DIFFUSE, diffuse_coeffs3);
	inside_wall = false;
	for(int i = 0; i < field.size_y * 10; ++i){
		for(int j = 0; j <= field.size_x * 10; ++j){
			if(walls_and_territories[i][j] == MAYBE && !(j == field.size_x * 10)){
				if(!inside_wall){
					wall_x_start = j;
				}
				inside_wall = true;
			}else{
				if(inside_wall){
					wall_x_stop = j;
					wall_x_middle = (wall_x_stop - wall_x_start) / 2.0;
// 	printf("%d , %d :: %d\n", wall_x_start, wall_x_stop, wall_x_middle); 
// for debuggin' stuff
					glPushMatrix();
						glTranslatef(- (wall_x_start + wall_x_middle) / 10.0 + (field.size_x - 1) / 2.0 + 0.45, 
							     i / 10.0 + -(field.size_y - 1) / 2.0 - 0.45, 
							     0);
						glScalef(2* wall_x_middle, 1, 1);
						draw_wall_part();
					glPopMatrix();
				}
				inside_wall = false;
			}
		}
	}
	/******
	 *
	 * MNOGO GORA IMPLEMENTACIJA
	 *
	 * ****
	for(int i = 0; i < field.size_y * 10; ++i){
		for(int j = 0; j < field.size_x * 10; ++j){
			if(walls_and_territories[i][j] == WALL){
				glPushMatrix();
					glTranslatef(- j / 10.0 + (field.size_x - 1) / 2.0 + 0.45, 
						     i / 10.0 + -(field.size_y - 1) / 2.0 - 0.45, 
						     0);
					draw_wall_part();
				glPopMatrix();
			}
		}
	}
*/
	/* Donja platforma */
	glTranslatef(0, 0, 1);
	GLfloat diffuse_coeffs2[] = {0.5, 0.5, 0.5, 1};
	glMaterialfv(GL_FRONT, GL_DIFFUSE, diffuse_coeffs2);
	glPushMatrix();
		glScalef(field.size_x, field.size_y, 1);
		glutSolidCube(1);
	glPopMatrix();
}

static void on_display(void)
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	gluLookAt(0, -field.size_y, -field.size_y,
		  0, 0, 0,
		  0, 1, 0);
	glMatrixMode(GL_MODELVIEW);
	glEnable(GL_LIGHTING);
	/* Svetlo global:no */
	glEnable(GL_LIGHT0); 
	/* Postavlja se jedinicna matrica zbog pozicije direkcionog svetla 
	 * i celokupnog zdravlja iscrtavanja 
	 */
	glLoadIdentity();
	float light_position[] =  {-1, 0.1, -1};
	/* Boje svetla uvek iste */
	float light_ambient[] =  {0.3, 0.3, 0.3, 1};
	float light_diffuse[] =  {0.7, 0.7, 0.7, 1};
	float light_specular[] =  {0.9, 0.9, 0.9, 1};
	glLightfv(GL_LIGHT0, GL_POSITION, light_position);
	glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
	glLightfv(GL_LIGHT0, GL_SPECULAR, light_specular);

	GLfloat ambient_coeffs[] = {0.3, 0.1, 0.1, 1};
	GLfloat diffuse_coeffs[] = {1, 0, 0, 1};
	GLfloat specular_coeffs[] = {1, 1, 1, 1};
	GLfloat shininess = 20;
	glMaterialfv(GL_FRONT, GL_AMBIENT, ambient_coeffs);
	glMaterialfv(GL_FRONT, GL_DIFFUSE, diffuse_coeffs);
	glMaterialfv(GL_FRONT, GL_SPECULAR, specular_coeffs);
	glMaterialf(GL_FRONT, GL_SHININESS, shininess);
	glShadeModel(GL_SMOOTH);
	draw_hero();
	draw_field();
	draw_ball();
	glutSwapBuffers();
}

static void on_keyboard(unsigned char key, int x, int y)
{
	UNUSED_ARG(x);
	UNUSED_ARG(y);
	switch (key) {
		case 27:
			deinitialize_game();
			exit(0);
			break;
		case ' ':
			animation_ongoing = !animation_ongoing;
			break;
		case 'w':
		case 'W':
			animation_ongoing = true;
			hero.direction = NORTH;
			break;
		case 's':
		case 'S':
			animation_ongoing = true;
			hero.direction = SOUTH;
			break;
		case 'a':
		case 'A':
			animation_ongoing = true;
			hero.direction = WEST;
			break;
		case 'd':
		case 'D':
			animation_ongoing = true;
			hero.direction = EAST;
			break;
		case 'r':
		case 'R':
			deinitialize_game();
			game_info_initialization();
			break;
		default:
			break;
	}
}

/* Namesta se pozicija iscrtavanja po prozoru,
 * proporcija display-a i vidljivost.
 * Namesta se jedinicna matrica da se ne bi primenjivala
 * afina transformacija vise puta pri 
 * visestrukoj izmeni velicine prozora.
 */
static void on_reshape(int width, int height)
{
	win_height = height;
	win_width = width;
	glViewport(0, 0, width, height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(60, (float)width / height, 1, 100);	
}

/* Onde gde se realizuje animacija
 */
static void on_timer(int value)
{
	if(value != TIMER_ID){
		return;
	}
	if(!animation_ongoing){
		glutTimerFunc(TIMER_INTERVAL, on_timer, TIMER_ID);
		return;
	}
	switch(hero.direction){
		case NORTH:
			hero.position_y += hero.velocity;
			break;
		case EAST:
			hero.position_x += hero.velocity;
			break;
		case SOUTH:
			hero.position_y -= hero.velocity;
			break;
		case WEST:
			hero.position_x -= hero.velocity;
			break;
		default:
			break;
	}

	ball.position_x += ball.velocity_x;
	ball.position_y += ball.velocity_y; 

	if(collision_detection()){
		hero.direction = PASSIVE;
	}

	ball_collision_detection();
	glutPostRedisplay();
	glutTimerFunc(TIMER_INTERVAL, on_timer, TIMER_ID);
}

int main(int argc, char **argv)
{
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGB | GLUT_DEPTH | GLUT_DOUBLE);
	glutInitWindowPosition(200, 100);
	glutInitWindowSize(500,500);
	glutCreateWindow("flosXONIX");
	glutKeyboardFunc(on_keyboard);
	glutReshapeFunc(on_reshape);
	glutDisplayFunc(on_display);
	glutTimerFunc(TIMER_INTERVAL, on_timer, TIMER_ID);
	glClearColor(0.75, 0.65, 0.55, 0);
	glEnable(GL_DEPTH_TEST);
	game_info_initialization();
	glutMainLoop();
	return 0;
}
