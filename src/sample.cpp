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
#include <limits>

#include <iostream>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <cstdio>
#include <vector>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <string>

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
#define PIN_CS1 10
#define PIN_CS2 11

#define HIST_SIZE 60

#define PIN_SWITCH 25
#define PITCH_MIN 1000000

class Listener {
private:
	int hist_size = HIST_SIZE;
	int cnt = 0;
	float speed10 = 0;
	float speed60 = 0;
	float speed10max = 5;
	float speed60max = 5;
	float hist[HIST_SIZE];
	int blink_count = 0;
	int low_cnt = 0;
	int high_cnt = 0;
	int low_ratio = 1;
	int high_ratio = 1;

public:
	void update(const unsigned int s, const float tempC){
		time_t timer;
		struct tm *local;
		struct tm *utc;

		timer = time(NULL);
		local = localtime(&timer);

		hist[cnt % hist_size] = tempC;
		if(cnt >= 10){
			speed10 = (tempC - hist[(cnt - 10) % hist_size]);
		}
		if(cnt >= 60){
			speed60 = (tempC - hist[(cnt - 60) % hist_size]);
		}

		int cur_relay = digitalRead(25);
		if(cur_relay == HIGH){
			low_cnt = 0;
			high_cnt++;
		} else if(cur_relay == LOW){
			low_cnt++;
			high_cnt = 0;
		}

		if(tempC < (58.0 - speed10max)){
			if(cur_relay == LOW){
				digitalWrite(25, HIGH);
			}
		}else if(tempC > 58.0){
			if(cur_relay == HIGH){
				digitalWrite(25, LOW);
			}
		}else if(tempC >= (58.0 - speed10max)){
			if(cur_relay == LOW && low_cnt >= low_ratio){
				digitalWrite(25, HIGH);
			}
			if(cur_relay == HIGH && high_cnt >= high_ratio){
				digitalWrite(25, LOW);
			}
		}
		cnt++;

		cur_relay = digitalRead(25);
		std::cout << std::setw(2) << std::setfill('0') << local->tm_hour;
		std::cout << ":" << std::setw(2) << std::setfill('0') << local->tm_min;
		std::cout << ":" << std::setw(2) << std::setfill('0') << local->tm_sec;
		std::cout << " " << tempC << " [" << s << "]" << std::endl;
		std::cout << "speed10:" << speed10 << std::endl;
		std::cout << "speed60:" << speed60 << std::endl;
		std::cout << "cur_relay:" << cur_relay << std::endl;
	}
};

class Max31855Reader {
private:
	unsigned int sclk;
	unsigned int mosi;
	std::vector<unsigned int> slaves;
	std::vector<Listener*> listeners;

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

	void readAndUpdate(){
	        wiringPiSetup();

		pinMode(sclk, OUTPUT);
		pinMode(mosi, INPUT);
		for(unsigned int ss : slaves){
			pinMode(ss,   OUTPUT);
		}

		pinMode(25, OUTPUT);

		// Put clock into its base state.
		digitalWrite(sclk, clock_base);

		while(true){
			for(unsigned int ss : slaves){
				digitalWrite(ss, LOW);
				unsigned char raw[4];
				for(unsigned int b = 0; b < 4; b++){
					for(unsigned int i = 0; i < 8; i++){
						digitalWrite(sclk, (clock_base == LOW ? HIGH : LOW));
						// read_leading
						int r;
						r = digitalRead(mosi);
						//printf("%d", r);
						digitalWrite(sclk, clock_base);
						raw[b] <<= 1;
						raw[b] = raw[b] | r;
					}
					//printf("|0x%02x, ", raw[b]);
				}
				//printf("\n");
				int value = raw[0] << 24 | raw[1] << 16 | raw[2] << 8 | raw[3];
				//printf("value:%x\n", value);

				// 0x7 -> b...111 D2 D1 D0
				float tempC = std::numeric_limits<float>::quiet_NaN();
				if(value & 0x7){
					//printf("float NaN\n");
				} else {
					// cut off D0 - D17
					value >>= 18;
					// Scale by 0.25 degrees C per bit and return value.
					tempC = value * 0.25;
				}
				//printf("xtempC:%f\n", tempC);

				for(Listener *l : listeners){
					l->update(ss, tempC);
				}

				digitalWrite(ss, HIGH);
			}
                	usleep(1000000);
		}
	}
public:
	Max31855Reader(unsigned int sclk, unsigned int mosi, unsigned int s) : sclk{sclk}, mosi{mosi} {
		add_slave(s);
	}

	void add_slave(unsigned int s){
		slaves.push_back(s);
	}

	void add_listener(Listener *l){
		listeners.push_back(l);
	}

	void startUpdate(){
		for(Listener *l : listeners){
			readAndUpdate();
		}
	}
};

int main(int argc, char *argv[])
{
	Max31855Reader reader{PIN_CLK, PIN_DO, PIN_CS1};
	Listener l;

//	float t = atof(argv[1]);


	//reader.add_slave(PIN_CS2);
	reader.add_listener(&l);
	reader.startUpdate();

	printf("done!\n");
}
