
/* Query the LPC915 power control MCU via the i2c bus, and obtain
   "battery charge level" (probably actually voltage across battery.) 
   Convert this voltage to a battery-remaining percentage based on empirical data. */

/* Adam Tilghman, 5/2005, released into the public domain */

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <time.h>

unsigned char batlvl_calibration[256] = {
		0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
		0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
		0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
		0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
		0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
		0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
		0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
		0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
		0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
		0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
		0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
		0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
		0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    2,    5,
		6,    7,    8,    9,   11,   13,   16,   20,   23,   29,   36,   42,   48,   54,   58,   62,
		65,   68,   69,   71,   73,   77,   80,  82,   83,   84,   85,   86,   88,   91,   92,   94,
		94,   95,   95,   95,   95,   95,   96,  96,   96,   97,   97,   98,   99,   99,  100,  100,
	};

#define NUM_SAMPLES 10

int main(int argc, char *argv[])
{
	char *filename;
	unsigned char res;
	int file;
	int reading, tot;
	char buf[100];
	int loopmode = 0;

	loopmode = ((argc == 2) && (argv[1][0] == '-') && (argv[1][1] == 'l'));

	do {
	filename = "/dev/i2c-0";
	if ( (file = open(filename,O_RDWR)) < 0) {
		fprintf(stderr, "Error: could not open %s\n", filename);
		exit(1);
	}

	/* use FORCE so that we can look at registers even when a driver is also running */
	if (ioctl(file,I2C_SLAVE_FORCE,0x55) < 0) {
		fprintf(stderr, "Error: could not open lpc915 controller via i2c\n");
		exit(1);
	}

	/* Take NUM_SAMPLES readings, and give the average value. */
	tot = 0;
	for (reading = 0; reading < NUM_SAMPLES; reading++) {
		res = i2c_smbus_read_byte_data(file, 0x02);  /* batt chg level */
		tot += (int) res;
		struct timeval tv = { 0, (1000*100) }; /* wait .10 seconds between readings */
		select(0, NULL, NULL, NULL, &tv);
	}

	close(file);

	printf("%d%s\n", batlvl_calibration[tot/NUM_SAMPLES], loopmode ? "":"%");

	if (loopmode) { fflush(stdout); sleep(10); }

	} while (loopmode);

}


