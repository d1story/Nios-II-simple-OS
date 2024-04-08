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
    char Setting; //0 bit do you need VGA,1 bit full screen app, 2 bit  
    short c_vga[320][240]; //note unless your the dock your bottom 20 pixels are going to get ignored
    char c_buffer[80][60]; //note unless your the dock your bottom 5 character are going to get ignored
    short mouse_X, mouse_Y;
    int L_Interact_Time, H_Interact_Time; //helps determine the Z axis maybe?
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
int moving_ADR;

int *Kernel_Stack_End;

//give it some address range.
proccess *Processes_Base = 0x01FFFFFC;

//give it some address range.
app *App_Base = 0x0100000C;

int Selected_APP;
int Total_APP;

int Running_Proccess;
int Total_Proccess;

//for making new proccess
int *newproc_ADR;
int newproc_APP;

void add_proccess(){
    proccess temp;
    temp.ID = Total_Proccess;
    temp.appID = newproc_APP;
    temp.was_at = newproc_ADR;
    temp.status = 1;
    temp.Stack_Start = 0x2efffc - Total_Proccess*0x10000;
    temp.Stack_End = temp.Stack_Start;
    *(Processes_Base+Total_Proccess) = temp;
    Total_Proccess++;
}

//maybe implement better logic such as prioritizing the app that is currently selected
//change ea
void switch_proccess(){
    Running_Proccess=(Running_Proccess+1)%Total_Proccess;
    proccess *t_running = (proccess *)(Processes_Base+Running_Proccess);
    moving_ADR = t_running->was_at;
    asm("movia et, moving_ADR");
    asm("ldw ea, (et)");
}

////////////////////////////////////ps2 stuff

int *LED = 0xFF200000;

char mouse_CNT;
short G_mouse_X, G_mouse_Y;
char byte1 = 0, byte2 = 0, byte3 = 0;
void handle_mouse(){
	while(1){
		//note when the fifo is not empty the interrupt will bring us back anyways
		//note they come in groups of 3..
		int data = MOUSE->Data;
		if(data & 1<<15){
			byte1 = byte2;
			byte2 = byte3;
			byte3 = data & 0xFF;
			mouse_CNT++;
		}

		if ((byte2 == (char)0xAA) && (byte3 == (char)0x00)){
			MOUSE->Data = 0xF4;
			printf("SETTED UP\n");
			mouse_CNT=2;
		}
		
		if(mouse_CNT==3){
			mouse_CNT = 0;
			
			G_mouse_X += byte2/16;
			G_mouse_Y -= byte3/16;

			G_mouse_X = min(G_mouse_X, 315);
			G_mouse_Y = min(G_mouse_Y, 235);
			G_mouse_Y = max(G_mouse_Y, 0);
			G_mouse_X = max(G_mouse_X, 0);

			if(byte1&1){
				//only care about left button for now
				if(G_mouse_Y>240-OMIT_BOTTOM){
					App_Base->mouse_X = G_mouse_X;
					App_Base->mouse_Y = G_mouse_Y;
				}
				else{
					App_Base[Selected_APP].mouse_X = G_mouse_X;
					App_Base[Selected_APP].mouse_Y = G_mouse_Y;
				}
			}
		} 
		*LED = G_mouse_X;
	}
}


void setup_PS2(){
    MOUSE->Data = 0xFF; // reset
    newproc_ADR = &handle_mouse;
    newproc_APP = 0;
    asm ("trap");
}

//cpu time
unsigned int L_Time, H_Time;

////////////////////////////////////interval stuff
struct R_TIMER{
    int Status, Control, L_Counter_Start, H_Counter_Start, L_Counter_Snap, H_Counter_Snap;
};
typedef struct R_TIMER R_TIMER;

volatile R_TIMER* TIMER = (R_TIMER*)0xFF202000;

void setup_Interval_Timer(){
    //makes interupt occur once every 0.00131072 second
    TIMER->L_Counter_Start = 0;
    TIMER->H_Counter_Start = 1000; //make it slow af TODO change back
    TIMER->Control=7;
}


////////////////////////////////////VGA stuff
short int Buffer1[240][512]; // 240 rows, 512 (320 + padding) columns
short int Buffer2[240][512];
short int *buffer;



//remember to replace with library and const.
volatile R_VGA* VGA = (R_VGA *)0xFF203020;

volatile char *charBuffer= 0x09000000;

//do not call this in interrupt it is blocking call it as a proccess.
void draw_all(){
    app *now = (app*)(App_Base+Selected_APP);
    app *dock = (app*)(App_Base);
    for(int i=0;i<320;i++){
        for(int j=0;j<240 - OMIT_BOTTOM;j++){
            buffer[j*512+i] = now->c_vga[i][j];
        }
        for(int j=240 - OMIT_BOTTOM;j<240;j++){
            buffer[j*512+i] = dock->c_vga[i][j];
        }
    }
    for(int i=0;i<80;i++){
        for(int j=0;j<55;j++){
			charBuffer[(j<<7)+i] = now->c_buffer[i][j];
		}
        for(int j=55;j<60;j++){
            charBuffer[(j<<7)+i] = dock->c_buffer[i][j];
        }
    }

    //this seems a lil sus
    int mousex, mousey;
    while(mousex != G_mouse_X && mousey != G_mouse_Y){
        mousex = G_mouse_X;
        mousey = G_mouse_Y;
    }
    for(int i=mousex;i<mousex+4;i++){
        for(int j=mousey;j<mousey+4; j++){
            buffer[j*512+i] = 111111<<10;
        }
    }
}

int done_setup;

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
    VGA->BackBuffer = (int) &Buffer1; 
    wait_sync();
    VGA->BackBuffer = (int) &Buffer2;
    //make a proccess
    newproc_ADR = &wait_sync;
    newproc_APP = 0;
    asm ("trap");
}

app new_app(){
    app temp;
    for(int i=0;i<320;i++){
        for(int j=0;j<240-OMIT_BOTTOM;j++)
            temp.c_vga[i][j] = 0b1111111111111111;
        for(int j=240-OMIT_BOTTOM;j<241;j++)
            temp.c_vga[i][j] = 0;
    }
    for(int i=0;i<80;i++)
        for(int j=0;j<60;j++)
            temp.c_buffer[i][j] = 0;
    temp.H_Interact_Time = H_Time;
    temp.L_Interact_Time = L_Time;
    temp.mouse_X = 0;
    temp.mouse_Y = 0;
    temp.ID = Total_APP;
    Total_APP ++;
    return temp;
}

//mine sweeper
void MINE_PROCCESS(){
    
}

//Multithread program
void MAIN_MULTI_PROCCESS(){

}

void SUB_MULTI_PROCCESS(){
    
}

//dock app.
const int dock_app_width = 40;//8 character wide.

void DOCK_PROCCESS(){
    proccess proc = *(Processes_Base+Running_Proccess);
    app ap = *(App_Base + proc.appID);
    while(1){
        if(ap.mouse_Y<10){
            if(ap.mouse_X<40){
                newproc_ADR = &MINE_PROCCESS;
                newproc_APP = Total_APP;
                Total_APP++;

            }
            else if(ap.mouse_X<80){
                newproc_ADR = &MAIN_MULTI_PROCCESS;
                newproc_APP = Total_APP;
                Total_APP++;
            }
            int nth_app = ap.mouse_X/40 - 2;
            if(nth_app<Total_APP) Selected_APP = nth_app;
        }
    }
}


void setup_DOCK(){
    newproc_ADR = &DOCK_PROCCESS;
    newproc_APP = 0;
    *App_Base = new_app();
    App_Base->c_buffer[4][58] = '1';
    App_Base->c_buffer[14][58] = '2';
    asm ("trap");
}



int main(){
	/*set up timer interrupt so it interrupts ever so often for os functions*/
    setup_Interval_Timer();
    setup_VGA();
    setup_DOCK();
	setup_PS2();
	//start interupts 
	__builtin_wrctl(3,1);
	__builtin_wrctl(0,1);
	//this is the first proccess so we go there.
	proccess *t_running = (proccess *)(Processes_Base+Running_Proccess);
    moving_ADR = t_running->Stack_End;
	asm("movia sp, moving_ADR");
    asm("ldw sp, (sp)");
	done_setup = 1;
	wait_sync();
}


//exceptions

void the_exception (void) __attribute__ ((section(".exceptions")));

void the_exception (void){
    /*
	if(done_setup){
		proccess *t_running = (proccess *)(Processes_Base+Running_Proccess);
    	t_running->Stack_End = moving_ADR; //note moving_ADR is just the value of sp
	}
	else{
		Kernel_Stack_End = moving_ADR; //note moving_ADR is just the value of sp
	}
    */
    asm("movia et, done_setup");
    asm("ldw et, (et)");
    asm("beq r0, et, E_DONE_SETUP");
    asm("movia et, Processes_Base");
    asm("ldw et, (et)");//potential issue maybe fix
    asm("movia bt, Running_Proccess");
    asm("ldw bt, (bt)");
    asm("muli bt, bt, 32");
    asm("add et, et, bt");
    asm("addi et, et, 12"); // Stack_End is the forth element.
    asm("stw sp, (et)");
    asm("br SKIP_DONE_SETUP");

    asm("E_DONE_SETUP:");
    asm("movia et, Kernel_Stack_End");
    asm("stw sp, (et)");
    
    asm("SKIP_DONE_SETUP:");

    //switching sp to kernel
    asm("movia et, Kernel_Stack_End");
    asm("ldw sp, (et)");

    asm ( ".set noat");
    asm ( ".set nobreak");
    asm ( "subi sp, sp, 128");
    asm ( "stw et, 96(sp)");
    asm ( "rdctl et, ctl4");
    asm ( "beq et, r0, SKIP_EA_DEC");
    asm ( "subi ea, ea,4");
    asm ( "SKIP_EA_DEC:" );
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
    //asm ("stw r25, 100(sp)" ); don't save because we will use bt as an extra register
    asm ("stw r26, 104(sp)");
    asm ("stw r28, 112(sp)");
    //asm ("stw r29, 116(sp)"); don't save because we need to change ea
    asm ("stw r30, 120(sp)");
    asm ("stw r31, 124(sp)");

    asm("call interrupt_handler");

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
    //asm ("ldw r25, 100(sp)" ); don't save because we will use bt as an extra register
    asm ("ldw r26, 104(sp)");
    asm ("ldw r28, 112(sp)");
    //asm ("ldw r29, 116(sp)"); don't save because we need to change ea
    asm ("ldw r30, 120(sp)");
    asm ("ldw r31, 124(sp)");
    asm("addi sp, sp, 128");

	/*
	if(done_setup){
		proccess *t_running = (proccess *)(Processes_Base+Running_Proccess);
		moving_ADR = t_running->Stack_End;
	}
	else{
		moving_ADR = Kernel_Stack_End;
	}
    */
    //switching sp back to proccess.

    asm("movia et, done_setup");
    asm("ldw et, (et)");
    asm("beq r0, et, EF_DONE_SETUP");
    asm("movia et, Processes_Base");
    asm("ldw et, (et)");//potential issue maybe fix
    asm("movia bt, Running_Proccess");
    asm("ldw bt, (bt)");
    asm("muli bt, bt, 32");
    asm("add et, et, bt");
    asm("addi et, et, 12"); // Stack_End is the forth element.
    asm("ldw sp, (et)");
    asm("br EF_SKIP_DONE_SETUP");

    asm("EF_DONE_SETUP:");
    asm("movia et, Kernel_Stack_End");
    asm("ldw sp, (et)");
    
    asm("EF_SKIP_DONE_SETUP:");

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
			printf("innn timer");
			L_Time++;
			if(L_Time == 0)
				H_Time++;
			switch_proccess();
		}
	}

    //means this is a software trap to add a proccess/app
    if(!ipending){
        add_proccess();
    }
}