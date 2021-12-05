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

class ESP1588_Sync
{
private:
	friend class ESP1588;

	ESP1588_Sync();

	void Reset();

	void FeedSync(PTP_PACKET & pkt, int port);

	bool GetLockStatus();
	bool GetEpochValid();

	void Housekeeping();

	int16_t GetLastDiffMs();

	uint32_t GetMillis();
	uint64_t GetEpochMillis64();

	uint32_t ulLastMillisReturn=0;

	bool bLockStatus=false;

	bool bFirst=false;

	bool bFastInitial=false;

	uint32_t ulOffset=0;

	uint64_t ulOffset64=0;


	uint32_t ulConfidentOffset=0;

	uint64_t ulConfidentOffset64=0;


	bool bEpochValid=false;


	int16_t lastDiffMs=0;

	int16_t diffHistory[64];
	uint16_t diffHistoryIdx=0;

	uint32_t ulAdjustmentTimestamp=0;

	uint32_t ulLastAcceptedPacket=0;

	int16_t rejectedPackets;

	uint16_t acceptedPackets;

	bool bTwoStep=false;

	uint32_t ulTwoStepReceiveTimestamp=0;
	uint16_t usTwoStepSeqId=0;

	bool bInitialDiffFinding=false;
	uint32_t ulInitialDiffFindingTimestamp=0;

	bool bEpochValidInternal=false;


};

