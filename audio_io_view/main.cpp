#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <portaudio.h>

int main(int argc, char** argv)
{
	PaError err;

	PaDeviceIndex nDevice;
	PaDeviceIndex nDeviceID;

	const PaDeviceInfo *pDeviceInfo;
	const PaHostApiInfo *pHostApiInfo;

	err = Pa_Initialize();
	if (err != paNoError){
		fprintf(stderr, "Error : Pa_Initialize(), %s\n", Pa_GetErrorText(err));
		exit(1);
	}

	nDevice = Pa_GetDeviceCount();
	for (nDeviceID = 0; nDeviceID<nDevice; nDeviceID++){
		pDeviceInfo = Pa_GetDeviceInfo(nDeviceID);
		pHostApiInfo = Pa_GetHostApiInfo(pDeviceInfo->hostApi);

		printf("%d : %s\n", nDeviceID, pDeviceInfo->name);
		printf("\tAPI name : %s\n", pHostApiInfo->name);
		printf("\tMax Output Channels : %d\n", pDeviceInfo->maxOutputChannels);
		printf("\tMax Input Channels : %d\n", pDeviceInfo->maxInputChannels);
	}

	err = Pa_Terminate();
	if (err != paNoError){
		fprintf(stderr, "Error : %s\n", Pa_GetErrorText(err));
		exit(1);
	}

	return 0;
}
