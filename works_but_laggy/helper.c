#include "helper.h"

void createBoard(int row, int col, struct BOARD *board){
    board->board_x = col;
    board->board_y = row;

    for (int i = 0; i < row; ++i) {
        for (int j = 0; j < col; ++j) {
            board->mine_pointer[i][j] = 0;
        }
    }

    for (int i = 0; i < row; ++i) {
        for (int j = 0; j < col; ++j) {
            board->open_pointer[i][j] = 0;
        }
    }


    for (int i = 0; i < row; ++i) {
        for (int j = 0; j < col; ++j) {
            board->flagged_pointer[i][j] = 0;
        }
    }

    for (int i = 0; i < row; i++){
        for (int j = 0; j < col; j++){
            board->board_pointer[i][j] = 'o';
        }
    }

    board->board_num_correct_flags = 0;
    board->board_num_mines = MINENUM;
}


int rand(){
    return L_Time * (H_Time+1);
}


void createMine(struct BOARD *board){
    for(int mine_num = 0; mine_num < 10; mine_num++) {
        int random_row = rand() % 9; 
        int random_col = rand() % 9;
        while(board->mine_pointer[random_row][random_col]){
            random_row = rand() % 9; 
            random_col = rand() % 9;
        }
        board->mine_pointer[random_row][random_col] = 1;
    }
}

// helper to create number hinting how many mines thaere are in its 3*3 grid
int countMines(int row, int col, struct BOARD *board){
    int mine_count = 0;

    if(!board->mine_pointer[row][col]){
        for (int dx = -1; dx <= 1; dx++) {
            for (int dy = -1; dy <= 1; dy++) {
                int newRow = row + dy;
                int newCol = col + dx;
                // Skip the current cell
                if (dx == 0 && dy == 0){
                    continue;
                }
                // Check bounds
                if ((newRow >= 0 && newRow < ROWSIZE) && (newCol >= 0 && newCol < COLSIZE)) {
                    // Increment count if a mine is found
                    if(board->mine_pointer[newRow][newCol]){
                        mine_count++;
                    }
                    
                }
            }
        }
    }

    return mine_count;
    
}

void createNumHint(struct BOARD *board){
    int col = board->board_x;
    int row = board->board_y;

    for (int curr_row = 0; curr_row < row; curr_row++){
        for (int curr_col = 0; curr_col < col; curr_col++){
            int mine_count = countMines(curr_row, curr_col, board);
            if(!board->mine_pointer[curr_row][curr_col]){
                board->board_pointer[curr_row][curr_col] = '0' + mine_count;
            }
        }
    }
}

void setupLogo(short (*c_vga)[320][240]){
    int relYpos = 16;
    int relXpos = 145;
    for(int i=0;i<30;i++){
        for(int j=0;j<30;j++){
            (*c_vga)[relXpos+i][relYpos+j] = mine_Logo[i][j];
        }
    }
}

void printCell(int pos_row, int pos_col, int row_num, int col_num, short (*c_vga)[320][240]){
    // Define the light blue base color in RGB (5-6-5 bits) format
    int base_color = 0b0111110000001111; // Adjust this value to match the exact light blue color

    // Define border colors
    int light_border_color = 0b0111111111111111; // Lighter color for the top-left border
    int dark_border_color = 0b0011100111100111; // Darker color for the bottom-right border

    // Set the border size
    int border_size = 1;

    // Draw the top and left borders
    for (int row = 0; row < border_size; row++){
        for (int col = 0; col < col_num; col++){
            (*c_vga)[pos_col + col][pos_row + row] = light_border_color;
        }
    }
    for (int row = border_size; row < row_num; row++){
        for (int col = 0; col < border_size; col++){
            (*c_vga)[pos_col + col][pos_row + row] = light_border_color;
        }
    }

    // Draw the cell's interior with the base color
    for (int row = border_size; row < row_num - border_size; row++){
        for (int col = border_size; col < col_num - border_size; col++){
            (*c_vga)[pos_col + col][pos_row + row] = base_color;
        }
    }

    // Draw the bottom and right borders
    for (int row = row_num - border_size; row < row_num; row++){
        for (int col = 0; col < col_num; col++){
            (*c_vga)[pos_col + col][pos_row + row] = dark_border_color;
        }
    }
    for (int row = border_size; row < row_num; row++){
        for (int col = col_num - border_size; col < col_num; col++){
            (*c_vga)[pos_col + col][pos_row + row] = dark_border_color;
        }
    }
}

void printBoard(struct BOARD *board, char *end, char *win, short (*c_vga)[320][240], char (*c_buffer)[80][60]){
    for (int row = 0; row < 9; row++){
        // printf("%i ", row+1);
        for (int column = 0; column < 9; column++){
            int pos_buffer_row = 17 + (column * 3);
            int pos_buffer_col = 27 + (row * 3);
            printCell(pos_buffer_row * 4 - 4, pos_buffer_col * 4 - 4, 12, 12, c_vga);
            if(board->open_pointer[row][column] == 1){
            (*c_buffer)[pos_buffer_col][pos_buffer_row] = board->board_pointer[row][column];
            }
            else if(board->flagged_pointer[row][column] == 1){
            (*c_buffer)[pos_buffer_col][pos_buffer_row] = 'z'+5;
            }
            else{                
                (*c_buffer)[pos_buffer_col][pos_buffer_row] = ' ';
            }

            if(*end && !(*win)){
                if(board->mine_pointer[row][column]==1)
                (*c_buffer)[pos_buffer_col][pos_buffer_row] = 15;
            }
            
        }
        //printf("\n");
    }
    
    printCell(45 * 4 - 4, 28 * 4 - 4, 12, 24, c_vga);
    (*c_buffer)[28][45] = 'O';
    (*c_buffer)[29][45] = 'P';
    (*c_buffer)[30][45] = 'E';
    (*c_buffer)[31][45] = 'N';

    printCell(45 * 4 - 4, 35 * 4 - 4, 12, 28, c_vga);
    (*c_buffer)[35][45] = 'z' + 5;
    (*c_buffer)[36][45] = 'F';
    (*c_buffer)[37][45] = 'L';
    (*c_buffer)[38][45] = 'A';
    (*c_buffer)[39][45] = 'G';

    printCell(45 * 4 - 4, 43 * 4 - 4, 12, 36, c_vga);
    (*c_buffer)[43][45] = 'R';
    (*c_buffer)[44][45] = 'E';
    (*c_buffer)[45][45] = 'S';
    (*c_buffer)[46][45] = 'T';
    (*c_buffer)[47][45] = 'A';
    (*c_buffer)[48][45] = 'R';
    (*c_buffer)[49][45] = 'T';

    int end_word_pos = 36;
    if(*end){
        if(!(*win)){  
            (*c_buffer)[end_word_pos][13] = 'Y';
            (*c_buffer)[end_word_pos + 1][13] = 'O';
            (*c_buffer)[end_word_pos + 2][13] = 'U';
            (*c_buffer)[end_word_pos + 3][13] = ' ';
            (*c_buffer)[end_word_pos + 4][13] = 'L';
            (*c_buffer)[end_word_pos + 5][13] = 'O';
            (*c_buffer)[end_word_pos + 6][13] = 'S';
            (*c_buffer)[end_word_pos + 7][13] = 'T';
        }
        else{
            (*c_buffer)[end_word_pos][13] = 2;
            (*c_buffer)[end_word_pos + 1][13] = 'Y';
            (*c_buffer)[end_word_pos + 2][13] = 'O';
            (*c_buffer)[end_word_pos + 3][13] = 'U';
            (*c_buffer)[end_word_pos + 4][13] = ' ';
            (*c_buffer)[end_word_pos + 5][13] = 'W';
            (*c_buffer)[end_word_pos + 6][13] = 'I';
            (*c_buffer)[end_word_pos + 7][13] = 'N';
        }
    }
    else{
        (*c_buffer)[end_word_pos][13] = 0;
        (*c_buffer)[end_word_pos + 1][13] = 0;
        (*c_buffer)[end_word_pos + 2][13] = 0;
        (*c_buffer)[end_word_pos + 3][13] = 0;
        (*c_buffer)[end_word_pos + 4][13] = 0;
        (*c_buffer)[end_word_pos + 5][13] = 0;
        (*c_buffer)[end_word_pos + 6][13] = 0;
        (*c_buffer)[end_word_pos + 7][13] = 0;
    }

}

//helper to reveal
void reveal(int open_row, int open_col, struct BOARD *board){
    if (open_row < 0 || open_row >= board->board_y || open_col < 0 || open_col >= board->board_x){
        return; // Out of bounds
    }
    if (board->open_pointer[open_row][open_col]) {
        return; // Already revealed
    }

    board->open_pointer[open_row][open_col] = 1;
    if(board->board_pointer[open_row][open_col] > '0'){
        return; // stop at cells that have number > 0
    }

    for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
            if (dx == 0 && dy == 0) continue; // Skip the current cell
            reveal(open_row + dy, open_col + dx, board);
        }
    }
    
}

void parseMouse(struct BOARD *board, char *status, char *end, char *win, int *clicked, int *G_mouse_Y, int *G_mouse_X){
    if(!(*clicked))
        return;
    *clicked = 0;
    if(*G_mouse_X < 132 && *G_mouse_X > 108 && *G_mouse_Y < 188  && *G_mouse_Y > 176 ){
        *status = 0;
        return;
    }
    if(*G_mouse_X < 164 && *G_mouse_X > 136 && *G_mouse_Y < 188 && *G_mouse_Y > 176){
        *status = 1;
        return;
    }
    if(*G_mouse_X < 204 && *G_mouse_X > 168 && *G_mouse_Y < 188 && *G_mouse_Y > 176){
        restart(COLSIZE, ROWSIZE, board, end, win);
        return;
    }
    if(!(*end)){
        //note 104 is the chosen upper edge
        int cnt_x = (*G_mouse_X-104)/12;
        int cnt_y = (*G_mouse_Y-64)/12;
        if(cnt_x >= 0 && cnt_x <9 && cnt_y >= 0 && cnt_y < 9){
            openOrFlag(board, status, end, cnt_x, cnt_y, win);
        }
    }
}

void restart(int col, int row, struct BOARD *board, char *end, char *win){
    createBoard(COLSIZE, ROWSIZE, board);
    createMine(board);
    createNumHint(board);
    *end = 0;
    *win = 0;
}


void openOrFlag(struct BOARD *board, char *status, char *end, int row, int col, char *win){

    if(*status == 0){
        // lost
        if(board->mine_pointer[row][col] == 1){
            //TODO animate this.
            *end = 1;
        }
        else{
            reveal(row, col, board);
        }
    }
    else {
        board->flagged_pointer[row][col] = 1;
        if(board->mine_pointer[row][col]){
            board->board_num_correct_flags++;
        }
        // wins 
        if(board->board_num_correct_flags == board->board_num_mines){
            *win = 1;
            *end = 1;
        }
    }

}