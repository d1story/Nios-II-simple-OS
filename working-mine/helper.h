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


void printCell(int pos_row, int pos_col, int row_num, int col_num);
// print the game board
void printBoard(struct BOARD *board, char *end, char *win); 

// initialize the board
void createBoard(int col, int row, struct BOARD *board);

void createMine(struct BOARD *board);

int countMines(int row, int col, struct BOARD *board);

void createNumHint(struct BOARD *board);


void reveal(int open_row, int open_col, struct BOARD *board);

void parseMouse(struct BOARD *board, char *status, char *end, char *win);

void openOrFlag(struct BOARD *board, char *status, char *end, int row, int col, char *win);

void restart(int col, int row, struct BOARD *board, char *end, char *win);

extern volatile int G_mouse_X, G_mouse_Y, clicked;

extern short int c_vga[240][320];
extern short int c_buffer[87][67]; //80-60

#endif // HELPER_H