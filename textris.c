#include <curses.h>
#include <stdlib.h>
#include <locale.h>
#include <string.h>
#include <pthread.h>

#define CHECK_BIT(var,pos) ((var) & (1<<(pos)))
#define BIT_MAP(map)(*(map + 0))
#define BYTE_MAP(map)(*(map + 1))
#define X_MAP(map)(*(map + 2))
#define Y_MAP(map)(*(map + 3))

typedef struct{
	int x;
	int y;
	int rotation;
	int type;
} shape;

//TODO convert to use bytes && add width and height
typedef struct{
	int xy[20][20];
	int xy_col[10][20];
} board;

typedef struct{
	int bucket1[14];
}bucket;

typedef struct{
	shape * s;
	board * b;
	bucket * bucket;
} thread_params;



typedef int *(*fptr)(int i, int j);

//TODO: add point of rotation - may need to increase resolution
char shape_bytes[7][4] = {
	{0b00000000, 0b00000000, 0b00000000, 0b00001111}, 	// i
	{0b00000000, 0b00000100, 0b00001110, 0b00000000}, 	// t
	{0b00000000, 0b00001000, 0b00001000, 0b00001100}, 	// l
	{0b00000000, 0b00000001, 0b00000001, 0b00000011}, 	// l - reverse
	{0b00000000, 0b00001100, 0b00000110, 0b00000000}, 	// z
	{0b00000000, 0b00000011, 0b00000110, 0b00000000}, // z - reverse
	{0b00000000, 0b00000110, 0b00000110, 0b00000000}  // square 
};



int DEBUG_CNT = 0;

/*
* Given the loop itterator values(i , j)
* Returns 4 values in an array - bit, byte, x, y
* bit/bytes - order to check bits in - i.e. check upward for the shapes lowest point - would need to be changed for checking left and right
* x/y - location mapping for shape rotation to board
*/
int * map_rotation0(int i, int j){
	int map[4] = {i, 3 - j, i, 3 - j};
	return map;
}

int * map_rotation1(int i, int j){
	int map[4] = {3 - j, i, 3 - i, 3 - j};
	return map;
}

int * map_rotation2(int i, int j){
	int map[4] = {i, j, 3 - i, 3 - j};
	return map;
}

int * map_rotation3(int i, int j){
	int map[4] = {j, i, i, 3 - j};
	return map;
}

fptr get_map_func(shape s, int direction){
	if(s.rotation == 0)
		return map_rotation0;
	else if(s.rotation == 1)
		return map_rotation1;
	else if(s.rotation == 2)
		return map_rotation2;
	else if(s.rotation == 3)
		return map_rotation3;
}



void draw_shape(shape s){

	int i, j;
	int * map;
	int * (*map_func)(int i, int j) = get_map_func(s, 0);

	attron(COLOR_PAIR(s.type+1));

	for(i = 0; i < 4; i++){
		for(j = 0; j < 4; j++){
			map = map_func(i , j);

			if(CHECK_BIT(shape_bytes[s.type][BYTE_MAP(map)], BIT_MAP(map))){
				mvaddch(s.y + Y_MAP(map), (s.x + X_MAP(map)) * 2 + 1,  ' '|A_REVERSE);
				mvaddch(s.y + Y_MAP(map), (s.x + X_MAP(map)) * 2 + 2,  ' '|A_REVERSE);
			}
			
		}
	}
	attroff(COLOR_PAIR(s.type+1));
}

void draw_board(board b){

	int x, y;

	for(x = 0; x < 10; x++){
		for(y = 0; y < 20; y++){
			if(b.xy[x][y] == 0){
				mvprintw(y, (x*2)+1, "  ");
			}else{
				attron(COLOR_PAIR(b.xy[x][y]));
				mvaddch(y, (x*2) + 1, ' '|A_REVERSE);
				mvaddch(y, (x*2) + 2, ' '|A_REVERSE);
				attroff(COLOR_PAIR(b.xy[x][y]));
			}
		}
	}
}

int check_move(shape s, board b, int x_direction, int y_direction, int rotation){

	shape s_new = s;
	s_new.rotation += rotation;
	if(s_new.rotation == 4)
		s_new.rotation = 0;
	else if(s_new.rotation == -1)
		s_new.rotation = 3; 

	int i, j;
	int * map;
	int * (*map_func)(int i, int j) = get_map_func(s_new, 0);

	for(i = 0; i < 4; i++){
		for(j = 0; j < 4; j++){

			map = map_func(i , j);

			if(CHECK_BIT(shape_bytes[s_new.type][BYTE_MAP(map)], BIT_MAP(map))){

				//check board boundaries
				if(	(s.x + X_MAP(map) + x_direction) < 0 || 
					(s.x + X_MAP(map) + x_direction) > 9 ||
					(s.y + Y_MAP(map) + y_direction) < 0 ||
					(s.y + Y_MAP(map) + y_direction) > 19){
					return 0;
				}

				//check within board
				if(b.xy[s.x + X_MAP(map) + x_direction][s_new.y + Y_MAP(map) + y_direction] > 0){
					return 0;
				}
			}
		}	
	}
	return 1;
}

int * random_bucket(){
	int new_b[] = {0,1,2,3,4,5,6};

	int i;
    for (i = 0; i < 6; i++){
		int j = i + rand() / (RAND_MAX / (7 - i) + 1);
		int t = new_b[j];
		new_b[j] = new_b[i];
		new_b[i] = t;
    }

    return new_b;
}

void init_bucket(bucket * b){

	int * b1 = random_bucket();
	int * b2 = random_bucket();

	int i;

	//both buckets
	for(i=0 ; i<7; i++){
		b->bucket1[i] = *(b1 + i);
		b->bucket1[i+7] =  *(b2 + i);
	}
}

void refill_bucket(bucket * b){

	int * b1 = random_bucket();
	int i;

	//second bucket
	for(i=0 ; i<7; i++)
		b->bucket1[i+7] = *(b1 + i);
}

void draw_bucket(bucket * b){
	int i;
	for(i =0; i<14; i++){
		mvprintw(0 + i, 40, "%d Bucket %d type %d       ", i, i/7 + 1, b->bucket1[i]);
	}
}

void new_shape(shape * s, bucket * b){
	s->x = 3;
	s->y = -1;
	s->type = b->bucket1[0];
	s->rotation = 0;

	//check if second bucket is empty
	if(b->bucket1[7] == -1)
		refill_bucket(b);

	//shuffle buckets
	int i;
	for(i = 0; i < 13; i++)
		b->bucket1[i] = b->bucket1[i+1];
	b->bucket1[13] = -1;

	draw_bucket(b);
}

int add_to_board(shape s, board * b){

	int i, j;

	int * map;
	int * (*map_func)(int i, int j) = get_map_func(s, 0);

	for(i = 0; i < 4; i++){
		for(j = 0; j < 4; j++){

			map = map_func(i , j);

			//set piece in board
			if(CHECK_BIT(shape_bytes[s.type][BYTE_MAP(map)], BIT_MAP(map))){

				if(s.y == -1)
					return -1;
				
				b->xy[s.x + X_MAP(map)][s.y + Y_MAP(map)] = s.type + 1;
			}
		}	
	}

	return 1;
}

board init_board(){
	board b;
	memset(b.xy, 0, sizeof(b.xy[0][0]) * 20 * 10);
	return b;
}

void drop_loop(void * tp_void_ptr){
	thread_params * tp_ptr = (thread_params *) tp_void_ptr;

	while(1){

		if(check_move(*tp_ptr->s, *tp_ptr->b, 0, 1, 0)){
			tp_ptr->s->y++; 
		}else{

			if(add_to_board(*tp_ptr->s, tp_ptr->b) <= 0){
				mvprintw(40 + DEBUG_CNT++, 0, "GAME OVER");
				break;
			}

			

			int clears = check_lines(tp_ptr->b);
			if(clears > 0){
				mvprintw(40 + DEBUG_CNT++, 0, "Cleared lines %d", clears);
			}

			new_shape(tp_ptr->s, tp_ptr->bucket);
			

		}

		usleep(250000);		//100ms
	}
}

void clear_line(board * b, int y){

	int x, shuffle_y;

	for(shuffle_y = y; shuffle_y > 0; shuffle_y--){
		for(x = 0; x < 10; x++){
			b->xy[x][shuffle_y] = b->xy[x][shuffle_y-1];
		}
	}

}

int check_lines(board * b){

	int x, y, blk_cnt, row_cnt = 0;

	for(y = 0; y < 20; y++){
		blk_cnt = 0;
		for(x = 0; x < 10; x++){
			if(b->xy[x][y] > 0)
				blk_cnt++;
		}
		if(blk_cnt == 10){
			row_cnt++;
			clear_line(b, y);
		}
	}
	return row_cnt;
}

int main(){  

	srand(time(NULL));

	setlocale(LC_ALL, "");

	initscr();			
	cbreak();
	noecho();
	halfdelay(1);
	keypad(stdscr, TRUE);

	start_color();
	init_pair(1, COLOR_RED, COLOR_BLACK);
	init_pair(2, COLOR_YELLOW, COLOR_BLACK);
	init_pair(3, COLOR_BLUE, COLOR_BLACK);
	init_pair(4, COLOR_GREEN, COLOR_BLACK);
	init_pair(5, COLOR_WHITE, COLOR_BLACK);
	init_pair(6, COLOR_CYAN, COLOR_BLACK);
	init_pair(7, COLOR_MAGENTA, COLOR_BLACK);
	
	//initialise boarders
	int i;
	for(i = 0; i < 20; i++){
		mvprintw(i,0,"|");
		mvprintw(i,21,"| %d", i);
	}

	for(i = 0; i < 21; i++){
		mvprintw(20,i,"_");	
	}

	bucket bu;
	init_bucket(&bu);

	shape s;
	new_shape(&s, &bu);

	board b = init_board();

	thread_params tp;
	tp.b = &b;
	tp.s = &s;
	tp.bucket = &bu;

	pthread_t pth;
    pthread_create(&pth, NULL, drop_loop, (void * ) &tp);

	// game loop
	while(1){

		int ch = getch();

		draw_board(b);
		
		if(ch){

			//hold - temporarily cycles trough
			if(ch == KEY_UP){
				s.type++;
				if(s.type == 7)
					s.type = 0;

			//left
			}else if(ch == KEY_LEFT && check_move(s, b, -1, 0, 0)){
				s.x--;

			//right
			}else if(ch == KEY_RIGHT && check_move(s, b, 1, 0, 0)){
				s.x++;

			//rotate
			}else if(ch == KEY_DOWN && check_move(s, b, 0, 0, 1)){
				s.rotation++;
				if(s.rotation == 4)
					s.rotation = 0;

			//hard drop
			}else if(ch == ' '){
				while(true){
					if(!check_move(s, b, 0, 1, 0))
						break;
					s.y++;
				}
			}
		}

		draw_shape(s);

		mvprintw(30, 2, "X %d\n", s.x);
		mvprintw(31, 2, "Y %d\n", s.y);

		refresh();

		if(DEBUG_CNT == 20)
			DEBUG_CNT = 0;
	}

	while(1){
		refresh();
	}

	endwin();
}