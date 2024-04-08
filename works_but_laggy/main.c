//changing 4/3
#include "helper.h"
#include <stdio.h>
void trap_helper(){
    asm("trap");
}

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
    volatile int Buffer,BackBuffer, Resolution, Status, Control;
};
typedef struct R_VGA R_VGA;

struct app{
    volatile int ID;
    volatile int mouse_X, mouse_Y;
    volatile int clicked;
    volatile int L_Interact_Time, H_Interact_Time; //helps determine the Z axis maybe?
    volatile short c_vga[320][240]; //note unless your the dock your bottom 20 pixels are going to get ignored
    volatile char c_buffer[80][60]; //note unless your the dock your bottom 5 character are going to get ignored
    volatile int shared_HEAP[100];
};
typedef struct app app;

//make sure you don't change this again this will break things if you change.
// most notable the initial set up in exception.
struct proccess{
    volatile int ID;
    volatile int appID;
    volatile int * volatile Stack_Start, * volatile Stack_End; //in theory Stack_start is unessecary.
    volatile int * volatile was_at; //address to the address of where the thing left off
    volatile unsigned int L_Start_Time, H_Start_Time;
    volatile int status; //1 is ready 0 is waiting.
};
typedef struct proccess proccess;

struct R_PS2{
    volatile int Data, Control;
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
volatile int * volatile newproc_ADR = 0;
volatile int newproc_APP = 0;


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

volatile int G_mouse_X, G_mouse_Y, T_mouse_X, T_mouse_Y;
volatile int interrupted;
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
        T_mouse_Y -= byte3;
    }

    if((byte1 & 0x40) == 0){
        T_mouse_X += byte2;
    }

    T_mouse_X = min(T_mouse_X, 315*8);
    T_mouse_Y = min(T_mouse_Y, 235*8);
    T_mouse_Y = max(T_mouse_Y, 0);
    T_mouse_X = max(T_mouse_X, 0);
    G_mouse_X = T_mouse_X/8;
    G_mouse_Y = T_mouse_Y/8;
    if(byte1&1){
        if(G_mouse_Y>240-OMIT_BOTTOM){
            App_Base->clicked = 1;
            App_Base->mouse_X = G_mouse_X;
            App_Base->mouse_Y = G_mouse_Y;
        }
        else{
            App_Base[Selected_APP].clicked = 1;
            App_Base[Selected_APP].mouse_X = G_mouse_X;
            App_Base[Selected_APP].mouse_Y = G_mouse_Y;
        }
    }
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

    // newproc_ADR = handle_mouse;
    // newproc_APP = 0;
    // trap_helper();
}

//cpu time
volatile unsigned int L_Time, H_Time;

////////////////////////////////////interval stuff
struct R_TIMER{
    volatile int Status, Control, L_Counter_Start, H_Counter_Start, L_Counter_Snap, H_Counter_Snap;
};
typedef struct R_TIMER R_TIMER;

volatile R_TIMER* TIMER = (R_TIMER*)0xFF202000;

void setup_Interval_Timer(){
    //makes interupt occur once every 0.00131072 second
    TIMER->L_Counter_Start = 0;
    TIMER->H_Counter_Start = 100; //make it slow af TODO change back
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

    for(int i=G_mouse_X;i<G_mouse_X+4;i++){
        for(int j=G_mouse_Y;j<G_mouse_Y+4; j++){
            buffer[j*512+i] = 1<<8;
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
    trap_helper();
}

void new_app(){
    for(int i=0;i<320;i++){
        for(int j=0;j<240-OMIT_BOTTOM;j++)
            App_Base[Total_APP].c_vga[i][j] = 0b1111111111111111;
        for(int j=240-OMIT_BOTTOM;j<241;j++)
            App_Base[Total_APP].c_vga[i][j] = 0b111111;
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
    new_app();
    newproc_APP = Total_APP-1;
    newproc_ADR = (int*)MAIN_MULTI_PROCCESS_ODD;
    trap_helper();
    newproc_ADR = (int*)MAIN_MULTI_PROCCESS_EVEN;
    newproc_APP = Total_APP-1;
    trap_helper();
}

//mine sweeper
void MINE_PROCCESS(){
    char game_end = 0;
    char game_win = 0;
    // 0 is open, 1 is flag
    char game_status = 0;
    struct BOARD game_board;
    short (*c_vga)[320][240] = &App_Base[Processes_Base[Running_Proccess].appID].c_vga;
    char (*c_buffer)[80][60] = &App_Base[Processes_Base[Running_Proccess].appID].c_buffer;
    int * gmx = &App_Base[Processes_Base[Running_Proccess].appID].mouse_X;
    int * gmy = &App_Base[Processes_Base[Running_Proccess].appID].mouse_Y;
    int * clicked = &App_Base[Processes_Base[Running_Proccess].appID].clicked;
    createBoard(COLSIZE, ROWSIZE, &game_board);
    createMine(&game_board);
    createNumHint(&game_board);

    setupLogo(c_vga);
    while(1){
        printBoard(&game_board, &game_end, &game_win, c_vga, c_buffer);
        parseMouse(&game_board, &game_status, &game_end, &game_win, clicked, gmy, gmx);
    }
}

//dock app.
const int dock_app_width = 40;//8 character wide.

//TODO ADD FUNCTIONALITY TO CLOSE APPS
void DOCK_PROCCESS(){
    while(1){
        if(App_Base[0].mouse_Y>220){
            if(App_Base[0].mouse_X<40 && App_Base[0].clicked){
                printf("Running_Proccess %d\n", Total_Proccess);
                App_Base[0].clicked = 0;
                new_app();
                newproc_ADR = (int *)MINE_PROCCESS;
                newproc_APP = Total_APP-1;
                trap_helper();
                App_Base[0].c_buffer[4+Total_APP*10][58] = '1';
            }
            else if(App_Base[0].mouse_X<80 && App_Base[0].clicked){
                printf("Running_Proccess %d\n", Total_Proccess);
                App_Base[0].clicked = 0;
                new_app();
                newproc_ADR = (int *)MAIN_MULTI_PROCCESS_ODD;
                newproc_APP = Total_APP - 1;
                trap_helper();
                App_Base[0].c_buffer[4+Total_APP*10][58] = '2';
            }
            int nth_app = App_Base[0].mouse_X/40 - 1;
            if(nth_app<Total_APP && nth_app>0 && App_Base[0].clicked){
                Selected_APP = nth_app;
                App_Base[0].clicked = 0;
            } 
        }
    }
}


void setup_DOCK(){
    newproc_ADR = (int *)DOCK_PROCCESS;
    newproc_APP = 0;
    new_app();
    trap_helper();
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
    //setup_test_proccess();
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
    interrupted = 1;
    int ipending = __builtin_rdctl(4);
    //this is interval timer.
	
	//reset so you don't get stuck in exception land before done_setup
	TIMER->Status = 0;
	if(done_setup){
        handle_mouse();
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
        newproc_ADR = (int *)MINE_PROCCESS;
        newproc_APP = Total_APP - 1;
        add_proccess();
    }
    
    //means this is a software trap to add a proccess/app
    if(!ipending){
        add_proccess();
    }
}


int mouse_click_sound[481] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -23855104, 12320768, 15925248, -19857408, -15269888, 17039360, 11927552, -2424832, 7602176, 983040, -32571392, -43057152, -9109504, 33685504, 43057152, 18743296, -11010048, -25624576, -10158080, 18481152, 14942208, 2097152, 22872064, 22085632, -20643840, -14024704, 35848192, 26673152, -19791872, -22020096, -2621440, -6553600, -3670016, 32505856, 43384832, -15466496, -56033280, -9830400, 20709376, -38666240, -88342528, -43122688, 35192832, 46399488, -3932160, -25690112, -6553600, -4653056, -10027008, -11862016, -44892160, -60227584, 851968, 62849024, 43581440, -4194304, 25690112, 89587712, -15990784, -291831808, -426901504, -278265856, -60555264, 42401792, 67371008, 85262336, 57344000, -57671680, -159973376, -183107584, -206700544, -191954944, -38797312, 121241600, 159842304, 143523840, 92733440, -1310720, -71761920, -112787456, -137035776, -92667904, 26214400, 176816128, 300154880, 323289088, 248315904, 131203072, 21692416, -18153472, -11206656, -33619968, -28508160, 58261504, 127991808, 122421248, 89653248, 56295424, 17825792, -34013184, -95944704, -142475264, -154533888, -116326400, -38993920, 7340032, -22806528, -110297088, -206831616, -242679808, -208601088, -175505408, -155975680, -109182976, -43188224, 12386304, 45154304, 54394880, 30736384, -26673152, -44761088, 8192000, 30605312, 15073280, 61472768, 133365760, 148242432, 110493696, 50593792, 30605312, 56033280, 67764224, 92667904, 106299392, 58392576, 79888384, 139591680, 51904512, -14942208, 62128128, 48300032, -35782656, -1835008, 78643200, 144834560, 142999552, 3932160, -71892992, 2883584, 25821184, -16908288, -56885248, -74907648, 14024704, 72417280, 7995392, 46596096, 93126656, -23068672, -32047104, 36765696, -36044800, -36175872, 28049408, -1114112, 85786624, 156172288, -13697024, -70909952, 32178176, 53870592, 148897792, 204734464, 36241408, -14024704, 23134208, -80412672, -10485760, 105971712, -89784320, 51642368, 913833984, 1354694656, 383320064, -1017315328, -993656832, 239861760, 613810176, -17825792, -7471104, 355663872, -40042496, -231997440, 447152128, 540999680, -479985664, -1043791872, -358023168, 465174528, 518389760, 256376832, 143261696, -104726528, -380174336, -113836032, 252706816, -238551040, -974454784, -545652736, 491847680, 458293248, -288489472, -242810880, 261816320, 75628544, -399835136, -240254976, 249692160, 250019840, -101187584, -42926080, 261816320, 45088768, -406519808, -389283840, -146472960, 6881280, 201719808, 248250368, 43319296, 32178176, 357171200, 491061248, 44695552, -426573824, -115408896, 457310208, 252641280, -249888768, -158466048, 68419584, -93519872, -186843136, 82378752, 308674560, 176160768, -49807360, -29687808, 97648640, 156237824, 162791424, -70582272, -396427264, -231276544, 140836864, -22413312, -383975424, -303235072, -24576000, 21561344, -88997888, -50528256, 137887744, 94175232, -160038912, -153092096, -5636096, -83623936, -110952448, 34471936, 3604480, -67960832, 104792064, 212729856, 82640896, 13828096, 7143424, -177471488, -360644608, -240189440, 11993088, 66781184, -35192832, -117833728, -173080576, -139591680, 54853632, 200802304, 148897792, 115277824, 185139200, 151977984, 35258368, 25624576, 25427968, -145620992, -338821120, -268566528, 31850496, 192479232, 50397184, -73269248, 40239104, 113049600, -40501248, -141492224, -17563648, 55443456, -112590848, -273285120, -233635840, -175833088, -207028224, -206045184, -162136064, -81068032, 89128960, 172490752, 50003968, -12779520, 127139840, 229113856, 115802112, -49545216, 12976128, 168165376, 2555904, -262864896, -158990336, -38469632, -235339776, -301793280, -64356352, 228327424, 505348096, 456261632, -38600704, -281280512, -96796672, -188809216, -534708224, -460193792, 61014016, 431095808, 204472320, -227540992, -79888384, 319488000, 144900096, -102694912, 189792256, 284753920, -113573888, -141164544, 236519424, 145162240, -401866752, -595460096, -247529472, 61997056, 34930688, 48824320, 298385408, 434634752, 384106496, 364511232, 115277824, -491847680, -730464256, -328335360, -165543936, -469172224, -276430848, 444203008, 535363584, -37552128, -117374976, 356646912, 349110272, -166526976, -242417664, 132710400, 148832256, -135004160, -187236352, -313262080, -679936000, -533594112, 142016512, 337051648, 32833536, 129433600, 520028160, 377028608, -127729664, -144637952, 183894016, 63045632, -247857152, -75890688, 66781184, -300417024, -482082816, -242810880, -154271744, -106627072, 205717504, 420872192, 348192768, 197394432, 88539136, 79953920, 43319296, -117899264, -113836032, 18415616, -89718784, -207028224, -116981760, -189267968, -346685440, -156106752, 189923328, 344522752, 345243648, 296550400, 284295168, 263651328, 67567616, -150208512, -193724416, -275709952, -427753472, -311361536, -5242880, 34340864, -154206208, -122748928, 91291648, 92340224};

int mine_Logo[30][30]={
{42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388},
{42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388},
{42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388},
{42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 29582, 33808, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388},
{42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 50712, 46486, 40147, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388},
{42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 50712, 46486, 40147, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388},
{42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 59164, 50712, 42388, 42388, 59164, 50712, 46486, 29582, 0, 42388, 42388, 42388, 0, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388},
{42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 50712, 65535, 42388, 42388, 50712, 50712, 46486, 52857, 46518, 0, 0, 0, 31695, 0, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388},
{42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 50744, 65535, 44405, 48599, 65535, 65535, 65535, 50712, 31695, 31695, 0, 65535, 35921, 0, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388},
{42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 33808, 48631, 65535, 65535, 65535, 65535, 48631, 50712, 33808, 33808, 33808, 0, 0, 0, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388},
{42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 38066, 48599, 65535, 46486, 50712, 50712, 50712, 52857, 48631, 48631, 0, 40179, 0, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388},
{42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 33808, 65535, 65535, 48599, 50712, 50712, 27501, 54938, 50744, 50744, 0, 42260, 0, 0, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388},
{42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 33840, 50712, 46486, 50712, 48631, 33808, 52857, 29582, 33808, 33808, 31727, 0, 0, 0, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388},
{42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 29614, 65535, 50744, 50712, 33808, 65535, 25388, 0, 50744, 33808, 0, 42260, 0, 0, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388},
{42388, 42388, 42388, 42388, 42388, 42388, 65535, 65535, 48599, 48631, 52857, 29582, 33840, 50712, 29582, 0, 0, 0, 0, 40179, 0, 0, 0, 0, 42388, 42388, 42388, 42388, 42388, 42388},
{42388, 42388, 42388, 42388, 42388, 42388, 52857, 50712, 33808, 33808, 29582, 52857, 0, 33840, 33840, 0, 0, 31727, 40179, 0, 0, 0, 33840, 40179, 42388, 42388, 42388, 42388, 42388, 42388},
{42388, 42388, 42388, 42388, 42388, 42388, 0, 0, 31695, 31695, 31695, 31727, 31727, 0, 0, 0, 0, 35921, 0, 0, 0, 0, 0, 0, 42388, 42388, 42388, 42388, 42388, 42388},
{42388, 42388, 42388, 42388, 42388, 42388, 0, 42388, 0, 33808, 33808, 33808, 31695, 31695, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 42388, 42388, 42388, 42388, 42388, 42388},
{42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 0, 35921, 42260, 0, 0, 0, 0, 42260, 35921, 0, 0, 0, 0, 0, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388},
{42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 0, 0, 40179, 0, 0, 0, 0, 40179, 0, 0, 0, 0, 0, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388},
{42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 0, 0, 0, 40147, 0, 0, 0, 0, 0, 0, 0, 0, 0, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388},
{42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 35953, 35953, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388},
{42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 48631, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388},
{42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 0, 0, 42388, 0, 0, 0, 0, 0, 0, 0, 42388, 0, 31727, 0, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388},
{42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 0, 40179, 0, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388},
{42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 40147, 40147, 0, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388},
{42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 0, 0, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388},
{42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388},
{42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388},
{42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388, 42388}
};