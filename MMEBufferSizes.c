#define _USE_MATH_DEFINES

#include <Windows.h>
#include <mmreg.h>

#include <math.h>
#include <stdio.h>
#include <stdbool.h>

#define SAMPLE_RATE 48000
#define BUFFER_COUNT 2
#define SINE_FREQUENCY_HZ 134.0
#define SINE_LENGTH_SECONDS 1.0
#define POLLING_PERIOD_MILLISECONDS 1

static void generateSineSamples(float* const samples, const size_t count, const unsigned long samplesPlayed) {
	for (size_t offset = 0; offset < count; ++offset) {
		samples[offset] = (float) sin(2 * M_PI * SINE_FREQUENCY_HZ * (samplesPlayed + offset) / SAMPLE_RATE);
	}
}

static LARGE_INTEGER performanceFrequency;
static LARGE_INTEGER performanceCounterReference;
static void setTimeReference() {
	if (QueryPerformanceFrequency(&performanceFrequency) == 0) abort();
	if (QueryPerformanceCounter(&performanceCounterReference) == 0) abort();
}
static float getTimeSeconds() {
	LARGE_INTEGER performanceCounter;
	if (QueryPerformanceCounter(&performanceCounter) == 0) abort();
	return ((float)(performanceCounter.QuadPart - performanceCounterReference.QuadPart)) / ((float)performanceFrequency.QuadPart);
}

int main(const int argc, const char* const* const argv) {
	if (argc != 2) {
		printf("usage: MMEBufferSizes <total buffer size in seconds>\n");
		return EXIT_FAILURE;
	}
	const long bufferSizeFrames = (long)(atof(argv[1]) * SAMPLE_RATE / BUFFER_COUNT);
	if (bufferSizeFrames <= 0) abort();

	printf("Playing a %.1lf Hz sine wave at a %d Hz sample rate for %.3lf seconds using %d buffers of %d frames, polling every %d milliseconds\n", SINE_FREQUENCY_HZ, SAMPLE_RATE, SINE_LENGTH_SECONDS, BUFFER_COUNT, bufferSizeFrames, POLLING_PERIOD_MILLISECONDS);

	WAVEFORMATEX waveFormatEx = {
		.wFormatTag = WAVE_FORMAT_IEEE_FLOAT,
		.nChannels = 1,
		.nSamplesPerSec = SAMPLE_RATE,
		.nAvgBytesPerSec = SAMPLE_RATE * sizeof(float),
		.nBlockAlign = sizeof(float),
		.wBitsPerSample = 32,
		.cbSize = 0,
	};
	HWAVEOUT waveOut;
	if (waveOutOpen(&waveOut, WAVE_MAPPER, &waveFormatEx, /*dwCallback=*/0, /*dwInstance=*/0, CALLBACK_NULL) != MMSYSERR_NOERROR) abort();
	
	WAVEHDR waveHeaders[BUFFER_COUNT];
	unsigned long framesPlayed = 0;
	
	for (int bufferIndex = 0; bufferIndex < BUFFER_COUNT; ++bufferIndex) {
		const DWORD bufferSizeBytes = bufferSizeFrames * waveFormatEx.nBlockAlign;
		const WAVEHDR waveHeader = {
			.lpData = malloc(bufferSizeBytes),
			.dwBufferLength = bufferSizeBytes,
			.dwFlags = 0,
		};

		generateSineSamples((float*)waveHeader.lpData, bufferSizeFrames, framesPlayed);
		framesPlayed += bufferSizeFrames;

		WAVEHDR* waveHeaderPtr = &waveHeaders[bufferIndex];
		*waveHeaderPtr = waveHeader;
		if (waveOutPrepareHeader(waveOut, waveHeaderPtr, sizeof(*waveHeaderPtr)) != MMSYSERR_NOERROR) abort();
	}

	if (waveOutPause(waveOut) != MMSYSERR_NOERROR) abort();
	for (int bufferIndex = 0; bufferIndex < BUFFER_COUNT; ++bufferIndex) {
		if (waveOutWrite(waveOut, &waveHeaders[bufferIndex], sizeof(waveHeaders[bufferIndex])) != MMSYSERR_NOERROR) abort();
	}

	timeBeginPeriod(1);
	setTimeReference();
	if (waveOutRestart(waveOut) != MMSYSERR_NOERROR) abort();

	printf("[%2.3f] Started\n", getTimeSeconds());

	int playingBufferIndex = 0;
	while (framesPlayed < SINE_LENGTH_SECONDS * SAMPLE_RATE) {
		printf("[%2.3f]", getTimeSeconds());
		bool playingBufferIsDone = false;
		for (int bufferIndex = 0; bufferIndex < BUFFER_COUNT; ++bufferIndex) {
			const DWORD flags = waveHeaders[bufferIndex].dwFlags;
			printf(" %d:%c%c%c", bufferIndex,
				flags & WHDR_PREPARED ? 'P' : 'p',
				flags & WHDR_INQUEUE ? 'Q' : 'q',
				flags & WHDR_DONE ? 'D' : 'd');
			if (bufferIndex == playingBufferIndex)
				playingBufferIsDone = flags & WHDR_DONE;
		}
		printf("\n");

		WAVEHDR* playingHeader = &waveHeaders[playingBufferIndex];
		if (playingBufferIsDone) {
			generateSineSamples((float*)playingHeader->lpData, bufferSizeFrames, framesPlayed);
			framesPlayed += bufferSizeFrames;
			if (waveOutWrite(waveOut, playingHeader, sizeof(*playingHeader)) != MMSYSERR_NOERROR) abort();
			printf("-> waveOutWrite(%d)\n", playingBufferIndex);
			playingBufferIndex = (playingBufferIndex + 1) % BUFFER_COUNT;
			continue;
		}

		Sleep(POLLING_PERIOD_MILLISECONDS);
	}

	return EXIT_SUCCESS;
}
