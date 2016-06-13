#include "Timer.h"
#include <chrono>

using namespace std;
using namespace std::chrono;

Timer::Timer()
{
}

float Timer::time() const
{
	return 0.0f;
}

float Timer::delta() const
{
	return 0.0f;
}

void Timer::reset()
{
}

void Timer::start()
{
}

void Timer::stop()
{
}

void Timer::tick()
{
	if (stopped)
	{
		deltaTime = 0;
		return;
	}

	high_resolution_clock::time_point start{ high_resolution_clock::now() };
	
	//currTime = duration_cast<duration<double>>(t2 - t1);

	//deltaTime = start - prevTime
}
