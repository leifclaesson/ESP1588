/*
 * SmoothTimeLoop.cpp
 *
 *  Created on: 17 Mar 2022
 *      Author: user
 */

#include "SmoothTimeLoop.h"

#define abs(a) ((a)<0?-(a):(a))


int32_t IRAM_ATTR WrapAround(int32_t input, int32_t wrap)
{

	if(input>=wrap)
	{
		input=-(wrap<<1)+input;	//two's complement wrap
	}
	else if(input<-wrap)
	{
		input+=(wrap<<1);	//two's complement wrap

	}

	return input;

}

SmoothTimeLoop::SmoothTimeLoop(int cycle_millis, int max_percent_adjustment)
{
	this->cycle_millis=cycle_millis;
	this->max_percent_adjustment=max_percent_adjustment;

	offset_millis=0;
}


uint32_t IRAM_ATTR SmoothTimeLoop::GetCycleMillis(uint32_t esp1588_millis, uint32_t system_millis)
{
	int32_t millis_since_last=system_millis-last_system_millis;
	
	uint32_t s=(system_millis+offset_millis) % cycle_millis;
	uint32_t t=esp1588_millis % cycle_millis;

	int32_t diff=WrapAround(t-s,(cycle_millis>>1));

	if(abs(diff)>1)
	{
		int needed_correction=diff;

		int max_corr=(millis_since_last * max_percent_adjustment)/100;

		if(needed_correction>max_corr) needed_correction=max_corr;
		else if(needed_correction<-max_corr) needed_correction=-max_corr;
	
		offset_millis += needed_correction;
		int bp=1;

	}

	last_system_millis=system_millis;

	return s;
}

