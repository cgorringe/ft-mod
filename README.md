## ft-mod
Music .MOD-file Player for the [Noisebridge Flaschen Taschen Project](https://noisebridge.net/wiki/Flaschen_Taschen).

By [Carl Gorringe](http://carl.gorringe.org)

### Status

This program is currently incomplete and not yet ready to use.


### How to Install

This project uses the C++ framework provided by the [flaschen-taschen](https://github.com/hzeller/flaschen-taschen) project.  Since it's being used as a sub-module, you'll want to clone this repo with the `--recursive` option:

```
  git clone --recursive https://github.com/cgorringe/ft-mod
```

### Install Dependencies
#### MacOS X
(not tested)

```
brew install portaudio
brew install zlib
```

Install **libopenmpt**:

* Download the "Makefile / Android ndk-build" tar.gz file from this [Download]( https://lib.openmpt.org/libopenmpt/download/ ) page.

Untar, then from it's directory:

```
make clean
make SHARED_SONAME=0
make check
sudo make install SHARED_SONAME=0
```

#### Ubuntu

(not tested)

```
apt-get install zlib portaudio libopenmpt-dev
```
May need to substitute above with `portaudio-v19` ?
 
Other possible dependencies:
`gcc, pkg-config, libmpg123, doxygen, libpulse, libpulse-simple, libFLAC, libsndfile, libSDL`

