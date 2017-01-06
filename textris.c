#include <curses.h>
#include <stdlib.h>
#include <locale.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include <menu.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))
#define CTRLD 	4

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
	int width;
	int height;
} board;

typedef struct{
	int bucket1[14];
}bucket;

typedef struct{
	shape * s;
	board * b;
	bucket * bucket;
	int * hold_cnt;
	int * drop_speed;
	int * running;
	int * lines_cleared;
} thread_params;

typedef int *(*fptr)(int i, int j);

//TODO: add point of rotation - may need to increase resolution
char shape_bytes[7][4] = {
	{0b00000000, 0b00000000, 0b00001111, 0b00000000}, 	// i
	{0b00000000, 0b00000100, 0b00001110, 0b00000000}, 	// t
	{0b00000000, 0b00001110, 0b00001000, 0b00000000}, 	// l
	{0b00000000, 0b00001110, 0b00000010, 0b00000000}, 	// l - reverse
	{0b00000000, 0b00001100, 0b00000110, 0b00000000}, 	// z
	{0b00000000, 0b00000011, 0b00000110, 0b00000000}, 	// z - reverse
	{0b00000000, 0b00000110, 0b00000110, 0b00000000}  	// square 
};

int DEBUG_CNT = 0;



int BUCKET_OFFSET = 4;
int HOLD_OFFSET = -6;

//active shape
shape s;

//board
board b;

//shape bucket
bucket bu;

//hold
int h_s = -1;
int hold_cnt = 0;

int drop_speed = 250000;

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

int in_array(int val, int * arr, int size){

	int i;

	for(i=0; i<size; i++){
		if(arr[i] == val)
			return 1;
	}
	return 0;
}

void v_border(int y, int x, int len){
	int i;
	for(i = 1; i < len - 1; i++)
		mvprintw(y + i, x, "|");		
	mvprintw(y, x, "+");
	mvprintw(y + len - 1, x, "+");
	
}

void h_border(int y, int x, int len){
	char * str = malloc(len + 1);
	str[0] = '+';
	memset(str + 1, '-', len - 2);
	str[len - 1] = '+';
	str[len] = 0;

	mvprintw(y, x, str);
}

int draw_shape(shape s, int board_x, int board_y){

	int i, j;
	int * map;
	int * (*map_func)(int i, int j) = get_map_func(s, 0);

	int y_vals[4];
	int height = 0;

	attron(COLOR_PAIR(s.type+1));

	for(i = 0; i < 4; i++){
		for(j = 0; j < 4; j++){
			map = map_func(i , j);

			if(CHECK_BIT(shape_bytes[s.type][BYTE_MAP(map)], BIT_MAP(map))){

				mvaddch(s.y + Y_MAP(map) + board_y, ((s.x + X_MAP(map))*2)  + (board_x) + 1,  ' '|A_REVERSE);
				mvaddch(s.y + Y_MAP(map) + board_y, ((s.x + X_MAP(map))*2) + (board_x) + 2,  ' '|A_REVERSE);

				if(!in_array(s.y + Y_MAP(map), y_vals, 4)){
					y_vals[height] = s.y + Y_MAP(map);
					height++;
				}
			}/*else{
				mvaddch(s.y + Y_MAP(map) + board_y, ((s.x + X_MAP(map))*2)  + (board_x*2) + 1,  'x');
				mvaddch(s.y + Y_MAP(map) + board_y, ((s.x + X_MAP(map))*2) + (board_x*2) + 2,  'x');
			}*/
			
		}
	}
	attroff(COLOR_PAIR(s.type+1));

	return height;
}

void draw_board(board b, int board_x, int board_y){

	int x, y;

	for(x = 0; x < 10; x++){
		for(y = 0; y < 20; y++){
			if(b.xy[x][y] == 0){
				mvprintw(y + board_y, (x*2) + (board_x) + 1, "  ");
			}else{
				attron(COLOR_PAIR(b.xy[x][y]));
				mvaddch(y + board_y, (x*2) + (board_x) + 1, ' '|A_REVERSE);
				mvaddch(y + board_y, (x*2) + (board_x) + 2, ' '|A_REVERSE);
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
					(s.x + X_MAP(map) + x_direction) > (b.width - 1) ||
					(s.y + Y_MAP(map) + y_direction) < 0 ||
					(s.y + Y_MAP(map) + y_direction) > (b.height - 1)){
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

//TODO - eliminate most white space - only a single block between shapes 
//     - maybe rotate them differently
void draw_bucket(bucket * b, int bucket_x, int bucket_y){
	
	int i, j;
	
	//clear
	for(i = 0; i < 4; i++){
		for(j=0; j<4; j++){
			mvprintw(i*4 + j + bucket_y, bucket_x + 1, "        ");	
		}	
	}

	for(i =0; i<4; i++){

		shape s;
		s.rotation = 0;
		s.type = b->bucket1[i];
		s.x = 0;
		s.y = i*4;

		draw_shape(s, bucket_x, bucket_y);
	}

	//borders
    v_border(bucket_y, bucket_x - 1, 4 * 4);
    v_border(bucket_y, bucket_x + 10, 4 * 4);
    h_border(bucket_y, bucket_x - 1, 12);
    h_border(bucket_y + (4 * 4) - 1, bucket_x - 1, 12);
    mvprintw(bucket_y + (4 * 4), bucket_x, "   Next");	
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

board init_board(int board_width, int board_height){
	board b;
	memset(b.xy, 0, sizeof(b.xy[0][0]) * board_width * board_height);
	b.width = board_width;
	b.height = board_height;
	return b;
}

void drop_loop(void * tp_void_ptr){
	thread_params * tp_ptr = (thread_params *) tp_void_ptr;

	while(* tp_ptr->running > 0){

		if(check_move(*tp_ptr->s, *tp_ptr->b, 0, 1, 0)){
			tp_ptr->s->y++; 
		}else{

			if(add_to_board(*tp_ptr->s, tp_ptr->b) <= 0){
				* tp_ptr->running = -1;
				break;
			}

			int clears = check_lines(tp_ptr->b);
			if(clears > 0){
				mvprintw(40 + DEBUG_CNT++, 0, "Cleared lines %d", clears);
			}

			* tp_ptr->lines_cleared = * tp_ptr->lines_cleared + clears;

			new_shape(tp_ptr->s, tp_ptr->bucket);

			* tp_ptr->hold_cnt = 0;
		}

		//need method of changing this value as levels increase
		usleep(* tp_ptr->drop_speed);		//100ms
	}
}

//TODO - add animation to this?
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

void draw_hold(int h_s, int hold_cnt, hold_x, hold_y){

	int j;
	
	h_border(hold_y, hold_x, 10);
	h_border(hold_y + 5, hold_x, 10);
	v_border(hold_y, hold_x, 6);
	v_border(hold_y, hold_x + 9, 6);
	mvprintw(hold_y + 6, hold_x, "   Hold");

	//clear
	for(j=0; j<4; j++)
		mvprintw(j + hold_y + 1, hold_x + 1, "        ");	

	if(h_s != -1){
		shape s;
		s.type = h_s;
		s.rotation = 0;
		s.x = 0;
		s.y = 1;

		draw_shape(s, hold_x, hold_y);

	}
}

void hold(shape * s, int * h_s, bucket * b){

	//hold empty
	if(* h_s == -1){
		* h_s = s->type;
		int x = s->x;
		int y = s->y;

		new_shape(s, b); 
		s->x = x;
		s->y = y;
	//swap
	}else{
		int type = s->type;
		s->type = * h_s;
		* h_s = type;
	}

}

int can_hold(shape s, shape h_s, board b){
	return 1;
}

void test_func(){
	mvprintw(40 + DEBUG_CNT++, 0, "TEST");
}

void shape_left(){
	if(check_move(s, b, -1, 0, 0))
		s.x--;
}	

void shape_right(){
	if(check_move(s, b, 1, 0, 0))
		s.x++;
}

void shape_rotate(){
	if(check_move(s, b, 0, 0, 1)){
		s.rotation++;
		if(s.rotation == 4){
			s.rotation = 0;
		}
	}
}

void shape_hard_drop(){
	while(true){
		if(!check_move(s, b, 0, 1, 0))
			break;
		s.y++;
	}
}

void shape_hold(){
	if(hold_cnt < 1){
		hold(&s, &h_s, &bu);
		hold_cnt++;
	}
}

void drop_speed_normal(){
	drop_speed = 250000;
}

void drop_speed_fast(){
	drop_speed = 50000;
}

double twenty_lines(){

	clear();

	int board_x = 15;
	int board_y = 1;
	int board_width = 10;
	int board_height = 20;

	//board boarders
	int i;
	for(i = 0; i < board_height; i++){
		mvprintw(i + board_y, board_x, "|");
		mvprintw(i + board_y, (board_width * 2) + (board_x) + 1, "|", i);
	}

	mvprintw(board_y + board_height, board_x, "+");	
	for(i = 1; i < (board_width * 2) + 1; i++)
		mvprintw(board_y + board_height, i + board_x, "-");	
	mvprintw(board_y + board_height, board_x + (board_width * 2) + 1, "+");	

	//bucket
	init_bucket(&bu);

	//shape
	new_shape(&s, &bu);

	//board
	b = init_board(board_width, board_height);

	int running = 1;
	int lines = 0;

	//drop loop params
	thread_params tp;
	tp.b = &b;
	tp.s = &s;
	tp.bucket = &bu;
	tp.hold_cnt = &hold_cnt;
	tp.drop_speed = &drop_speed;
	tp.running = &running; 
	tp.lines_cleared = &lines;

	pthread_t pth;
    pthread_create(&pth, NULL, drop_loop, (void * ) &tp);

    struct timeval tval_before, tval_after, tval_result;
    gettimeofday(&tval_before, NULL);

   	char tmbuf[64];

	// game loop
	while(running > 0 && lines < 10){

		timeout(100);

		draw_board(b, board_x, board_y);
		draw_hold(h_s, hold_cnt, 1, 1);
		draw_shape(s, board_x, board_y);
		draw_bucket(&bu, 45, 1);

		mvprintw(10, 2, "Lines: %d", lines);

		gettimeofday(&tval_after, NULL);
		timersub(&tval_after, &tval_before, &tval_result);
		strftime(tmbuf, sizeof tmbuf, "%M:%S", localtime(&tval_result.tv_sec));
		mvprintw(14, 2, "        ");
		mvprintw(14, 2, "%s.%02d", tmbuf, tval_result.tv_usec/10000);

		int ch = getch();
		if(ch != ERR){

			//hold - temporarily cycles trough
			if(ch == 'x'){
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
			}else if(ch == KEY_UP && check_move(s, b, 0, 0, 1)){
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
			//hold
			}else if(ch == 'z' && hold_cnt < 1){
				hold(&s, &h_s, &bu);
				hold_cnt++;
			}
		}

		refresh();

		if(DEBUG_CNT == 20)
			DEBUG_CNT = 0;
	}

	//stop drop loop
	running = -1;

	//delay for drop loop to stop
	usleep(1000000);

	clear();

	return 1;
}

void print_in_middle(WINDOW *win, int starty, int startx, int width, char *string, chtype color);

void draw_menu(WINDOW *win, MENU *menu){

	int i;

	int MENU_WIDTH = 20;
	int SCREEN_WIDTH = 100;

	clear();

	/* Print a border around the main window and print a title */
	box(win, 0, 0);

	print_in_middle(win, 1, 0, MENU_WIDTH, "Game mode", COLOR_PAIR(0));
	mvwaddch(win, 2, 0, ACS_LTEE);
	mvwhline(win, 2, 1, ACS_HLINE, MENU_WIDTH);
	mvwaddch(win, 2, MENU_WIDTH + 1, ACS_RTEE);
	mvprintw(3, (SCREEN_WIDTH/2) - (36/2), "textris - An ASCII based Tetris game");

	//decorative shapes
	for(i =0; i<7; i++){
		shape s;
		s.rotation = 0;
		s.type = i;
		s.x = 0;
		s.y = 0;
		draw_shape(s, (i*10) + (SCREEN_WIDTH/2) - ((10*7)/2), 15);
	}
	wrefresh(win);
	refresh();
}

void textris(){
	int score = 0;

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

	//menu

	int MENU_WIDTH = 20;
	int SCREEN_WIDTH = 100;

	char *choices[] = {
                        "20 Lines",
                        "40 Lines",
                        "Classic",
                        "Read Me",
                        "Exit",
                  };

	ITEM **menu_items;
	int c;				
	MENU *menu;
	int n_choices, i;
	ITEM *cur_item;
	WINDOW *menu_win;

	n_choices = ARRAY_SIZE(choices);
	menu_items = (ITEM **)calloc(n_choices + 1, sizeof(ITEM *));

	for(i = 0; i < n_choices; ++i)
	        menu_items[i] = new_item(choices[i], "");
	menu_items[n_choices] = (ITEM *)NULL;

	menu = new_menu((ITEM **)menu_items);

	/* Create the window to be associated with the menu */
	menu_win = newwin(10, MENU_WIDTH + 2, 5, (SCREEN_WIDTH/2) - (MENU_WIDTH/2));
	keypad(menu_win, TRUE);

	/* Set main window and sub window */
	set_menu_win(menu, menu_win);
	set_menu_sub(menu, derwin(menu_win, 6, MENU_WIDTH, 3, 1)); //must be less than window

	/* Set menu mark to the string " * " */
	set_menu_mark(menu, " * ");
	post_menu(menu);
	wrefresh(menu_win);

	draw_menu(menu_win, menu);

	refresh();

	while((c = wgetch(menu_win)) != KEY_F(1))
	{   switch(c)
	    {	case KEY_DOWN:
		        menu_driver(menu, REQ_DOWN_ITEM);
				break;
			case KEY_UP:
				menu_driver(menu, REQ_UP_ITEM);
				break;
			case KEY_LEFT:
				twenty_lines();
				draw_menu(menu_win, menu);
				break;
		}
	}	

	for(i = 0; i < n_choices; ++i)
		free_item(menu_items[i]);
	free_menu(menu);
	endwin();

	//double res = twenty_lines();

	/*while(1){
		mvprintw(0, 0, "20 lines cleared in %f seconds", res);
		refresh();
	}

	endwin();*/
}

void print_in_middle(WINDOW *win, int starty, int startx, int width, char *string, chtype color)
{	int length, x, y;
	float temp;

	if(win == NULL)
		win = stdscr;
	getyx(win, y, x);
	if(startx != 0)
		x = startx;
	if(starty != 0)
		y = starty;
	if(width == 0)
		width = 80;

	length = strlen(string);
	temp = (width - length)/ 2;
	x = startx + (int)temp;
	wattron(win, color);
	mvwprintw(win, y, x, "%s", string);
	wattroff(win, color);
	refresh();
}

int main(){  
	textris();
}

		/*
		int ch = getch();
		if(ch != ERR){

			//hold - temporarily cycles trough
			if(ch == 'x'){
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
			}else if(ch == KEY_UP && check_move(s, b, 0, 0, 1)){
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
			//hold
			}else if(ch == 'z' && hold_cnt < 1){
				hold(&s, &h_s, &bu);
				hold_cnt++;
			}
		}*/