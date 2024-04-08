//working mine game.
//remember to add and include files and setup timer
#include <stdio.h>
#include "global.h"
#include "helper.h"

int min(int a, int b){
    if(a <= b) return a;
    else return b;
}

int max(int a, int b){
    if(a >= b) return a;
    else return b;
}

////////////////////////////////////ps2 stuff
int soundCNT;
struct AUDIO{
    int control, fifo, left, right;
};
typedef struct AUDIO AUDIO;

volatile AUDIO *R_AUDIO = (AUDIO *)(0xFF203040); 
void makeSound(){
    int space = R_AUDIO->fifo >> 24; 
    while(space && soundCNT){
        space--;
        R_AUDIO->left = mouse_click_sound[soundCNT];
        R_AUDIO->right = mouse_click_sound[soundCNT];
        soundCNT = (soundCNT+1)%400;
    }
}


void handle_mouse(){
    MOUSE->Data = 0xEB;
    while(1){
        if((MOUSE->Data & 0xFF) == 0xFA) break;
    }
    //note they come in groups of 3..
    int mouse_CNT = 0;
    char byte1 = 0, byte2 = 0, byte3 = 0;
    while(mouse_CNT < 3){
        int data = MOUSE->Data;
        if(data & 1<<15){
            byte1 = byte2;
            byte2 = byte3;
            byte3 = data & 0xFF;
            mouse_CNT++;
        }
    }

    if((byte1 & 0x80) == 0){
        G_mouse_Y -= byte3;
    }

    if((byte1 & 0x40) == 0){
        G_mouse_X += byte2;
    }



    if(byte1&1){
        clicked = 1;
        soundCNT = 100;
    }
    G_mouse_X = min(G_mouse_X, 315*8);
    G_mouse_Y = min(G_mouse_Y, 235*8);
    G_mouse_Y = max(G_mouse_Y, 0);
    G_mouse_X = max(G_mouse_X, 0);
}


void setup_PS2(){
    MOUSE->Data = 0xFF; // reset
    while(1){
        if((MOUSE->Data & 0xFF) == 0xFA) break;
    }
    MOUSE->Data = 0xF0;
    while(1){
        if((MOUSE->Data & 0xFF) == 0xFA) break;
    }
    printf("DONE!");
    MOUSE->Control = 1;
}


//do not call this in interrupt it is blocking call it as a proccess.
void draw_all(){
    for(int i=0;i<320;i++){
        for(int j=0;j<240;j++){
            buffer[j*512+i] = c_vga[j][i];
        }
    }

    for(int i=0;i<80;i++){
        for(int j=0;j<60;j++){
			charBuffer[(j<<7)+i] = c_buffer[i][j];
		}
    }

    for(int i=G_mouse_X/8;i<G_mouse_X/8+4;i++){
        for(int j=G_mouse_Y/8;j<G_mouse_Y/8+4; j++){
            buffer[j*512+i] = 1<<8;
            //printf("DRAWING\n");
        }
    }
}

volatile int done_setup = 0;

void wait_sync(){
    if(VGA->Status&1) return;
    else buffer = VGA->BackBuffer;
    draw_all();
    VGA->Buffer = 1;
}

void setup_VGA(){
    for(int i=0;i<320;i++){
        for(int j=0;j<240;j++){
            c_vga[j][i] = 0b1010010110010100;
        }
    }
    VGA->BackBuffer = (int) Buffer1; 
    VGA->Buffer = 1;
    while(1){
        if(VGA->Status&1) continue;
        else break;
    }
    VGA->BackBuffer = (int) Buffer2;
}

void setupLogo(){
    int relYpos = 16;
    int relXpos = 145;
    for(int i=0;i<30;i++){
        for(int j=0;j<30;j++){
            c_vga[relYpos+j][relXpos+i] = mine_Logo[i][j];
        }
    }
}

int main(){
    char game_end = 0;
    char game_win = 0;
    // 0 is open, 1 is flag
    char game_status = 0;
    struct BOARD game_board;
    createBoard(COLSIZE, ROWSIZE, &game_board);
    createMine(&game_board);
    createNumHint(&game_board);

	setup_VGA();
    setup_PS2();

    setupLogo();
    while(1){
        wait_sync();
        handle_mouse();
        makeSound();
        printBoard(&game_board, &game_end, &game_win);
        parseMouse(&game_board, &game_status, &game_end, &game_win);
    }
}
