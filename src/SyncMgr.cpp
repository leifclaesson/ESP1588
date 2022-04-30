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
#include "SyncMgr.h"
#include "PTP.h"

#define DIFFHIST_SIZE ((int) (sizeof(diffHistory)/sizeof(diffHistory[0])))

ESP1588_Sync::ESP1588_Sync()
{

}

void ESP1588_Sync::Reset()
{
	bFirst=true;

	bFastInitial=true;

	ulAdjustmentTimestamp=millis();

	for(int i=0;i<DIFFHIST_SIZE;i++)
	{
		diffHistory[i]=-32768;
	}

	diffHistoryIdx=0;

	rejectedPackets=0;
	acceptedPackets=0;

	bLockStatus=false;

}

void ESP1588_Sync::FeedSync(PTP_PACKET & pkt, int port)
{
	uint32_t ulNow=millis();

	uint32_t ulTwoStepOffset=0;

	if(port==319)
	{
		bTwoStep=(pkt.header.flagField[0] & 2)!=0;
	}

	if(bTwoStep)
	{
		/*
		 * Two-step PTP works as follows.
		 * For every sync period, first a sync packet (port 319) is sent with the TwoStep flag set, and timestamp zero.
		 * The sending hardware makes a note of when the packet is actually transmitted.
		 * Then, a follow-up sync packet (port 320) with the same Sequence ID is sent, with the actual timestamp of the previous packet.
		 *
		 * The client (that's us) notes the time when we the first packet is received, and when the follow-up packet arrives, check how much time elapsed and
		 * add that to the timestamp of the second packet, for an more accurate overall timestamp which compensates for the internal delay inside the master clock.
		 *
		 * Of course, since we have not currently implemented PEER DELAY, and we're based on milliseconds, this additional precision is moot -- but we still have to
		 * support two-step or it simply will not work with 2-step master clocks.
		 */

		if(port==319)
		{
			usTwoStepSeqId=pkt.header.sequenceId;
			ulTwoStepReceiveTimestamp=ulNow;
			return;
		}
		else if(port==320)
		{
			if(pkt.header.sequenceId!=usTwoStepSeqId) return;

			int32_t diff=ulNow-ulTwoStepReceiveTimestamp;

			ulTwoStepOffset=diff;
		}
	}


	uint32_t ptpmillis=
			(ntohl(pkt.msg.sync.timestamp_secs)*1000) + (ntohl(pkt.msg.sync.timestamp_nanos)/1000000);

	uint64_t ptpmillis64=
			((((uint64_t) ntohs(pkt.msg.sync.timestamp_secs_ESB)<<32) + ntohl(pkt.msg.sync.timestamp_secs))*1000)
			+ (ntohl(pkt.msg.sync.timestamp_nanos)/1000000);

	ptpmillis+=ulTwoStepOffset;
	ptpmillis64+=ulTwoStepOffset;


	if(bFirst)
	{
		ulOffset=ptpmillis-ulNow;
		ulAdjustmentTimestamp=ulNow;


		//We'll accept the first packet as a baseline to compare against, but we know it might have been one of the very delayed ones,
		//so let's not report that we're locked yet. Set the "initial diff finding" flag, and in a second or so, do one big jump
		//based on the least delayed packet we found.

		bInitialDiffFinding=true;
		ulInitialDiffFindingTimestamp=ulNow;



		if(ptpmillis64>1633942188395LL)	//if it's older than when I wrote this code it can't be valid, time machines notwithstanding.
		{
			bEpochValidInternal=true;
		}
		else
		{
			bEpochValidInternal=false;
		}

#ifdef PTP_SYNCMGR_DEBUG
		csprintf("MILLIS64  %llu\n",ptpmillis64);
#endif

		ulOffset64=ptpmillis64 - (millis()+ulOffset);
	}

	int32_t diff=ptpmillis-ulOffset-ulNow;





	if(diff<-200 || diff>200)	//too far out
	{
		rejectedPackets++;

#ifdef PTP_SYNCMGR_DEBUG
		csprintf("SyncMgr Rejecting diff %d (%u)\n",diff,rejectedPackets);
#endif

		//are we throwing away every packet? Then maybe _we're_ out of sync.

		//If we have four seconds worth of bad packets (but at least 16 packets) with not a single good packet, let's resync.

		int numpkts=16;

		if(pkt.header.logMessageInterval<-2) numpkts=4<<(-pkt.header.logMessageInterval);



		if(rejectedPackets>numpkts)
		{
#ifdef PTP_SYNCMGR_DEBUG
			csprintf("SyncMgr RESETTING\n");
#endif
			Reset();
		}

		return;
	}

	rejectedPackets=0;


	//if(diff<-1000) return;	//that's just too old, we're only interested in the newest packets anyway


	//TL;DR -- For best accuracy we have to look at several packets and act only on the only the ones that incurred the least transmission delay.


	//WiFi access points send broadcast and multicast data in chunks, along with the beacon (the same one that broadcasts the SSID),
	//instead of continuously. The purpose of this is to let mobile device WiFi radios sleep most of the time,
	//only waking up in time for the next broadcast/multicast delivery, in order to save battery.

	//This feature is called DTIM (delivery traffic indication message).

	//WiFi beacons are sent every 102.4 milliseconds, that is, roughly 10 times per second.
	//Broadcasts/multicasts may be sent by every beacon (DTIM 1) but iPhone devices, in order to improve battery life, sleep through two beacons and wake up only
	//every _third_ beacon. So, to accomodate this, the default setting in most access points these days DTIM 3, so that iPhones don't miss these packets.

	//This means the PTP packets will arrive only at these intervals (perhaps three times per second!) no matter when they were sent, causing _extreme_ jitter.

	//Unfortunately, this means most of the packets we get are too old to be useful! We only care about the latest packet, all the ones that were buffered are already too old.
	//We need to filter for the most recent packets and base the rest of our logic on that.
	//Because of this, there is no advantage in setting a high sync packet rate in our PTP clock. 1/8 second (-2) is appropriate, any more is really just wasting data.

	//If you have a separate WiFi network (SSID) for your IoT devices like I do, you can set DTIM to 1 on that network.
	//See the following explanation: https://www.sniffwifi.com/2016/05/go-to-sleep-go-to-sleep-go-to-sleep.html

	//Personally I care much more about ESP8266 and ESP32 than iPhones so if I only had one WiFi network and had to choose compromise settings,
	//I'd still set DTIM to 1 and broadcast right through iPhone's naps. You snooze, you lose. :-)




	diffHistory[diffHistoryIdx]=diff;

	diffHistoryIdx++;
	diffHistoryIdx%=DIFFHIST_SIZE;


	//now find the highest recent diff number, that will be from the most recent sync packet, and essentially eats through most of the jitter.

	//let's look four seconds back, but at least 8 packets, and at most the entire history buffer.

	int numpackets=8;

	if(pkt.header.logMessageInterval<=-2)
	{
		numpackets=4<<(-pkt.header.logMessageInterval);
	}
	if(numpackets>DIFFHIST_SIZE) numpackets=DIFFHIST_SIZE;



	//start at the last value we wrote and work our way backwards. Add DIFFHIST_SIZE to make sure the values stay positive, then MOD with the history buffer size.

	int idx=(diffHistoryIdx+DIFFHIST_SIZE-1) % DIFFHIST_SIZE;

	int16_t peak_diff=-32768;

	for(int i=0;i<numpackets;i++)
	{
		if(peak_diff<diffHistory[idx]) peak_diff=diffHistory[idx];

		idx--;
		if(idx<0) idx=DIFFHIST_SIZE-1;
	}



	//We'll be nudging one millisecond at a time, so the easiest way to control the amount is to choose the interval.
	//If we're within +/- one millisecond we'll do nothing
	//More than that, we'll use progressively larger intervals


	int interval=5000;

	lastDiffMs=peak_diff;


	bool bWasDiffFinding=bInitialDiffFinding;

	if(!bFirst && bInitialDiffFinding && (ulNow-ulInitialDiffFindingTimestamp)>1500)	//make ONE big adjustment to eat right through the jitter
	{
		bInitialDiffFinding=0;

#ifdef PTP_SYNCMGR_DEBUG
		csprintf("SyncMgr Initial diff adjustment: %d\n",peak_diff);
#endif

		ulOffset+=peak_diff;

		for(int i=0;i<DIFFHIST_SIZE;i++)
		{
			if(diffHistory[i]!=-32768)
			{
				diffHistory[i]-=peak_diff;
			}
		}

		peak_diff=0;


	}


#ifdef PTP_SYNCMGR_DEBUG
	char adjust[2]={' ',0};	//a one-char text string that contains space, plus or minus depending on what we're doing
#endif

	if(!bWasDiffFinding)
	{


		if(abs(peak_diff)>=3) interval=2000;
		if(abs(peak_diff)>=10) interval=1000;

		if(bFastInitial)	//if we're far out, adjust more quickly
		{
			if(abs(peak_diff)>=20) interval=250;
			if(abs(peak_diff)>=40) interval=125;
		}



		if(acceptedPackets>=5)
		{

			if(bFastInitial)	//..until we achieve initial "lock"
			{
				if(abs(peak_diff)<10) bFastInitial=false;
			}

			if(!bLockStatus)
			{
				if(abs(peak_diff)<10) bLockStatus=true;
			}
			else
			{
				if(abs(peak_diff)>20) bLockStatus=false;
			}
		}

		if((ulNow-ulAdjustmentTimestamp)>=(uint32_t) interval)
		{
			ulAdjustmentTimestamp=ulNow;

			//nudge one millisecond at a time

			if(peak_diff>1)
			{
				ulOffset++;
	#ifdef PTP_SYNCMGR_DEBUG
				adjust[0]='+';	//advance
	#endif
			}
			else if(peak_diff<-1)
			{
				ulOffset--;
	#ifdef PTP_SYNCMGR_DEBUG
				adjust[0]='-';	//retard
	#endif
			}
		}

		ulConfidentOffset=ulOffset;

		ulConfidentOffset64=ulOffset64;

		bEpochValid=bEpochValidInternal;

	}


	ulLastAcceptedPacket=ulNow;

	if(acceptedPackets<0xFFFF)
	{
		acceptedPackets++;
	}



/*#ifdef PTP_SYNCMGR_DEBUG
	csprintf("SyncMgr %s %s millis: %u  diff: %d  peakdiff=%d\n",GetLockStatus()?"LOCKED":"UNLOCKED",adjust,ptpmillis,diff,peak_diff);
#endif
*/


/*#ifdef PTP_SYNCMGR_DEBUG
	csprintf("SyncMgr %s %s %s millis: %llu  %llu  diff=%lld\n",GetLockStatus()?"LOCKED":"UNLOCKED",adjust,GetEpochValid()?"EPOCH":"     ",ptpmillis64,GetEpochMillis64(),(int64_t) (GetEpochMillis64()-ptpmillis64));
#endif
*/



	bFirst=false;

}

uint64_t ESP1588_Sync::GetEpochMillis64()
{
	return millis()+ulConfidentOffset+ulConfidentOffset64;
}

uint32_t IRAM_ATTR ESP1588_Sync::GetMillis()
{
	uint32_t ret=millis()+ulConfidentOffset;

	int32_t diff=ret-ulLastMillisReturn;

	if(diff<0 && diff>-1000)	//if we're jumping back between 1 and 1000 milliseconds, just return the same value and wait for reality to catch up.
	{
		return ulLastMillisReturn;
	}

	ulLastMillisReturn=ret;

	return ret;
}

bool ESP1588_Sync::GetLockStatus()
{
	return bLockStatus;
}

bool ESP1588_Sync::GetEpochValid()
{
	return bEpochValid;
}

int16_t ESP1588_Sync::GetLastDiffMs()
{
	return lastDiffMs;
}


void ESP1588_Sync::Housekeeping()
{
	if(bLockStatus)
	{
		if(millis()-ulLastAcceptedPacket>5000)
		{
			bLockStatus=false;
		}
	}


#ifdef PTP_SYNCMGR_DEBUG
	if(millis()-ulLastAcceptedPacket>5000)
	{
		csprintf("SyncMgr is not receiving packets.\n");
	}
#endif

}
