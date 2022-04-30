#if defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#endif

#include <ESP1588.h>


// These define's must be placed at the beginning before #include "ESP8266TimerInterrupt.h"
// _TIMERINTERRUPT_LOGLEVEL_ from 0 to 4
// Don't define _TIMERINTERRUPT_LOGLEVEL_ > 0. Only for special ISR debugging only. Can hang the system.
#define TIMER_INTERRUPT_DEBUG         0
#define _TIMERINTERRUPT_LOGLEVEL_     0

// Select a Timer Clock
#define USING_TIM_DIV1                false           // for shortest and most accurate timer
#define USING_TIM_DIV16               false           // for medium time and medium accurate timer
#define USING_TIM_DIV256              true            // for longest timer but least accurate. Default

#include "ESP8266TimerInterrupt.h"


/*
 *
 * Leif Claesson 2021-10-11
 *
 * This example shows how to use my ESP1588 library to synchronize to an IEEE 1588 PTP (Precision Time Protocol) master clock.
 * It will flash a number of of LEDs in sequence.
 * The major difference from the classic Blink example is that the blinks will be synchronized with all other units.
 *
 * Imagine a whole row of cars with synchronized turn signals.. :).
 *
 * 2022-04-30 - This version uses ESP8266TimerInterrupt to continuously flash from power-up without hiccups
 *
 * 
 */


#define TIMER_INTERVAL_MS        10

// Init ESP8266 timer 1
ESP8266Timer ITimer;




#ifndef STASSID
#define STASSID "your-ssid"
#define STAPSK  "your-password"
#endif

const char* ssid     = STASSID;
const char* password = STAPSK;


uint8_t led_pin[]={2};  //just the on-board LED

//uint8_t led_pin[]={4,5,12,13,14};  //which pins to use for LEDs. (D2,D1,D6,D7,D5)

//uint8_t led_pin[]={5,4,15,13,12,14,16};  //which pins to use for LEDs.





bool invert_led=false;	//invert if active low


#define NUM_LEDS ((int) (sizeof(led_pin)/sizeof(led_pin[0])))



/*
 * esp1588.GetMillis() returns continuous millis() until PTP lock is achieved, at which point the return value *jumps* to match the PTP epoch.
 *
 * A smooth transition between millis() and epoch is not useful (they could differ by days!) but if we just need a short time loop,
 * for example to drive lights, then a smooth transition is desirable. I've provided a utility class to do just that.
 *
 * SmoothTimeLoop(2000,10);
 * creates a 2 second long time loop (values: 0 - 1999 repeating), and it will speed up or slow down 10% while transitioning to epoch.
 */

SmoothTimeLoop timeloop(1000*NUM_LEDS,20);


void blink();

void IRAM_ATTR TimerHandler()
{
	blink();
}

void setup()
{
 Serial.begin(115200);

 // Interval in microsecs
 if (ITimer.attachInterruptInterval(TIMER_INTERVAL_MS * 1000, TimerHandler))
 {
   Serial.println(F("Starting  ITimer OK"));
 }
 else
   Serial.println(F("Can't set ITimer correctly. Select another freq. or interval"));


  // The WiFi part is borrowed from the ESP8266WiFi example.

  // We start by connecting to a WiFi network

  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  /* Explicitly set the ESP8266 to be a WiFi-client, otherwise, it by default,
	 would try to act as both a client and an access-point and could cause
	 network-issues with your other WiFi-devices on your WiFi-network. */
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);




  for(int i=0;i<NUM_LEDS;i++)
  {
	pinMode(led_pin[i],OUTPUT); //set all LED pins as output
  }



  esp1588.SetDomain(0);	//the domain of your PTP clock, 0 - 31
  esp1588.Begin();


  while (WiFi.status() != WL_CONNECTED) {
	delay(250);
	Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());	



}



void loop()
{

  esp1588.Loop();	//this needs to be called OFTEN, at least several times per second but more is better. forget controlling program flow with delay() in your code.

  
    static uint32_t last_millis=0;

  if(((esp1588.GetMillis()+250) / 4000) != ((last_millis+250) / 4000))	//print a status message every four seconds, slightly out of sync with the LEDs blinking for improved blink accuracy.
  {
	last_millis=esp1588.GetMillis();

	ESP1588_Tracker & m=esp1588.GetMaster();
	ESP1588_Tracker & c=esp1588.GetCandidate();


	Serial.printf("PTP status: %s   Master %s, Candidate %s\n",esp1588.GetLockStatus()?"LOCKED":"UNLOCKED",m.Healthy()?"OK":"no",c.Healthy()?"OK":"no");

	//this function is defined below, prints out the master and candidate clock IDs and some other info.
	PrintPTPInfo(m);
	PrintPTPInfo(c);

	Serial.printf("\n");

  }



}


void IRAM_ATTR blink()
{
	//turn the LEDs on one by one, once per second
	//by using esp1588.GetMillis() instead of millis(), it will magically be synchronized with other units.
	//timeloop provides a smooth transition rather than jumping into sync.

	int cur_interval=timeloop.GetCycleMillis(esp1588.GetMillis(),millis()) / 500;
	int cur_led=cur_interval>>1;
	int halfcycle=cur_interval & 1;

	for(int i=0;i<NUM_LEDS;i++)
	{
		digitalWrite(led_pin[i],(i==cur_led && halfcycle)^invert_led);
	}
}


void PrintPTPInfo(ESP1588_Tracker & t)
{
	const PTP_ANNOUNCE_MESSAGE & msg=t.GetAnnounceMessage();
	const PTP_PORTID & pid=t.GetPortIdentifier();

	Serial.printf("    %s: ID ",t.IsMaster()?"Master   ":"Candidate");
	for(int i=0;i<(int) (sizeof(pid.clockId)/sizeof(pid.clockId[0]));i++)
	{
		Serial.printf("%02x ",pid.clockId[i]);
	}

	Serial.printf(" Prio %3d ",msg.grandmasterPriority1);

	Serial.printf(" %i-step",t.IsTwoStep()?2:1);

	Serial.printf("\n");
}

