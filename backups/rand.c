//changing 4/3
// #include "helper.h"
// #include "g.h"
#include <stdio.h>
int min(int a, int b){
    if(a <= b) return a;
    else return b;
}

int max(int a, int b){
    if(a >= b) return a;
    else return b;
}

//
struct R_VGA{
    int Buffer,BackBuffer, Resolution, Status, Control;
};
typedef struct R_VGA R_VGA;

struct app{
    int ID;
    int mouse_X, mouse_Y;
    int clicked;
    int L_Interact_Time, H_Interact_Time; //helps determine the Z axis maybe?
    short c_vga[320][240]; //note unless your the dock your bottom 20 pixels are going to get ignored
    char c_buffer[80][60]; //note unless your the dock your bottom 5 character are going to get ignored
    int shared_HEAP[100];
};
typedef struct app app;

//make sure you don't change this again this will break things if you change.
// most notable the initial set up in exception.
struct proccess{
    int ID;
    int appID;
    int *Stack_Start, *Stack_End; //in theory Stack_start is unessecary.
    int *was_at; //address to the address of where the thing left off
    unsigned int L_Start_Time, H_Start_Time;
    int status; //1 is ready 0 is waiting.
};
typedef struct proccess proccess;

struct R_PS2{
    int Data, Control;
};
typedef struct R_PS2 R_PS2;

volatile R_PS2* MOUSE = (R_PS2*)0xFF200100;


//for dock
const int OMIT_BOTTOM = 20;

//mad sketchy idk how to do this better.
volatile int moving_ADR;

int Kernel_Stack_End = 0x1000000;
//holds the current_proccess_stack_end address.
volatile int current_Proccess_Stack_End = 0;
volatile int main_Stack = 0;

volatile proccess Processes_Base[17];

volatile app App_Base[7];

volatile int Selected_APP = 0;
volatile int Total_APP = 0;

volatile int Running_Proccess = 0;
volatile int Total_Proccess = 0;

//for making new proccess
volatile int *newproc_ADR = 0;
volatile int newproc_APP = 0;
volatile int new_APP = 0;


void add_proccess(){
    Processes_Base[Total_Proccess].ID = Total_Proccess;
    Processes_Base[Total_Proccess].appID = newproc_APP;
    Processes_Base[Total_Proccess].was_at = newproc_ADR;
    Processes_Base[Total_Proccess].status = 1;
    Processes_Base[Total_Proccess].Stack_Start = (int*)(0x2efffc - Total_Proccess*0x10000);
    Processes_Base[Total_Proccess].Stack_End = Processes_Base[Total_Proccess].Stack_Start;
    Total_Proccess++;
}

//maybe implement better logic such as prioritizing the app that is currently selected
//change ea
void switch_proccess(){
    if(Running_Proccess!=-1){
        asm("movia et, moving_ADR");
        asm("stw ea, (et)");
        Processes_Base[Running_Proccess].was_at = (int*)moving_ADR;
    }
    Running_Proccess=(Running_Proccess+1)%Total_Proccess;
    moving_ADR = Processes_Base[Running_Proccess].was_at;
    current_Proccess_Stack_End = &Processes_Base[Running_Proccess].Stack_End;
    asm("movia et, moving_ADR");
    asm("ldw ea, (et)");
}

////////////////////////////////////ps2 stuff

int *LED = 0xFF200000;

volatile char mouse_CNT = 1;
volatile int G_mouse_X, G_mouse_Y;
volatile char byte1 = 0, byte2 = 0, byte3 = 0;
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

    newproc_ADR = handle_mouse;
    newproc_APP = 0;
    asm ("trap");
}

//cpu time
volatile unsigned int L_Time, H_Time;

////////////////////////////////////interval stuff
struct R_TIMER{
    int Status, Control, L_Counter_Start, H_Counter_Start, L_Counter_Snap, H_Counter_Snap;
};
typedef struct R_TIMER R_TIMER;

volatile R_TIMER* TIMER = (R_TIMER*)0xFF202000;

void setup_Interval_Timer(){
    //makes interupt occur once every 0.00131072 second
    TIMER->L_Counter_Start = 0;
    TIMER->H_Counter_Start = 10; //make it slow af TODO change back
    TIMER->Control=7;
}


////////////////////////////////////VGA stuff
volatile short int Buffer1[240][512]; // 240 rows, 512 (320 + padding) columns
volatile short int Buffer2[240][512];
volatile short int *buffer;



//remember to replace with library and const.
volatile R_VGA* VGA = (R_VGA *)0xFF203020;

volatile char *charBuffer= 0x09000000;

//do not call this in interrupt it is blocking call it as a proccess.
void draw_all(){
    //note App_Base[0] is dock
    for(int i=0;i<320;i++){
        for(int j=0;j<240 - OMIT_BOTTOM;j++){
            buffer[j*512+i] = App_Base[Selected_APP].c_vga[i][j];
        }
        for(int j=240 - OMIT_BOTTOM;j<240;j++){
            buffer[j*512+i] = App_Base[0].c_vga[i][j];
        }
    }
    for(int i=0;i<80;i++){
        for(int j=0;j<55;j++){
			charBuffer[(j<<7)+i] = App_Base[Selected_APP].c_buffer[i][j];
		}
        for(int j=55;j<60;j++){
            charBuffer[(j<<7)+i] = App_Base[0].c_buffer[i][j];
        }
    }

    // //this seems a lil sus
    // int mousex, mousey;
    // while(mousex != G_mouse_X && mousey != G_mouse_Y){
    //     mousex = G_mouse_X;
    //     mousey = G_mouse_Y;
    //     printf("%d\n",mousey);
    // }
    for(int i=G_mouse_X;i<G_mouse_X+4;i++){
        for(int j=G_mouse_Y;j<G_mouse_Y+4; j++){
            buffer[j*512+i] = 1<<8;
            //printf("DRAWING\n");
        }
    }
}

volatile int done_setup = 0;

void wait_sync(){
    while(1){
        if(done_setup) draw_all();
        VGA->Buffer = 1;
        while(1){
            if(VGA->Status&1) continue;
            else break;
        }
        buffer = VGA->BackBuffer;
		if(!done_setup) break;
    }
}

void setup_VGA(){
    VGA->BackBuffer = (int) Buffer1; 
    wait_sync();
    VGA->BackBuffer = (int) Buffer2;
    //make a proccess
    newproc_ADR = (int *)wait_sync;
    newproc_APP = 0;
    asm ("trap");
}

void new_app(){
    for(int i=0;i<320;i++){
        for(int j=0;j<240-OMIT_BOTTOM;j++)
            App_Base[Total_APP].c_vga[i][j] = 0b1111111111111111;
        for(int j=240-OMIT_BOTTOM;j<241;j++)
            App_Base[Total_APP].c_vga[i][j] = 0;
    }
    for(int i=0;i<80;i++)
        for(int j=0;j<60;j++)
            App_Base[Total_APP].c_buffer[i][j] = 0;
    App_Base[Total_APP].H_Interact_Time = H_Time;
    App_Base[Total_APP].L_Interact_Time = L_Time;
    App_Base[Total_APP].mouse_X = 0;
    App_Base[Total_APP].mouse_Y = 0;
    App_Base[Total_APP].ID = Total_APP;
    Total_APP++;
}

//Multithread program produces a random number. <-- seems pretty random to me.
volatile int number = 0;
void MAIN_MULTI_PROCCESS(){

}

int SUB_MULTI_PROCCESS(){
    return number%3 + 1;
}

void MAIN_MULTI_PROCCESS_EVEN(){
    while(1){
        number ++;
    }
}

void MAIN_MULTI_PROCCESS_ODD(){
    while(1){
        if(number%2){
            number ++;
        }
    }
}

void setup_test_proccess(){
    new_APP = 1;
    newproc_APP = Total_APP;
    newproc_ADR = (int*)MAIN_MULTI_PROCCESS_ODD;
    asm("trap");
    new_APP = 0;
    newproc_ADR = (int*)MAIN_MULTI_PROCCESS_EVEN;
    newproc_APP = Total_APP-1;
    asm("trap");
}

//mine sweeper
// void MINE_PROCCESS(){
//     char game_end = 0;
//     char game_win = 0;
//     // 0 is open, 1 is flag
//     char game_status = 0;
//     struct BOARD game_board;
//     createBoard(COLSIZE, ROWSIZE, &game_board);
//     createMine(&game_board);
//     createNumHint(&game_board);

// 	setup_VGA();
//     setup_PS2();

//     setupLogo();
//     while(1){
//         printBoard(&game_board, &game_end, &game_win);
//         parseMouse(&game_board, &game_status, &game_end, &game_win);
//     }
// }

//dock app.
const int dock_app_width = 40;//8 character wide.

//TODO ADD FUNCTIONALITY TO CLOSE APPS
void DOCK_PROCCESS(){
    while(1){
        if(App_Base[0].mouse_Y>220){
            printf("Running_Proccess %d\n",App_Base[0].mouse_Y);

            if(App_Base[0].mouse_X<40 && App_Base[0].clicked){
                App_Base[0].clicked = 0;
                new_APP = 1;
                newproc_ADR = (int *)MAIN_MULTI_PROCCESS_ODD;
                newproc_APP = Total_APP;
                asm ("trap");
            }
            else if(App_Base[0].mouse_X<80 && App_Base[0].clicked){
                App_Base[0].clicked = 0;
                new_APP = 1;
                newproc_ADR = (int *)MAIN_MULTI_PROCCESS_ODD;
                newproc_APP = Total_APP;
                asm ("trap");
            }
            int nth_app = App_Base[0].mouse_X/40 - 2;
            if(nth_app<Total_APP) Selected_APP = nth_app;
        }
    }
}


void setup_DOCK(){
    newproc_ADR = (int *)DOCK_PROCCESS;
    newproc_APP = 0;
    new_APP = 1;
    asm ("trap");
    App_Base[0].c_buffer[4][58] = '1';
    App_Base[0].c_buffer[14][58] = '2';
}


volatile int *KEY = 0xFF200050;

int main(){
	/*set up timer interrupt so it interrupts ever so often for os functions*/
    Running_Proccess = -1;
    setup_Interval_Timer();
    setup_DOCK();
    setup_VGA();
	setup_PS2();
    setup_test_proccess();
    //added push button to add proccess for easy debug.
    *(KEY+2) = 15;
	__builtin_wrctl(3,1 + 2);
	__builtin_wrctl(0,1);
	done_setup = 1;
    while(1){

    }
}


//exceptions

void the_exception (void) __attribute__ ((section(".exceptions")));


void the_exception (void){
    //store to old process stack
    asm ( ".set noat");
    asm ( ".set nobreak");
    asm ( "subi sp, sp, 128");
    asm ("stw r1,  4(sp)" );
    asm ("stw r2,  8(sp)" );
    asm ("stw r3,  12(sp)" );
    asm ("stw r4,  16(sp)" );
    asm ("stw r5,  20(sp)" );
    asm ("stw r6,  24(sp)" );
    asm ("stw r7,  28(sp)" );
    asm ("stw r8,  32(sp)" );
    asm ("stw r9,  36(sp)" );
    asm ("stw r10, 40(sp)" );
    asm ("stw r11, 44(sp)" );
    asm ("stw r12, 48(sp)" );
    asm ("stw r13, 52(sp)" );
    asm ("stw r14, 56(sp)" );
    asm ("stw r15, 60(sp)" );
    asm ("stw r16, 64(sp)" );
    asm ("stw r17, 68(sp)" );
    asm ("stw r18, 72(sp)" );
    asm ("stw r19, 76(sp)" );
    asm ("stw r20, 80(sp)" );
    asm ("stw r21, 84(sp)" );
    asm ("stw r22, 88(sp)" );
    asm ("stw r23, 92(sp)" );
    asm ("stw r25, 100(sp)" ); //don't save because we will use bt as an extra register
    asm ("stw r26, 104(sp)");
    asm ("stw r28, 112(sp)");
    //asm ("stw r29, 116(sp)"); don't save because we need to change ea
    asm ("stw r30, 120(sp)");
    asm ("stw r31, 124(sp)");

    //if there is current_Proccess then store into proccess stack otherwise still in main put into main stack.
    asm("movia et, current_Proccess_Stack_End");
    asm("ldw et, (et)");
    asm("bne et, r0, SKIP_STORE_MAIN_SP");
    asm("movia et, main_Stack");
    asm("stw sp, (et)");
    asm("br SKIP_STORE_P_STACK:");
    asm("SKIP_STORE_MAIN_SP:");
    asm("stw sp, (et)");

    asm("SKIP_STORE_P_STACK:");

    asm("movia et, Kernel_Stack_End");
    asm("ldw sp, (et)");

    asm ( "rdctl et, ctl4");
    asm ( "beq et, r0, SKIP_EA_DEC");
    asm ( "subi ea, ea,4");
    asm ( "SKIP_EA_DEC:" );

    asm("call interrupt_handler");

    //if there is current_Proccess then load proccess stack otherwise still in main load main stack.
    asm("movia et, current_Proccess_Stack_End");
    asm("ldw et, (et)");
    asm("bne et, r0, SKIP_STORE_MAIN_SP2");
    asm("movia et, main_Stack");
    asm("ldw sp, (et)");
    asm("br SKIP_STORE_P_STACK2:");
    asm("SKIP_STORE_MAIN_SP2:");
    asm("ldw r7, -4(et)");
    asm("ldw sp, (et)");
    //no need to load registers if it is first time calling proccess.
    asm("beq sp, r7, SKIP_LOAD");
    asm("SKIP_STORE_P_STACK2:");

    asm ("ldw r1,  4(sp)" );
    asm ("ldw r2,  8(sp)" );
    asm ("ldw r3,  12(sp)" );
    asm ("ldw r4,  16(sp)" );
    asm ("ldw r5,  20(sp)" );
    asm ("ldw r6,  24(sp)" );
    asm ("ldw r7,  28(sp)" );
    asm ("ldw r8,  32(sp)" );
    asm ("ldw r9,  36(sp)" );
    asm ("ldw r10, 40(sp)" );
    asm ("ldw r11, 44(sp)" );
    asm ("ldw r12, 48(sp)" );
    asm ("ldw r13, 52(sp)" );
    asm ("ldw r14, 56(sp)" );
    asm ("ldw r15, 60(sp)" );
    asm ("ldw r16, 64(sp)" );
    asm ("ldw r17, 68(sp)" );
    asm ("ldw r18, 72(sp)" );
    asm ("ldw r19, 76(sp)" );
    asm ("ldw r20, 80(sp)" );
    asm ("ldw r21, 84(sp)" );
    asm ("ldw r22, 88(sp)" );
    asm ("ldw r23, 92(sp)" );
    asm ("ldw r25, 100(sp)" ); //don't save because we will use bt as an extra register
    asm ("ldw r26, 104(sp)");
    asm ("ldw r28, 112(sp)");
    //asm ("ldw r29, 116(sp)"); don't save because we need to change ea
    asm ("ldw r30, 120(sp)");
    asm ("ldw r31, 124(sp)");
    asm("addi sp, sp, 128");

    asm("SKIP_LOAD:");


    asm("eret");
}

//maybe implement memory leak controller??????
void interrupt_handler(void){
    int ipending = __builtin_rdctl(4);
    //this is interval timer.
	
	//reset so you don't get stuck in exception land before done_setup
	TIMER->Status = 0;
	if(done_setup){
		if((ipending & 1)){
			L_Time++;
			if(L_Time == 0)
				H_Time++;
			switch_proccess();
		}
	}

    if(ipending & 2){
        *(KEY+3) = 15;
        new_app();
        newproc_ADR = (int *)MAIN_MULTI_PROCCESS_ODD;
        newproc_APP = Total_APP - 1;
        add_proccess();
    }
    
    //means this is a software trap to add a proccess/app
    if(!ipending){
        if(new_APP) new_app();
        add_proccess();
    }
}
