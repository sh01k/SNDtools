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
	printf("Usage: playrec {-i/--input filename} {-o/--output filename} [-c/--channel number of input channels] [-a/--amp amplitude of output] [-h/--help]\n");
}

class playrecStream
{
private:
	volatile bool w_end_flg;
	volatile bool r_end_flg;
	volatile int rpos;
	volatile int wpos;

	boost::mutex state_guard;

public:
	SNDFILE *fp_read;
	SNDFILE *fp_write;
	SF_INFO *sf_info_read;
	SF_INFO *sf_info_write;

	int channels;
	float amp;

	float **streamBuffOut;
	float **streamBuffIn;
	int nFrameLength;

	float *buff_read;
	float *buff_write;

	playrecStream(SNDFILE *_fp_read, SNDFILE *_fp_write, SF_INFO *_sf_info_read, SF_INFO *_sf_info_write, int _channels, int _nFrameLength, float **_streamBuffOut, float **_streamBuffIn, float _amp){
		fp_read = _fp_read;
		sf_info_read = _sf_info_read;
		fp_write = _fp_write;
		sf_info_write = _sf_info_write;
		channels = _channels;
		nFrameLength = _nFrameLength;
		streamBuffOut = _streamBuffOut;
		streamBuffIn = _streamBuffIn;
		amp = _amp;

		buff_read = (float *)malloc(nFrameLength*sf_info_read->channels*sizeof(float));
		buff_write = (float *)malloc(nFrameLength*sf_info_write->channels*sizeof(float));
		memset(buff_read, 0, nFrameLength*sf_info_read->channels*sizeof(float));
		memset(buff_write, 0, nFrameLength*sf_info_write->channels*sizeof(float));

		w_end_flg = false;
		r_end_flg = false;
		rpos = 0;
		wpos = NUM_BUFF - 1;
	}

	int readwriteData(){
		int i;
		int j;
		sf_count_t count;

		if (fp_read){
			count = sf_read_float(fp_read, buff_read, nFrameLength*sf_info_read->channels);

			/* Change Amplitude */
			for (i = 0; i<nFrameLength; i++){
				for (j=0; j<sf_info_read->channels; j++){
					streamBuffOut[wpos][i*channels+j] = amp*buff_read[i*sf_info_read->channels+j];
				}
			}

			if (count != nFrameLength*sf_info_read->channels){
				//memset(streamBuffOut[wpos], 0, nFrameLength*sf_info_read->channels*sizeof(float));
				for (i = floor(count/sf_info_read->channels); i<nFrameLength; i++){
					for (j = floor(count/nFrameLength); j<sf_info_read->channels; j++){
						streamBuffOut[wpos][i*channels+j] = amp*buff_read[i*sf_info_read->channels+j];
					}
				}
				//sf_seek(fp_read, 0, SEEK_SET);
				//w_end_flg = true;
			}
		}

		if (fp_write){
			for (i=0; i<nFrameLength*sf_info_write->channels; i++){
				buff_write[i] = streamBuffIn[wpos][i];
			}
			count = sf_write_float(fp_write, buff_write, nFrameLength*sf_info_write->channels);
		}

		wpos = (wpos + 1) % NUM_BUFF;

		return 0;
	}

	void writeBuff(){
		while (1){
			boost::mutex::scoped_lock lk(state_guard);

			if (rpos != wpos && w_end_flg == false){
				readwriteData();
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

	float* rposRtnOut(){
		boost::mutex::scoped_lock lk(state_guard);
		return streamBuffOut[rpos];
	}

	float* rposRtnIn(){
		boost::mutex::scoped_lock lk(state_guard);
		return streamBuffIn[rpos];
	}

};

int paplayrecCallback(
	const void *inputBuffer,
	void *outputBuffer,
	unsigned long framesPerBuffer,
	const PaStreamCallbackTimeInfo* timeInfo,
	PaStreamCallbackFlags statusFlags,
	void *userData)
{
	playrecStream *pstConsoleAudioInfo = (playrecStream *)userData;

	memcpy(outputBuffer, pstConsoleAudioInfo->rposRtnOut(), framesPerBuffer*pstConsoleAudioInfo->channels*sizeof(float));
	memcpy(pstConsoleAudioInfo->rposRtnIn(), inputBuffer, framesPerBuffer*pstConsoleAudioInfo->channels*sizeof(float));

	pstConsoleAudioInfo->rposInc();

	return 0;
}

int main(int argc, char** argv)
{
	SNDFILE *fp_read = NULL;
	SNDFILE *fp_write = NULL;
	SF_INFO sf_info_read;
	SF_INFO sf_info_write;
	sf_count_t count;
	PaError err;

	PaDeviceIndex nDevice;
	PaDeviceIndex nDeviceID;

	const PaDeviceInfo *pDeviceInfo = NULL;
	const PaHostApiInfo *pHostApiInfo = NULL;

	PaStream *pStream = NULL;
	PaStreamParameters stInputParameters;
	PaStreamParameters stOutputParameters;

	float **streamBuffOut;
	float **streamBuffIn;

	//int samplerate = 0;
	int channels = 0;
	int input_channels = 0;
	char *input = NULL;
	char *output = NULL;
	float amp = 1.0;

	/* Read options */
	struct option options[] =
	{
		{"help", no_argument, 0, 'h'},
		{"input", required_argument, 0, 'i'},
		{"channel", required_argument, 0, 'c'},
		{"output", required_argument, 0, 'o'},
		{"amp", required_argument, 0, 'a'},
		{0, 0, 0, 0}
	};

	int c;
	int index;

	while((c = getopt_long(argc, argv, "hi:c:o:a:", options, &index)) != -1){
		switch(c){
			case 'h':
				usage();
				exit(1);
			case 'i':
				input = optarg;
				break;
			case 'o':
				output = optarg;
				break;
			case 'c':
				input_channels = atoi(optarg);
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

	if (!output){
		fprintf(stderr, "ERROR : no output option\n");
		usage();
		exit(1);
	}

	if(!input_channels){
      input_channels = 1;
  }

	memset(&sf_info_read, 0, sizeof(SF_INFO));

	fp_read = sf_open(input, SFM_READ, &sf_info_read);
	if (!fp_read){
		printf("File Open Error!\n");
		exit(1);
	}

	if(input_channels>=sf_info_read.channels){
		channels = input_channels;
	}else{
		channels = sf_info_read.channels;
	}

	memset(&sf_info_write, 0, sizeof(SF_INFO));

  sf_info_write.samplerate=sf_info_read.samplerate;
  sf_info_write.channels=input_channels;
  sf_info_write.format=SF_FORMAT_WAV | SF_FORMAT_PCM_16;

	fp_write = sf_open(output, SFM_WRITE, &sf_info_write);
	if(!fp_write){
		 printf("File Open Error!\n");
		 exit(2);
	}

	printf("Samplerate : %d\n", sf_info_read.samplerate);
	printf("Channel (input) : %d\n", input_channels);
	printf("Channel (output) : %d\n", sf_info_read.channels);

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
	streamBuffOut = (float **)malloc(NUM_BUFF*sizeof(float *));
	for (int i = 0; i<NUM_BUFF; i++){
		streamBuffOut[i] = (float *)malloc(nFrameLength*channels*sizeof(float));
		memset(streamBuffOut[i], 0, nFrameLength*channels*sizeof(float));
	}
	streamBuffIn = (float **)malloc(NUM_BUFF*sizeof(float *));
	for (int i = 0; i<NUM_BUFF; i++){
		streamBuffIn[i] = (float *)malloc(nFrameLength*channels*sizeof(float));
		memset(streamBuffIn[i], 0, nFrameLength*channels*sizeof(float));
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

	stInputParameters.channelCount = channels; //input_channels;
	stInputParameters.device = nDeviceID;
	stInputParameters.sampleFormat = paFloat32;

	stOutputParameters.channelCount = channels; //sf_info_read.channels;
	stOutputParameters.device = nDeviceID;
	stOutputParameters.sampleFormat = paFloat32;

	playrecStream stConsoleAudioInfo(fp_read, fp_write, &sf_info_read, &sf_info_write, channels, nFrameLength, streamBuffOut, streamBuffIn, amp);

	boost::thread thr_writeBuff(boost::bind(&playrecStream::writeBuff, &stConsoleAudioInfo));
	boost::thread thr_waitKey(boost::bind(&playrecStream::waitKey, &stConsoleAudioInfo));

	err = Pa_OpenStream(
		&pStream,
		&stInputParameters,
		&stOutputParameters,
		sf_info_read.samplerate,
		nFrameLength,
		paNoFlag,
		paplayrecCallback,
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

	if(fp_write){
		sf_close(fp_write);
		fp_write=NULL;
	}

	return 0;
}
