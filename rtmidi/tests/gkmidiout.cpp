//*****************************************//
//  midiout.cpp
//  by Gary Scavone, 2003-2004.
//
//  Simple program to test MIDI output.
//
//*****************************************//

#include <iostream>
#include <cstdlib>
#include "RtMidi.h"

// Platform-dependent sleep routines.
#if defined(__WINDOWS_MM__)
#include <windows.h>
#define SLEEP( milliseconds ) Sleep( (DWORD) milliseconds ) 
#else // Unix variants
#include <unistd.h>
#define SLEEP( milliseconds ) usleep( (unsigned long) (milliseconds * 1000.0) )
#endif

int main( int argc, char *argv[] )
{
  RtMidiOut *midiout = 0;
  std::vector<unsigned char> message;
  int i;

  // RtMidiOut constructor
  try {
    midiout = new RtMidiOut();
  }
  catch ( RtError &error ) {
    error.printMessage();
    exit( EXIT_FAILURE );
  }


  midiout->openVirtualPort("gkmidi1");

  // Send out a series of MIDI messages.

  message.push_back(0xE8);
  message.push_back(0x00);
  message.push_back(0x00);

  for (i = 0; i < 16384; i+=128) {
    message[1]=(i+0x0000)%128;
    message[2]=(i+0x0000)/128;
    midiout->sendMessage( &message );
    printf("sent %2x %2x %2x\n",message[0],message[1],message[2]);
    SLEEP(1000);
  }

  // Clean up
  delete midiout;

  return 0;
}

