#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <inttypes.h>

struct uinput_user_dev   uinp;
struct input_event     event;

int ufile, retcode, i, me, counter;
int opt = 0; /* 1 means we pressed the option key to toggle mouse on. */
int alt = 0; /* 1 means we pressed the alt key. */
int ctl = 0; /* 1 means we pressed the ctl key. */
int shf = 0; /* 1 means we pressed the shift key. */
int pressed = 0; /* Bitfield of held mouse move  keys. Nonzero = key repeat. */
int repeatrate = 50; /* Key repeat rate in ms */
int accel_thresh = 8; /* Number of key repeats before acceleration. */
int accel = 1;
uint32_t accel_count = 0;
int accel_base = 3;

int send_event(int ufile, __u16 type, __u16 code, __s32 value)
{
    struct input_event event;

    memset(&event, 0, sizeof(event));
    event.type = type;
    event.code = code;
    event.value = value;

    //gettimeofday(&event.time);
#if 0
    if ((type == EV_SYN) && (code == SYN_REPORT) && (value ==  0))
        return 0;
    printf("Send( T=%d, C=%d, V=%d )\n",event.type, event.code, event.value);
    return 0;
#endif  
    if (write(ufile, &event, sizeof(event)) != sizeof(event)) {
        fprintf(stderr, "Error sending Event");
        return -1;
    }

return 0;
}

void check_hold(int jx, int jy) {
    if (pressed)
        accel_count++;
    else
    if (jx == 0 && jy == 0) {
        accel_count = 0;
    } else {
        accel_count++;
    }

    accel = 1 + accel_count / accel_thresh;
}

static int jx, jy;

void processEvent(struct input_event evt) {
    //static int jx, jy;
    int moving = 0;

#if 0
    printf("Got ( T=%d, C=%d, V=%d )\n",evt.type, evt.code, evt.value);
#endif  
  
    //option
    if(evt.code == 357) {
        if(evt.value == 1 /*|| evt.value == 2*/) { 
            if(opt == 1) {
#if 0
                printf("toggled off\n");
#endif
                opt = 0 ;
            } else if(opt == 0) {
#if 0
                printf("toggled on\n");
#endif
                opt = 1 ;	
            }
        }
    }
    if(evt.code == 29) {
        if(evt.value == 1 /*|| evt.value == 2*/) 
	  ctl == 1;
	else 
	  ctl = 0;
    }
    if(evt.code == 56) {
        if(evt.value == 1 /*|| evt.value == 2*/) 
	  alt = 1 ;	
	else
	  alt = 0 ;
    }
    if(evt.code == 42) {
        if(evt.value == 1 /*|| evt.value == 2*/) 
	  shf = 1 ;	
	else
	  shf = 0 ;
    }

    if(opt == 1) {
        switch(evt.code) {
			case 103: 
			//up
            moving = 1;
            if (evt.value == 1)
                pressed |= 1;
            else 
                pressed &= ~1;
            jy = evt.value == 0 ? 0 : -accel_base;
            check_hold(jx, jy);
		   	break;
		   	
		   	case 108:
			//down
            moving = 1;
            if (evt.value == 1)
                pressed |= 2;
            else 
                pressed &= ~2;
            jy = evt.value == 0 ? 0 : accel_base;
            check_hold(jx, jy);
		    break;
	
		   	case 105:
			//left
            moving = 1;
            if (evt.value == 1)
                pressed |= 4;
            else 
                pressed &= ~4;
            jx = evt.value == 0 ? 0 : -accel_base;
            check_hold(jx, jy);
		    break;
			
			case 106:
			//right
            moving = 1;
            if (evt.value == 1)
                pressed |= 8;
            else 
                pressed &= ~8;
            jx = evt.value == 0 ? 0 : accel_base;
            check_hold(jx, jy);
            break;
          		
   			case 107:
			//	left mouse
  	        send_event(ufile, EV_KEY, BTN_MIDDLE, evt.value);
  	        break;
			
			case 166:
            //right mouse
            send_event(ufile, EV_KEY, BTN_RIGHT, evt.value);
            break;
			
	        case 200:
            //middle  mouse
            send_event(ufile, EV_KEY, BTN_LEFT, evt.value);
            break;
# if 0 /* F1 is already on the HOME key.  What was this for? */
		case 104:
	    //F1
	    send_event(ufile, EV_KEY, KEY_F1, evt.value);
	    break;
	        case 109:
	    //F5
	    send_event(ufile, EV_KEY, KEY_F5, evt.value);
	    break;
#endif
 		default:
#if 0
            printf("PASS ");
#endif
    	    send_event(ufile, EV_KEY,evt.code, evt.value);
			break; 	
  		}

        if (moving) {
#if 0
            printf("MOV(%d) ", accel);
#endif
			send_event(ufile, EV_REL, REL_Y, jy*accel); 
			send_event(ufile, EV_REL, REL_X, jx*accel); 
        }
  		
  		//the sync crap
  		send_event(ufile, EV_SYN, SYN_REPORT, 0);
    } else {
        char cmd[32];
	int lcd, kbd;
	FILE *f;
	if ((evt.type == EV_KEY) && (evt.value == 1)) {
	  switch(evt.code) {
	  case 200: // Play button
	    sprintf(cmd, "OnPlay");
	    system(cmd);
	    return;
	  case 166: // Stop button
	    sprintf(cmd, "OnStop");
	    system(cmd);
	    return;
	  case 74: // Vol- button        
	    if (shf) {
	      if (f = popen("kbval", "r")) {
		fscanf(f, "%d", &kbd); pclose(f); 
		kbd -= 50; if (kbd < 0) kbd = 0;
		sprintf(cmd, "kbbrightness %d", kbd);
		system(cmd);
		return;
	      }
	    } else if (alt) {
	      if (f = popen("lcdval", "r")) {
	        fscanf(f, "%d", &lcd); pclose(f); 
		lcd -= 50; if (lcd < 0) lcd = 0;
		sprintf(cmd, "lcdbrightness %d", lcd);
		system(cmd);
		return;
	      }
	    }
	    break;
	  case 78: // Vol+ button
	    if (shf) {
	      if (f = popen("kbval", "r")) {
		fscanf(f, "%d", &kbd); pclose(f);
		kbd += 50; if (kbd > 500) kbd = 500;
		sprintf(cmd, "kbbrightness %d", kbd);
		system(cmd);
		return;
	      }
	    } else if (alt) {
	      if (f = popen("lcdval", "r")) {
		fscanf(f, "%d", &lcd); pclose(f); 
		lcd += 50; if (lcd > 500) lcd = 500;
		sprintf(cmd, "lcdbrightness %d", lcd);
		system(cmd);
		return;
	      }
	    }
	  }
	}
	// Normal key handling.
	send_event(ufile, EV_KEY,evt.code, evt.value);
	send_event(ufile, EV_SYN, SYN_REPORT, 0);
    }
			 
    //printf("outside\n");
}




int main(void) {
    int evdev = -1;

    ufile = open("/dev/input/uinput", O_WRONLY);
    if (ufile == 0) {
        printf("Could not open uinput.\n");
        exit(1);
    }

    memset(&uinp, 0, sizeof(uinp));
    strncpy(uinp.name, "Zipit2 Mouse Emulator", 20);
    uinp.id.version = 4;
    uinp.id.bustype = BUS_USB;

    ioctl(ufile, UI_SET_EVBIT, EV_KEY);
    ioctl(ufile, UI_SET_EVBIT, EV_REL);
    ioctl(ufile, UI_SET_RELBIT, REL_X);
    ioctl(ufile, UI_SET_RELBIT, REL_Y);

    for (i=0; i<256; i++) {
        ioctl(ufile, UI_SET_KEYBIT, i);
    }

    ioctl(ufile, UI_SET_KEYBIT, BTN_MOUSE);
    ioctl(ufile, UI_SET_KEYBIT, BTN_LEFT);
    ioctl(ufile, UI_SET_KEYBIT, BTN_RIGHT);
    ioctl(ufile, UI_SET_KEYBIT, BTN_MIDDLE);	

    retcode = write(ufile,&uinp, sizeof(uinp));

    if (ioctl(ufile,UI_DEV_CREATE) < 0) {  
        fprintf(stderr, "Error creating Device\n");
        exit(1);
    }

    // The kbd seems to be on event0 in IZ2S.
    //evdev = open("/dev/input/event1", O_RDONLY);
    evdev = open("/dev/input/event0", O_RDONLY);
    int value = 1;
    ioctl(evdev, EVIOCGRAB, &value);
    struct input_event ev[64];	
	
    for (;;) {
        if (opt){
            /* When mousing, use select with 100ms timeout for acceleration. */
            struct timeval t;
            fd_set fds;
            int ready;
            
            t.tv_sec = 0;
            t.tv_usec = repeatrate * 1000;
            FD_ZERO(&fds);
            FD_SET(evdev, &fds);
            ready = select(evdev+1,&fds,NULL,NULL,&t);
            if (ready == -1)
                continue;
            if (ready == 0) {
                if (pressed) {
                   if (pressed & 1)
                     jy = -accel_base;
                   if (pressed & 2)
                     jy = accel_base;
                   if (pressed & 4)
                     jx = -accel_base;
                   if (pressed & 8)
                     jx = accel_base;
                   check_hold(jx, jy);
                   send_event(ufile, EV_REL, REL_Y, jy*accel); 
                   send_event(ufile, EV_REL, REL_X, jx*accel); 
                   // What is the sync crap?
                   send_event(ufile, EV_SYN, SYN_REPORT, 0);
                }
                continue;
            }
            /* evdev is ready for a read, so fall thru to read() below. */
        }

        me=read(evdev,ev,sizeof(struct input_event)*64);
    	for (counter = 0;
            counter < (int) (me / sizeof(struct input_event));
            counter++) {
            if (EV_KEY == ev[counter].type) {
                processEvent(ev[counter]);
            }
        }

    }

    // destroy the device
    ioctl(ufile, UI_DEV_DESTROY);
    close(ufile);
}
