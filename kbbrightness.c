#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <string.h>
  
#define MAP_SIZE 4096UL
#define MAP_MASK ( MAP_SIZE - 1 )

typedef unsigned int u32;
static int fd = -1;

static int getmem(u32 addr){  
	void *map, *regaddr;
	u32 val;

	if (fd == -1) {
		fd = open("/dev/mem", O_RDWR | O_SYNC);
		if (fd<0) {
			perror("open(\"/dev/mem\")");
			exit(1);
		}
	}

	map = mmap(0, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, addr & ~MAP_MASK );

	if (map == (void*)-1 ) {
		perror("mmap()");
		exit(1);
	}

	regaddr = map + (addr & MAP_MASK);

	val = *(u32*) regaddr;
	munmap(0,MAP_SIZE);

	return val;
}

static void putmem(u32 addr, u32 val){
	void *map, *regaddr;
	static int fd = -1;

	if (fd == -1) {
		fd = open("/dev/mem", O_RDWR | O_SYNC);
		if (fd<0) {
			perror("open(\"/dev/mem\")");
			exit(1);
		}
	}
 
	map = mmap(0,MAP_SIZE,PROT_READ | PROT_WRITE,MAP_SHARED,fd,addr & ~MAP_MASK);
	if (map == (void*)-1 ) {
		perror("mmap()");
		exit(1);
	}
		 
	regaddr = map + (addr & MAP_MASK);

	*(u32*) regaddr = val;
	munmap(0,MAP_SIZE);
}

static void setreg(u32 addr, u32 mask, int shift, u32 val)
{
u32 mem;

mem = getmem(addr);
mem &= ~(mask << shift);
val &= mask;
mem |= val << shift;
putmem(addr, mem);   
}

static int getreg(u32 addr, u32 mask, int shift)
{
  u32 mem;

  mem = getmem(addr);
  return ((mem >> shift) & mask);
}

void setkb(int period, int prescale, int dcycle)
{
  setreg(0x40C00008,0xffffffff,0,period); //PWMPERVAL1 -- PWM2 Period Cycle Length
  setreg(0x40C00000, 0x000003f,0,prescale); // PWMCTL1_PRESCALE
  setreg(0x40C00000, 0x0000001,5,0); // PWMCTL2_SD -- PWM2 Abrupt Shutdown
  setreg(0x40C00004, 0x0000001,10,0); // PWMDUTY2_FDCYCLE 
  setreg(0x40C00004, 0x00003ff,0,dcycle); // PWM2 Duty Cycle
  setreg(0x41300004, 0x0000001,1,1); // CM PWM clock enabled
}

void setlcd(int period, int prescale, int dcycle)
{
  setreg(0x40E00054,0x00000003,22,2); 		//GAFR0L_11 -- GPIO 11 Alternative function
  setreg(0x40B00018,0xffffffff,0,period); 	//PWMPERVAL2 -- PWM2 Period Cycle Length
  setreg(0x40B00010, 0x000003f,0,prescale); 	// PWMCTL2_PRESCALE
  setreg(0x40B00010, 0x0000001,5,0); 		// PWMCTL2_SD -- PWM2 Abrupt Shutdown
  setreg(0x40B00014, 0x0000001,10,0); 		// PWMDUTY2_FDCYCLE 
  setreg(0x40B00014, 0x00003ff,0,dcycle); 	// PWM2 Duty Cycle
  setreg(0x41300004, 0x0000001,1,1); 		// CM PWM clock enabled
}

int main(int argc, char *argv[]) {

	int period=511;
	int prescale=63;
	int dcycle=0;	
	int kbd=1;

	const char *applet = strrchr (argv[0], '/');

	if (applet == NULL)
	  applet = argv[0];
	else
	  applet++;

	if (!strcmp(applet, "lcdval"))
	{
	  dcycle = getreg(0x40B00014, 0x00003ff,0); // PWM2 Duty Cycle
	  printf("%d\n", dcycle);
	  exit(0);
	}
	else if (!strcmp(applet, "kbval"))
	{
	  dcycle = getreg(0x40C00004, 0x00003ff,0); // PWM2 Duty Cycle
	  printf("%d\n", dcycle);
	  exit(0);
	}
#if 0
	/* Combine with applets from screenblanker. */
	else if (!strcmp(applet, "chvt"))
	{
	  exit(0);
	}
	else if (!strcmp(applet, "fgconsole"))
	{
	  exit(0);
	}
	else if (!strcmp(applet, "screenblanker"))
	{
	  /* Infinite loop... */
	  exit(0);
	}
#endif
	else if (!strcmp(applet, "lcdoff"))
	  kbd=0;
	else if (!strcmp(applet, "lcdon"))
	{
	  kbd=0;
	  dcycle=500;
	}
	else if (!strcmp(applet, "kbledson"))
	  dcycle=500;
	else if (strcmp(applet, "kbledsoff"))
	{
	  int i=1; /* if less than 3 args assume single arg is dcycle */
	  if (!strcmp(applet, "lcdbrightness"))
	  {
	    kbd=0;
	    dcycle=500;
	  }
	  if (argc > 3) 
	  {
	    period = atoi(argv[1]);
	    prescale = atoi(argv[2]);
	    i = 3;
	  }
	  if (argc > 1) 
	    dcycle = atoi(argv[i]);
	  /* else we will be turning the LEDs to default. (kb=0, lcd=500) */
	}

      if (kbd)
	setkb(period, prescale, dcycle);
      else /* screen backlight */
	setlcd(period, prescale, dcycle);

}
