SNDtools
====
## Description
CLI basic sound processing tools written in C++. The binary files in `bin/` folder were complied with Mac OS 10.9.

## Usage
### play
- playback a wave file  
`play {-i/--input filename} [-a/--amp amplitude] [-h/--help]`

### rec
- record a microphone input  
`rec {-o/--output filename} {-s/--samplerate sampling rate} [-c/--channels number of channels] [-a/--amp amplitude] [-h/--help]`

### play_multifilter
- playback a monaural wave file by multiple outputs with filtering  
`play_multi_filter {-i/--input filename} {-f/--filter directory of filter files} [-o/--output number of output channels]  [-a/--amp amplitude] [-h/--help]`

## Requirements
- [boost](http://www.boost.org/)
- [portaudio](http://www.portaudio.com/)
- [libsndfile](http://www.mega-nerd.com/libsndfile/)
- [fftw](http://www.fftw.org/)

## License
[MIT](https://github.com/sh01k/SNDtools/blob/master/LICENSE)

## Author
[Shoichi Koyama](http://www.sh01.org/)
