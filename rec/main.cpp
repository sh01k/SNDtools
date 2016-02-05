#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <sndfile.h>
#include <portaudio.h>
#include <boost/thread.hpp>
#include <boost/bind.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/optional.hpp>

#define SETTING_INI "setting.ini"
#define FRAME_SIZE_MAX 4096
#define NUM_BUFF 2

using namespace std;

void usage(void)
{
	printf("Usage: play {-i/--input filename} [-a/--amp amplitude] [-h/--help]\n");
}

class playStream
{
private:
	volatile bool w_end_flg;
	volatile bool r_end_flg;
	volatile int rpos;
	volatile int wpos;

	boost::mutex state_guard;

public:
	SNDFILE *fp_read;
	SF_INFO *sf_info_read;

	float amp;

	float **streamBuff;
	int nFrameLength;

	playStream(SNDFILE *_fp_read, SF_INFO *_sf_info_read, int _nFrameLength, float **_streamBuff, float _amp){
		fp_read = _fp_read;
		sf_info_read = _sf_info_read;
		nFrameLength = _nFrameLength;
		streamBuff = _streamBuff;
		amp = _amp;

		w_end_flg = false;
		r_end_flg = false;
		rpos = 0;
		wpos = NUM_BUFF - 1;
	}

	int readData(){
		int i;
		sf_count_t count;

		if (fp_read){
			count = sf_read_float(fp_read, streamBuff[wpos], nFrameLength*sf_info_read->channels);

			/* Change Amplitude */
			for (i = 0; i<count; i++)
				streamBuff[wpos][i] = amp*streamBuff[wpos][i];

			if (count != nFrameLength*sf_info_read->channels){
				sf_seek(fp_read, 0, SEEK_SET);
				w_end_flg = true;
			}
		}

		wpos = (wpos + 1) % NUM_BUFF;

		return 0;
	}

	void writeBuff(){
		while (1){
			boost::mutex::scoped_lock lk(state_guard);

			if (rpos != wpos && w_end_flg == false){
				readData();
			}
			else if (w_end_flg == true && r_end_flg == true){
				break;
			}

		}
	}

	int waitKey(){
		getchar();

		boost::mutex::scoped_lock lk(state_guard);
		r_end_flg = true;
		w_end_flg = true;

		return 0;
	}

	void rposInc(){
		boost::mutex::scoped_lock lk(state_guard);

		rpos = (rpos + 1) % NUM_BUFF;

		if (w_end_flg == true && wpos == rpos)
			r_end_flg = true;
	}

	float* rposRtn(){
		boost::mutex::scoped_lock lk(state_guard);
		return streamBuff[rpos];
	}

};

int paplayCallback(
	const void *inputBuffer,
	void *outputBuffer,
	unsigned long framesPerBuffer,
	const PaStreamCallbackTimeInfo* timeInfo,
	PaStreamCallbackFlags statusFlags,
	void *userData)
{
	playStream *pstConsoleAudioInfo = (playStream *)userData;

	SF_INFO *sf_info_read = NULL;
	sf_info_read = pstConsoleAudioInfo->sf_info_read;

	memcpy(outputBuffer, pstConsoleAudioInfo->rposRtn(), framesPerBuffer*sf_info_read->channels*sizeof(float));

	pstConsoleAudioInfo->rposInc();

	return 0;
}

int main(int argc, char** argv)
{
	SNDFILE *fp_read = NULL;
	SF_INFO sf_info_read;
	sf_count_t count;
	PaError err;

	PaDeviceIndex nDevice;
	PaDeviceIndex nDeviceID;

	const PaDeviceInfo *pDeviceInfo = NULL;
	const PaHostApiInfo *pHostApiInfo = NULL;

	PaStream *pStream = NULL;
	PaStreamParameters stInputParameters;
	PaStreamParameters stOutputParameters;

	float **streamBuff;

	//int samplerate = 0;
	//int channels = 0;
	char *input = NULL;
	float amp = 1.0;

	/* Read options */
	struct option options[] =
	{
		{"help", no_argument, 0, 'h'},
		{"input", required_argument, 0, 'i'},
		{"amp", required_argument, 0, 'a'},
		{0, 0, 0, 0}
	};

	int c;
	int index;

	while((c = getopt_long(argc, argv, "hi:a:", options, &index)) != -1){
		switch(c){
			case 'h':
				usage();
				exit(1);
			case 'i':
				input = optarg;
				break;
			case 'a':
				amp = atof(optarg);
				break;
			default:
				break;
		}
	}

	if (!input){
		fprintf(stderr, "ERROR : no input option\n");
		usage();
		exit(1);
	}

	memset(&sf_info_read, 0, sizeof(SF_INFO));

	fp_read = sf_open(input, SFM_READ, &sf_info_read);
	if (!fp_read){
		printf("File Open Error!\n");
		exit(1);
	}

	printf("Samplerate : %d\n", sf_info_read.samplerate);
	printf("Channel : %d\n", sf_info_read.channels);

	/* Read ini file*/
	boost::property_tree::ptree pt;
	read_ini(SETTING_INI, pt);

	char *audio_driver_name = NULL;
	int nFrameLength = 0;

	if(boost::optional<string> driver_name = pt.get_optional<string>("audio.driver_name")) {
			audio_driver_name = (char *)driver_name.get().c_str();
			//printf("Audio driver name: %s\n", audio_driver_name);

	}
	else{
			fprintf(stderr, "Cannot find audio driver setting.\n");
			exit(1);
	}

	if(boost::optional<int> frame_size = pt.get_optional<int>("audio.frame_size")) {
			nFrameLength = (int)frame_size.get();
			//printf("Frame size: %d\n", nFrameLength);
			if(nFrameLength>FRAME_SIZE_MAX || nFrameLength <= 0){
				fprintf(stderr, "Frame size setting is invalid.\n");
				exit(1);
			}
	}
	else{
			fprintf(stderr, "Cannot find frame size setting.\n");
			exit(1);
	}

	/* Count Samples */
	count = sf_seek(fp_read, 0, SEEK_END);
	printf("Number of samples : %d\n", (int)count);
	printf("Time [s] : %f\n", (double)count / sf_info_read.samplerate);
	sf_seek(fp_read, 0, SEEK_SET);

	err = Pa_Initialize();
	if (err != paNoError){
		fprintf(stderr, "Error : Pa_Initialize(), %s\n", Pa_GetErrorText(err));
		exit(1);
	}

	/* Memory Allocation of Ring Buffer */
	streamBuff = (float **)malloc(NUM_BUFF*sizeof(float *));
	for (int i = 0; i<NUM_BUFF; i++){
		streamBuff[i] = (float *)malloc(nFrameLength*sf_info_read.channels*sizeof(float));
		memset(streamBuff[i], 0, nFrameLength*sf_info_read.channels*sizeof(float));
	}

	/* Select Audio Device */
	nDevice = Pa_GetDeviceCount();
	if (strcmp(audio_driver_name, "Default") == 0){
		nDeviceID = Pa_GetDefaultOutputDevice();
		pDeviceInfo = Pa_GetDeviceInfo(nDeviceID);
	}
	else{
		for (nDeviceID = 0; nDeviceID<nDevice; nDeviceID++){
			pDeviceInfo = Pa_GetDeviceInfo(nDeviceID);
			if (strcmp(pDeviceInfo->name, audio_driver_name) == 0){
				break;
			}
			if (nDeviceID == nDevice){
				fprintf(stderr, "Error : Cannot find Audio Driver\n\t%s\n", audio_driver_name);
				exit(1);
			}
		}
	}

	pHostApiInfo = Pa_GetHostApiInfo(pDeviceInfo->hostApi);

	/* Information of  Audio Device */
	printf("Audio driver: %s\n", pDeviceInfo->name);
	//printf("\tapi name: %s\n", pHostApiInfo->name);
	//printf("\tmax output channels: %d\n", pDeviceInfo->maxOutputChannels);
	//printf("\tmax input channels: %d\n", pDeviceInfo->maxInputChannels);

	memset(&stInputParameters, 0, sizeof(PaStreamParameters));
	memset(&stOutputParameters, 0, sizeof(PaStreamParameters));

	stOutputParameters.channelCount = sf_info_read.channels;
	stOutputParameters.device = nDeviceID;
	stOutputParameters.sampleFormat = paFloat32;

	playStream stConsoleAudioInfo(fp_read, &sf_info_read, nFrameLength, streamBuff, amp);

	boost::thread thr_writeBuff(boost::bind(&playStream::writeBuff, &stConsoleAudioInfo));
	boost::thread thr_waitKey(boost::bind(&playStream::waitKey, &stConsoleAudioInfo));

	err = Pa_OpenStream(
		&pStream,
		NULL,
		&stOutputParameters,
		sf_info_read.samplerate,
		nFrameLength,
		paNoFlag,
		paplayCallback,
		&stConsoleAudioInfo);

	if (err != paNoError){
		fprintf(stderr, "Error : %s\n", Pa_GetErrorText(err));
		Pa_Terminate();
		exit(1);
	}

	err = Pa_StartStream(pStream);
	if (err != paNoError){
		fprintf(stderr, "Error : %s\n", Pa_GetErrorText(err));
	}
	else{
		fprintf(stderr, "Now playing...\n");
		fprintf(stderr, "Press Enter key to stop stream.\n");

		//wait
		thr_writeBuff.join();

		err = Pa_StopStream(pStream);
		if (err != paNoError){
			fprintf(stderr, "Error : %s\n", Pa_GetErrorText(err));
		}
	}

	err = Pa_Terminate();
	if (err != paNoError){
		fprintf(stderr, "Error : %s\n", Pa_GetErrorText(err));
		exit(1);
	}

	if (fp_read){
		sf_close(fp_read);
		fp_read = NULL;
	}

	return 0;
}
