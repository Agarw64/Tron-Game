#include <unistd.h>
#include <stdio.h>
#include <stdint.h>

#include "address_map_niosv.h"

//POINTERS for readingg and reporting to hardware
typedef uint16_t pixel_t;
volatile pixel_t *pVGA = (pixel_t *)FPGA_PIXEL_BUF_BASE;
volatile uint32_t *pKEY = (uint32_t*)KEY_BASE;
volatile uint32_t *pSW = (uint32_t*)SW_BASE;
volatile uint32_t *disp = (uint32_t*)(HEX3_HEX0_BASE);

const pixel_t blk = 0x0000;
const pixel_t wht = 0xffff;
const pixel_t red = 0xf800;
const pixel_t grn = 0x07e0;
const pixel_t blu = 0x001f;

const uint8_t seven_seg_digits[10] = {
    0x3F, 
	0x06, 
	0x5B, 
	0x4F, 
	0x66, 
	0x6D, 
	0x7D, 
	0x07, 
	0x7F, 
	0x6F
};

//structs and global vars 
typedef struct {
    int x, y;           
    int dx, dy;           
    pixel_t color;
    int alive; 
} Player;


//made thse volatile so ISR or main reads value from memory 
volatile Player p1;
volatile Player p2;
volatile int counter1 = 0; 
volatile int counter2 = 0; 
volatile int game_running = 0; 
volatile int pending_turn = 0; // 0=None, 1=Right, 2=Left

// GRAPHICS and printing pixels

void drawPixel(int y, int x, pixel_t colour) {
    if (x >= 0 && x < MAX_X && y >= 0 && y < MAX_Y)
        *(pVGA + (y<<YSHIFT) + x ) = colour;
}

void rect(int y1, int y2, int x1, int x2, pixel_t c) {
    for(int y=y1; y<y2; y++)
        for(int x=x1; x<x2; x++)
            drawPixel(y, x, c);
}

void drawBorder(void) {
    int BORDER_WIDTH = 5;
    rect(0, MAX_Y, 0, MAX_X, blk);
    rect(0, BORDER_WIDTH, 0, MAX_X, wht); 
    rect(0, MAX_Y, MAX_X - BORDER_WIDTH, MAX_X, wht); 
    rect(MAX_Y - BORDER_WIDTH, MAX_Y, 0, MAX_X, wht); 
    rect(0, MAX_Y, 0, BORDER_WIDTH, wht); 
}

void drawObstacles(void) {
    int obstacleSize = 5;
    int x_positions[] = {50, 150, 250};  
    int y_positions[] = {40, 120, 200};
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            rect(y_positions[j], y_positions[j]+obstacleSize, x_positions[i], x_positions[i]+obstacleSize, wht);
        }
    }
}

void display(int c1, int c2) {
    uint32_t pattern = (seven_seg_digits[c1]) | (seven_seg_digits[c2] << 8);
    *disp = pattern;
}

// check boudaries and obstacles (for some rzn wasn't working with just "ahead" variable in robot function
int is_safe(int x, int y) {
    //Don't read memory if outside bounds
    if (x <= 0 || x >= MAX_X - 1 || y <= 0 || y >= MAX_Y - 1) {
		return 0; }
    if (*(pVGA + (y << YSHIFT) + x) != blk) {
		return 0; }
    return 1;
}

// Robot movement

void updateRobotDirection(volatile Player* r) {
    int nx = r->x + r->dx;
    int ny = r->y + r->dy;

    if (!is_safe(nx, ny)) {
        // Try Left
        int ldx = -r->dy; int ldy = r->dx;
        if (is_safe(r->x + ldx, r->y + ldy)) {
            r->dx = ldx; r->dy = ldy;
            return;
        }
        // Try Right
        int rdx = r->dy; int rdy = -r->dx;
        if (is_safe(r->x + rdx, r->y + rdy)) {
            r->dx = rdx; r->dy = rdy;
            return;
        }
    }
}

void movePlayer(volatile Player* p) {
    if (!p->alive) {
		return; }
    p->x += p->dx;
    p->y += p->dy;
    drawPixel(p->y, p->x, p->color);
}

//INTERRUPTS

//reads button press interrupt from key, and then deletes it 
void key_ISR() {
	//pKEY is edge capture reguster
    int press = *(pKEY + 3); //Reads interrupt
    *(pKEY + 3) = press; // Clears Interrupt

//implementing double press cancel feature 
    if (press & 0x1) { // KEY0 (Right)
        if (pending_turn == 1) {
			pending_turn = 0; } //cancel signal by turning pending turn off (if it was on/1 before)
        else pending_turn = 1;
    } 
    
    if (press & 0x2) { // KEY1 (Left)
        if (pending_turn == 2) { //KEY1 (Left) 
			pending_turn = 0; } //cancels signal if pending turn was alr on 
        else pending_turn = 2;
    }
}

void mtimer_ISR() {
	//sets up ointers to cpu's clock and schedules 
    volatile uint64_t *mtime_ptr = (uint64_t *)MTIMER_BASE; //running stopwatch
	// sets the alarm and when mtime catches this number the interrupt is fired(thats why we make volatile so accessible outside)
    volatile uint64_t *mtimecmp_ptr = (uint64_t *)(MTIMER_BASE + 8);  

    // SPEED CONTROL
    int switches = *pSW; //reads binary value from switch
    int delay_ticks = 3000000; // base speed

    // Check switches 9 down to 0 
    for(int i=9; i>=0; i--) {
        if (switches & (1<<i)) {
            // Subtract time based on gear. Higher gear = Less delay = Faster
            delay_ticks = 3000000 - (i * 300000); 
            break;
        }
    }
    if (delay_ticks < 100000) 
		delay_ticks = 100000; //just sticks to base speed

    *mtimecmp_ptr = *mtime_ptr + delay_ticks; // just set next alarm to current time + wait/delay 

    // GAME LOGIC 
	//Put in timer ISR because game will always operate at the speed decided by switches (by delay ticks so essentially the time of cmp_ptr)
    if (game_running) {
        
        // Human Turn
        if (pending_turn == 1) { // Right
            int old_dx = p1.dx; p1.dx = -p1.dy; p1.dy = old_dx;
        } else if (pending_turn == 2) { // Left
            int old_dx = p1.dx; p1.dx = p1.dy; p1.dy = -old_dx;
        }
        pending_turn = 0;

        updateRobotDirection(&p2);

        int p1_crash = !is_safe(p1.x + p1.dx, p1.y + p1.dy);
        int p2_crash = !is_safe(p2.x + p2.dx, p2.y + p2.dy);

        if (p1_crash || p2_crash) {
            p1.alive = !p1_crash;
            p2.alive = !p2_crash;
            game_running = 0; 
        } else {
            movePlayer(&p1);
            movePlayer(&p2);
        }
    }
}

//SETUP
//sets destination address if interrupt happens
void setup_trap_handler(void *handler_addr) {
    asm volatile("csrw mtvec, %0" :: "r"(handler_addr));
}

//turns global interupt switch on (gives permission), syaing all interrupts are good to work 
void setup_cpu_irqs() {
    // Bit 7 (Timer) = 0x80
    // Bit 18 (Key)  = 0x40000
    asm volatile("csrs mie, %0" :: "r"(0x40080));
    asm volatile("csrsi mstatus, 0x8"); 
}

//takes interrupt call and calls correct ISR to handle job
__attribute__((interrupt("machine"))) 
void handler() {
    int mcause_value;
    asm volatile("csrr %0, mcause" : "=r"(mcause_value));
    int is_interrupt = mcause_value & 0x80000000;
    int code = mcause_value & 0x7FFFFFFF;
    if (is_interrupt) {
        if (code == 7) mtimer_ISR();
        else if (code >= 16) key_ISR();
    }
}

//MAIN
//initializes positions of players and new round
void initialize_round() {
    drawBorder();
    drawObstacles();
    
    p1.x = MAX_X/3; p1.y = MAX_Y/2;
    p1.dx = 1; p1.dy = 0; p1.color = red; p1.alive = 1;
    drawPixel(p1.y, p1.x, p1.color);

    p2.x = 2*MAX_X/3; p2.y = MAX_Y/2;
    p2.dx = -1; p2.dy = 0; p2.color = blu; p2.alive = 1;
    drawPixel(p2.y, p2.x, p2.color);

    pending_turn = 0;
    game_running = 1;
}

int main() {
    printf("start\n");
    *(pKEY + 3) = 0xF; //edge capture register
    *(pKEY + 2) = 0x3; //interruptmask register, controls which buttons can interrupt cpu 
    
	//creating pointers for timer (can hoenstly delete because volatile and defined above)
    volatile uint64_t *mtime_ptr = (uint64_t *)MTIMER_BASE;
    volatile uint64_t *mtimecmp_ptr = (uint64_t *)(MTIMER_BASE + 8);
    *mtimecmp_ptr = *mtime_ptr + 5000000; //sets first delay/clock for initial run 

    setup_trap_handler((void *)&handler); //if interrupt happens, run handler
    setup_cpu_irqs();//turn on master switch for interrupts in CPU

    display(counter1, counter2);

    while (counter1 < 9 && counter2 < 9) {
        initialize_round();
        while (game_running == 1); 

        if (!p1.alive && !p2.alive) { } 
        else if (!p1.alive) counter2++; 
        else if (!p2.alive) counter1++;
        
        display(counter1, counter2);
        for(volatile int i=0; i<3000000; i++); 
    }

    pixel_t c = (counter1 >= 9) ? red : blu;
    rect(0, MAX_Y, 0, MAX_X, c);
    printf("done\n");
    return 0;
}