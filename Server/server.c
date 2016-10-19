#include <wiringPi.h>
#include <softPwm.h>

//#include <cstring>
#include <iostream>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

//start pwm at zero cycles
#define INITIAL_VALUE 100
#define PWM_RANGE 200

#define INPUT_START -127
#define INPUT_END 127
#define OUTPUT_START (1024)*.2
#define OUTPUT_END (1024)*0.8 //Output range based on PWM duty cycle for 333 Hz (refer to Talon SR datasheet)
#define SLOPE (OUTPUT_END-OUTPUT_START)/(INPUT_END-INPUT_START)

#define BUFLEN 3
//arbitrary, matches client
#define PORT 8888

void kill(char *msg){
	perror(msg);
	exit(1);
}


int mapRange(int input) {
	int p1 = input-INPUT_START;
	int p2 = SLOPE*p1;
	int p3 = OUTPUT_START+p2;
	return p3;	
}

int main(int argc, char** argv){
	int zeroRight;
	int zeroLeft;	
	
	//two arbitrary GPIO pins on raspberrypi
	//software driven PWM so no need for special pin
	const int left_pin = 13;
	const int right_pin = 12;

	//initialize wiringpi and setup pins for pwm output
	wiringPiSetupGpio();
	pinMode(left_pin, PWM_OUTPUT);
	pinMode(right_pin, PWM_OUTPUT);
	
	pwmSetMode(PWM_MODE_MS);
	pwmSetRange(1024); //Input resolution for Talon SR
	
	pwmSetClock((int)(19.2*1000000)/(333*1024)); //freq = 19.2e6/clock/range


	struct sockaddr_in si_me;
	struct sockaddr_in si_other;
	int s;
	int slen = sizeof(si_other);
	char buf[BUFLEN];

	if((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1){
		kill("socket_initialization");
	}
	
	//for testing purposes server has to be killed and reran many times
	//was running into bind issues with addresses already being used
	//this fix could be resolved with client/server handshake but
	//data being transfered is so simple there is no need for handshaking
	int temp = 1;
	//allow bind to address that is already being used (by previous run of server)
	/*if(setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &temp, sizeof(int)) < 0){
	  kill("setSockOpt");
	}*/

	memset((char *) &si_me, 0, sizeof(si_me));
	si_me.sin_family = AF_INET;
	si_me.sin_port = htons(PORT);
	si_me.sin_addr.s_addr = htonl(INADDR_ANY);
	if(bind(s, (struct sockaddr *) &si_me, sizeof(si_me))==-1){
		kill("bind");
	}
	
	printf("Entering reading state\n");
	unsigned failedCount = 0;
	
	bool enabled = false;
	
	//don't reallocate every iteration
	int leftPWM = 0;
	int rightPWM = 0;
	
	char state = 'd';
	//values incoming from -127 to 127
	signed char left = 0;
	signed char right = 0;
	
	//run forever, no quit support yet
	while(1){
		if(recvfrom(s, buf, BUFLEN, 0, (struct sockaddr *) &si_other, (socklen_t *) &slen)==-1){
		  failedCount++;
		  if(failedCount > 10){
		    kill("recvFromFailed");
		  }
		}
		state = buf[0];
		left = buf[1];
		right = buf[2];
		
		//initial byte represents robot state
		if(state == 'e' || state == 'm'){
		  enabled = true;
		}else if(state == 'd'){
		  enabled = false;
		}
		
		leftPWM = mapRange(left);
		rightPWM = mapRange(right);		

		//debug printing
		printf("state %c left %d right %d leftPWM %d rightPWM %d\n",state, left, right, leftPWM, rightPWM);
		
		//finally write out the pwm signal from second and third bytes in packet
		/*if(enabled){
		  softPwmWrite(left_pin, leftPWM);
		  softPwmWrite(right_pin, rightPWM);
		  printf("left %d, right %d\n", leftPWM, rightPWM);
		}else{
		  softPwmWrite(left_pin, INITIAL_VALUE);
		  softPwmWrite(right_pin, INITIAL_VALUE);
		}*/
		pwmWrite(left_pin, leftPWM);	
		pwmWrite(right_pin, rightPWM);	
	}

	return 0;
}
