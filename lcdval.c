#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <fcntl.h>
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

typedef unsigned int u32;

struct timeval tv;

static int mfd = -1;

static int getmem(u32 addr){  
  void *map, *regaddr;
  u32 val;
  
  if (mfd == -1) {
    mfd = open("/dev/mem", O_RDWR | O_SYNC);
    if (mfd<0) {
      perror("open(\"/dev/mem\")");
      exit(1);
    }
  }

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

static void putmem(u32 addr, u32 val){
  void *map, *regaddr;
  
  if (mfd == -1) {
    mfd = open("/dev/mem", O_RDWR | O_SYNC);
    if (mfd<0) {
      perror("open(\"/dev/mem\")");
      exit(1);
    }
  }
  
  map = mmap(0,MAP_SIZE,PROT_READ | PROT_WRITE,MAP_SHARED,mfd,addr & ~MAP_MASK);
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
  close(mfd); mfd=-1; // Close /dev/mem just in case...
}

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

int is_a_console(int fd) 
{
  char arg;
  
  arg = 0;
  return (ioctl(fd, KDGKBTYPE, &arg) == 0
          && ((arg == KB_101) || (arg == KB_84)));
}

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

#if 0
int deallocvt(int fd, int vc)			
{
  if (ioctl(fd, VT_DISALLOCATE, vc))
    return -1;
  return 0;
}
#endif

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

int main(int argc, char *argv[]) {

	int period=511;
	int prescale=63;
	int dcycle=0;	
	int kbd=1;
	int fd;

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
