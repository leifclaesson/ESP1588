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

#include "PTP.h"

int BMCA_compare(const PTP_ANNOUNCE_MESSAGE & a,const PTP_ANNOUNCE_MESSAGE & b);

#ifdef PTP_TRACKER_DEBUG
void PrintPortID(PTP_PORTID & id);
#endif

class ESP1588;

class ESP1588_Tracker
{
public:
	bool IsMaster() { return bIsMaster; };

	bool Healthy();
	const PTP_PORTID GetPortIdentifier() { return id; }
	const PTP_ANNOUNCE_MESSAGE GetAnnounceMessage() { return msgAnnounce; }

	bool IsTwoStep() { return bTwoStep; }
	int8_t GetLogAnnounceInternal() { return logAnnounceInterval; }
	int8_t GetLogSyncInternal() { return logSyncInterval; }


private:
	friend class ESP1588;

	ESP1588_Tracker();
	~ESP1588_Tracker();

	bool HasValidSource();

	void Take(ESP1588_Tracker & candidate);

	bool bIsMaster=false;

	PTP_PORTID id;
	PTP_ANNOUNCE_MESSAGE msgAnnounce;

	void Reset();

	void Start(PTP_ANNOUNCE_PACKET & pkt);

	void FeedAnnounce(PTP_ANNOUNCE_PACKET & pkt);
	void FeedSync(PTP_PACKET & pkt, int port);

	void Housekeeping();


	int8_t logSyncInterval;
	int8_t logAnnounceInterval;


	uint8_t announceCount;
	uint8_t syncCount;
	uint8_t syncCount2;

	bool MaintenanceInterval(uint8_t & counter, int8_t logMsgInterval);


	uint8_t maintenanceCounterAnnounce;
	uint8_t maintenanceCounterSync;

	bool bHealthy;

	bool bTwoStep=false;

	void CheckHealth();

#ifdef PTP_TRACKER_DEBUG
	void debug_id();
#endif


};

