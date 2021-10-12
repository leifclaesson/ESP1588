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

#include <Arduino.h>
#include <WiFiUDP.h>
#include "Tracker.h"

#ifdef PTP_TRACKER_DEBUG
void PrintPortID(PTP_PORTID & id)
{
	csprintf("%04x/",id.portNumber);
	for(int i=0;i<8;i++)
	{
		csprintf("%02x%s",id.clockId[i],i<7?":":"");
	}
}
#endif

ESP1588_Tracker::ESP1588_Tracker()
{
	Reset();
}

ESP1588_Tracker::~ESP1588_Tracker()
{

}

void ESP1588_Tracker::Take(ESP1588_Tracker & candidate)
{
	id=candidate.id;
	msgAnnounce=candidate.msgAnnounce;
	logSyncInterval=candidate.logSyncInterval;
	logAnnounceInterval=candidate.logAnnounceInterval;
	syncCount=candidate.syncCount;
	syncCount2=candidate.syncCount2;
	announceCount=candidate.announceCount;
	maintenanceCounterSync=candidate.maintenanceCounterSync;
	maintenanceCounterAnnounce=candidate.maintenanceCounterAnnounce;
	bHealthy=candidate.bHealthy;

	candidate.Reset();
}

void ESP1588_Tracker::Reset()
{
	memset(&id,0,sizeof(id));
	memset(&msgAnnounce,0xFF,sizeof(msgAnnounce));

	logSyncInterval=0x7F;
	logAnnounceInterval=0x7F;
	syncCount=0;
	syncCount2=0;
	announceCount=0;
	maintenanceCounterSync=0;
	maintenanceCounterAnnounce=0;
	bHealthy=false;
	bTwoStep=false;

}

bool ESP1588_Tracker::HasValidSource()
{
	return logAnnounceInterval!=0x7f;
}

void ESP1588_Tracker::Start(PTP_ANNOUNCE_PACKET & pkt)
{
	Reset();
	id=pkt.header.sourcePortId;

	FeedAnnounce(pkt);
}

void ESP1588_Tracker::FeedAnnounce(PTP_ANNOUNCE_PACKET & pkt)
{
	logAnnounceInterval=pkt.header.logMessageInterval;
	msgAnnounce=pkt.announce;

	if(announceCount<5)
	{
		announceCount++;
	}

	CheckHealth();

}

void ESP1588_Tracker::FeedSync(PTP_PACKET & pkt, int port)
{

	logSyncInterval=pkt.header.logMessageInterval;

	switch(port)
	{
	case 319:
		bTwoStep=(pkt.header.flagField[0] & 2)!=0;
		if(syncCount<10) syncCount++;
		break;
	case 320:
		if(syncCount2<10) syncCount2++;
		break;
	}

	CheckHealth();

}




bool ESP1588_Tracker::MaintenanceInterval(uint8_t & counter, int8_t logMsgInterval)
{
	int interval=logMsgInterval+2;
	if(interval<0) interval=0;

	counter++;
	if(counter>=1<<interval)
	{
		counter=0;

		return true;
	}
	else return false;
}


void ESP1588_Tracker::CheckHealth()
{
	if(!syncCount || (bTwoStep && !syncCount2))
	{
		bHealthy=false;
	}
	else
	{
		if(announceCount>3 && (syncCount>6 && (!bTwoStep || syncCount2>6)))
		{
			bHealthy=true;
		}
	}

}

bool ESP1588_Tracker::Healthy()
{
	return bHealthy && HasValidSource();
}

void ESP1588_Tracker::Housekeeping()
{
	//runs every second
	if(!HasValidSource()) return;

	//decrement announceCount slowly enough that it builds up while we're still getting announcement packets
	//..but it will eventually deplete if they stop coming or if we miss too many

	if(MaintenanceInterval(maintenanceCounterAnnounce,logAnnounceInterval))
	{
		if(announceCount>0) announceCount--;
	}

	if(MaintenanceInterval(maintenanceCounterSync,logSyncInterval))
	{
		if(syncCount>0) syncCount--;
		if(syncCount2>0) syncCount2--;
	}

	CheckHealth();


#ifdef PTP_TRACKER_DEBUG
	debug_id();
	PrintPortID(id);
	csprintf(" healthy=%i, announce: %i, sync: %i, sync2: %i\n",Healthy(),announceCount,syncCount,syncCount2);
#endif



}


#ifdef PTP_TRACKER_DEBUG
void ESP1588_Tracker::debug_id()
{
	csprintf("%s ",bIsMaster?"Master":"Candidate");
}
#endif

