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

	/*
	int x;
	int y;
	*/
} board;

typedef struct{
	shape * s;
	board * b;
	/* data */
} thread_params;

typedef int *(*fptr)(int i, int j);

//TODO: add point of rotation - may need to increase resolution
char shape_bytes[7][4] = {
	{0b00000000, 0b00000000, 0b00000000, 0b00001111}, 	// i
	{0b00000000, 0b00000100, 0b00001110, 0b00000000}, 	// t
	{0b00000000, 0b00001000, 0b00001000, 0b0001100}, 	// l
	{0b00000000, 0b00000001, 0b00000001, 0b00000011}, 	// l - reverse
	{0b00000000, 0b00001100, 0b00000110, 0b00000000}, 	// z
	{0b00000000, 0b00000011, 0b00000110, 0b00000000, }, // z - reverse
	{0b00000000, 0b00000110, 0b00000110, 0b00000000, }  // square 
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

void new_shape(shape * s){
	//new shape
	s->x = 3;
	s->y = -1;
	s->type++;
	s->type = rand() % 6;
	s->rotation = 0;
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
				//mvprintw(s.y + Y_MAP(map), (s.x + X_MAP(map)) * 2 + 1, CHECK_BIT(shape_bytes[s.type][BYTE_MAP(map)], BIT_MAP(map)) ? "# " : "");
				mvaddch(s.y + Y_MAP(map), (s.x + X_MAP(map)) * 2 + 1,  ' '|A_REVERSE);
				mvaddch(s.y + Y_MAP(map), (s.x + X_MAP(map)) * 2 + 2,  ' '|A_REVERSE);
			}
			
		}
	}
	attroff(COLOR_PAIR(s.type+1));
}

//TODO: only clear set bits
void clear_shape(shape s){
	int byte, bit;

	//bytes
	for(byte = 0; byte < 4; byte++){

		char c_byte = shape_bytes[s.type][byte];

		//bit
		for(bit = 0; bit < 4; bit++){
			mvprintw(s.y+byte, s.x+(bit*2), c_byte & 0x01 ? "  " : "");
			c_byte = c_byte >> 1;
		}
	}
}


void draw_board(board b){

	int x, y;

	for(x = 0; x < 10; x++){
		for(y = 0; y < 20; y++){
			if(b.xy[x][y] == 0){
				mvprintw(y, (x*2)+1, "  ");
			}else{
				attron(COLOR_PAIR(b.xy[x][y]));
				//mvprintw(y, (x*2)+1, "# ");
				mvaddch(y, (x*2) + 1, ' '|A_REVERSE);
				mvaddch(y, (x*2) + 2, ' '|A_REVERSE);
				attroff(COLOR_PAIR(b.xy[x][y]));
			}
		}
	}

}

int check_move(shape s, board b, int x_direction, int y_direction){

	int i, j;
	int * map;
	int * (*map_func)(int i, int j) = get_map_func(s, 0);

	for(i = 0; i < 4; i++){
		for(j = 0; j < 4; j++){

			map = map_func(i , j);

			if(CHECK_BIT(shape_bytes[s.type][BYTE_MAP(map)], BIT_MAP(map))){

				//check board boundaries
				if(	(s.x + X_MAP(map) + x_direction) < 0 || 
					(s.x + X_MAP(map) + x_direction) > 9 ||
					(s.y + Y_MAP(map) + y_direction) < 0 ||
					(s.y + Y_MAP(map) + y_direction) > 19){
					return 0;
				}

				//check within board
				if(b.xy[s.x + X_MAP(map) + x_direction][s.y + Y_MAP(map) + y_direction] > 0){
					return 0;
				}
			}
		}	
	}
	return 1;
}

int check_rotation(shape s, board b, int r_direction){

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

		if(check_move(*tp_ptr->s, *tp_ptr->b, 0, 1)){
			tp_ptr->s->y++; 
		}

		//100ms
		usleep(250000);
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



//TODO add dropping to a thread - constant speed without keys
int main(){


	srand(time(NULL));
	int r =  

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

	shape s;
	s.x = 1;
	s.y = 0;
	s.rotation = 0;
	s.type = 6;

	board b = init_board();

	thread_params tp;
	tp.b = &b;
	tp.s = &s;

	pthread_t pth;
    pthread_create(&pth, NULL, drop_loop, (void * ) &tp);

	// game loop
	while(1){

		int ch = getch();

		if(check_move(s, b, 0, 1)){
			//s.y++;
		}else{

			if(add_to_board(s, &b) <= 0){
				mvprintw(40 + DEBUG_CNT++, 0, "GAME OVER");
				break;
			}

			new_shape(&s);

			int clears = check_lines(&b);
			if(clears > 0){
				mvprintw(0, 40, "Cleared lines %d", clears);
			}

		}

		draw_board(b);
		
		if(ch){

			//hold - temporarily cycles trough
			if(ch == KEY_UP){
				s.type++;
				if(s.type == 7)
					s.type = 0;

			//left
			}else if(ch == KEY_LEFT && check_move(s, b, -1, 0)){
				s.x--;

			//right
			}else if(ch == KEY_RIGHT && check_move(s, b, 1, 0)){
				s.x++;

			//rotate
			}else if(ch == KEY_DOWN /* && check_rotation()*/){
				s.rotation++;
				if(s.rotation == 4)
					s.rotation = 0;

			//hard drop
			}else if(ch == ' '){
				while(true){
					if(!check_move(s, b, 0, 1))
						break;
					s.y++;
				}
			}
		}

		draw_shape(s);

		//show positions
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