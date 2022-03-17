/*
 * SmoothTimeLoop.h
 *
 *  Created on: 17 Mar 2022
 *      Author: user
 */

#ifndef LIBRARIES_ESP1588_SMOOTHTIMELOOP_H_
#define LIBRARIES_ESP1588_SMOOTHTIMELOOP_H_

#ifdef _WIN32
#include <stdint.h>
#else
#include <Arduino.h>
#endif

class SmoothTimeLoop
{
public:
	SmoothTimeLoop(int cycle_millis, int max_percent_adjustment);

	uint32_t GetCycleMillis(uint32_t esp1588_millis, uint32_t system_millis);

private:

	int cycle_millis;
	int max_percent_adjustment;

	uint32_t last_system_millis;

	int offset_millis;

};

#endif /* LIBRARIES_ESP1588_SMOOTHTIMELOOP_H_ */

