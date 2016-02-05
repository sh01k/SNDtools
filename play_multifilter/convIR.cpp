#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <limits.h>
#include <memory.h>

#include "convIR.h"

convIR::convIR(void)
{

}

convIR::~convIR(void)
{

}

int convIR::param_init(int _samplerate, int _channels, int _size_frame)
{
	samplerate = _samplerate;
	channels = _channels;
	size_frame = _size_frame;
	size_fft = size_frame * 2;

	return 0;
}

int convIR::init(double *_sig_imp)
{
	sig_imp = _sig_imp;

	fftBuffer = (double*)malloc(size_frame*channels*sizeof(double));
	memset(fftBuffer, 0, size_frame*channels*sizeof(double));

	//FFT Impulse Response
	imp = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * size_fft);
	imp_spec = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * size_fft);
	memset(imp, 0, size_fft*sizeof(fftw_complex));

	fftw_plan p_imp;
	p_imp = fftw_plan_dft_1d(size_fft, imp, imp_spec, FFTW_FORWARD, FFTW_ESTIMATE);

	for (int i = 0; i<size_frame; i++){
		imp[i][0] = sig_imp[i];
	}

	fftw_execute(p_imp);

	fftw_destroy_plan(p_imp);

	//Init FFTW
	in = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * size_fft);
	out = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * size_fft);
	in_spec = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * size_fft);
	out_spec = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * size_fft);

	p_in = fftw_plan_dft_1d(size_fft, in, in_spec, FFTW_FORWARD, FFTW_ESTIMATE);
	ip_out = fftw_plan_dft_1d(size_fft, out_spec, out, FFTW_BACKWARD, FFTW_ESTIMATE);

	return 0;
}

int convIR::execute(float *_sig_in, float *_sig_out){
	int i, ich;

	sig_in = _sig_in;
	sig_out = _sig_out;

	for (ich = 0; ich<channels; ich++){
		memset(in, 0, size_fft*sizeof(fftw_complex));
		memset(out, 0, size_fft*sizeof(fftw_complex));

		for (i = 0; i<size_frame; i++){
			in[i][0] = (double)sig_in[ich + i*channels];
		}

		fftw_execute(p_in);

		for (i = 0; i<size_fft; i++){
			out_spec[i][0] = in_spec[i][0] * imp_spec[i][0] - in_spec[i][1] * imp_spec[i][1];
			out_spec[i][1] = in_spec[i][0] * imp_spec[i][1] + in_spec[i][1] * imp_spec[i][0];
			//out_spec[i][0] = in_spec[i][0];
			//out_spec[i][1] = in_spec[i][1];
		}

		fftw_execute(ip_out);

		for (i = 0; i<size_fft; i++){
			if (i<size_frame){
				sig_out[ich + i*channels] = (float)(out[i][0] / (double)size_fft + fftBuffer[ich + i*channels]);
			}
			else{
				fftBuffer[ich + (i - size_frame)*channels] = out[i][0] / (double)size_fft;
			}
		}
	}

	return 0;
}

int convIR::terminate()
{
	//Terminate FFTW
	fftw_destroy_plan(p_in);
	fftw_destroy_plan(ip_out);
	fftw_free(in);
	fftw_free(imp);
	fftw_free(out);
	fftw_free(in_spec);
	fftw_free(imp_spec);
	fftw_free(out_spec);

	return 0;
}
