PROGRAMS = x32midiosc
RM = /bin/rm
SRC_PATH = rtmidi
INCLUDE = rtmidi
OBJECT_PATH = .
vpath %.o $(OBJECT_PATH)

OBJECTS	 = RtMidi.o

CC       = g++
DEFS     = -D__MACOSX_CORE__ -DOS_IS_MACOSX=1 
CFLAGS   = -O0 -Wall -g -GGDB
CFLAGS  += -I$(INCLUDE) -I$(INCLUDE)/include
LIBRARY  = -framework CoreMIDI -framework CoreFoundation -framework CoreAudio

%.o : $(SRC_PATH)/%.cpp
	$(CC) $(CFLAGS) $(DEFS) -c $(<) -o $(OBJECT_PATH)/$@

all : $(PROGRAMS)

x32midiosc : x32midiosc.cpp $(OBJECTS)
	$(CC) $(CFLAGS) $(DEFS) -o x32midiosc x32midiosc.cpp $(OBJECT_PATH)/RtMidi.o $(LIBRARY)

clean : 
	$(RM) -f $(PROGRAMS)
	$(RM) -f *.o
	$(RM) -f *~

strip : 
	strip $(PROGRAMS)
