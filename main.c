typedef unsigned char u8;
typedef signed char s8;
typedef unsigned short u16;
typedef signed short s16;
typedef unsigned long int u32;

#include "graphics.h"

// ship can move up or down every 4 frames
// enemies animate every 5 frames
// bullets move 8 pixels per frame
// one enemy per row, one bullet per row
// tanks get pushed back if shot from front
// blinker turns into bouncer if not collected
// collecting blinker spawns hollow on opposite side
// hollow enemy pass thru bullets
// bouncer bounces back and forth until shot
// in row blinker is active, ship can move left or right 
//   but not shoot until it turns into bouncer
// only one blinker active at a time
// multiple bouncers are permitted
// ship position is centered after losing a ship
// levels 1..5  enemy_limit=level+2
// levels 6..9  enemy_limit=level-2  faster

// playfield
// row 0: lines 3,4    
// row 1: lines 6,7    
// row 2: lines 9,10   
// row 3: lines 12,13  
// row 4: lines 15,16  
// row 5: lines 18,19  
// row 6: lines 21,22  

// sprite list:
// 0..13: 2 sprite bullets for each row
// 14: ship
// 15..21: enemies

// shifted palette chars
// 80..8f upper ship left and right
// 90..9f lower ship left and right
// a0..af upper explode left and right
// b0..bf lower explode left and right
// c0..cf upper enemy left and right
// d0..df lower enemy left and right
// e0..ef upper arrow left and right
// f0..ff lower arrow left and right


/*
Define macros to access VDP registers

These macros can be used as if they were variables. 
This makes things easier to read and we don't have a lot of
typecasts troughout the code.

  Read data register located at address >8800   
  Write data register located at address >8C00   
  Address register located at address >8C02
*/  
#define VDP_READ_DATA_REG   (*(volatile unsigned char*)0x8800)
//#define VDP_WRITE_DATA_REG  (*(volatile unsigned char*)0x8C00)
//#define VDP_ADDRESS_REG     (*(volatile unsigned char*)0x8C02)
#define VDP_STATUS_REG      (*(volatile unsigned char*)0x8802)

#define SND_REG      (*(volatile unsigned char*)0x8400)

/* Location of the screen in VDP memory.
   This is the based on the VDP configuration set by the TI firmware  */
#define VDP_SCREEN_ADDRESS  0

/* Location of things in VDP memory. Set up for bitmap mode */
#define CLRTAB 0x0000  // Char color table
#define CLRTAB2 0x0800 // Char color table
#define CLRTAB3 0x1000 // Char color table
#define SCRTAB 0x1800  // Screen Table
#define SPRTAB 0x1F80  // Sprite list table
#define PATTAB 0x2000  // Char pattern table
#define PATTAB2 0x2800 // Char pattern table
#define PATTAB3 0x3000 // Char pattern table
#define SPRPAT 0x3800  // Sprite patterns

#define SPRTAB_BULLETS (SPRTAB)
#define SPRTAB_SHIP (SPRTAB+14*4)
#define SPRTAB_ENEMIES (SPRTAB+15*4)
#define SPRTAB_END (SPRTAB+22*4)


register volatile u8 *vdp_address_reg_addr asm("r14");
register volatile u8 *vdp_write_data_reg_addr asm("r15");

#define VDP_ADDRESS_REG     (*vdp_address_reg_addr)
#define VDP_WRITE_DATA_REG  (*vdp_write_data_reg_addr)


static inline void set_vdp_write_address(u16 addr)
{
	addr += 0x4000;
	asm volatile (
		//"ori %0, >4000  \n\t"
		"swpb %0  \n\t"
		"movb %0,*r14  \n\t"
		"swpb %0  \n\t"
		"movb %0,*r14  \n\t"
		: "=r"(addr)
		: "0"(addr)
	);
}


static void vdp_memset(u16 addr, u8 ch, u16 count)
{
	set_vdp_write_address(addr);
#if 0
	do {
		VDP_WRITE_DATA_REG = ch;
	} while (--count);
#else
	asm volatile (
		"1:  \n\t"
		"movb %2,*r15  \n\t"
		"dec %0  \n\t"
		"jne 1b  \n\t"
		: "=r"(count)
		: "0"(count),"r"(ch)
	);
#endif
}

static void vdp_write(u16 addr, const u8 *src, u16 count)
{
	set_vdp_write_address(addr);
#if 0
	do {
		VDP_WRITE_DATA_REG = *src++;
	} while (--count);
#else
	asm volatile (
		"1:\n\t"
		"movb *%0+,*r15  \n\t"
		"dec %1\n\t"
		"jne 1b\n\t"
		: "=r"(src),"=r"(count)
		: "0"(src),"1"(count)
	);
#endif
}

static void vdp_write8(u16 addr, const u8 *src, u16 count)
{
#if 0
	VDP_ADDRESS_REG = addr & 0xff;
	VDP_ADDRESS_REG = (addr | 0x4000) >> 8;
	do {
		VDP_WRITE_DATA_REG = *src++;
		VDP_WRITE_DATA_REG = *src++;
		VDP_WRITE_DATA_REG = *src++;
		VDP_WRITE_DATA_REG = *src++;
		VDP_WRITE_DATA_REG = *src++;
		VDP_WRITE_DATA_REG = *src++;
		VDP_WRITE_DATA_REG = *src++;
		VDP_WRITE_DATA_REG = *src++;
	} while (--count);
#else
	set_vdp_write_address(addr);
	asm volatile (
		"1:\n\t"
		"movb *%0+,*r15  \n\t"
		"movb *%0+,*r15  \n\t"
		"movb *%0+,*r15  \n\t"
		"movb *%0+,*r15  \n\t"
		"movb *%0+,*r15  \n\t"
		"movb *%0+,*r15  \n\t"
		"movb *%0+,*r15  \n\t"
		"movb *%0+,*r15  \n\t"
		"dec %1\n\t"
		"jne 1b\n\t"
		: "=r"(src),"=r"(count)
		: "0"(src),"1"(count)
	);

#endif

}

#if 0
static void vdp_read(u16 addr, u8 *dest, u16 count)
{
	VDP_ADDRESS_REG = addr & 0xff;
	VDP_ADDRESS_REG = (addr | 0x4000) >> 8;
	asm("nop");
	do {
		*dest++ = VDP_READ_DATA_REG;
	} while (--count);
}
#endif









static void vsync(void)
{
	VDP_STATUS_REG; // clear interrupt so we catch the edge
	asm volatile (
		"	li r12,4\n"
	    "	tb 0\n"
	    "	jeq -4\n"
			::
			:"r12");
	VDP_STATUS_REG; // clear interrupt flag manually since we polled CRU
}



// linker seems to want this even though unused
void memcpy(void *dst, void *src, unsigned int count)
{
	char *a = dst, *b = src;
	while (count--)
		*a++ = *b++;
}
void memset(void *dst, char byte, unsigned int count)
{
	char *a = dst;
	while (count--)
		*a++ = byte;
}


static const u8 vdpini[] = {
	0x02,		// VDP Register 0: 02 (Bitmap Mode)
	0xE2,		// VDP Register 1: 16x16 Sprites
	SCRTAB/0x400,	// VDP Register 2: Screen Image Table
	CLRTAB/0x40+0x7F, // VDP Register 3: Color Table
	PATTAB/0x800+3,	// VDP Register 4: Pattern Table
	SPRTAB/0x80,	// VDP Register 5: Sprite List Table
	SPRPAT/0x800,	// VDP Register 6: Sprite Pattern Table
	0xF1,		// VDP Register 7: White on Black
};

static void init_vdp(void)
{
	const u8 *src = vdpini;
	u16 i;

	// initialize global register variables
	vdp_address_reg_addr = (u8*)0x8C02;
 	vdp_write_data_reg_addr = (u8*)0x8C00;

	// initialize VDP registers from table
	for (i = 0x8000; i < 0x8800; i += 0x100) {
		VDP_ADDRESS_REG = *src++;
		VDP_ADDRESS_REG = i >> 8;
	}
}

static const u8 wall[] = { 0x00, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0x00, 0x00};
static const u8 wall_pal[] = {
	0xbb,0xab,0xaa,0xa9,0x99,0x98,0x88,0x9d,0xdd,0xd5,0x55,0x54,
	0x44,0x4c,0x52,0x72,0x73,0x33,0x32,0x3e,0xee,0xeb,
};


static const u8 ship_pal[] = {
	0x01,0x61,0x61,0x61,0x51,0x41,0xf1,0x61,
	0xf1,0x41,0x51,0x61,0x61,0x61,0x01,0x01,
};

static const u8 explode_pal[] = {
	0x11,0xb1,0xa1,0x91,0x61,0xa1,0xa1,0xf1,
	0xb1,0x91,0xa1,0x61,0xa1,0xb1,0x01,0x01,
};

static const u8 enemy_pal[] = {
	0x01,0xa1,0x91,0x61,0xd1,0xc1,0xa1,0x61,
	0xa1,0xc1,0x51,0xd1,0x61,0x91,0xa1,0x01,
};

static const u8 arrow_pal[] = {
	0xf1,0xf1,0x51,0x51,0x51,0x51,0xf1,0xf1,
	0x51,0x51,0x51,0x51,0x51,0xf1,0xf1,0xf1,
};


static u16 random(void)
{
	static u16 seed = 0xaaaa;
	static const u16 random_mask = 0xb400;

	asm volatile (
		"srl %0,1  \n\t"
		"jnc 1f    \n\t"
		"xor %2,%0 \n\t"
		"1:        \n\t"
		: "=r"(seed)
		: "0"(seed),"m"(random_mask)
	);
	return seed;
}


static u16 read_joystick(void)
{
	u16 result;
	asm volatile (
		"li %0,>600 \n\t"
		"li r12,>24 \n\t"
		"ldcr %0,3 \n\t"
		"li r12,>06 \n\t"
	    "stcr %0,8 \n\t"
			:"=r"(result)
			:
			:"r12");
	return result;
}
#define JOYSTICK_FIRE 0x0100
#define JOYSTICK_UP 0x1000
#define JOYSTICK_DOWN 0x0800
#define JOYSTICK_LEFT 0x0200
#define JOYSTICK_RIGHT 0x0400

static const u8 sh[] = {34, 36, 38, 35, 37, 39};

static const u8 ex[] = {32, 0x80, 0x82, 0x84, 32, 0x81, 0x83, 0x85, 32};

static void shifted_bg(u16 i, u8 base, const u8 *pal)
{
	for (u16 j = 0; j < 8; j++) {
		vdp_memset(PATTAB+i+(base+j)*8, 0xff >> j, 8);
		vdp_memset(PATTAB+i+(base+8+j)*8, 0xff00 >> j, 8);
		vdp_memset(PATTAB+i+(base+16+j)*8, 0xff >> j, 8);
		vdp_memset(PATTAB+i+(base+24+j)*8, 0xff00 >> j, 8);

		vdp_write(CLRTAB+i+(base+j)*8, pal, 8);
		vdp_write(CLRTAB+i+(base+8+j)*8, pal, 8);
		vdp_write(CLRTAB+i+(base+16+j)*8, pal+8, 8);
		vdp_write(CLRTAB+i+(base+24+j)*8, pal+8, 8);
	}
}

static void setup(void)
{
	// clear the screen
	vdp_memset(SCRTAB, ' ', 32 * 24);

	// initialize each VDP bank chars
	for (u16 i = 0; i < 0x1800; i += 0x800) {
		// numbers
		vdp_write(PATTAB+i+'0'*8, number_ch, 8*10);
		vdp_memset(CLRTAB+i+'0'*8, 0xf1, 8*10);

		// wall color and pattern
		vdp_memset(PATTAB+i+' '*8, 0x00, 8);
		vdp_memset(CLRTAB+i+' '*8, 0xf1, 8);
		vdp_write(PATTAB+i+'!'*8, wall, 8);
		vdp_memset(CLRTAB+i+'!'*8, 0xc1, 8);

		// draw shifted backgrounds
		shifted_bg(i, 0x80, ship_pal);
		shifted_bg(i, 0xa0, explode_pal);
		shifted_bg(i, 0xc0, enemy_pal);
		shifted_bg(i, 0xe0, arrow_pal);
	}	
	
	// load sprite patterns
	vdp_write8(SPRPAT, sprite_pat, sizeof(sprite_pat)/8);

	// get ship sprite into char[0..3]
	set_vdp_write_address(PATTAB);
	for (u16 i = 0 ; i < 32; i++) {
		VDP_WRITE_DATA_REG = ~sprite_pat[2*32+i];
	}
	vdp_write8(CLRTAB, ship_pal, 1);
	vdp_write8(CLRTAB+8, ship_pal+8, 1);
	vdp_write8(CLRTAB+16, ship_pal, 1);
	vdp_write8(CLRTAB+24, ship_pal+8, 1);
}



/* Global variables, data segment is  in fast RAM, 
 * so faster than accessing stack-relative.
 */

enum {
	IDLE,
	FLUTTER,
	FLIPPER,
	HOTDOG,
	DELTA,
	PHI,
	PRIZE,
	ARROW,
	SAUCER,
	
	BALL,
	TANK,
	EXPLODE,

} enemy_types;

static const struct {
	u8 base, mask, score;
} spridx[] = {
	// sprite index, mask bits (4=anim 8=dir)
	{0, 0, 0}, // IDLE
	{40, 4, 2}, // FLUTTER  20
	{48, 12, 4}, // FLIPPER  40
	{108, 0, 1}, // HOTDOG  10
	{80, 8, 2}, // DELTA  20
	{32, 4, 6}, // PHI  60
	{96, 4, 80}, // PRIZE  800 
	{84, 8, 10}, // ARROW  100
	{104, 0, 0}, // SAUCER
	{96, 0, 10}, // BALL  100
	{64, 12, 5}, // TANK  50
	{16, 12, 0}, // EXPLODE
};

// enemies have 
//   speed and direction: signed 4.4 fixed point range -7.F to +7.F
//   x position unsigned 8.4 fixed point 0.0 to 240.0
//   y position is implied by enemy number
//   type is 0 to 15, store in low bits of x position
static struct {
	u16 x; // horizontal pos in pixels, 8.8 fixed point
	s8 v; // velocity in pixels (direction and speed) 4.4 fixed point
	u8 type; // enemy type 0..12
} enemy[7] = {
#if 0
	{0x0100, 20, TANK},
	{0xf000, -102, BALL},
	{0x0100, 10, 3},
	{0x0100, 1, 4},
	{0xf100, -1, 5},
	{0x0100, 5, 6},
	{0xf000, -20, ARROW},
#endif
};
	// up to seven bullets, direction determined by position relative to center
static u16 bullet[7]; // unsigned 8.4 fixed point, moves by 6.4 pixels/frame
static struct {
	u8 dir; // right=0 or left=1
	u8 move;
	u8 y; // current row 0 to 6
	u8 x; // pixel coordinate
} ship = {
	1, // dir
	0, // move
	0, // y
	120, // x
};
u16 
	count10 = 10,  
	ships = 99,
	ecount = 0,
	level = 1,
	score = 0,
	hiscore = 0,
    countdown = 0, // saucer/prize countdown
	wpat = 0, // wall pattern index
	js = 0xff00;  // joystick bits (inverted)
u8 	demo = 1,
	wcount = 0;  // wall counter


static const u16 *sound = (u16*)0;
static const u8 *noise = (u8*)0;

#define STEP6(t1,v1,t2,v2) \
	(((((u32)t1*6+(u32)t2*0)/6)&0xfff0) | (v1*6+v2*0)/6), \
	(((((u32)t1*5+(u32)t2*1)/6)&0xfff0) | (v1*5+v2*1)/6), \
	(((((u32)t1*4+(u32)t2*2)/6)&0xfff0) | (v1*4+v2*2)/6), \
	(((((u32)t1*3+(u32)t2*3)/6)&0xfff0) | (v1*3+v2*3)/6), \
	(((((u32)t1*2+(u32)t2*4)/6)&0xfff0) | (v1*2+v2*4)/6), \
	(((((u32)t1*1+(u32)t2*5)/6)&0xfff0) | (v1*1+v2*5)/6), \
	(((((u32)t1*0+(u32)t2*6)/6)&0xfff0) | (v1*0+v2*6)/6)

#define STEP9(t1,v1,t2,v2) \
	(((((u32)t1*9+(u32)t2*0)/9)&0xfff0) | (v1*9+v2*0)/9), \
	(((((u32)t1*8+(u32)t2*1)/9)&0xfff0) | (v1*8+v2*1)/9), \
	(((((u32)t1*7+(u32)t2*2)/9)&0xfff0) | (v1*7+v2*2)/9), \
	(((((u32)t1*6+(u32)t2*3)/9)&0xfff0) | (v1*6+v2*3)/9), \
	(((((u32)t1*5+(u32)t2*4)/9)&0xfff0) | (v1*5+v2*4)/9), \
	(((((u32)t1*4+(u32)t2*5)/9)&0xfff0) | (v1*4+v2*5)/9), \
	(((((u32)t1*3+(u32)t2*6)/9)&0xfff0) | (v1*3+v2*6)/9), \
	(((((u32)t1*2+(u32)t2*7)/9)&0xfff0) | (v1*2+v2*7)/9), \
	(((((u32)t1*1+(u32)t2*8)/9)&0xfff0) | (v1*1+v2*8)/9), \
	(((((u32)t1*0+(u32)t2*9)/9)&0xfff0) | (v1*0+v2*9)/9)

static const u16 sound_shoot[] = {0x2006,0x2205,0x2404,STEP9(0x2600,3,0x3f00,5),0x3f0f};
static const u16 sound_move[] = {STEP6(0x3f00,2,0x1000,5),0x100f};
static const u16 sound_ball[] = {0x0823,0x0812,0x0803,0x080f};
static const u16 sound_saucer[] = {STEP9(0x1000,0,0x0100,4),0x010f};
static const u8 noise_spawn[] = {0xe6, STEP9(0xf0,10,0xf0,0), 0xff};

static void play_sounds(void)
{
	if (sound) {
		u16 s = *sound++;
		// 10 bits frequency divider
		// 4 bits attenuator
		SND_REG = 0xc0 | ((s >> 4) & 0xf);
		SND_REG = (s >> 8);
		SND_REG = 0xd0 | (s & 0xf);
		if ((s & 0xf) == 0xf)
			sound = 0;
	}
	if (noise) {
		u8 n = *noise++;
		SND_REG = n;
		if (n == 0xff)
			noise = 0;
	}
}







static void rainbow(void)
{
	const u8 row[32] = "AAaaAAAaaaAAaAaaaaAaAAaaaAAAaaAA";

	vdp_memset(SPRTAB, 0xd0, 1); // sprite list terminator

	set_vdp_write_address(SCRTAB+32*0);
	for (u16 j = 0; j < 24; j++) {
		for (u16 i = 0; i < 32; i++) {
			VDP_WRITE_DATA_REG = row[i]+(j&7);
		}
	}
	//for (u16 j = 20; j < 24; j++) {
	//	vdp_memset(SCRTAB + 32*j, ' ', 32);
	//}

	vdp_memset(SCRTAB + 32*10 + 13, 0, 6);
	vdp_memset(SCRTAB + 32*11 + 13, 0, 6);
	vdp_memset(SCRTAB + 32*11 + 16, level+'0', 1);
	vdp_memset(SCRTAB + 32*12 + 13, 0, 6);

#define OFF1 88
#define OFF2 0

	SND_REG = 0xd2;
	SND_REG = 0xff;
	for (u16 i = 0; i < 256; i++) {
		u8 off = 0;
		u16 s = 0x400 - ((i & 0xf0)*3 + (i & 0xf)*16);
		SND_REG = 0xc0 | (s & 0xf);
		SND_REG = s >> 4;
		for (u16 j = 0; j < 0x1800; j += 0x800) {
			vdp_write8(PATTAB+j + 'A'*8, rainbow_ch+((OFF2-i+off) & 0xff), 8);
			vdp_write8(CLRTAB+j + 'A'*8, rainbow_co+((OFF2-i+off) & 0xff), 8);
			vdp_write8(PATTAB+j + 'a'*8, rainbow_ch+((OFF1+i+off) & 0xff), 8);
			vdp_write8(CLRTAB+j + 'a'*8, rainbow_co+((OFF1+i+off) & 0xff), 8);
			off += 64;
		}
#if 0
		vdp_write8(PATTAB2 + 'A'*8, rainbow_ch+((OFF2-i+64) & 0xff), 8);
		vdp_write8(CLRTAB2 + 'A'*8, rainbow_co+((OFF2-i+64) & 0xff), 8);
		vdp_write8(PATTAB2 + 'a'*8, rainbow_ch+((OFF1+i+64) & 0xff), 8);
		vdp_write8(CLRTAB2 + 'a'*8, rainbow_co+((OFF1+i+64) & 0xff), 8);

		vdp_write8(PATTAB3 + 'A'*8, rainbow_ch+((OFF2-i+64+64) & 0xff), 8);
		vdp_write8(CLRTAB3 + 'A'*8, rainbow_co+((OFF2-i+64+64) & 0xff), 8);
		vdp_write8(PATTAB3 + 'a'*8, rainbow_ch+((OFF1+i+64+64) & 0xff), 8);
		vdp_write8(CLRTAB3 + 'a'*8, rainbow_co+((OFF1+i+64+64) & 0xff), 8);
#endif
		vsync();
	}
	SND_REG = 0xdf;
	SND_REG = 0xff;	
}

static void draw_score(void)
{
	u16 places[] = {1000,100,10,1};
	u8 digit;
	u16 i,j = score;
	set_vdp_write_address(SCRTAB + 18);
	for (i = 0; i < 4; i++) {
		digit = '0';
		while (j >= places[i]) {
			j -= places[i];
			digit++;
		}
		VDP_WRITE_DATA_REG = digit;
	}
	VDP_WRITE_DATA_REG = '0';
}

static void draw_ships(void)
{
	u16 i, addr = SCRTAB;
	u16 a = 0x0002, b = 0x0103;
	for (i = 0; i < 6; i++) {
		if (i == ships) {
			a = 0x2020;
			b = 0x2020;
		}
		set_vdp_write_address(addr);
		VDP_WRITE_DATA_REG = a >> 8;
		VDP_WRITE_DATA_REG = a & 0xff;
		VDP_WRITE_DATA_REG = ' ';
		set_vdp_write_address(addr+32);
		VDP_WRITE_DATA_REG = b >> 8;
		VDP_WRITE_DATA_REG = b & 0xff;
		VDP_WRITE_DATA_REG = ' ';
		addr += 3;
	}
}

static void spawn_enemy(void)
{
	u16 r = random();
	u8 row, type;
	do {
		// spawn a new enemy
		r = random();
		row = (r >> 13) & 7;
		type = ((r >> 8) & 7) + 1;
		if (type == SAUCER)
			row = ship.y;
	} while (
		row == 7 || 
		enemy[row].type != IDLE || 
		((type == SAUCER || type == PRIZE) && countdown != 0)
	);

	enemy[row].type = type;
	if (type == ARROW) {
		noise = noise_spawn;
		enemy[row].v = 20;
	} else {
		enemy[row].v = (r & 15)+5;
	}
	if (r & 0x1000) {
		enemy[row].x = 0x0100;
	} else {
		enemy[row].x = 0xef00;
		enemy[row].v = -enemy[row].v;
	}
	if (type == PRIZE || type == SAUCER) {
		enemy[row].v = 0;
		countdown = 160;
	}
	if (level >= 6) {
		enemy[row].v *= 2;
	}
}

static void respawn_enemies(void)
{
	memset(enemy, 0, sizeof(enemy));
	u16 n = level < 6 ? level + 2 : level - 2;
	countdown = 0;
	do {
		spawn_enemy();
	} while (--n);
}


static void draw_field(void)
{

	vdp_memset(SCRTAB, ' ', 32*2);

	for (u16 y = 2; y < 24; y++) {
		switch (y) {
		case 2: case 23:
			vdp_memset(SCRTAB + y*32, '!', 32);
			break;
		case 5: case 8: case 11: case 14: case 17: case 20:
			vdp_memset(SCRTAB + y*32, '!', 14);
			vdp_memset(SCRTAB + y*32+14, ' ', 4);
			vdp_memset(SCRTAB + y*32+18, '!', 14);
			break;
		default:
			vdp_memset(SCRTAB + y*32, ' ', 32);
			break;
		}
	}

	// setup sprite list
	// bullets for each row
	for (u16 i = 0; i < 7; i++) {
		u8 sp[] = {
			i * 24 + 23, 128, 0, 0,
			i * 24 + 23, 128, 4, 0};
		vdp_write(SPRTAB + i*8, sp, sizeof(sp));
	}
	// ship
	{
		u8 sp[] = {
			24 + 23, 128, 0, 0};
		vdp_write(SPRTAB + 14*4, sp, sizeof(sp));
	}
	// enemies for each row
	for (u16 i = 0; i < 7; i++) {
		u8 sp[] = {
			i * 24 + 23, 128, 12, 0};
		vdp_write(SPRTAB + (i+15)*4, sp, sizeof(sp));
	}

	vdp_memset(SPRTAB + 22*4, 0xd0, 1); // sprite list terminator
	ecount = level * 26 + 47;
}

static void load_level(void)
{
	draw_field();
	memset(bullet, 0, sizeof(bullet));
	draw_score();
	sound = (u16*)0;
	noise = (u8*)0;
	respawn_enemies();
	draw_ships();
	ship.dir = 0;
	ship.x = 120;
	ship.y = 3;
}



static void draw_shifted(u16 addr, u8 old_x, u8 x, u8 bg)
{
	if ((old_x ^ x) & 0xf8) {
		// char position changed
		// erase left or right chars

		if (old_x < x) {
			// moving right
			set_vdp_write_address(addr-1);
			VDP_WRITE_DATA_REG = ' ';
			set_vdp_write_address(addr+32-1);
			VDP_WRITE_DATA_REG = ' ';
		} else {
			// moving left
			set_vdp_write_address(addr+3);
			VDP_WRITE_DATA_REG = ' ';
			set_vdp_write_address(addr+32+3);
			VDP_WRITE_DATA_REG = ' ';
		}
	}

	u8 shift = x & 7;
	set_vdp_write_address(addr);
	VDP_WRITE_DATA_REG = bg+shift;
	VDP_WRITE_DATA_REG = bg;
	VDP_WRITE_DATA_REG = bg+shift+8;
	set_vdp_write_address(addr+32);
	bg += 16;
	VDP_WRITE_DATA_REG = bg+shift;
	VDP_WRITE_DATA_REG = bg;
	VDP_WRITE_DATA_REG = bg+shift+8;
}

static const u16 row_offset[7] = {
	SCRTAB + 32 * 3,
	SCRTAB + 32 * 6,
	SCRTAB + 32 * 9,
	SCRTAB + 32 * 12,
	SCRTAB + 32 * 15,
	SCRTAB + 32 * 18,
	SCRTAB + 32 * 21,
};

static void erase_ship(u8 row, u8 x)
{
	u16 addr = row_offset[row] + (x >> 3);

	set_vdp_write_address(addr);
	VDP_WRITE_DATA_REG = ' ';
	VDP_WRITE_DATA_REG = ' ';
	if (x&7) VDP_WRITE_DATA_REG = ' ';
	set_vdp_write_address(addr+32);
	VDP_WRITE_DATA_REG = ' ';
	VDP_WRITE_DATA_REG = ' ';
	if (x&7) VDP_WRITE_DATA_REG = ' ';
}

static void do_player_ship(void)
{
	u8 old_x = ship.x;

	if (demo) {
		switch(random()&31) {
		case 0: case 1: js = (js & ~JOYSTICK_UP) | JOYSTICK_DOWN; break;
		case 2: case 3: js = (js & ~JOYSTICK_DOWN) | JOYSTICK_UP; break;
		case 4: ship.dir = !ship.dir; break;
		}
		js &= ~JOYSTICK_FIRE;

	} else {
		js = read_joystick();

		if (!(js & JOYSTICK_RIGHT)) {
			ship.dir = 0;
			if (ship.x != 0x78 || enemy[ship.y].type == PRIZE) {
				if (ship.x < 0xe8)
					ship.x += 4;
			}
		} else if (!(js & JOYSTICK_LEFT)) {
			ship.dir = 1;
			if (ship.x != 0x78 || enemy[ship.y].type == PRIZE) {
				if (ship.x > 0x08)
					ship.x -= 4;
			}
		}
		play_sounds();
	}

	if (!demo && ship.move) {
		ship.move--;
	} else if (ship.x <= 0x70 || ship.x >= 0x80) {
		// can't move if not centered
	} else if (!(js & JOYSTICK_DOWN)) {
		if (ship.y < 6) {
			erase_ship(ship.y, old_x);
			ship.y++;
			ship.x = 0x78;
		}
		ship.move = 3;
		sound = sound_move;
	} else if (!(js & JOYSTICK_UP)) {
		if (ship.y > 0) {
			erase_ship(ship.y, old_x);
			ship.y--;
			ship.x = 0x78;
		}
		ship.move = 3;
		sound = sound_move;
	}

	u16 addr = row_offset[ship.y] + (ship.x >> 3);
	draw_shifted(addr, old_x, ship.x, 0x80);

	set_vdp_write_address(SPRTAB_SHIP);
	VDP_WRITE_DATA_REG = ship.y*24+23;
	VDP_WRITE_DATA_REG = ship.x;
	VDP_WRITE_DATA_REG = ship.dir*4+8;
	VDP_WRITE_DATA_REG = 1; // black

	if (!(js & JOYSTICK_FIRE) && bullet[ship.y] == 0 && enemy[ship.y].type != PRIZE && ship.x == 0x78) {
		u16 i = ship.y;
		bullet[i] = 0x7800;
		u8 sp_c[] = {15, 6};
		vdp_write(SPRTAB_BULLETS + i*8 + 3, sp_c, 1);
		vdp_write(SPRTAB_BULLETS + i*8 + 7, sp_c+1, 1);
		u8 sp_x[] = {bullet[i] >> 8};
		vdp_write(SPRTAB_BULLETS + i*8 + 1, sp_x, 1);
		vdp_write(SPRTAB_BULLETS + i*8 + 5, sp_x, 1);
		sound = sound_shoot;
	}
}

static void lose_ship(void)
{
	set_vdp_write_address(SPRTAB_BULLETS+ship.y*8+3);
	VDP_WRITE_DATA_REG = 0; // sprite color transparent
	set_vdp_write_address(SPRTAB_BULLETS+ship.y*8+7);
	VDP_WRITE_DATA_REG = 0; // sprite color transparent


	u16 addr = row_offset[ship.y] + (ship.x >> 3);
	draw_shifted(addr, ship.x, ship.x, 0x80);
	u8 spr_base = ship.dir ? 28 : 33;
	static const u8 anim[] = {0,1,2,3,4,4,4,4,3,2,1,0};
	sound = sound_shoot;
	noise = (u8*)0;
	for (u16 i = 0; i < sizeof(anim); i++) {
		set_vdp_write_address(SPRTAB_SHIP+1);
		VDP_WRITE_DATA_REG = ship.x;
		VDP_WRITE_DATA_REG = (spr_base+anim[i])*4;
		play_sounds();
		for (u16 j = 0; j < 10; j++)
			vsync();
		play_sounds();
		for (u16 j = 0; j < 10; j++)
			vsync();
		if (i == 6) {
			erase_ship(ship.y, ship.x);
			ship.x = 0x78;
			if (ships == 0) {
				// game over

				sound = sound_move;
				for (u16 i = 0; i < 255; i++) {
					vdp_memset(CLRTAB+'0'*8, 0xe0+(i&16), 10*8);
					if ((i & 0xf) == 0)
						play_sounds();
					vsync();
				}
				demo = 1;
				if (score > hiscore)
					hiscore = score;
				score = hiscore;
				draw_score();
				return;
			}
			ships--;
			draw_ships();
			addr = row_offset[ship.y] + (ship.x >> 3);
			draw_shifted(addr, ship.x, ship.x, 0x80);
			noise = noise_spawn;
		}
	}
}

static void clear_enemy(u16 i)
{
	erase_ship(i, enemy[i].x >> 8);
	enemy[i].type = IDLE;
	enemy[i].x = 0;
	set_vdp_write_address(SPRTAB_ENEMIES+(i*4)+3);
	VDP_WRITE_DATA_REG = 0; // sprite color transparent

}


/*==========================================================================
 *                                 main
 *==========================================================================
 * Description: Entry point for program
 *
 * Parameters : None
 *
 * Returns    : Nothing
 */
void main()
{
	init_vdp();
	
	setup();


	draw_field();
	draw_score();
	respawn_enemies();

	for(;;) {


		//VDP_ADDRESS_REG = 0xf7;
		//VDP_ADDRESS_REG = 0x87;


		// cycle wall color every N frames
		if (wcount++ == 0) {
			u8 c;
			if (++wpat >= sizeof(wall_pal)) 
				wpat = 0;
			c = wall_pal[wpat];
			vdp_memset(CLRTAB + '!'*8+1, c, 5);
			vdp_memset(CLRTAB2 + '!'*8+1, c, 5);
			vdp_memset(CLRTAB3 + '!'*8+1, c, 5);
		}
		//VDP_ADDRESS_REG = 0xf4;
		//VDP_ADDRESS_REG = 0x87;

		u8 ship_y = ship.y;
		for (u16 i = 0; i < 7; i++) {
			static u16 old_x, t, bx;
			old_x = enemy[i].x;
			t = enemy[i].type;

			// handle bullet
			bx = bullet[i];
			if (bx) {
				if (bx && bx != 0x7800 && bx + 0x0f00 >= old_x && bx <= old_x + 0x0f00 && 
					t != IDLE && t != SAUCER && t != PRIZE) {
					// bullet hitting enemy
					if (t == TANK && (bx < 0x7800) == (enemy[i].v > 0)) {
						if (old_x > 0x1000 && old_x < 0xE000) {
							erase_ship(i, old_x >> 8);
							enemy[i].x -= enemy[i].v << 7;
						}
					} else if (t != EXPLODE) {
						if (!demo) {
							score += spridx[t].score;
							draw_score();
							if (level < 9 && --ecount == 0) {
								level++;
								if (ships < 5) ships++;
								rainbow();
								load_level();
								break;
							}
						}
						t = EXPLODE;
						enemy[i].type = t;
						enemy[i].v = old_x < 0x7800 ? -32 : 32;
					}
					bx = 0;
				}
				if (bx < 0x666 || bx >= 0xf000) {
					// sprite off
					bx = 0;
					//u8 sp_c[] = {0}; // set sprite color to 0 (invisible)
					//vdp_write(SPRTAB + i*8 + 3, sp_c, 1);
					//vdp_write(SPRTAB + i*8 + 7, sp_c, 1);
					set_vdp_write_address(SPRTAB + i*8 + 3);
					VDP_WRITE_DATA_REG = 0;
					set_vdp_write_address(SPRTAB + i*8 + 7);
					VDP_WRITE_DATA_REG = 0;
				} else {
					if (bx < 0x7800 || (bx == 0x7800 && ship.dir == 1)) {
						bx -= 0x666;
					} else {
						bx += 0x666;
					}
					u8 sp_x[] = {bx >> 8}; // set sprite x
					vdp_write(SPRTAB + i*8 + 1, sp_x, 1);
					vdp_write(SPRTAB + i*8 + 5, sp_x, 1);
				}
				bullet[i] = bx;
			}

			// handle player
			if (ship_y == i) {
				do_player_ship();

				if (demo && !(read_joystick() & JOYSTICK_FIRE)) {
					demo = 0;
					ships = 4;
					score = 0;
					level = 1;

					rainbow();
					load_level();

					break;
				} 

				if (abs((s16)(ship.x - (old_x >> 8))) < 14) {
					erase_ship(i, old_x >> 8);
					if (t == PRIZE) {
						countdown = 0;
						score += 80;
						draw_score();
						t = SAUCER;
						old_x = 0xf000 - old_x;
						enemy[i].type = t;
						enemy[i].x = old_x;
						goto spawn_saucer;
					} else {
						clear_enemy(i);
						spawn_enemy();

						set_vdp_write_address(SPRTAB_ENEMIES+(i*4)+2);
						VDP_WRITE_DATA_REG = 0; // sprite transparent

						if (!demo) lose_ship();
						break;
					}

				}
			}

			// handle enemy
			if (t == IDLE)
				continue;
			if (enemy[i].v == 0) {
				// SAUCER or PRIZE
				if (t == SAUCER) {
					if (--countdown != 0)
						continue;
					if (ship.y == i) {
						spawn_saucer:
						enemy[i].v = old_x < 0x8000 ? 15 : -15;
						if (level >= 6) enemy[i].v *= 2;
						sound = sound_saucer;
					} else {
						enemy[i].type = IDLE;
						spawn_enemy();
						continue;
					}
				} else if (t == PRIZE) {
					if (--countdown == 0) {
						t = BALL;
						enemy[i].type = t;
						enemy[i].v = old_x < 0x8000 ? 102 : -102;
					}
				}
			}

			enemy[i].x += enemy[i].v << 4; // sign-extend and shift
			
		//VDP_ADDRESS_REG = 0xf6;
		//VDP_ADDRESS_REG = 0x87;

			if (enemy[i].x >= 0xf000) {
				// hit edge of screen
				if (t == ARROW || t == TANK || t == BALL) {
					enemy[i].v = -enemy[i].v;
					enemy[i].x = old_x;
					if (t == ARROW) {
						enemy[i].type = TANK;
						enemy[i].v = old_x < 0x7800 ? 25 : -25;
					} else if (t == BALL) {
						sound = sound_ball;
					}
					continue;
				}
				// erase chars
				erase_ship(i, old_x >> 8);
				enemy[i].type = IDLE;
				spawn_enemy();
				continue;
			}

			u8 bg = 0xc0; // default enemy background
			if (t == EXPLODE) {
				bg = 0xa0; // explode
			} else if (t == ARROW || t == SAUCER) {
				bg = 0xe0; // arrow/saucer
			}
			u16 addr = row_offset[i] + (enemy[i].x >> 11);
			draw_shifted(addr, old_x>>8, enemy[i].x>>8, bg);

			// update sprite
			u8 sprite = 0;
			set_vdp_write_address(SPRTAB_ENEMIES+(i*4)+1);
			VDP_WRITE_DATA_REG = enemy[i].x >> 8; // x pos
			if (count10 <= 5)
				sprite += 4;
			if (enemy[i].v < 0)
				sprite += 8;
			VDP_WRITE_DATA_REG = spridx[t].base + (spridx[t].mask & sprite); // sprite index
			VDP_WRITE_DATA_REG = 1; // color (black)

		//VDP_ADDRESS_REG = 0xf4;
		//VDP_ADDRESS_REG = 0x87;
		}


		//VDP_ADDRESS_REG = 0xf1;
		//VDP_ADDRESS_REG = 0x87;


		//VDP_ADDRESS_REG = 0xf1;
		//VDP_ADDRESS_REG = 0x87;

		if (--count10 == 0) count10 = 10;

		vsync();

	}
}
