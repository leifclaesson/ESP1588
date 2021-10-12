#if defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266WiFi.h>
#else
//#include <WiFi.h>
#endif

#include <ESP1588.h>


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
 */



#ifndef STASSID
#define STASSID "your-ssid"
#define STAPSK  "your-password"
#endif

const char* ssid     = STASSID;
const char* password = STAPSK;


uint8_t led_pin[]={4,5,12,13,14};  //which pins to use for LEDs. (D2,D1,D6,D7,D5)

//uint8_t led_pin[]={2};  //just the on-board LED

bool invert_led=false;	//invert if active low



#define NUM_LEDS ((int) (sizeof(led_pin)/sizeof(led_pin[0])))

void setup()
{
 Serial.begin(115200);

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

  while (WiFi.status() != WL_CONNECTED) {
	delay(500);
	Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());	




  for(int i=0;i<NUM_LEDS;i++)
  {
	pinMode(led_pin[i],OUTPUT); //set all LED pins as output
  }



  esp1588.SetDomain(0);	//the domain of your PTP clock, 0 - 31
  esp1588.Begin();
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
  

  //turn the LEDs on one by one, once per second
  //by using esp1588.GetMillis() instead of millis(), magically it will be synchronized with other units.

  bool halfcycle=esp1588.GetMillis() % 1000 > 500;
  int current=(esp1588.GetMillis() % (1000 * NUM_LEDS)) / 1000;
  for(int i=0;i<NUM_LEDS;i++)
  {
	digitalWrite(led_pin[i],(i==current && halfcycle)^invert_led);
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

