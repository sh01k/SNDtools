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

#include "convIR.h"

#define SETTING_INI "setting.ini"
#define FRAME_SIZE_MAX 4096
#define NUM_BUFF 2

using namespace std;

void usage(void)
{
	printf("Usage: play_multi_filter {-i/--input filename} {-f/--filter directory of filter files}\n");
	printf("       [-o/output number of output channels]  [-a/--amp amplitude] [-h/--help]\n");
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

	convIR *conv_m;

	int output_channels;

	float amp;

	float **streamBuff;
	float *readBuff;
	float **filterBuff;
	int nFrameLength;

	playStream(SNDFILE *_fp_read, SF_INFO *_sf_info_read, int _output_channels, int _nFrameLength, convIR *_conv_m, float **_streamBuff, float *_readBuff, float **_filterBuff, float _amp){
		fp_read = _fp_read;
		sf_info_read = _sf_info_read;
		output_channels = _output_channels;
		nFrameLength = _nFrameLength;
		conv_m = _conv_m;
		streamBuff = _streamBuff;
		readBuff = _readBuff;
		filterBuff = _filterBuff;
		amp = _amp;

		w_end_flg = false;
		r_end_flg = false;
		rpos = 0;
		wpos = NUM_BUFF - 1;
	}

	int readData(){
		int i, j;
		sf_count_t count;

		if (fp_read){
			//count = sf_read_float(fp_read, streamBuff[wpos], nFrameLength*sf_info_read->channels);
			count = sf_read_float(fp_read, readBuff, nFrameLength*sf_info_read->channels);

			/* Change Amplitude */
			for (i = 0; i<count; i++)
				readBuff[i] = amp*readBuff[i];

			for (i = 0; i < output_channels; i++){
				conv_m[i].execute(readBuff,filterBuff[i]);
				//memcpy(filterBuff[i], readBuff, nFrameLength*sizeof(float));
			}

			//conv->execute(readBuff, streamBuff[wpos]);
			for (i = 0; i < nFrameLength; i++){
				for (j = 0; j < output_channels; j++){
					streamBuff[wpos][i*output_channels + j] = filterBuff[j][i];
				}
			}

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

	int output_channels = pstConsoleAudioInfo->output_channels;

	memcpy(outputBuffer, pstConsoleAudioInfo->rposRtn(), framesPerBuffer*output_channels*sizeof(float));

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
	float *readBuff;
	float **filterBuff;

	int size_imp;
	FILE *fp_imp;
	double **imp = NULL;

	int output_channels = 1;

	char *input = NULL;
	char *filter_dir = NULL;
	char filter[128];
	float amp = 1.0;

	/* Read options */
	struct option options[] =
	{
		{"help", no_argument, 0, 'h'},
		{"input", required_argument, 0, 'i'},
		{"channel", required_argument, 0, 'c'},
		{"filter", required_argument, 0, 'f'},
		{"amp", required_argument, 0, 'a'},
		{0, 0, 0, 0}
	};

	int c;
	int index;

	while((c = getopt_long(argc, argv, "hi:c:f:a:", options, &index)) != -1){
		switch(c){
			case 'h':
				usage();
				exit(1);
			case 'i':
				input = optarg;
				break;
			case 'c':
				output_channels = atoi(optarg);
				break;
			case 'f':
				filter_dir = optarg;
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

	if (!filter_dir){
		fprintf(stderr, "ERROR : no filter directory option\n");
		usage();
		exit(1);
	}

	if (output_channels < 1){
		fprintf(stderr, "ERROR : number of output channels is invalid\n");
		usage();
		exit(1);
	}

	memset(&sf_info_read, 0, sizeof(SF_INFO));

	fp_read = sf_open(input, SFM_READ, &sf_info_read);
	if (!fp_read){
		printf("File Open Error!\n");
		exit(2);
	}

	if (sf_info_read.channels != 1){
		fprintf(stderr, "ERROR : input file must be mono\n");
		usage();
		exit(1);
	}

	printf("Samplerate : %d\n", sf_info_read.samplerate);
	printf("Channel : %d\n", sf_info_read.channels);
	printf("Output channel : %d\n", output_channels);

	/* Read ini file */
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
	printf("Time [sec] : %f\n", (double)count / sf_info_read.samplerate);
	sf_seek(fp_read, 0, SEEK_SET);

	/* Read filter file */
	imp = (double **)malloc(output_channels*sizeof(double *));
	for (int i = 0; i < output_channels; i++){
		sprintf(filter, "%s/%d.bin", filter_dir, i+1);
		printf("Reading %s\n", filter);

		if ((fp_imp = fopen(filter, "rb")) == NULL){
			printf("File Open Error! : %s\n", filter);
			exit(2);
		}

		fseek(fp_imp, 0L, SEEK_END);
		size_imp = (int)(ftell(fp_imp) / sizeof(double)); //Get size of IR
		//printf("Size of Impulse Response: %d\n", size_imp);
		fseek(fp_imp, 0L, SEEK_SET);

		if (nFrameLength != size_imp){
			fprintf(stderr, "Frame Size and Impulse Response Length must be same.\n");
			fclose(fp_imp);
			exit(1);
		}

		imp[i] = (double *)malloc(size_imp*sizeof(double));
		memset(imp[i], 0, size_imp*sizeof(double));

		fread(imp[i], 1, size_imp*sizeof(double), fp_imp); //Read IR (double)

		fclose(fp_imp);
	}

	/* Memory Allocation of Ring Buffer */
	streamBuff = (float **)malloc(NUM_BUFF*sizeof(float *));
	for (int i = 0; i<NUM_BUFF; i++){
		streamBuff[i] = (float *)malloc(nFrameLength*output_channels*sizeof(float));
		memset(streamBuff[i], 0, nFrameLength*output_channels*sizeof(float));
	}

	/* Memory Allocation of Read Buffer */
	readBuff = (float *)malloc(nFrameLength*sf_info_read.channels*sizeof(float));
	memset(readBuff, 0, nFrameLength*sf_info_read.channels*sizeof(float));

	/* Memory Allocation of Filter Buffer */
	filterBuff = (float **)malloc(output_channels*sizeof(float *));
	for (int i = 0; i < output_channels; i++){
		filterBuff[i] = (float *)malloc(nFrameLength*sizeof(float));
		memset(filterBuff[i], 0, nFrameLength*sizeof(float));
	}

	/* Initialize convIR */
	convIR *conv_m;
	conv_m = new convIR[output_channels];
	for (int i = 0; i < output_channels; i++){
		conv_m[i].param_init(sf_info_read.samplerate, sf_info_read.channels, nFrameLength);
		conv_m[i].init(imp[i]);
	}

	err = Pa_Initialize();
	if (err != paNoError){
		fprintf(stderr, "Error : Pa_Initialize(), %s\n", Pa_GetErrorText(err));
		exit(4);
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
				fprintf(stderr, "Error : Cannot find AUDIO Driver\n\t%s\n", audio_driver_name);
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

	stOutputParameters.channelCount = output_channels;
	stOutputParameters.device = nDeviceID;
	stOutputParameters.sampleFormat = paFloat32;

	playStream stConsoleAudioInfo(fp_read, &sf_info_read, output_channels, nFrameLength, conv_m, streamBuff, readBuff, filterBuff, amp);

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
		exit(5);
	}

	if (fp_read){
		sf_close(fp_read);
		fp_read = NULL;
	}

	return 0;
}
