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
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <errno.h>
  
/* From <linux/vt.h> */
#define VT_ACTIVATE     0x5606  /* make vt active */
#define VT_WAITACTIVE   0x5607  /* wait for vt active */
#define VT_DISALLOCATE  0x5608  /* free memory associated to vt */

struct vt_stat {
        unsigned short v_active;        /* active vt */
        unsigned short v_signal;        /* signal to send */
        unsigned short v_state;         /* vt bitmask */
};
enum { VT_GETSTATE = 0x5603 }; /* get global vt state info */

static const int KDGKBTYPE = 0x4B33; /* get keyboard type */
static const int KB_84 = 0x01;
static const int KB_101 = 0x02;    /* this is what we always answer */

#define MAP_SIZE 4096UL
#define MAP_MASK ( MAP_SIZE - 1 )

#define GPIO_AC    0	/* AC Power */
#define POWERBUTTON  1	/* power button */
#define GPIO_RLED 10	/* LED GPIO (right) */
#define GPIO_LCD  11	/* LCD GPIO */
#define GPIO_MLED 83	/* LED GPIO (mid) */
#define GPIO_LLED 85	/* LED GPIO (left) */
#define GPIO_LID  98	/* LID GPIO */
#define GPIO_BASE 0x40E00000 /* PXA270 GPIO Register Base */

#define GPDR 0x0C /* PXA270 GPIO Direction Registers */
#define GPSR 0x18 /* PXA270 GPIO Set Registers */
#define GPCR 0x24 /* PXA270 GPIO Clear Registers */

typedef unsigned long u32;

struct timeval tv;

static int mfd = -1;

/***************************************************/
/*** The mmap LED register utilities start here. ***/
/***************************************************/
static void getmfd(void){  
  if (mfd == -1)
    mfd = open("/dev/mem", O_RDWR | O_SYNC);
  if (mfd<0) {
    perror("open(\"/dev/mem\")");
    exit(1);
  }
}

/***************************************************/
static int getmem(u32 addr){  
  void *map, *regaddr;
  u32 val;

  getmfd();
  map = mmap(0, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, mfd, addr & ~MAP_MASK );
  
  if (map == (void*)-1 ) {
    perror("mmap()");
    exit(1);
  }
  
  regaddr = map + (addr & MAP_MASK);
  
  val = *(u32*) regaddr;
  munmap(0,MAP_SIZE);
  
  return val;
}

/***************************************************/
static void putmem(u32 addr, u32 val){
  void *map, *regaddr;
  
  getmfd();
  map = mmap(0,MAP_SIZE,PROT_READ | PROT_WRITE,MAP_SHARED,mfd,addr & ~MAP_MASK);
  if (map == (void*)-1 ) {
    perror("mmap()");
    exit(1);
  }
  
  regaddr = map + (addr & MAP_MASK);
  
  *(u32*) regaddr = val;
  munmap(0,MAP_SIZE);
}

/***************************************************/
static void setreg(u32 addr, u32 mask, int shift, u32 val)
{
  u32 mem;

  mem = getmem(addr);
  mem &= ~(mask << shift);
  val &= mask;
  mem |= val << shift;
  putmem(addr, mem);   
}

/***************************************************/
static int getreg(u32 addr, u32 mask, int shift)
{
  u32 mem;

  mem = getmem(addr);
  return ((mem >> shift) & mask);
}

/***************************************************/
void setkb(int period, int prescale, int dcycle)
{
  setreg(0x40C00008,0xffffffff,0,period); //PWMPERVAL1 -- PWM2 Period Cycle Length
  setreg(0x40C00000, 0x000003f,0,prescale); // PWMCTL1_PRESCALE
  setreg(0x40C00000, 0x0000001,5,0); // PWMCTL2_SD -- PWM2 Abrupt Shutdown
  setreg(0x40C00004, 0x0000001,10,0); // PWMDUTY2_FDCYCLE 
  setreg(0x40C00004, 0x00003ff,0,dcycle); // PWM2 Duty Cycle
  setreg(0x41300004, 0x0000001,1,1); // CM PWM clock enabled
  close(mfd); mfd=-1; // Close /dev/mem just in case...
}

/***************************************************/
void setlcd(int period, int prescale, int dcycle)
{
  setreg(0x40E00054,0x00000003,22,2); //GAFR0L_11 -- GPIO 11 Alternative function
  setreg(0x40B00018,0xffffffff,0,period); //PWMPERVAL2 -- PWM2 Period Cycle Length
  setreg(0x40B00010, 0x000003f,0,prescale); // PWMCTL2_PRESCALE
  setreg(0x40B00010, 0x0000001,5,0); // PWMCTL2_SD -- PWM2 Abrupt Shutdown
  setreg(0x40B00014, 0x0000001,10,0); // PWMDUTY2_FDCYCLE 
  setreg(0x40B00014, 0x00003ff,0,dcycle); // PWM2 Duty Cycle
  setreg(0x41300004, 0x0000001,1,1); // CM PWM clock enabled

  close(mfd); mfd=-1; // Close /dev/mem just in case...
}

/***************************************************/
int getkeyspressed ( void )
{
    static const char filename[] = "/proc/interrupts";
    FILE *file = fopen ( filename, "r" );
    char *result = NULL;
    int value;
    
    if ( file != NULL )
    {
        char line [ 256 ]; 
        while ( fgets ( line, sizeof line, file ) != NULL ) 
        {
            if (strstr(line, "pxa27x-keyboard")) {
		result = strtok(line, " ");
	        result = strtok(NULL, " ");
		value = atoi(result);
	    }		    
	}
        fclose ( file );
    }
    return value;
}

/***************************************************/
int is_a_console(int fd) 
{
  char arg;
  
  arg = 0;
  return (ioctl(fd, KDGKBTYPE, &arg) == 0
          && ((arg == KB_101) || (arg == KB_84)));
}

/***************************************************/
static int open_a_console(char *fnam) 
{
  int fd;
  
  /* try read-only */
  fd = open(fnam, O_RDWR);
  
  /* if failed, try read-only */
  if (fd < 0 && errno == EACCES)
      fd = open(fnam, O_RDONLY);
  
  /* if failed, try write-only */
  if (fd < 0 && errno == EACCES)
      fd = open(fnam, O_WRONLY);
  
  /* if failed, fail */
  if (fd < 0)
      return -1;
  
  /* if not a console, fail */
  if (! is_a_console(fd))
    {
      close(fd);
      return -1;
    }
  
  /* success */
  return fd;
}

/***************************************************/
int get_a_console_fd(void)
{
  int fd = open_a_console("/dev/tty");
  if (fd < 0) fd = open_a_console("/dev/tty0");
  if (fd < 0) fd = open_a_console("/dev/console");
  if (fd < 0)
  { 
    for (fd = 0; fd < 3; fd++)
      if (is_a_console(fd))
	break;
    if (fd > 2)
      return -1;
  }
  return fd;
}

/***************************************************/
int fgconsole(int fd)			
{
  struct vt_stat vtstat;
  
  vtstat.v_active = 0;
  if (ioctl(fd, VT_GETSTATE, &vtstat)) 
  {
    close(fd); 
    return -1;
  }
  return vtstat.v_active;
}

/***************************************************/
int chvt(int fd, int vc)			
{
  if ((ioctl(fd, VT_ACTIVATE, vc)) ||
      (ioctl(fd, VT_WAITACTIVE, vc)))
  {
    close(fd); 
    return -1;
  }
  close(fd);
  return 0;
}

/***************************************************/
#if 0
int deallocvt(int fd, int vc)			
{
  if (ioctl(fd, VT_DISALLOCATE, vc))
    return -1;
  return 0;
}
#endif

/***************************************************/
int setlogcons(int fd, int vc)			
{
  struct { char fn, subarg; } arg;
  arg.fn = 11;            /* redirect kernel messages */
  arg.subarg = vc;        /* to specified console */
  if (ioctl(fd, TIOCLINUX, &arg)) 
  {
    close(fd); 
    return -1;
  }
  close(fd);
  return 0;
  }

/***************************************************/
int screenblanker_main (int argc, char *argv[]) 
{
  int prev_count, current_count;
  int delay=180; // blank screen after 2 minutes with no key press.
  int fgcon, lcd, kbd;
  int fd;

  // Let setup-net worry about this.  
  // If we want to dis/enable it below like the screen, thats ok.
  //system("rightledoff");

  if (argc > 1) 
    delay = atoi(argv[1]);	
  if (delay < 5)
    delay = 5; // Lets be reasonable.  Five seconds is not a long time.
  
  while (1) {
    prev_count = getkeyspressed();
    sleep(delay);
    current_count = getkeyspressed();
    if (current_count == prev_count) // If long time, no key...
      {
	char buff[50];

	// First get current console, and lcd, kbd settings.
	lcd = getreg(0x40B00014, 0x00003ff,0); // PWM2 Duty Cycle
	kbd = getreg(0x40C00004, 0x00003ff,0); // PWM2 Duty Cycle
	close(mfd); mfd=-1; // Close /dev/mem in case of continue...
	if (lcd < 200) lcd = 200; // lcd val could be 0 on bootup.

	if ((fd = get_a_console_fd()) < 0)
	  continue;
	if ((fgcon = fgconsole(fd)) < 1)
	  continue;
	// Go to virtual console 3 right before blanking.
	if (chvt(fd, 3)) continue;
	close(fd);
	
	setlcd(511, 63, 0);
	setkb(511, 63, 0);
	while(getkeyspressed() == current_count) {
	  tv.tv_sec = 2;
	  tv.tv_usec = 0;
	  select(0,NULL,NULL,NULL, &tv);
	}
	// Restore previous lcd, kbd settings, and saved foreground console.
	if ((fd = get_a_console_fd()) >= 0)
	  chvt(fd, fgcon);
	setlcd(511, 63, lcd);
	setkb(511, 63, kbd);
      }
  }		
}

/***************************************************/
/*** The gpio based lid/pwr utilities start here ***/
/***************************************************/
int regoffset(int gpio) {
  if (gpio < 32) return 0;
  if (gpio < 64) return 4;
  if (gpio < 96) return 8;
  return 0x100;
}

/***************************************************/
void gpio_dir(void *map_base, int gpio, int val) {
  volatile u32 *reg = (u32*)((u32)map_base + GPDR + regoffset(gpio));
  if (val)
    *reg |= 1 << (gpio & 31);    /* Set Direction bit: 1=Output */
  else
    *reg &= ~(1 << (gpio & 31)); /* Clear the bit: 0=Input */
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
int reg_read(void *map_base, int gpio, u32 mask, int shift) {
  volatile u32 *reg = (u32*)((u32)map_base + regoffset(gpio));
  return (*reg >> (gpio&31)) & mask;
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
  if (val == -2) {
    int lid = (gpio_read(map_base, GPIO_LID));
    int oldlid;
    int lcd, kbd;
    lcd = kbd = 500;
    while(1) {
    oldlid = lid;
    lid = (gpio_read(map_base, GPIO_LID));
    if (gpio_read(map_base, POWERBUTTON)==0) { 
      if(lid) { /* Skip poweroff attempt if lid is shut. */
        if(munmap(map_base, MAP_SIZE) == -1) exit(255) ;
        close(fd);
        system("poweroff") ;
        sleep(5);
      }
    }
    if (lid != oldlid) { 
      if (lid) { /* Lid was just opened */
#if 0
        char cmd[32];
        sprintf(cmd, "lcdbrightness %d", lcd);
        system(cmd);
        sprintf(cmd, "kbbrightness %d", kbd);
        system(cmd);
#else
	setlcd(511, 63, lcd);
	setkb(511, 63, kbd);
#endif
      }
      else { /* Lid was just closed */
#if 0
        FILE *f;
        if (f = popen("lcdval", "r"))
          { fscanf(f, "%d", &lcd); pclose(f); }
        if (f = popen("kbdval", "r"))
          { fscanf(f, "%d", &kbd); pclose(f); }
        system("lcdoff") ;
        system("kbledsoff") ;
#else
	// system or popen seems to leak 10k of RAM the first time we run it.
	lcd = getreg(0x40B00014, 0x00003ff,0); // PWM2 Duty Cycle
	kbd = getreg(0x40C00004, 0x00003ff,0); // PWM2 Duty Cycle
	setlcd(511, 63, 0);
	setkb(511, 63, 0);
#endif
      }
    }
    sleep(2);
    }
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
  else if (gpio == GPIO_MLED) /* For some reason this GPIO defaults to input. */
  {
    gpio_dir(map_base, gpio, 1); /* Make it an output, then set it. */
    gpio_set(map_base, gpio, val);
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
  if (!strcmp(applet, "lcdoff") || !strcmp(applet, "screenoff"))
    return getsetgpio(GPIO_BASE, GPIO_LCD, 0);

  // Looks like uclibc inlines strings shorter than 4 chars.  Maybe as int?
  // Because "strings" does not find "lid" in the executable.  Weird.
  if (!strcmp(applet, "lid"))
    return getsetgpio(GPIO_BASE, GPIO_LID, -1);
  if (!strcmp(applet, "acpower"))
    return getsetgpio(GPIO_BASE, GPIO_AC, -1);
  if (!strcmp(applet, "powerbtn"))
    return getsetgpio(GPIO_BASE, POWERBUTTON, -1);
  if (!strcmp(applet, "powerbuttond") || !strcmp(applet, "litepwrd")) /* powerd */
    return getsetgpio(GPIO_BASE, POWERBUTTON, -2);

  if (!strcmp(applet, "rightled"))
    return getsetgpio(GPIO_BASE, GPIO_RLED, -1);
  if (!strcmp(applet, "rightledon"))
    return getsetgpio(GPIO_BASE, GPIO_RLED, 0);
  if (!strcmp(applet, "rightledoff"))
    return getsetgpio(GPIO_BASE, GPIO_RLED, 1);
  if (!strcmp(applet, "leftled"))
    return getsetgpio(GPIO_BASE, GPIO_LLED, -1);
  if (!strcmp(applet, "leftledon"))
    return getsetgpio(GPIO_BASE, GPIO_LLED, 0);
  if (!strcmp(applet, "leftledoff"))
    return getsetgpio(GPIO_BASE, GPIO_LLED, 1);
  if (!strcmp(applet, "middled"))
    return getsetgpio(GPIO_BASE, GPIO_MLED, -1);
  if (!strcmp(applet, "middledon"))
    return getsetgpio(GPIO_BASE, GPIO_MLED, 0);
  if (!strcmp(applet, "middledoff"))
    return getsetgpio(GPIO_BASE, GPIO_MLED, 1);
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
  else { /*** LED register utilities start here. ***/
  int period=511;
  int prescale=63;
  int dcycle=0;  
  int kbd=1;
  int fd;

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

  // The clear utility goes along well with the other apps here.
  else if (!strcmp(applet, "clear")) 
  {
    write(1,"\e[H\e[J",6);
    exit(0);
  }

  /* Combine with applets from screenblanker. */
  else if (!strcmp(applet, "chvt"))
  {
    if (argc < 2) 
      exit(-1);
    if ((fd = get_a_console_fd()) < 0)
      exit(-1);
    chvt(fd, atoi(argv[1]));
    exit(0);
  }
  else if (!strcmp(applet, "fgconsole"))
  {
    if ((fd = get_a_console_fd()) < 0)
      exit(-1);
    if ((dcycle = fgconsole(fd)) < 1)
      exit(-1);
    printf("%d\n", dcycle);
    exit(0);
  }
  else if (!strcmp(applet, "deallocvt"))
  {
    int i;
    if ((fd = get_a_console_fd()) < 0)
      exit(-1);
    if (argc < 2) 
      ioctl(fd, VT_DISALLOCATE, 0); // deallocvt(fd, 0); // Deallocate all unused consoles
    else for (i = 1; i < argc; i++)
    {
      int num = atoi(argv[i]);
      if (num > 1) 
        ioctl(fd, VT_DISALLOCATE, num); //deallocvt(fd, num);
    }
    close(fd);
    exit(0);
  }
  else if (!strcmp(applet, "setlogcons"))
  {
    if ((fd = get_a_console_fd()) < 0)
      exit(-1);
    if (argc < 2) 
      setlogcons(fd, 0);
    else
      setlogcons(fd, atoi(argv[1]));
    exit(0);
  }
  else if (!strcmp(applet, "screenblanker") || !strcmp(applet, "screensaver"))
  {
    screenblanker_main(argc, argv);
    exit(0);
  }

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
}

