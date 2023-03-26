#ifndef NNX_PIT_HEADER
#define NNX_PIT_HEADER

#ifdef __cplusplus
extern "C"
{
#endif

#include <nnxtype.h>

#define PIT_FREQUENCY 1193182

	NTHALAPI
	VOID
	NTAPI
	PitUniprocessorPollSleepMs(
		UINT16 msDelay);

	NTHALAPI
	VOID
	NTAPI
	PitUniprocessorInitialize(
		VOID);

	NTHALAPI
	VOID
	NTAPI
	PitUniprocessorPollSleepTicks(
		UINT16 tickDelay);

	NTHALAPI
	VOID
	NTAPI
	PitUniprocessorSetupCalibrationSleep(
		VOID);

	NTHALAPI 
	VOID 
	NTAPI 
	PitUniprocessorStartCalibrationSleep(
		VOID);

	inline VOID PitUniprocessorPollSleepUs(UINT16 usDelay)
	{
		UINT64 tickDelay;
		PitUniprocessorPollSleepMs(usDelay / 1000);
		usDelay = usDelay % 1000;
		tickDelay = ((UINT64) usDelay) * PIT_FREQUENCY / 1000000ULL;
		for (; tickDelay > 65535; tickDelay -= 65535)
		{
			PitUniprocessorPollSleepTicks(65535);
		}
		PitUniprocessorPollSleepTicks((UINT16)tickDelay);
	}

#ifdef __cplusplus
}
#endif

#endif