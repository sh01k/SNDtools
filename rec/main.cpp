#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <sndfile.h>
#include <portaudio.h>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>

#define SETTING_INI "setting.ini"
#define FRAME_SIZE_MAX 4096

using namespace std;

void usage(void)
{
  printf("Usage: rec {-o/--output filename} {-s/--samplerate sampling rate}\n"
	       "           [-c/--channels number of channels] [-a/--amp amplitude]\n"
         "           [-h/--help]\n"
	 );
}

typedef struct
{
    SNDFILE *fp_write;
    SF_INFO *sf_info_write;

    float *inbuff;
    float *outbuff;

}CONSOLEAUDIOINFO;

int parecCallback(
    const void *inputBuffer,
    void *outputBuffer,
    unsigned long framesPerBuffer,
    const PaStreamCallbackTimeInfo* timeInfo,
    PaStreamCallbackFlags statusFlags,
    void *userData)
{
    SNDFILE *fp_write = NULL;
    SF_INFO *sf_info_write = NULL;
    CONSOLEAUDIOINFO *pstConsoleAudioInfo = (CONSOLEAUDIOINFO *)userData;

    sf_count_t count;

    fp_write = pstConsoleAudioInfo->fp_write;
    sf_info_write = pstConsoleAudioInfo->sf_info_write;

    memcpy(pstConsoleAudioInfo->inbuff, (float *)inputBuffer, framesPerBuffer*(sf_info_write->channels)*sizeof(float));

    for(int i=0; i<(int)framesPerBuffer; i++)
	    for(int j=0; j<sf_info_write->channels; j++)
	      pstConsoleAudioInfo->outbuff[i*sf_info_write->channels+j] = pstConsoleAudioInfo->inbuff[i*sf_info_write->channels+j];

    if(fp_write){
	     count = sf_write_float(fp_write, pstConsoleAudioInfo->outbuff, framesPerBuffer *sf_info_write->channels);
    }

    return 0;
}


int main(int argc, char** argv)
{
    SNDFILE *fp_write = NULL;
    SF_INFO sf_info_write;

    PaError err;

    PaDeviceIndex nDevice;
    PaDeviceIndex nDeviceID;

    const PaDeviceInfo *pDeviceInfo;
    const PaHostApiInfo *pHostApiInfo;

    PaStream *pStream = NULL;
    PaStreamParameters stInputParameters;
    PaStreamParameters stOutputParameters;

    CONSOLEAUDIOINFO stConsoleAudioInfo;

    float *inbuff;
    float *outbuff;

    int samplerate = 0;
    int channels = 0;
    char *output = NULL;
    float amp = 1.0;

    struct option options[] =
    {
      {"help", 0, NULL, 'h'},
	    {"output", 1, NULL, 'o'},
	    {"amp", 2, NULL, 'a'},
	    {"samplerate", 1, NULL, 's'},
	    {"channels", 1, NULL, 'c'},
	    {0, 0, 0, 0}
	  };

    int c;
    int index;

    while((c = getopt_long(argc, argv, "ho:a:s:c:", options, &index)) != -1){
      switch(c){
        case 'h':
  				usage();
  				exit(1);
        case 'o':
          output = optarg;
	        break;
	      case 'a':
	        amp = atof(optarg);
	        break;
	      case 's':
	        samplerate = atoi(optarg);
	        break;
	      case 'c':
	        channels = atoi(optarg);
	        break;
	      default:
	        break;
	      }
    }

    if(!output || !samplerate){
	     fprintf(stderr, "ERROR : invalid arguments\n");
	      usage();
	      exit(1);
    }

    if(!channels){
      channels = 1;
    }

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

    memset(&sf_info_write, 0, sizeof(SF_INFO));

    sf_info_write.samplerate=samplerate;
    sf_info_write.channels=channels;
    sf_info_write.format=SF_FORMAT_WAV | SF_FORMAT_PCM_16;

    fp_write = sf_open(output, SFM_WRITE, &sf_info_write);
    if(!fp_write){
	     printf("File Open Error!\n");
	     exit(2);
    }

    printf("Samplerate: %d\n", sf_info_write.samplerate);
    printf("Channel: %d\n", sf_info_write.channels);

    /* Memory Allocation of Buffer */
    inbuff = (float *)malloc(nFrameLength*channels*sizeof(float));
    memset(inbuff, 0, nFrameLength*channels*sizeof(float));

    outbuff = (float *)malloc(nFrameLength*channels*sizeof(float));
    memset(outbuff, 0, nFrameLength*channels*sizeof(float));

    err = Pa_Initialize();
    if(err != paNoError){
      fprintf(stderr, "Error : Pa_Initialize(), %s\n", Pa_GetErrorText(err));
	    exit(1);
    }

    /* Select Audio Device */
    nDevice = Pa_GetDeviceCount();
    if(strcmp(audio_driver_name, "Default")==0){
      nDeviceID = Pa_GetDefaultOutputDevice();
	    pDeviceInfo = Pa_GetDeviceInfo(nDeviceID);
    }else{
      for(nDeviceID = 0; nDeviceID<nDevice; nDeviceID++){
         pDeviceInfo = Pa_GetDeviceInfo(nDeviceID);
         if(strcmp(pDeviceInfo->name, audio_driver_name)==0){
           break;
         }
         if(nDeviceID==nDevice){
           fprintf(stderr, "Error : Cannot find Audio Driver\n\t%s\n",audio_driver_name);
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

    stInputParameters.channelCount = channels;
    stInputParameters.device = nDeviceID;
    stInputParameters.sampleFormat = paFloat32;

    memset(&stConsoleAudioInfo, 0, sizeof(CONSOLEAUDIOINFO));
    stConsoleAudioInfo.fp_write = fp_write;
    stConsoleAudioInfo.sf_info_write = &sf_info_write;
    stConsoleAudioInfo.inbuff = inbuff;
    stConsoleAudioInfo.outbuff = outbuff;

    err = Pa_OpenStream(
      &pStream,
	    &stInputParameters,
	    NULL,
	    sf_info_write.samplerate,
	    nFrameLength,
	    paNoFlag,
	    parecCallback,
	    &stConsoleAudioInfo);

    if(err != paNoError){
      fprintf(stderr, "Error : %s\n", Pa_GetErrorText(err));
	    Pa_Terminate();
	    exit(1);
    }

    err = Pa_StartStream(pStream);
    if(err != paNoError){
      fprintf(stderr, "Error : %s\n", Pa_GetErrorText(err));}
    else{
      printf("Now recording...\n");
	    printf("Press Enter key to stop stream...\n");
      getchar();

	    err = Pa_StopStream(pStream);
	    if(err != paNoError){
	       fprintf(stderr, "Error : %s\n", Pa_GetErrorText(err));
	    }
    }

    err = Pa_CloseStream(pStream);
    if(err != paNoError){
	     fprintf(stderr, "Error : %s\n", Pa_GetErrorText(err));
    }

    err = Pa_Terminate();
    if(err != paNoError){
	    fprintf(stderr, "Error : %s\n", Pa_GetErrorText(err));
	    exit(1);
    }

    if(fp_write){
	    sf_close(fp_write);
	    fp_write=NULL;
    }

    return 0;
}
