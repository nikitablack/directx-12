#pragma once

#include <cstdint>

class Timer
{
public:
	Timer();

	float time() const;
	float delta() const;

	void reset();
	void start();
	void stop();
	void tick();

private:
	double secondsPerCount{ 0.0 };
	double deltaTime{ -1.0 };

	int64_t baseTime{ 0 };
	int64_t pausedTime{ 0 };
	int64_t stopTime{ 0 };
	int64_t prevTime{ 0 };
	int64_t currTime{ 0 };

	bool stopped{ false };
};