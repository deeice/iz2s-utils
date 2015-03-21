/*****************************************************
 * Turn the ZipitZ2 LCD ON
 * simple error checking.
 *
 * 05/02/10 Russell K. Davis
 * (Large) portions ripped from devmem2
 *
 ****************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <string.h>

#define MAP_SIZE 4096UL


#define GPIO_AC    0	/* AC Power */
#define POWERBUTTON  1	/* power button */
#define GPIO_RLED 10	/* LED GPIO */
#define GPIO_LCD  11	/* LCD GPIO */
#define GPIO_LID  98	/* LID GPIO */
#define GPIO_BASE 0x40E00000 /* PXA270 GPIO Register Base */

#define GPSR 0x18
#define GPCR 0x24

typedef unsigned long u32;

/***************************************************/
int regoffset(int gpio) {
	if (gpio < 32) return 0;
	if (gpio < 64) return 4;
	if (gpio < 96) return 8;
	return 0x100;
}

/***************************************************/
void gpio_set(void *map_base, int gpio, int val) {
        volatile u32 *reg = (u32*)((u32)map_base + (val ? GPSR : GPCR) + regoffset(gpio));
        *reg = 1 << (gpio & 31);
}

/***************************************************/
int gpio_read(void *map_base, int gpio) {
	volatile u32 *reg = (u32*)((u32)map_base + regoffset(gpio));
	return (*reg >> (gpio&31)) & 1;
}

/***************************************************/
/* Simple fn to get or set (or clear) one gpio bit */
int getsetgpio(int gpio_base, int gpio, int val) {

	int fd;
	int retval=0;
	void *map_base; 

	/* Should use O_RDONLY for O_RDWR, and no PROT_WRITE if (val < 0) */
	fd = open("/dev/mem", O_RDWR | O_SYNC);
   	if (fd < 0) exit(255);
	
    	map_base = mmap(0, MAP_SIZE, PROT_READ | PROT_WRITE , MAP_SHARED, fd, gpio_base);
	if(map_base == (void *) -1) exit(255);

	/* Negative val means Monitor or Read the GPIO. */
	if (val == -2) 
	  while(1) {
		if (gpio_read(map_base, POWERBUTTON)==0) { 
			if((gpio_read(map_base, GPIO_LID) == 1)) {
				if(munmap(map_base, MAP_SIZE) == -1) exit(255) ;
        			close(fd);
			       	system("poweroff") ;
				sleep(5);
			}
		}
		sleep(2);
	  }
	else if (val == -1) 
	  switch(gpio_read(map_base,gpio))
	  {
		case 0: /* lid is closed | on battery | power button is pressed */
			retval = 0;
			break;
		case 1: /* lid is open | on AC mains | power button is not pressed */
			retval = 1;
			break;
		default: /* will never reach here unless something has gone terribly wrong */
			retval = 255;
	  }
	else
	  gpio_set(map_base, gpio, val);

	if(munmap(map_base, MAP_SIZE) == -1) exit(255) ;
	close(fd);

	return retval;
}

/***************************************************/
int main(int argc, char **argv) {

	const char *applet = strrchr (argv[0], '/');

	if (applet == NULL)
	  applet = argv[0];
	else
	  applet++;

	if (!strcmp(applet, "lcdon") || !strcmp(applet, "screenon"))
	  return getsetgpio(GPIO_BASE, GPIO_LCD, 1);
	else if (!strcmp(applet, "lcdoff") || !strcmp(applet, "screenoff"))
	  return getsetgpio(GPIO_BASE, GPIO_LCD, 0);
	if (!strcmp(applet, "rightledon"))
	  return getsetgpio(GPIO_BASE, GPIO_RLED, 0);
	else if (!strcmp(applet, "rightledoff"))
	  return getsetgpio(GPIO_BASE, GPIO_RLED, 1);
	else if (!strcmp(applet, "lid"))
	  return getsetgpio(GPIO_BASE, GPIO_LID, -1);
	else if (!strcmp(applet, "acpower"))
	  return getsetgpio(GPIO_BASE, GPIO_AC, -1);
	else if (!strcmp(applet, "powerbtn"))
	  return getsetgpio(GPIO_BASE, POWERBUTTON, -1);
	else if (!strcmp(applet, "powerbuttond"))
	  return getsetgpio(GPIO_BASE, POWERBUTTON, -2);
#if 0
	/* These are in the screenblanker.c multi-call app. */
	else if (!strcmp(applet, "kbledson"))
	  printf("Running kbledson\n");
	else if (!strcmp(applet, "kbledsoff"))
	  printf("Running kbledsoff\n");

	/* No source for this yet.  I think it polls the lid switch. */
	else if (!strcmp(applet, "powerd"))
	  printf("Running powerd\n");
#endif	
	else 
	  printf("Unknown applet\n");
	
}

