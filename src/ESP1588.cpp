/*
	This file is part of the ESP1588 library.

	Copyright 2021 Leif Claesson - https://github.com/leifclaesson
	Created on: 9 Oct 2021

	ESP1588 is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	ESP1588 is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with ESP1588.  If not, see <https://www.gnu.org/licenses/>.
*/

#if defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#endif
#include "ESP1588.h"

#ifndef NO_GLOBAL_INSTANCES
#ifndef NO_GLOBAL_ESP1588
ESP1588 esp1588;
#endif
#endif

ESP1588::ESP1588()
{
	trackerCurMaster.bIsMaster=true;
	strShortStatus.reserve(16);
}

ESP1588::~ESP1588()
{
}

void ESP1588::SetDomain(uint8_t domain)
{
	ucDomain=domain;
}

bool ESP1588::Begin()
{

	syncmgr.Reset();

	IPAddress ipMulticast=IPAddress(224,0,1,129);

#if defined(ARDUINO_ARCH_ESP8266)
	if(Udp.beginMulticast(WiFi.localIP(), ipMulticast, 319) && Udp2.beginMulticast(WiFi.localIP(), ipMulticast, 320))
#else
	if(Udp.beginMulticast(ipMulticast, 319) && Udp2.beginMulticast(ipMulticast, 320))
#endif
	{
#ifdef PTP_MAIN_DEBUG
		Serial.printf("Joined multicast group %s\n",ipMulticast.toString().c_str() );
#endif
		return true;
	}
	else
	{
		Udp.stop();
		Udp2.stop();
#ifdef PTP_MAIN_DEBUG
		Serial.printf("### multicast join failed\n");
#endif
		return false;
	}


}

static char packetBuffer[256];

void ESP1588::Loop()
{

	for(int i=0;i<2;i++)
	{
		WiFiUDP * udp=i==1?&Udp2:&Udp;
		int port=i==1?320:319;

		udp->parsePacket();

		int len=udp->read(packetBuffer,sizeof(packetBuffer));

		if(len>=(int) sizeof(PTP_PACKET))
		{
			PTP_PACKET & pkt=*((PTP_PACKET *) packetBuffer);

			pps_counter++;

			if(pkt.header.domainNumber!=ucDomain) return;

			if(port==320 && (pkt.header.txSpecificMsgType & 0xF)==0xb && len==(int) sizeof(PTP_ANNOUNCE_PACKET))
			{
				PTP_ANNOUNCE_PACKET & pkt=*((PTP_ANNOUNCE_PACKET *) packetBuffer);

				// this code forms part of the BMCA (Best Master Clock Algorithm) as defined in IEEE Standard 1588-2008

				if(!trackerCurMaster.HasValidSource())	//if we don't have any current master, take it!
				{
					trackerCurMaster.Start(pkt);
				}
				else if(pkt.header.sourcePortId==trackerCurMaster.id)	//is this our current master?
				{
					trackerCurMaster.FeedAnnounce(pkt);
				}
				else if(pkt.header.sourcePortId==trackerCandidate.id)	//is this the candidate we're tracking
				{
					trackerCandidate.FeedAnnounce(pkt);

					if((trackerCandidate.Healthy() && trackerCandidate.msgAnnounce>trackerCurMaster.msgAnnounce) ||
						(!trackerCurMaster.Healthy() && trackerCandidate.Healthy()))
					{

						//if the candidate we're tracking is healthy (has announce messages and sync messages) and
						// is better than our current master, take it!
						//Also, if the candidate is healthy and the current master is not, take it!
						trackerCurMaster.Take(trackerCandidate);
					}

				}
				else if(trackerCandidate.msgAnnounce<pkt.announce)	//is this better than the candidate we're tracking?
				{
					//start tracking this new candidate
					trackerCandidate.Start(pkt);
				}
			}
			else if(((pkt.header.txSpecificMsgType & 0xF)==0x0 || (pkt.header.txSpecificMsgType & 0xF)==0x8)
					&& len==(int) sizeof(PTP_PACKET))
			{
				if(pkt.header.sourcePortId==trackerCurMaster.id)	//is this sync packet from our current master?
				{
					trackerCurMaster.FeedSync(pkt,port);
					syncmgr.FeedSync(pkt,port);
				}
				else if(pkt.header.sourcePortId==trackerCandidate.id)	//is this sync packet from our current candidate?
				{
					trackerCandidate.FeedSync(pkt,port);

				}
		//		csprintf("SYNC ");
			}

		}
	}



	if(millis()-ulMaintenance>=1000)
	{
		ulMaintenance=millis();
		Maintenance();
	}


}

void ESP1588::Maintenance()
{
	last_pps_count=pps_counter;
	pps_counter=0;

	trackerCurMaster.Housekeeping();
	trackerCandidate.Housekeeping();

	syncmgr.Housekeeping();

}


void ESP1588::Quit()
{
	Udp.stop();
	Udp2.stop();
	trackerCurMaster.Reset();
	trackerCandidate.Reset();
	syncmgr.Reset();
}

uint32_t IRAM_ATTR ESP1588::GetMillis()
{
	return syncmgr.GetMillis();
}

bool ESP1588::GetLockStatus()
{
	return syncmgr.GetLockStatus();
}

bool ESP1588::GetEverLocked()
{
	if(bEverLocked) return true;
	if(GetLockStatus())
	{
		bEverLocked=true;
		return true;
	}
	return false;
}

bool ESP1588::GetEpochValid()
{
	return syncmgr.GetEpochValid();
}

uint64_t ESP1588::GetEpochMillis64()
{
	return syncmgr.GetEpochMillis64();
}

int16_t ESP1588::GetLastDiffMs()
{
	return syncmgr.GetLastDiffMs();
}

const String & ESP1588::GetShortStatusString()
{
	if(GetLockStatus())
	{
		strShortStatus="OK (";
		strShortStatus+=String(GetLastDiffMs());
		strShortStatus+="ms)";
	}
	else
	{
		if(GetEpochValid())
		{
			strShortStatus="not OK";
		}
		else
		{
			strShortStatus="NOT OK";
		}
	}
	return strShortStatus;
}

uint16_t ESP1588::GetRawPPS()			//raw packets per second
{
	return last_pps_count;
}
