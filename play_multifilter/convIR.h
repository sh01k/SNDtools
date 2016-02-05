#ifndef _CONVIR_H_
#define _CONVIR_H_

#include <fftw3.h>

class convIR{
private:
	int samplerate;
	int channels;
	int size_frame;
	int size_fft;

	fftw_complex *in, *in_spec, *imp, *imp_spec, *out, *out_spec;
	fftw_plan p_in, ip_out;

public:
	convIR(void);
	~convIR(void);
	double *sig_imp, *fftBuffer;
	float *sig_in, *sig_out;
	int param_init(int _samplerate, int _channels, int _size_frame);
	int init(double *_sig_imp);
	int execute(float *_sig_in, float *_sig_out);
	int terminate();
};

#endif
