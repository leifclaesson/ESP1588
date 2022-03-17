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

#pragma once

#define PACKED
#pragma pack(push,1)

//#define PTP_SYNCMGR_DEBUG
//#define PTP_TRACKER_DEBUG
//#define PTP_MAIN_DEBUG

#include <WiFiUDP.h>

#if defined(ARDUINO_ARCH_ESP32)
#include <lwip/def.h>
#endif

#if defined(PTP_SYNCMGR_DEBUG) | defined(PTP_TRACKER_DEBUG)
//#include <LeifESPBase.h>
#define csprintf Serial.printf
#endif


typedef uint8_t		PTP_CLOCKID[8];

struct PTP_PORTID
{
	PTP_CLOCKID		clockId;
	uint16_t		portNumber;
	bool operator== (const PTP_PORTID & other) const
	{
		return memcmp(this,&other,sizeof(*this))==0;
	};
	bool operator!= (const PTP_PORTID & other) const
	{
		return memcmp(this,&other,sizeof(*this))!=0;
	};
	bool operator < (const PTP_PORTID & other) const
	{
		return memcmp(this,&other,sizeof(*this)) < 0;
	};
};

struct PTP_HEADER
{
	uint8_t         txSpecificMsgType;
	uint8_t         versionPTP;
	uint16_t        msgLen;
	uint8_t         domainNumber;
	uint8_t         reserved0;
	uint8_t         flagField[2];
	int64_t         correctionField;
	uint32_t        reserved1;
	PTP_PORTID	    sourcePortId;
	uint16_t        sequenceId;
	uint8_t         controlField;
	int8_t          logMessageInterval;

};

struct PTP_SYNC_MESSAGE
{
	uint16_t		timestamp_secs_ESB;	//extra significant bits
	uint32_t		timestamp_secs;
	uint32_t		timestamp_nanos;
};

struct PTP_FOLLOWUP_MESSAGE
{
	uint16_t        arrivalTimestamp_secs_ESB;	//extra significant bits
	uint32_t        arrivalTimestamp_secs;
	uint32_t        arrivalTimestamp_nanos;
};

struct PTP_PACKET
{
	PTP_HEADER header;
	union {
		PTP_SYNC_MESSAGE     sync;
		PTP_FOLLOWUP_MESSAGE followUp;
	} msg;
};

struct PTP_CLOCK_QUALITY
{
	uint8_t clockClass;
	uint8_t clockAccuracy;
	uint16_t offsetScaledLogVariance;
};


struct PTP_ANNOUNCE_MESSAGE
{
	PTP_SYNC_MESSAGE originTimestamp;			// 10 octets
	uint16_t currentUtcOffset;					// 2 octets
	uint8_t reserved1;							// 1 octet
	uint8_t grandmasterPriority1;				// 1 octet
	PTP_CLOCK_QUALITY grandmasterClockQuality;	// 4 octets
	uint8_t grandmasterPriority2;				// 1 octet
	PTP_CLOCKID grandmasterIdentity;			// 8 octets
	uint16_t stepsRemoved;						// 2 octets
	uint8_t	timeSource;							// 1 octet

	int bmca_compare(const PTP_ANNOUNCE_MESSAGE & a, const PTP_ANNOUNCE_MESSAGE & b) const
	{
		//return 1 if A better than B, -1 if B better than A, 0 if equal

		if(a.grandmasterPriority1 != b.grandmasterPriority1)
		{
			//uint8_t
			return a.grandmasterPriority1<b.grandmasterPriority1?1:-1;
		}

		if(a.grandmasterClockQuality.clockClass != b.grandmasterClockQuality.clockClass)
		{
			//uint8_t
			return a.grandmasterClockQuality.clockClass < b.grandmasterClockQuality.clockClass?1:-1;
		}

		if(a.grandmasterClockQuality.clockAccuracy != b.grandmasterClockQuality.clockAccuracy)
		{
			//uint8_t
			return a.grandmasterClockQuality.clockAccuracy < b.grandmasterClockQuality.clockAccuracy?1:-1;
		}

		if(a.grandmasterClockQuality.offsetScaledLogVariance != b.grandmasterClockQuality.offsetScaledLogVariance)
		{
			//uint16_t
			return (ntohs(a.grandmasterClockQuality.offsetScaledLogVariance) < ntohs(b.grandmasterClockQuality.offsetScaledLogVariance))?1:-1;
		}

		if(a.grandmasterPriority2 != b.grandmasterPriority2)
		{
			//uint8_t
			return a.grandmasterPriority2<b.grandmasterPriority2?1:-1;
		}

		return memcmp(a.grandmasterIdentity,b.grandmasterIdentity,sizeof(a.grandmasterIdentity));
	};

	bool operator== (const PTP_ANNOUNCE_MESSAGE & other) const
	{
		return this->bmca_compare(*this, other)==0;
	};
	bool operator!= (const PTP_ANNOUNCE_MESSAGE & other) const
	{
		return this->bmca_compare(*this, other)!=0;
	};
	bool operator < (const PTP_ANNOUNCE_MESSAGE & other) const
	{
		return this->bmca_compare(*this, other) < 0;
	};
	bool operator > (const PTP_ANNOUNCE_MESSAGE & other) const
	{
		return this->bmca_compare(*this, other) > 0;
	};

};

struct PTP_ANNOUNCE_PACKET
{
	PTP_HEADER header;
	PTP_ANNOUNCE_MESSAGE announce;
};


#pragma pack(pop)
#undef PACKED

