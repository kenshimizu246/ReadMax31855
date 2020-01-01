#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
#include <getopt.h>
#include <fcntl.h>
#include <time.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>

#include <wiringPi.h>

		/*
		 +----------+---------------------+----------------------------------------------------------------
		 | BIT      | NAME                | DESCRIPTION
		 +----------+---------------------+----------------------------------------------------------------
		 | D[31:18] | 14-Bit Thermocouple | Temperature Data These bits contain 
		 |          |                     | the signed 14-bit thermocouple temperature
		 |          |                     | value. See Table 4.|
		 +----------+---------------------+----------------------------------------------------------------
		 | D17      | Reserved            | This bit always reads 0.
		 +----------+---------------------+----------------------------------------------------------------
		 | D16      | Fault               | This bit reads at 1 when any of the SCV, SCG, or OC 
		 |          |                     | faults are active. Default value is 0.
		 +----------+---------------------+----------------------------------------------------------------
		 | D[15:4]  | 12-Bit Internal     | These bits contain the signed 12-bit value of
		 |          | Temperature Data    | the reference junction temperature. See Table 5.
		 +----------+---------------------+----------------------------------------------------------------
		 | D3       | Reserved            | This bit always reads 0.
		 +----------+---------------------+----------------------------------------------------------------
		 | D2       | SCV Fault           | This bit is a 1 when the thermocouple is short-circuited
		 |          |                     | to VCC. Default value is 0.
		 +----------+---------------------+----------------------------------------------------------------
		 | D1       | SCG Fault           | This bit is a 1 when the thermocouple is short-circuited
		 |          |                     | to GND. Default value is 0.
		 +----------+---------------------+----------------------------------------------------------------
		 | D0       | OC Fault            | This bit is a 1 when the thermocouple is open (no connections).
		 |          |                     | Default value is 0.
		 +----------+---------------------+----------------------------------------------------------------
		*/

#define PIN_CLK 14
#define PIN_DO 12
#define PIN_CS 10

#define PITCH_MIN 1000000

int main(int argc, char *argv[])
{
	time_t timer;
	struct tm *local;
	struct tm *utc;

	unsigned int sclk = PIN_CLK;
	unsigned int mosi = PIN_DO;
	unsigned int ss   = PIN_CS;

	// Clock is normally high in mode 2 and 3.
	// Clock is normally low in mode 0 and 1.
	unsigned int clock_base = LOW;

	// Read on trailing edge in mode 1 and 3.
	// Read on leading edge in mode 0 and 2.
	bool read_leading = true;

	// Bit order is MSBFIRST
	unsigned char mask = 0x80;
	// write, lshift
	// read, rshift

	wiringPiSetup();

	pinMode(sclk, OUTPUT);
	pinMode(mosi, INPUT);
	pinMode(ss,   OUTPUT);
	
	// Put clock into its base state.
        // self._gpio.output(self._sclk, self._clock_base)
	digitalWrite(sclk, clock_base);

	while(true){
		digitalWrite(ss, LOW);

		printf("\n===============================\n");
		timer = time(NULL);
		local = localtime(&timer);
		printf("%02d:%02d:%02d\n", local->tm_hour,local->tm_min,local->tm_sec);
		unsigned char raw[4];
		for(unsigned int b = 0; b < 4; b++){
			for(unsigned int i = 0; i < 8; i++){
				digitalWrite(sclk, (clock_base == LOW ? HIGH : LOW));
				// read_leading
				int r;
				r = digitalRead(mosi);
				printf("%d", r);
				digitalWrite(sclk, clock_base);
				raw[b] <<= 1;
				raw[b] = raw[b] | r;
			}
			printf("|0x%02x, ", raw[b]);
		}
		printf("\n");
		int value = raw[0] << 24 | raw[1] << 16 | raw[2] << 8 | raw[3];
		printf("value:%x\n", value);
		// 0x7 -> b...111 D2 D1 D0
		if(value & 0x7){
			printf("float NaN\n");
		} else {
			// cut off D0 - D17
			value >>= 18;
		}
		// Scale by 0.25 degrees C per bit and return value.
		float tempC = value * 0.25;
		printf("tempC:%f\n", tempC);

		digitalWrite(ss, HIGH);
		usleep(1000000);
	}
	printf("done!\n");
}
