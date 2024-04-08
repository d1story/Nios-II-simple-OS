#ifndef HELPER_H
#define HELPER_H

#include <stdio.h>

#define ROWSIZE 9
#define COLSIZE 9
#define MINENUM 10


struct BOARD {
  char mine_pointer[ROWSIZE][COLSIZE];
  char open_pointer[ROWSIZE][COLSIZE];
  char flagged_pointer[ROWSIZE][COLSIZE];
  char board_pointer[ROWSIZE][COLSIZE];
  int board_x;
  int board_y;
  int board_num_mines;
  int board_num_correct_flags;
}; // board


void printCell(int pos_row, int pos_col, int row_num, int col_num, short (*c_vga)[320][240]);
// print the game board
void printBoard(struct BOARD *board, char *end, char *win, short (*c_vga)[320][240], char (*c_buffer)[80][60]); 

// initialize the board
void createBoard(int col, int row, struct BOARD *board);

void createMine(struct BOARD *board);

int countMines(int row, int col, struct BOARD *board);

void createNumHint(struct BOARD *board);

void reveal(int open_row, int open_col, struct BOARD *board);

void parseMouse(struct BOARD *board, char *status, char *end, char *win, int *clicked, int *G_mouse_Y, int *G_mouse_X);

void openOrFlag(struct BOARD *board, char *status, char *end, int row, int col, char *win);

void restart(int col, int row, struct BOARD *board, char *end, char *win);

extern volatile unsigned int L_Time, H_Time;

extern int mine_Logo[30][30];


extern int mouse_click_sound[481];


#endif // HELPER_H