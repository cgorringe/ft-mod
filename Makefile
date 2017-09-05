FLASCHEN_TASCHEN_API_DIR = ft/api

CXXFLAGS = -Wall -O3 -I$(FLASCHEN_TASCHEN_API_DIR)/include -I.
LDFLAGS = -L$(FLASCHEN_TASCHEN_API_DIR)/lib -lftclient
FTLIB = $(FLASCHEN_TASCHEN_API_DIR)/lib/libftclient.a

## test ##
# CPPFLAGS_OPENMPT123 += $(CPPFLAGS_SDL2) $(CPPFLAGS_SDL) $(CPPFLAGS_PORTAUDIO) $(CPPFLAGS_PULSEAUDIO) $(CPPFLAGS_FLAC) $(CPPFLAGS_SNDFILE)
# LDFLAGS_OPENMPT123  += $(LDFLAGS_SDL2) $(LDFLAGS_SDL) $(LDFLAGS_PORTAUDIO) $(LDFLAGS_PULSEAUDIO) $(LDFLAGS_FLAC) $(LDFLAGS_SNDFILE)
# LDLIBS_OPENMPT123   += $(LDLIBS_SDL2) $(LDLIBS_SDL) $(LDLIBS_PORTAUDIO) $(LDLIBS_PULSEAUDIO) $(LDLIBS_FLAC) $(LDLIBS_SNDFILE)
# LDLIBS_LIBOPENMPT  += -lopenmpt
#LDLIBS   += -lportaudio
#LDLIBS   += -lFLAC
#LDLIBS   += -lsndfile

LDFLAGS += -lportaudio -lopenmpt

ALL = ft-mod

all : $(ALL)

% : %.cc $(FTLIB)
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

$(FTLIB):
	make -C $(FLASCHEN_TASCHEN_API_DIR)/lib

clean:
	rm -f $(ALL)