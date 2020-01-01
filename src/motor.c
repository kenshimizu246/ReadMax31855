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

// 12 GPIOPIN_ONE
// 16 GPIOPIN_TWO

#define PIN_CLK 14
#define PIN_DO 12
#define PIN_CS 10

#define PIN_ONE 1
#define PIN_TWO 4

int main(int argc, char *argv[])
{
	wiringPiSetup();

	pinMode(PIN_ONE, OUTPUT);
	pinMode(PIN_TWO, OUTPUT);

	for(int i = 0; i < 10; i++){
		if((i % 2) == 0){
			digitalWrite(PIN_ONE, LOW);
			digitalWrite(PIN_TWO, HIGH);
			printf("PIN_ONE low, PIN_TWO high!\n");
		}else{
			digitalWrite(PIN_ONE, HIGH);
			digitalWrite(PIN_TWO, LOW);
			printf("PIN_ONE high, PIN_TWO low!\n");
		}
		usleep(5000000);
	}
	printf("done!\n");
}
