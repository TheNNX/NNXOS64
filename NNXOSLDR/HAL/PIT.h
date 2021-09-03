#ifndef NNX_PIT_HEADER
#define NNX_PIT_HEADER

#ifdef __cplusplus
extern "C" {
#endif

#include <nnxint.h>

#define PIT_FREQUENCY 1193182

	VOID PitUniprocessorPollSleepMs(UINT16 msDelay);
	VOID PitUniprocessorInitialize();
	VOID PitUniprocessorPollSleepTicks(UINT16 tickDelay);
	inline VOID PitUniprocessorPollSleepUs(UINT16 usDelay) {
		UINT64 tickDelay;
		PitUniprocessorPollSleepMs(usDelay / 1000);
		usDelay = usDelay % 1000;
		tickDelay = ((UINT64)usDelay) * PIT_FREQUENCY / 1000000ULL;
		for (; tickDelay > 65535; tickDelay -= 65535) {
			PitUniprocessorPollSleepTicks(65535);
		}	
		PitUniprocessorPollSleepTicks(tickDelay);
	}

#ifdef __cplusplus
}
#endif

#endif