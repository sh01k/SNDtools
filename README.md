SNDtools
====
## Description
CLI basic sound processing tools written in C++. The binary files in `bin/` folder were complied with Mac OS 10.9.

## Usage
### play
Playback a wave file  
`play {-i/--input filename} [-a/--amp amplitude] [-h/--help]`

### rec
Record microphone inputs  
`rec {-o/--output filename} {-s/--samplerate sampling rate} [-c/--channels number of channels] [-a/--amp amplitude] [-h/--help]`

### playrec
Simultaneously playback a wave file and record microphone inputs  
`playrec {-i/--input filename} {-o/--output filename} [-c/--channel number of input channels] [-a/--amp amplitude of output] [-h/--help]`

### play_multifilter
playback a monaural wave file by multiple outputs with filtering  
`play_multi_filter {-i/--input filename} {-f/--filter directory of filter files} [-o/--output number of output channels]  [-a/--amp amplitude] [-h/--help]`

- Input wave file must be monaural.
- The input file is convoluted by the filters in the filter directory, and these are output of each loudspeaker.
- Filter files must be put in the filter directory and their names must be [# of channel].bin, for example, 1.bin and 2.bin for ch1 and ch2, respectively.
- The file format of the filter files is 64 bit binary (see a sample MATLAB code "filter_gen.m")
- The number of filter files must be equal to or larger than the number of the output channels.  

## Requirements
- [boost](http://www.boost.org/)
- [portaudio](http://www.portaudio.com/)
- [libsndfile](http://www.mega-nerd.com/libsndfile/)
- [fftw](http://www.fftw.org/)

## License
[MIT](https://github.com/sh01k/SNDtools/blob/master/LICENSE)

## Author
[Shoichi Koyama](http://www.sh01.org/)
