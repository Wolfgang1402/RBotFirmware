// RBotFirmware
// Rob Dobson 2016

#pragma once

#include "AxisValues.h"

class MotionPipelineElem
{
public:
	AxisFloats _pt1MM;
	AxisFloats _pt2MM;
	float _accMMpss;
	float _speedMMps;
	AxisFloats _destActuatorCoords;
	AxisBools _stepDirn;
	AxisU32s _absSteps;
	uint32_t _axisMaxSteps;
	bool _primaryAxisMove;
	float _nominalSpeedMMps;
	float _nominalStepRatePerSec;
	float _maxStepRatePerSec;
	AxisFloats _unitVec;
	float _moveDistMM;
	float _maxEntrySpeedMMps;
	float _entrySpeedMMps;
	bool _nominalLengthFlag;
	bool _recalcFlag;
	bool _isRunning;
	float _exitSpeedMMps;
	uint32_t _accelUntil;
	uint32_t _decelAfter;
	uint32_t _totalMoveTicks;
	float _accelPerTick;
	float _decelPerTick;
	float _initialStepRate;

public:
	MotionPipelineElem()
	{
		_accMMpss = 0;
		_speedMMps = 0;
		_axisMaxSteps = 0;
		_primaryAxisMove = false;
		_nominalSpeedMMps = 0;
		_nominalStepRatePerSec = 0;
		_maxStepRatePerSec = 0;
		_moveDistMM = 0;
		_maxEntrySpeedMMps = 0;
		_entrySpeedMMps = 0;
		_exitSpeedMMps = 0;
		_isRunning = false;
		_nominalLengthFlag = false;
		_recalcFlag = false;
		_accelUntil = 0;
		_decelAfter = 0;
		_totalMoveTicks = 0;
		_accelPerTick = 0;
		_decelPerTick = 0;
		_initialStepRate = 0;
	}

	MotionPipelineElem(AxisFloats& pt1, AxisFloats& pt2)
	{
		_pt1MM = pt1;
		_pt2MM = pt2;
		_accMMpss = 0;
		_speedMMps = 0;
		_axisMaxSteps = 0;
		_primaryAxisMove = false;
		_nominalSpeedMMps = 0;
		_nominalStepRatePerSec = 0;
		_maxStepRatePerSec = 0;
		_moveDistMM = 0;
		_maxEntrySpeedMMps = 0;
		_entrySpeedMMps = 0;
		_exitSpeedMMps = 0;
		_isRunning = false;
		_nominalLengthFlag = false;
		_recalcFlag = false;
		_accelUntil = 0;
		_decelAfter = 0;
		_totalMoveTicks = 0;
		_accelPerTick = 0;
		_decelPerTick = 0;
		_initialStepRate = 0;
	}

	double delta()
	{
		return _pt1MM.distanceTo(_pt2MM);
	}

	void calcMaxSpeedReverse(float exitSpeed)
	{
		// If entry speed is already at the maximum entry speed, no need to recheck. Block is cruising.
		// If not, block in state of acceleration or deceleration. Reset entry speed to maximum and
		// check for maximum allowable speed reductions to ensure maximum possible planned speed.
		if (_entrySpeedMMps != _maxEntrySpeedMMps) 
		{
			// If nominal length true, max junction speed is guaranteed to be reached. Only compute
			// for max allowable speed if block is decelerating and nominal length is false.
			if ((!_nominalLengthFlag) && (_maxEntrySpeedMMps > exitSpeed)) 
			{
				float maxEntrySpeed = maxAllowableSpeed(-_accMMpss, exitSpeed, this->_moveDistMM);
				_entrySpeedMMps = std::min(maxEntrySpeed, _maxEntrySpeedMMps);
			}
			else
			{
				_entrySpeedMMps = _maxEntrySpeedMMps;
			}
		}
	}

	void calcMaxSpeedForward(float prevMaxExitSpeed)
	{
		// If the previous block is an acceleration block, but it is not long enough to complete the
		// full speed change within the block, we need to adjust the entry speed accordingly. Entry
		// speeds have already been reset, maximized, and reverse planned by reverse planner.
		// If nominal length is true, max junction speed is guaranteed to be reached. No need to recheck.
		if (prevMaxExitSpeed > _nominalSpeedMMps)
			prevMaxExitSpeed = _nominalSpeedMMps;
		if (prevMaxExitSpeed > _maxEntrySpeedMMps)
			prevMaxExitSpeed = _maxEntrySpeedMMps;
		if (prevMaxExitSpeed <= _entrySpeedMMps)
		{
			// We're acceleration limited
			_entrySpeedMMps = prevMaxExitSpeed;
			// since we're now acceleration or cruise limited
			// we don't need to recalculate our entry speed anymore
			//_recalcFlag = false;
		}
		// Now max out the exit speed
		maximizeExitSpeed();
	}

	float maxAllowableSpeed(float acceleration, float target_velocity, float distance)
	{
		return sqrtf(target_velocity * target_velocity - 2.0F * acceleration * distance);
	}

	void maximizeExitSpeed()
	{
		// If block is being executed then don't change
		if (_isRunning)
			return;

		// If nominalLengthFlag then guaranteed to reach nominal speed regardless of entry
		// speed so just use nominal speed
		if (_nominalLengthFlag)
			_exitSpeedMMps = std::min(_nominalSpeedMMps, _exitSpeedMMps);

		// Otherwise work out max exit speed based on entry and acceleration
		float maxExitSpeed = maxAllowableSpeed(-_accMMpss, _entrySpeedMMps, _moveDistMM);
		_exitSpeedMMps = std::min(maxExitSpeed, _exitSpeedMMps);
	}

	// Calculate trapezoid parameters so that the entry- and exit-speed is compensated by the provided factors.
	// The factors represent a factor of braking and must be in the range 0.0-1.0.
	//                                +--------+ <- nominal_rate
	//                               /          \
	// nominal_rate*entry_factor -> +            \
	//                              |             + <- nominal_rate*exit_factor
	//                              +-------------+
	//                                  time -->
	void calculateTrapezoid()
	{
		// If block is currently being processed don't change it
		if (_isRunning)
			return;

		// Initial rate
		float initialStepRate = _nominalStepRatePerSec * (_entrySpeedMMps / _nominalSpeedMMps);
		float finalStepRate = _nominalStepRatePerSec * (_exitSpeedMMps / _nominalSpeedMMps);

		Log.trace("trapezoid initRate %0.3f steps/s, finalRate %0.3f steps/s", initialStepRate, finalStepRate);

		// How many steps ( can be fractions of steps, we need very precise values ) to accelerate and decelerate
		// This is a simplification to get rid of rate_delta and get the steps/s� accel directly from the mm/s� accel
		float accInStepUnits = (_accMMpss * _axisMaxSteps) / _moveDistMM;
		float maxPossStepRate = sqrtf((_axisMaxSteps * accInStepUnits) + ((powf(initialStepRate, 2) + powf(finalStepRate, 2)) / 2.0F));

		Log.trace("trapezoid accInStepUnits %0.3f steps/s2, maxPossStepRate %0.3f steps/s", accInStepUnits, maxPossStepRate);

		// Now this is the maximum rate we'll achieve this move, either because
		// it's the higher we can achieve, or because it's the higher we are
		// allowed to achieve
		_maxStepRatePerSec = std::min(maxPossStepRate, _nominalStepRatePerSec);

		// Now figure out how long it takes to accelerate in seconds
		float timeToAccelerateSecs = (_maxStepRatePerSec - initialStepRate) / accInStepUnits;

		// Now figure out how long it takes to decelerate
		float timeToDecelerateSecs = (finalStepRate - _maxStepRatePerSec) / -accInStepUnits;

		Log.trace("trapezoid maxStepRatePerSec %0.3f steps/s, timeToAccelerate %0.3f s, timeToDecelerate %0.3f s", _maxStepRatePerSec, timeToAccelerateSecs, timeToDecelerateSecs);

		// Now we know how long it takes to accelerate and decelerate, but we must
		// also know how long the entire move takes so we can figure out how long
		// is the plateau if there is one
		float plateauTimeSecs = 0;

		// Only if there is actually a plateau ( we are limited by nominal_rate )
		if (maxPossStepRate > _nominalStepRatePerSec)
		{
			// Figure out the acceleration and deceleration distances ( in steps )
			float accelDistance = ((initialStepRate + _maxStepRatePerSec) / 2.0F) * timeToAccelerateSecs;
			float decelDistance = ((_maxStepRatePerSec + finalStepRate) / 2.0F) * timeToDecelerateSecs;

			// Figure out the plateau steps
			float plateauDist = _axisMaxSteps - accelDistance - decelDistance;

			// Figure out the plateau time in seconds
			plateauTimeSecs = plateauDist / _maxStepRatePerSec;
		}

		// Figure out how long the move takes total ( in seconds )
		float totalMoveTimeSecs = timeToAccelerateSecs + timeToDecelerateSecs + plateauTimeSecs;
		//puts "total move time: #{total_move_time}s time to accelerate: #{time_to_accelerate}, time to decelerate: #{time_to_decelerate}"

		Log.trace("plateauTime %0.3f s, totalMoveTime %0.3f s", plateauTimeSecs, totalMoveTimeSecs);

		// We now have the full timing for acceleration, plateau and deceleration,
		// yay \o/ Now this is very important these are in seconds, and we need to
		// round them into ticks. This means instead of accelerating in 100.23
		// ticks we'll accelerate in 100 ticks. Which means to reach the exact
		// speed we want to reach, we must figure out a new/slightly different
		// acceleration/deceleration to be sure we accelerate and decelerate at
		// the exact rate we want

		// First off round total time, acceleration time and deceleration time in ticks
		uint32_t accelTicks = uint32_t(floorf(timeToAccelerateSecs * STEP_TICKER_FREQUENCY));
		uint32_t decelTicks = uint32_t(floorf(timeToDecelerateSecs * STEP_TICKER_FREQUENCY));
		uint32_t totalTicks = uint32_t(floorf(totalMoveTimeSecs    * STEP_TICKER_FREQUENCY));

		// Now deduce the plateau time for those new values expressed in tick
		//uint32_t plateau_ticks = total_move_ticks - acceleration_ticks - deceleration_ticks;

		// Now we figure out the acceleration value to reach EXACTLY maximum_rate(steps/s) in EXACTLY acceleration_ticks(ticks) amount of time in seconds
		// This can be moved into the operation below, separated for clarity, note we need to do this instead of using time_to_accelerate(seconds)
		// directly because time_to_accelerate(seconds) and acceleration_ticks(seconds) do not have the same value anymore due to the rounding
		float acceleration_time = 1.0f * accelTicks / STEP_TICKER_FREQUENCY;  
		float deceleration_time = 1.0f * decelTicks / STEP_TICKER_FREQUENCY;

		float acceleration_in_steps = (timeToAccelerateSecs > 0.0F) ? (_maxStepRatePerSec - initialStepRate) / timeToAccelerateSecs : 0;
		float deceleration_in_steps = (timeToDecelerateSecs > 0.0F) ? (_maxStepRatePerSec - finalStepRate) / timeToDecelerateSecs: 0;

		// we have a potential race condition here as we could get interrupted anywhere in the middle of this call, we need to lock
		// the updates to the blocks to get around it
		//this->locked = true;
		// Now figure out the two acceleration ramp change events in ticks
		this->_accelUntil = accelTicks;
		this->_decelAfter = totalTicks - decelTicks;

		// Now figure out the acceleration PER TICK, this should ideally be held as a float, even a double if possible as it's very critical to the block timing
		// steps/tick^2

		this->_accelPerTick = acceleration_in_steps / STEP_TICKER_FREQUENCY_2;
		this->_decelPerTick = deceleration_in_steps / STEP_TICKER_FREQUENCY_2;

		// We now have everything we need for this block to call a Steppermotor->move method !!!!
		// Theorically, if accel is done per tick, the speed curve should be perfect.
		_totalMoveTicks = totalTicks;

		//puts "accelerate_until: #{this->accelerate_until}, decelerate_after: #{this->decelerate_after}, acceleration_per_tick: #{this->acceleration_per_tick}, total_move_ticks: #{this->total_move_ticks}"

		this->_initialStepRate = initialStepRate;

		// prepare the block for stepticker
		//this->prepare();
		//this->locked = false;


	}



};