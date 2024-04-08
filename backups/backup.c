#include<stdio.h>
void HEX_PS2(char b1, char b2, char b3);

struct R_PS2{
    int Data, Control;
};
typedef struct R_PS2 R_PS2;

volatile R_PS2* MOUSE = (R_PS2*)0xFF200100;

int main(void) {
__builtin_wrctl(3,(1<<7));
__builtin_wrctl(0,1);
/* Declare volatile pointers to I/O registers (volatile means that IO load
and store instructions will be used to access these pointer locations,
instead of regular memory loads and stores) */
// PS/2 mouse needs to be reset (must be already plugged in)
MOUSE->Data = 0xFF; // reset
MOUSE->Control = 1; // reset
while(1){

}
}
/****************************************************************************************
* Subroutine to show a string of HEX data on the HEX displays
****************************************************************************************/
void HEX_PS2(char b1, char b2, char b3) {
volatile int * HEX3_HEX0_ptr = (int *)0xFF200020;
volatile int * HEX5_HEX4_ptr = (int *)0xFF200030;
/* SEVEN_SEGMENT_DECODE_TABLE gives the on/off settings for all segments in
* a single 7-seg display in the DE1-SoC Computer, for the hex digits 0 - F
*/
unsigned char seven_seg_decode_table[] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07,
0x7F, 0x67, 0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71};
unsigned char hex_segs[] = {0, 0, 0, 0, 0, 0, 0, 0};
unsigned int shift_buffer, nibble;
unsigned char code;
int i;
shift_buffer = (b1 << 16) | (b2 << 8) | b3;
for (i = 0; i < 6; ++i) {
nibble = shift_buffer & 0x0000000F; // character is in rightmost nibble
code = seven_seg_decode_table[nibble];
hex_segs[i] = code;
shift_buffer = shift_buffer >> 4;
}
/* drive the hex displays */
*(HEX3_HEX0_ptr) = *(int *)(hex_segs);
*(HEX5_HEX4_ptr) = *(int *)(hex_segs + 4);
}


void the_exception (void) __attribute__ ((section(".exceptions")));

void the_exception (void){
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

    asm("eret");
}


int mouse_CNT;
int mouse_Data_Queue[3];
int G_mouse_X, G_mouse_Y;
int mouse_Done;
int min(int a, int b){
    if(a <= b) return a;
    else return b;
}

int max(int a, int b){
    if(a >= b) return a;
    else return b;
}

char byte1 = 0, byte2 = 0, byte3 = 0;
void handle_mouse(){
    int data = MOUSE->Data;
    //note when the fifo is not empty the interrupt will bring us back anyways
    //note they come in groups of 3.
	byte1 = byte2;
	byte2 = byte3;
	byte3 = data & 0xFF;
	HEX_PS2(byte1, byte2, byte3);

	if ((byte2 == (char)0xAA) && (byte3 == (char)0x00)){
		MOUSE->Data = 0xF4;
		printf("SETTED UP\n");
		mouse_CNT=1;
	}
	mouse_CNT++;
	
    if(mouse_CNT==3){
        mouse_CNT = 0;
		
        G_mouse_X += byte2/2;
		G_mouse_Y -= byte3/2;

        G_mouse_X = min(G_mouse_X, 315);
        G_mouse_Y = min(G_mouse_Y, 235);
        G_mouse_Y = max(G_mouse_Y, 0);
        G_mouse_X = max(G_mouse_X, 0);
		printf("%d %d %d\n", byte1, byte2, byte3);
		printf("%d %d\n", G_mouse_X, G_mouse_Y);
    } 
	
}
//maybe implement memory leak controller??????
void interrupt_handler(void){
	int ipending = __builtin_rdctl(4);
	if((ipending & 1<<7))
		handle_mouse();
}

