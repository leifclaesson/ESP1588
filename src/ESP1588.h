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

#ifndef ESP1588_H_
#define ESP1588_H_

#include <WiFiUDP.h>

#include "Tracker.h"
#include "SyncMgr.h"
#include "SmoothTimeLoop.h"


#ifndef NO_GLOBAL_INSTANCES
#ifndef NO_GLOBAL_ESP1588
extern ESP1588 esp1588;
#endif
#endif



class ESP1588
{
public:
	ESP1588();
	virtual ~ESP1588();

	void SetDomain(uint8_t domain);
	bool Begin();
	void Loop();
	void Quit();

	bool GetLockStatus();			//true if we're locked to a PTP clock
	uint32_t GetMillis();			//returns PTP global epoch-based 32-bit milliseconds value

	bool GetEverLocked();			//true if we're even been locked to a PTP clock

	int16_t GetLastDiffMs();		//returns last difference between our time and the received sync packets


	bool GetEpochValid();			//return true if the epoch is valid, i.e. actual time and date
	uint64_t GetEpochMillis64();	//returns PTP global epoch-based 64-bit millisecond value.
									//This does includes the ESB (extra significant bits) from the sync packet but please note this is MILLISECONDS not nanoseconds.

	ESP1588_Tracker & GetMaster() { return trackerCurMaster; }
	ESP1588_Tracker & GetCandidate() { return trackerCandidate; }

	const String & GetShortStatusString();

	uint16_t GetRawPPS();			//raw packets per second

protected:

	String strShortStatus;


	ESP1588_Tracker trackerCurMaster;
	ESP1588_Tracker trackerCandidate;

	WiFiUDP Udp;
	WiFiUDP Udp2;

	uint32_t ulMaintenance=0;

	void Maintenance();

	ESP1588_Sync syncmgr;

	uint16_t pps_counter=0;
	uint16_t last_pps_count=0;

	uint8_t ucDomain=0;

	bool bInitialized=false;
	bool bEverLocked=false;



};

#endif /* ESP1588_H_ */
