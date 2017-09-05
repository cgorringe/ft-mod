// -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; -*-
//
// ft-mod
// Copyright (c) 2017 Carl Gorringe (carl.gorringe.org)
// https://github.com/cgorringe/ft-mod
// 9/2/2017
//
// Plays & Displays MOD Music Modules.
//
// Don't have any?  Download modules from modarchive.org
//
// How to run:
//
//  export FT_DISPLAY=localhost
//  ./ft-mod ...
//
// To see command line options:
//  ./ft-mod -?
//
// --------------------------------------------------------------------------------
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation version 2.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://gnu.org/licenses/gpl-2.0.txt>
//

#include "ft/api/include/udp-flaschen-taschen.h"

#include <getopt.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <string>
#include <signal.h>

//-- used by mod player --
#include <memory.h>
//#include <stdint.h>
//#include <stdio.h>
//#include <stdlib.h>
//#include <string.h>
#include <errno.h>
//#include <unistd.h>
#include <libopenmpt/libopenmpt.h>
#include <libopenmpt/libopenmpt_stream_callbacks_file.h>
// must have PortAudio installed
// on Mac: brew install portaudio
#include <portaudio.h>
#define BUFFERSIZE 480
#define SAMPLERATE 48000
static int16_t audio_left[BUFFERSIZE];
static int16_t audio_right[BUFFERSIZE];
static int16_t * const audio_buffers[2] = { audio_left, audio_right };
static int16_t audio_buffer[BUFFERSIZE * 2];
// ------

// Defaults                      large  small
#define DISPLAY_WIDTH  (9*5)  //  9*5    5*5
#define DISPLAY_HEIGHT (7*5)  //  7*5    4*5
#define Z_LAYER 7      // (0-15) 0=background
#define DELAY 25
#define MAX_PATH_LENGTH 250

const int kOutSTDOUT = 0;
const int kOutPA  = 1;

volatile bool interrupt_received = false;
static void InterruptHandler(int signo) {
    interrupt_received = true;
}

// ------------------------------------------------------------------------------------------
// Command Line Options

// option vars
const char *opt_hostname = NULL;
int opt_layer  = Z_LAYER;
double opt_timeout = 60*60*24;  // timeout in 24 hrs
int opt_width  = DISPLAY_WIDTH;
int opt_height = DISPLAY_HEIGHT;
int opt_xoff=0, opt_yoff=0;
int opt_delay   = DELAY;
char opt_filepath[MAX_PATH_LENGTH];
const char *opt_output_text = "";
int opt_output = kOutSTDOUT;

int usage(const char *progname) {

    fprintf(stderr, "ft-mod (c) 2017 Carl Gorringe (carl.gorringe.org)\n");
    fprintf(stderr, "Usage: %s [options] <mod-file>\n", progname);
    fprintf(stderr, "Options:\n"
        "\t-g <W>x<H>[+<X>+<Y>] : Output geometry. (default 45x35+0+0)\n"
        "\t-l <layer>     : Layer 0-15. (default 7)\n"
        "\t-t <timeout>   : Timeout exits after given seconds. (default 24hrs)\n"
        "\t-h <host>      : Flaschen-Taschen display hostname. (FT_DISPLAY)\n"
        "\t-d <delay>     : Delay between frames in milliseconds. (default 25)\n"
        "\t-o <output>    : Output audio: 'pa' = PortAudio. (default stdout)\n"
    );
    return 1;
}

int cmdLine(int argc, char *argv[]) {

    // command line options
    int opt;
    while ((opt = getopt(argc, argv, "?g:l:t:h:d:o:")) != -1) {
        switch (opt) {
        case '?':  // help
            return usage(argv[0]);
            break;
        case 'g':  // geometry
            if (sscanf(optarg, "%dx%d%d%d", &opt_width, &opt_height, &opt_xoff, &opt_yoff) < 2) {
                fprintf(stderr, "Invalid size '%s'\n", optarg);
                return usage(argv[0]);
            }
            break;
        case 'l':  // layer
            if (sscanf(optarg, "%d", &opt_layer) != 1 || opt_layer < 0 || opt_layer >= 16) {
                fprintf(stderr, "Invalid layer '%s'\n", optarg);
                return usage(argv[0]);
            }
            break;
        case 't':  // timeout
            if (sscanf(optarg, "%lf", &opt_timeout) != 1 || opt_timeout < 0) {
                fprintf(stderr, "Invalid timeout '%s'\n", optarg);
                return usage(argv[0]);
            }
            break;
        case 'h':  // hostname
            opt_hostname = strdup(optarg); // leaking. Ignore.
            break;
        case 'd':  // delay
            if (sscanf(optarg, "%d", &opt_delay) != 1 || opt_delay < 1) {
                fprintf(stderr, "Invalid delay '%s'\n", optarg);
                return usage(argv[0]);
            }
            break;
        case 'o':  // output
            opt_output_text = strdup(optarg);
            break;
        default:
            return usage(argv[0]);
        }
    }

    // assign default filepath
    strncpy(opt_filepath, "", MAX_PATH_LENGTH);

    // retrieve arg text from remaining arguments
    // TODO: convert this to store an array of filenames
    std::string str;
    if (argv[optind]) {
        str.append(argv[optind]);
        for (int i = optind + 1; i < argc; i++) {
            str.append(" ").append(argv[i]);
        }
        const char *text = str.c_str();
        while (isspace(*text)) { text++; }  // remove leading spaces
        if (text && (strlen(text) > 0)) {
            strncpy(opt_filepath, text, MAX_PATH_LENGTH);
        }
    }
    if (strlen(opt_filepath) == 0) {
        fprintf(stderr, "Missing MOD file.\n");
        return usage(argv[0]);
    }

    // set opt_output
    if (strcmp(opt_output_text, "pa") == 0) {
        opt_output = kOutPA;
        fprintf(stderr, "Output: PortAudio\n");
    }
    else {
        opt_output = kOutSTDOUT;
        fprintf(stderr, "Output: stdout\n");
    }

    return 0;
}

// ------------------------------------------------------------------------------------------
// from libopenmpt example

static ssize_t xwrite( int fd, const void * buffer, size_t size ) {
  size_t written = 0;
  ssize_t retval = 0;
  while ( written < size ) {
    retval = write( fd, (const char *)buffer + written, size - written );
    if ( retval < 0 ) {
      if ( errno != EINTR ) {
        break;
      }
      retval = 0;
    }
    written += retval;
  }
  return written;
}

// ------------------------------------------------------------------------------------------

int main(int argc, char *argv[]) {

    // parse command line
    if (int e = cmdLine(argc, argv)) { return e; }

    srandom(time(NULL)); // seed the random generator

    // open socket and create our canvas
    const int socket = OpenFlaschenTaschenSocket(opt_hostname);
    UDPFlaschenTaschen canvas(socket, opt_width, opt_height);
    canvas.Clear();

/*
    // pixel buffer
    uint8_t pixels[ opt_width * opt_height ];
    for (int i=0; i < opt_width * opt_height; i++) { pixels[i] = 0; }  // clear pixel buffer

    // color palette
    Color palette[256];
    int curPalette = (opt_palette < 0) ? 1 : opt_palette;
    setPalette(curPalette, palette);
//*/

    // handle break
    signal(SIGTERM, InterruptHandler);
    signal(SIGINT, InterruptHandler);

    // other vars
    int count=0;
    time_t starttime = time(NULL);

    // setup MOD player
    FILE *modfile = NULL;
    openmpt_module *mod = NULL;
    size_t mod_count = 0;
    size_t mod_written = 0;
    PaError pa_error = paNoError;
    PaStream *audio_stream = NULL;

    fprintf(stderr, "Playing: %s\n", opt_filepath);

    modfile = fopen(opt_filepath, "rb");
    if (!modfile) {
        fprintf(stderr, "Error: Couldn't open file or wrong path.\n");
        return 1;
    }

    mod = openmpt_module_create(openmpt_stream_get_file_callbacks(), modfile, NULL, NULL, NULL);
    fclose(modfile);
    if (!mod) {
        fprintf(stderr, "Error: Not a MOD file?\n");
        return 1;
    }

    // PortAudio
    if (opt_output == kOutPA) {
        pa_error = Pa_Initialize();
        if (pa_error != paNoError) {
            fprintf(stderr, "Error: PortAudio init failed.\n");
            return 1;
        }
        pa_error = Pa_OpenDefaultStream(&audio_stream, 0, 2, paInt16 | paNonInterleaved, SAMPLERATE, 
            paFramesPerBufferUnspecified, NULL, NULL);
        if ( !((pa_error == paNoError) && audio_stream) ) {
            fprintf(stderr, "Error: PortAudio opening stream failed.\n");
            return 1;
        }
        pa_error = Pa_StartStream(audio_stream);
        if (pa_error != paNoError) {
            fprintf(stderr, "Error: PortAudio starting stream failed.\n");
            return 1;
        }
    }

    do {
    /*
        // copy pixel buffer to canvas
        int dst = 0;
        for (int y=0; y < opt_height; y++) {
            for (int x=0; x < opt_width; x++) {
                canvas.SetPixel( x, y, palette[ pixels[dst] ] );
                dst++;
            }
        }
    //*/

    /*
        // send canvas
        canvas.SetOffset(opt_xoff, opt_yoff, opt_layer);
        canvas.Send();
        usleep(opt_delay * 1000);
    //*/

        // play mod
        if (opt_output == kOutPA) {
            mod_count = openmpt_module_read_stereo(mod, SAMPLERATE, BUFFERSIZE, audio_left, audio_right);
            if (mod_count == 0) { break; }
            pa_error = Pa_WriteStream(audio_stream, audio_buffers, (unsigned long)mod_count);
            if (pa_error == paOutputUnderflowed) {
                // not an error
            }
            else if (pa_error != paNoError) {
                fprintf(stderr, "Error: Writing to stream failed.\n");
                interrupt_received = true;
            }
        }
        else if (opt_output == kOutSTDOUT) {
            mod_count = openmpt_module_read_interleaved_stereo(mod, SAMPLERATE, BUFFERSIZE, audio_buffer);
            if (mod_count == 0) { break; }
            mod_written = xwrite( STDOUT_FILENO, audio_buffer, mod_count * 2 * sizeof(int16_t) );
            if (mod_written == 0) {
                fprintf(stderr, "Error: Writing audio to STDOUT failed.\n");
                interrupt_received = true;
            }
        }

        // print info
        fprintf(stderr, "%d \r", count);

        count++;
        if (count == INT_MAX) { count=0; }

    } while ( (difftime(time(NULL), starttime) <= opt_timeout) && !interrupt_received );

    fprintf(stderr, "\nDone.\n");

    // cleanup MOD player
    if (opt_output == kOutPA) {
        if (audio_stream) {
            if ( Pa_IsStreamActive(audio_stream) == 1 ) {
                Pa_StopStream(audio_stream);
            }
            Pa_CloseStream(audio_stream);
            audio_stream = NULL;
        }
        Pa_Terminate();
    }
    if (mod) {
        openmpt_module_destroy(mod);
        mod = NULL;
    }

    // clear canvas on exit
    canvas.Clear();
    canvas.Send();
    
    if (interrupt_received) return 1;
    return 0;
}
