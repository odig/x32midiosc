static int localport = 10000;
static int remoteport = 10023;
static char remoteIP[32];

static bool debug=false;
static bool debugLock=false;

static bool noToggle=false;

///////////////////////////////////////////////////////////////////////////////
//OS dependent includes
///////////////////////////////////////////////////////////////////////////////

#if OS_IS_LINUX == 1 || OS_IS_MACOSX == 1 || OS_IS_CYGWIN == 1
#include <unistd.h>             //  usleep
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <signal.h>
#elif OS_IS_WIN32 == 1
#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include "stdint.h"
#define socklen_t   int
#define snprintf _snprintf
#else
#error "Invalid Platform"
#endif

///////////////////////////////////////////////////////////////////////////////
//general includes
///////////////////////////////////////////////////////////////////////////////

#include <iostream>
#include <stdlib.h>
#include <cstdlib>
#include <string.h>


#include <RtMidi.h>

// Platform-dependent sleep routines.                                                                                                                                
#if OS_IS_LINUX == 1 || OS_IS_MACOSX == 1 || OS_IS_CYGWIN == 1
#include <unistd.h>
#define SLEEP( milliseconds ) usleep( (unsigned long) (milliseconds * 1000.0) )
#else
#include <windows.h>
#define SLEEP( milliseconds ) Sleep( (DWORD) milliseconds )
#endif

///////////////////////////////////////////////////////////////////////////////
//defines
///////////////////////////////////////////////////////////////////////////////

#define PROTOCOL_UDP            17
#define MAX_RX_UDP_PACKET       2048
#define MAX_MIDI_PORT           9
#define MAX_CHANNELS            (((MAX_MIDI_PORT)*8)+1)

int maxMidiPort = MAX_MIDI_PORT;
int maxChannels;
bool useAllPorts=false;

typedef enum 
{
    NO_LOCK,
    OSC_LOCK,
    MIDI_LOCK
} CHANNEL_LOCKER;

typedef struct $
{
    CHANNEL_LOCKER channelLocker;
    int lockCount;
} CHANNEL_LOCK;

static CHANNEL_LOCK channelLock[MAX_CHANNELS];
static int muteState[MAX_CHANNELS];
static int soloState[MAX_CHANNELS];
static int selectState[MAX_CHANNELS];

const char *channelNameIn[] = {
    "ToX32_C1-8",
    "ToX32_C9-16",
    "ToX32_C17-24",
    "ToX32_C25-32",
    "ToX32_B1-08",
    "ToX32_B9-16",
    "ToX32_A1-8",
    "ToX32_A9-16",
    "ToX32_M1-6+M"
};

const char *channelNameOut[] = {
    "FromX32_C1-8",
    "FromX32_C9-16",
    "FromX32_C17-24",
    "FromX32_C25-32",
    "FromX32_B1-08",
    "FromX32_B9-16",
    "FromX32_A1-8",
    "FromX32_A9-16",
    "FromX32_M1-6+M"
};

const char *CLIENT_HELP_STR = 
    "\n"
    " x32midiosc Version 0.03 help\n"
    "\n"
    " Invoking \"x32midiosc\":\n"
    " x32midiosc [local port] [X32 port] [X32 IP] (use virtual ports; OSX only)\n"
    " x32midiosc --list (Gives a list of available midi ports)\n"
    " x32midiosc [local port] [X32 port] [X32 IP] [in1] .. [in6] [out1] .. [out6]\n"
    "   [inX] and [outx] MIDI port number x32midiosc read from and DAW writes to.\n"
    "   according to the 'x32midiosc --list' output\n"
    "   [in1]  receiving MIDI data on this port will be mapped to  CH01-08 on X32\n"
    "   [in2]  receiving MIDI data on this port will be mapped to  CH09-16 on X32\n"
    "   [in3]  receiving MIDI data on this port will be mapped to  CH17-24 on X32\n"
    "   [in4]  receiving MIDI data on this port will be mapped to  CH25-32 on X32\n"
    "   [in5]  receiving MIDI data on this port will be mapped to Bus01-08 on X32\n"
    "   [in6]  receiving MIDI data on this port will be mapped to Bus09-16 on X32\n"
	"          plus Master Fader on X32\n"
    "   \n"
    "   [out1] CH01-08 on X32 will be sending MIDI data to this port\n"
    "   [out2] CH09-16 on X32 will be sending MIDI data to this port\n"
    "   [out3] CH17-24 on X32 will be sending MIDI data to this port\n"
    "   [out4] CH25-32 on X32 will be sending MIDI data to this port\n"
    "   [out5] BUS01-08 on X32 will be sending MIDI data to this port\n"
    "   [out6] BUS09-16 on X32 and Master Fader will be sending\n"
	"          MIDI data to this port\n"
    "   \n"
    "   Sample: x32midiosc 10000 10023 192.168.1.2 0 1 2 3 4 5 6 7 8 9 10 11\n"
    "   \n"
    " x32midiosc [local port] [X32 port] [X32 IP] [in1] .. [in9] [out1] .. [out9]\n"
    "   [inX] and [outx] MIDI port number x32midiosc read from and DAW writes to.\n"
    "   according to the 'x32midiosc --list' output\n"
    "   [in1]  receiving MIDI data on this port will be mapped to  CH01-08 on X32\n"
    "   [in2]  receiving MIDI data on this port will be mapped to  CH09-16 on X32\n"
    "   [in3]  receiving MIDI data on this port will be mapped to  CH17-24 on X32\n"
    "   [in4]  receiving MIDI data on this port will be mapped to  CH25-32 on X32\n"
    "   [in5]  receiving MIDI data on this port will be mapped to Bus01-08 on X32\n"
    "   [in6]  receiving MIDI data on this port will be mapped to Bus09-16 on X32\n"
	"          plus Master Fader on X32\n"
    "   [in7]  receiving MIDI data on this port will be mapped to Aux01-08 on X32\n"
    "   [in8]  receiving MIDI data on this port will be mapped to Aux09-16 on X32\n"
    "   [in9]  receiving MIDI data on this port will be mapped to Matrix 01-08 on X32\n"
    "   \n"
    "   [out1] CH01-08 on X32 will be sending MIDI data to this port\n"
    "   [out2] CH09-16 on X32 will be sending MIDI data to this port\n"
    "   [out3] CH17-24 on X32 will be sending MIDI data to this port\n"
    "   [out4] CH25-32 on X32 will be sending MIDI data to this port\n"
    "   [out5] BUS01-08 on X32 will be sending MIDI data to this port\n"
    "   [out6] BUS09-16 on X32 and Master Fader will be sending\n"
	"          MIDI data to this port\n"
    "   [out7] AUX01-08 on X32 will be sending MIDI data to this port\n"
    "   [out8] AUX09-16 on X32 will be sending MIDI data to this port\n"
    "   [out9] MATRIX01-08 on X32 will be sending MIDI data to this port\n"
    "   \n"
    "   Sample: x32midiosc 10000 10023 172.17.100.2 9 10 11 12 13 14 15 16 17 1 2 3 4 5 6 7 8 9\n"

	
    "\n";

#if OS_IS_LINUX == 1 || OS_IS_MACOSX == 1 || OS_IS_CYGWIN == 1
    socklen_t addressSize = sizeof(sockaddr_in);
#else
    int addressSize = sizeof(sockaddr_in);
#endif

static volatile bool doExit=false;

#define MAX_POLL_ANSWER 16

typedef struct {
    int udpSocket;
    int channel;
    RtMidiIn *midiin;
    RtMidiOut *midiout;
    volatile bool sendPollAnswer[MAX_POLL_ANSWER];
    char display[8*7*2];
} midiInfo_t;


void dumpBuffer(char *buffer, size_t bufferLen)
{
    size_t dataIndex=0;
    size_t totalIndex=0;

    while (dataIndex<bufferLen)
    {
        printf("\t\t");
        for (dataIndex = totalIndex; dataIndex<totalIndex+16 && dataIndex < bufferLen; dataIndex++) 
        {
            printf("----");
        }
        printf("\n\t\t");
        for (dataIndex = totalIndex; dataIndex<totalIndex+16 && dataIndex < bufferLen; dataIndex++) 
        {
            printf("%03lu ",dataIndex+1);
        }
        printf("\n\t\t");
        for (dataIndex = totalIndex; dataIndex<totalIndex+16 && dataIndex < bufferLen; dataIndex++) 
        {
            printf("  %c ",(buffer[dataIndex] >= ' ') && (buffer[dataIndex] <  127)?buffer[dataIndex]:(uint8_t)'.');
        }
        printf("\n\t\t");
        for (dataIndex = totalIndex; dataIndex<totalIndex+16 && dataIndex < bufferLen; dataIndex++) 
        {
            printf(" %02X ",(unsigned char) buffer[dataIndex]);
        }
        printf("\n");

        totalIndex+=16;
    }    
}

#if OS_IS_LINUX == 1 || OS_IS_MACOSX == 1 || OS_IS_CYGWIN == 1
void signalHandler(int s)
{
    printf("Caught signal %d\n",s);
    doExit=true;
}
#endif

void registerSignalHandler(void)
{
#if OS_IS_LINUX == 1 || OS_IS_MACOSX == 1 || OS_IS_CYGWIN == 1
   struct sigaction sigIntHandler;

   sigIntHandler.sa_handler = signalHandler;
   sigemptyset(&sigIntHandler.sa_mask);
   sigIntHandler.sa_flags = 0;

   sigaction(SIGINT, &sigIntHandler, NULL);
#else
	return;
#endif
}
bool lockChannel(int channel, CHANNEL_LOCKER channelLocker)
{
    if(channel>=MAX_CHANNELS)
    {
        return false;  
    }

    if(channelLock[channel].channelLocker==NO_LOCK || channelLock[channel].channelLocker==channelLocker)
    {
        channelLock[channel].channelLocker=channelLocker;
        channelLock[channel].lockCount=5;
        if(debugLock) printf("\t\t[%d] LOCKED for %s\n",channel+1,channelLock[channel].channelLocker==MIDI_LOCK?"MIDI":"OSC");
        return true;
    }

    return false;
}

void lockChannelHandler(void)
{
    for (int i=0;i<MAX_CHANNELS;i++)
    {
        if(channelLock[i].channelLocker!=NO_LOCK)
        {
            if(debugLock) printf("\t\t[%d]=%d %s\n",i+1,channelLock[i].lockCount,channelLock[i].channelLocker==MIDI_LOCK?"MIDI":"OSC");
            channelLock[i].lockCount--;
            if(channelLock[i].lockCount<=0)
            {
                channelLock[i].channelLocker=NO_LOCK;
                channelLock[i].lockCount=0;       
            }
        }
    }
}

void lockChannelClear(void)
{
    for (int i=0;i<MAX_CHANNELS;i++)
    {
        channelLock[i].channelLocker=NO_LOCK;
        channelLock[i].lockCount=0;       
    }
}

int networkInit(int port)
{
    int udpSocket;
    int err;
    const int REUSE_TRUE = 1, BROADCAST_TRUE = 1;
    struct sockaddr_in receiveAddress;

#if OS_IS_WIN32 == 1
    //fucking windows winsock startup
    WSADATA wsa;
    err = WSAStartup(MAKEWORD(2, 0), &wsa);
    if (err != 0)
    {
        printf("Error starting Windows udpSocket subsystem.\n");
        return err;
    }
#endif

    //create udpSocket
    udpSocket = socket(AF_INET, SOCK_DGRAM, PROTOCOL_UDP);
    if (udpSocket < 0)
    {
        printf("Create udpSocket error.");
        return udpSocket;
    }
    
    //initialize server address to localhost:port
    receiveAddress.sin_family = AF_INET;
    receiveAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    receiveAddress.sin_port = htons(port);

    //set udpSocket to reuse the address
    err = setsockopt(udpSocket, SOL_SOCKET, SO_REUSEADDR, (const char *) &REUSE_TRUE, sizeof(REUSE_TRUE));
    if (err != 0)
    {
        printf("Error setting udpSocket reuse.");
        return err;
    }

    //enable broadcasting for this
    err = setsockopt(udpSocket, SOL_SOCKET, SO_BROADCAST, (const char *) &BROADCAST_TRUE, sizeof(BROADCAST_TRUE));
    if (err != 0)
    {
        printf("Error setting udpSocket broadcast.");
        return err;
    }
    
    //disable blocking, polling is used.
#if OS_IS_LINUX == 1 || OS_IS_MACOSX == 1 || OS_IS_CYGWIN == 1
    err = fcntl(udpSocket, F_SETFL, O_NONBLOCK);
#elif OS_IS_WIN32 == 1
    unsigned long val = 1;
    err = ioctlsocket(udpSocket, FIONBIO, &val);
#endif
    if (err != 0)
    {
        printf("Error setting udpSocket unblock.");
        return err;
    }

    //bind for listening
    err = bind(udpSocket, (struct sockaddr *) &receiveAddress, addressSize);
    if (err != 0)
    {
        printf("Error udpSocket bind.");
        return err;
    }
    return udpSocket;
}

void networkSend(int udpSocket, struct sockaddr_in *address, const uint8_t *data, int dataLen)
{
#if OS_IS_LINUX == 1 || OS_IS_MACOSX == 1 || OS_IS_CYGWIN == 1
    int actSend = sendto(udpSocket,
                         data,
                         dataLen,
                         0,
                         (struct sockaddr*) address,
                         addressSize);
#else
    int actSend = sendto(udpSocket,
                         (const char *) data,
                         dataLen,
                         0,
                         (struct sockaddr*) address,
                         addressSize);
#endif
    //check if transmission was successful 
    if (dataLen != actSend)
    {
        printf("Error sending packet.");
    }
}


int networkReceive(int udpSocket, uint8_t *data)
{
    struct sockaddr_in receiveAddress;

    receiveAddress.sin_family = AF_INET;
    receiveAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    receiveAddress.sin_port = htons(0);

    //receive from network
#if OS_IS_LINUX == 1 || OS_IS_MACOSX == 1 || OS_IS_CYGWIN == 1
    int received = recvfrom(udpSocket,
                             data,
                             MAX_RX_UDP_PACKET,
                             0,
                             (struct sockaddr *) &receiveAddress,
                             (socklen_t *) & addressSize);
#else
    int received = recvfrom(udpSocket,
                             (char *) data,
                             MAX_RX_UDP_PACKET,
                             0,
                             (struct sockaddr *) &receiveAddress,
                             & addressSize);
#endif

    return received;
}

int networkHalt(int udpSocket)
{
    //close udpSocket...
#if OS_IS_LINUX == 1 || OS_IS_MACOSX == 1 || OS_IS_CYGWIN == 1
    close(udpSocket);
#elif OS_IS_WIN32 == 1
    closesocket(udpSocket);
    WSACleanup();
#endif
    return 0;
}



typedef struct {
    char address[256];
    
    char sPar[128][256];
    int iPar[128];
    float fPar[128];
    bool bPar[128];

    int sCount;
    int iCount;
    int fCount;
    int bCount;
} OSCSTRUCT;

typedef enum {
    OSC_ADDRESS,
    OSC_PARAMETRER_DESC,
    OSC_PARAMETER
} OSC_DECODE_STATE;

void printOSC(OSCSTRUCT *osc)
{
    int i;
    printf("%s\n",osc->address);

    for (i=0;i<osc->iCount;i++)
    {
        printf("\t%i\n",osc->iPar[i]);
    }

    for (i=0;i<osc->fCount;i++)
    {
        printf("\t%f\n",osc->fPar[i]);
    }

    for (i=0;i<osc->sCount;i++)
    {
        printf("\t%s\n",osc->sPar[i]);
    }

    for (i=0;i<osc->bCount;i++)
    {
        printf("\t%i\n",osc->bPar[i]);
    }
}

int decodeOsc(const uint8_t *buffer, int bufferLen, OSCSTRUCT *osc, char *debugString, size_t debugStringLen)
{
    OSC_DECODE_STATE mode=OSC_ADDRESS;
    int dataIndex;

    uint8_t parameterBytes[128];
    size_t parameterIndex=0;
    size_t parameterLength;

    osc->address[0]='\0';
    osc->sCount=0;
    osc->iCount=0;
    osc->fCount=0;
    osc->bCount=0;

    debugString[0]='\0';

    if(debug)
    {
        printf("\n");
        for (dataIndex = 0; dataIndex < bufferLen; dataIndex++) 
        {
            printf("---");
        }
        printf("\n");
        for (dataIndex = 0; dataIndex < bufferLen; dataIndex++) 
        {
            printf("%02d ",dataIndex+1);
        }
        printf("\n");
        for (dataIndex = 0; dataIndex < bufferLen; dataIndex++) 
        {
            printf(" %c ",(buffer[dataIndex] >= (uint8_t)' ') && (buffer[dataIndex] < (uint8_t) 127)?buffer[dataIndex]:(uint8_t)'.');
        }
        printf("\n");
        for (dataIndex = 0; dataIndex < bufferLen; dataIndex++) 
        {
            printf("%02X ",buffer[dataIndex]);
        }
        printf("\n");
    }

    for (dataIndex = 0; dataIndex < bufferLen; dataIndex++) 
    {
        uint8_t     ch;
        
        ch = buffer[dataIndex];
        

        if (mode==OSC_ADDRESS)
        {
            if (ch == 10) 
            {
                strncat(debugString,"\n",debugStringLen);
            } 
            else if (ch == 13) 
            {
                strncat(debugString,"\r",debugStringLen);
            } 
            else if (ch == '"') 
            {
                strncat(debugString,"\\\"",debugStringLen);
            } 
            else if (ch == '\\') 
            {
                strncat(debugString,"\\\\",debugStringLen);
            } 
            else if ( (ch >= ' ') && (ch < 127) ) 
            {
                char c[2];
                c[0]=ch;
                c[1]='\0';
                strncat(debugString,c,debugStringLen);
            } 
            else if (ch == 0) 
            {
                if (dataIndex<256-1)
                {
                    memcpy(osc->address,buffer,dataIndex);
                    osc->address[dataIndex]='\0';
                }
                dataIndex += 3-(dataIndex%4);
                mode=OSC_PARAMETRER_DESC;
            } 
            else 
            {
                snprintf(debugString+strlen(debugString),debugStringLen-strlen(debugString),"\\x%02x", (unsigned int) ch);
            }
        }
        else if (mode==OSC_PARAMETRER_DESC)
        {
            if (ch == 10) 
            {
                strncat(debugString,"\n",debugStringLen);
            } 
            else if (ch == 13) 
            {
                strncat(debugString,"\r",debugStringLen);
            } 
            else if (ch == 'i' || ch=='f' || ch=='s' || ch=='b') 
            {
                char c[2];
                c[0]=ch;
                c[1]='\0';

                if (parameterIndex<128-1)
                {
                    parameterBytes[parameterIndex++]=ch;
                }
                snprintf(debugString+strlen(debugString),debugStringLen-strlen(debugString),"%c", (unsigned int) ch);
            } 
            else if (ch == '"') 
            {
                strncat(debugString,"\\\"",debugStringLen);
            } 
            else if (ch == '\\') 
            {
                strncat(debugString,"\\\\",debugStringLen);
            } 
            else if ( (ch >= ' ') && (ch < 127) ) 
            {
                char c[2];
                c[0]=ch;
                c[1]='\0';
                strncat(debugString,c,debugStringLen);
            } 
            else if (ch == 0) 
            {
                parameterBytes[parameterIndex]='\0';
                dataIndex += 3-(dataIndex%4);
                mode=OSC_PARAMETER;
                parameterLength = strlen((const char *) parameterBytes);
                parameterIndex = 0;
            } 
            else 
            {
                snprintf(debugString+strlen(debugString),debugStringLen-strlen(debugString),"\\x%02x", (unsigned int) ch);
            }
        }
        else if (mode==OSC_PARAMETER)
        {
            for (parameterIndex=0; parameterIndex < parameterLength; parameterIndex++) 
            {
                if (parameterBytes[parameterIndex]=='i') 
                {
                    uint32_t i;
                    uint8_t *p = (uint8_t *) &i;
                    
                    *(p+0)=buffer[dataIndex+3];
                    *(p+1)=buffer[dataIndex+2];
                    *(p+2)=buffer[dataIndex+1];
                    *(p+3)=buffer[dataIndex+0];
                    
                    if(debug)
                    {
                        int index;
                        printf("\n");
                        for (index=dataIndex; index < dataIndex+4; index++) 
                        {
                            printf("---");
                        }
                        printf("\n");
                        for (index=dataIndex; index < dataIndex+4; index++) 
                        {
                            printf("%02d ",index+1);
                        }
                        printf("\n");
                        for (index=dataIndex; index < dataIndex+4; index++) 
                        {
                            printf(" %c ",(buffer[index] >= (uint8_t)' ') && (buffer[index] < (uint8_t) 127)?buffer[index]:(uint8_t)'.');
                        }
                        printf("\n");
                        for (index=dataIndex; index < dataIndex+4; index++) 
                        {
                            printf("%02X ",buffer[index]);
                        }
                        printf("\n");
                        printf("val=%d\n",i);
                     }

                    snprintf(debugString+strlen(debugString),debugStringLen-strlen(debugString)," %d", i);
                    dataIndex+=3;
                    osc->iPar[osc->iCount++]=i;
                }
                else if (parameterBytes[parameterIndex]=='f') 
                {
                    float z;
                    uint8_t *p = (uint8_t *) &z;
                    
                    *(p+0)=buffer[dataIndex+3];
                    *(p+1)=buffer[dataIndex+2];
                    *(p+2)=buffer[dataIndex+1];
                    *(p+3)=buffer[dataIndex+0];
                    
                    snprintf(debugString+strlen(debugString),debugStringLen-strlen(debugString)," %f", z);
                    dataIndex+=3;

                    osc->fPar[osc->fCount++]=z;
                }
                else if (parameterBytes[parameterIndex]=='s') 
                {
                    strncat(debugString," ",debugStringLen);

                    osc->sPar[osc->sCount][0] = '\0';

                    for (; dataIndex < bufferLen; dataIndex++) 
                    {
                        ch = buffer[dataIndex];

                        if (ch == 10) 
                        {
                            strncat(debugString,"\n",debugStringLen);
                            strncat(osc->sPar[osc->sCount],"\n",256);
                        } 
                        else if (ch == 13) 
                        {
                            strncat(debugString,"\r",debugStringLen);
                            strncat(osc->sPar[osc->sCount],"\r",256);
                        } 
                        else if (ch == '"') 
                        {
                            strncat(debugString,"\\\"",debugStringLen);
                            strncat(osc->sPar[osc->sCount],"\\\"",256);
                        } 
                        else if (ch == '\\') 
                        {
                            strncat(debugString,"\\\\",debugStringLen);
                            strncat(osc->sPar[osc->sCount],"\\\\",256);
                        } 
                        else if ( (ch >= ' ') && (ch < 127) ) 
                        {
                            char c[2];
                            c[0]=ch;
                            c[1]='\0';
                            strncat(debugString,c,debugStringLen);
                            strncat(osc->sPar[osc->sCount],c,256);

                        } 
                        else if (ch == 0) 
                        {
                            dataIndex += ((dataIndex+1)%4);
                        } 
                        else 
                        {
                            snprintf(debugString+strlen(debugString),debugStringLen-strlen(debugString),"\\x%02x", (unsigned int) ch);
                            snprintf(osc->sPar[osc->sCount]+strlen(osc->sPar[osc->sCount]),256-strlen(osc->sPar[osc->sCount]),"\\x%02x", (unsigned int) ch);
                        }
                    }
                }
                else if (parameterBytes[parameterIndex]=='b') {
                    
                }
            }
        }
    }
    
    return 0;
}

void midiSendPitchBand(midiInfo_t midiInfo[], int midiInterface, int channel,float f)
{
    uint16_t val;


    int channelNumber = (channel+((midiInterface)*8));
    if (lockChannel(channelNumber,OSC_LOCK))
    {
        //val = f * 0x3fff;
        val = f * 0x3a7b;

        std::vector<unsigned char> message;
        message.push_back(0xE0+(channel));
        message.push_back(val&0x7f);
        message.push_back((val>>7)&0x7f);


        try 
        {
            midiInfo[midiInterface].midiout->sendMessage( &message );
            printf("[%02d][%d] sent PitchBand to DAW %02x %02x %02x (%4x)\n",channelNumber+1,midiInterface+1,message[0],message[1],message[2], val);
            message.clear();
        }
        catch ( RtError &error ) 
        {
            error.printMessage();
        }
    }
    else
    {
        if(debugLock) printf("\t\t[%02d] LOCKED from Midi\n",channelNumber+1);
    }
}

void midiSendPan(midiInfo_t midiInfo[], int midiInterface, int channel,float f)
{
    uint16_t val;


    if (useAllPorts || midiInterface < maxMidiPort)
    {
        int channelNumber = (channel+((midiInterface)*8));
        if (true) //lockChannel(channelNumber,OSC_LOCK))
        {
            val = (f * 127)+1;

            std::vector<unsigned char> message;
            message.push_back(0xB0);
            message.push_back(0x10+(channel));
            message.push_back(val&0x7f);

            try 
            {
                midiInfo[midiInterface].midiout->sendMessage( &message );
                printf("[%02d][%d] sent Pan %02x %02x %02x (%4x)\n",channelNumber+1,midiInterface+1,message[0],message[1],message[2], val);
                message.clear();
            }
            catch ( RtError &error ) 
            {
                error.printMessage();
            }
        }
        else
        {
            if(debugLock) printf("\t\t[%02d] LOCKED from Midi\n",channelNumber+1);
        }
    }
}

void midiSendNoteOn(int channelNumber, midiInfo_t midiInfo[], int midiInterface, int channel, int note, int velocity)
{
    std::vector<unsigned char> message;
    message.push_back(0x90+(channel));
    message.push_back(note&0x7f);
    message.push_back(velocity&0x7f);

    try 
    {
        midiInfo[midiInterface].midiout->sendMessage( &message );
        printf("[%02d][%d] sent NoteOn to DAW %02x %02x %02x\n",channelNumber+1,midiInterface+1,message[0],message[1],message[2]);
        message.clear();
    }
    catch ( RtError &error ) 
    {
        error.printMessage();
    }
}

void midiSendNoteOnForMute(midiInfo_t midiInfo[], int midiInterface, int channel, int note, int velocity)
{
    int channelNumber = ((note&0x0f)+((midiInterface)*8));
    int val=0x7f;

    if(noToggle)
    {
        //LOGIC has invertet logic on receive 0x7F is enbale mute
        val = velocity?0:0x7f;
    }

    if (noToggle || muteState[channelNumber]!=velocity)
    {
        midiSendNoteOn(channelNumber, midiInfo, midiInterface, channel, note, val);
    }
}   

void midiSendNoteOnForMuteNoToggle(midiInfo_t midiInfo[], int midiInterface, int channel, int note, int velocity)
{
    int channelNumber = ((note&0x0f)+((midiInterface)*8));
    int val=0x7f;

    //LOGIC has invertet logic on receive 0x7F is enbale mute
    val = velocity?0:0x7f;

    midiSendNoteOn(channelNumber, midiInfo, midiInterface, channel, note, val);
}   

void midiSendNoteOnForSolo(int channelNumber, midiInfo_t midiInfo[], int midiInterface, int channel, int note, int velocity)
{
    int val=0x7f;

    if(noToggle)
    {
        val = velocity?0x7f:0;
    }

    if (noToggle || soloState[channelNumber]!=velocity)
    {
        midiSendNoteOn(channelNumber, midiInfo, midiInterface, channel, note, val);
    }
}   

void midiSendNoteOnForSelect(int channelNumber, midiInfo_t midiInfo[], int midiInterface, int channel, int note, int velocity)
{
    int val=0x7f;

    printf("\t\tselectState[%d] is %02x\n",channelNumber,selectState[channelNumber]);
    if (noToggle || selectState[channelNumber]==0)
    {
        midiSendNoteOn(channelNumber, midiInfo, midiInterface, channel, note, val);
    }
}   

void midiSendNoteBankSwitch(midiInfo_t midiInfo[], int channel, int note, int velocity)
{
    int midiInterface=-1;
    
    midiSendNoteOn(0, midiInfo, 0, channel, note, velocity);
    return;

    for(int i=0;i<maxChannels-1;i++)
    {
        if(selectState[i]!=0)
        {
            midiInterface=i/8;
            midiSendNoteOn(i, midiInfo, midiInterface, channel, note, velocity);
        }
    }
}   

void midiSendNoteBankSwitchInit(midiInfo_t midiInfo[], int midiInterface, int channel, int note, int velocity)
{
    midiSendNoteOn(midiInterface*8, midiInfo, midiInterface, channel, note, velocity);
}   

void mapOSC(OSCSTRUCT *osc, midiInfo_t midiInfo[])
{
    //map faders
    if(!strcmp("/ch/01/mix/fader",osc->address) && osc->fCount>0)   {midiSendPitchBand(midiInfo,0,0,osc->fPar[0]); return;}
    if(!strcmp("/ch/02/mix/fader",osc->address) && osc->fCount>0)   {midiSendPitchBand(midiInfo,0,1,osc->fPar[0]); return;}
    if(!strcmp("/ch/03/mix/fader",osc->address) && osc->fCount>0)   {midiSendPitchBand(midiInfo,0,2,osc->fPar[0]); return;}
    if(!strcmp("/ch/04/mix/fader",osc->address) && osc->fCount>0)   {midiSendPitchBand(midiInfo,0,3,osc->fPar[0]); return;}
    if(!strcmp("/ch/05/mix/fader",osc->address) && osc->fCount>0)   {midiSendPitchBand(midiInfo,0,4,osc->fPar[0]); return;}
    if(!strcmp("/ch/06/mix/fader",osc->address) && osc->fCount>0)   {midiSendPitchBand(midiInfo,0,5,osc->fPar[0]); return;}
    if(!strcmp("/ch/07/mix/fader",osc->address) && osc->fCount>0)   {midiSendPitchBand(midiInfo,0,6,osc->fPar[0]); return;}
    if(!strcmp("/ch/08/mix/fader",osc->address) && osc->fCount>0)   {midiSendPitchBand(midiInfo,0,7,osc->fPar[0]); return;}
    if(!strcmp("/ch/09/mix/fader",osc->address) && osc->fCount>0)   {midiSendPitchBand(midiInfo,1,0,osc->fPar[0]); return;}
    if(!strcmp("/ch/10/mix/fader",osc->address) && osc->fCount>0)   {midiSendPitchBand(midiInfo,1,1,osc->fPar[0]); return;}
    if(!strcmp("/ch/11/mix/fader",osc->address) && osc->fCount>0)   {midiSendPitchBand(midiInfo,1,2,osc->fPar[0]); return;}
    if(!strcmp("/ch/12/mix/fader",osc->address) && osc->fCount>0)   {midiSendPitchBand(midiInfo,1,3,osc->fPar[0]); return;}
    if(!strcmp("/ch/13/mix/fader",osc->address) && osc->fCount>0)   {midiSendPitchBand(midiInfo,1,4,osc->fPar[0]); return;}
    if(!strcmp("/ch/14/mix/fader",osc->address) && osc->fCount>0)   {midiSendPitchBand(midiInfo,1,5,osc->fPar[0]); return;}
    if(!strcmp("/ch/15/mix/fader",osc->address) && osc->fCount>0)   {midiSendPitchBand(midiInfo,1,6,osc->fPar[0]); return;}
    if(!strcmp("/ch/16/mix/fader",osc->address) && osc->fCount>0)   {midiSendPitchBand(midiInfo,1,7,osc->fPar[0]); return;}
    if(!strcmp("/ch/17/mix/fader",osc->address) && osc->fCount>0)   {midiSendPitchBand(midiInfo,2,0,osc->fPar[0]); return;}
    if(!strcmp("/ch/18/mix/fader",osc->address) && osc->fCount>0)   {midiSendPitchBand(midiInfo,2,1,osc->fPar[0]); return;}
    if(!strcmp("/ch/19/mix/fader",osc->address) && osc->fCount>0)   {midiSendPitchBand(midiInfo,2,2,osc->fPar[0]); return;}
    if(!strcmp("/ch/20/mix/fader",osc->address) && osc->fCount>0)   {midiSendPitchBand(midiInfo,2,3,osc->fPar[0]); return;}
    if(!strcmp("/ch/21/mix/fader",osc->address) && osc->fCount>0)   {midiSendPitchBand(midiInfo,2,4,osc->fPar[0]); return;}
    if(!strcmp("/ch/22/mix/fader",osc->address) && osc->fCount>0)   {midiSendPitchBand(midiInfo,2,5,osc->fPar[0]); return;}
    if(!strcmp("/ch/23/mix/fader",osc->address) && osc->fCount>0)   {midiSendPitchBand(midiInfo,2,6,osc->fPar[0]); return;}
    if(!strcmp("/ch/24/mix/fader",osc->address) && osc->fCount>0)   {midiSendPitchBand(midiInfo,2,7,osc->fPar[0]); return;}
    if(!strcmp("/ch/25/mix/fader",osc->address) && osc->fCount>0)   {midiSendPitchBand(midiInfo,3,0,osc->fPar[0]); return;}
    if(!strcmp("/ch/26/mix/fader",osc->address) && osc->fCount>0)   {midiSendPitchBand(midiInfo,3,1,osc->fPar[0]); return;}
    if(!strcmp("/ch/27/mix/fader",osc->address) && osc->fCount>0)   {midiSendPitchBand(midiInfo,3,2,osc->fPar[0]); return;}
    if(!strcmp("/ch/28/mix/fader",osc->address) && osc->fCount>0)   {midiSendPitchBand(midiInfo,3,3,osc->fPar[0]); return;}
    if(!strcmp("/ch/29/mix/fader",osc->address) && osc->fCount>0)   {midiSendPitchBand(midiInfo,3,4,osc->fPar[0]); return;}
    if(!strcmp("/ch/30/mix/fader",osc->address) && osc->fCount>0)   {midiSendPitchBand(midiInfo,3,5,osc->fPar[0]); return;}
    if(!strcmp("/ch/31/mix/fader",osc->address) && osc->fCount>0)   {midiSendPitchBand(midiInfo,3,6,osc->fPar[0]); return;}
    if(!strcmp("/ch/32/mix/fader",osc->address) && osc->fCount>0)   {midiSendPitchBand(midiInfo,3,7,osc->fPar[0]); return;}
    if(!strcmp("/bus/01/mix/fader",osc->address) && osc->fCount>0)   {midiSendPitchBand(midiInfo,4,0,osc->fPar[0]); return;}
    if(!strcmp("/bus/02/mix/fader",osc->address) && osc->fCount>0)   {midiSendPitchBand(midiInfo,4,1,osc->fPar[0]); return;}
    if(!strcmp("/bus/03/mix/fader",osc->address) && osc->fCount>0)   {midiSendPitchBand(midiInfo,4,2,osc->fPar[0]); return;}
    if(!strcmp("/bus/04/mix/fader",osc->address) && osc->fCount>0)   {midiSendPitchBand(midiInfo,4,3,osc->fPar[0]); return;}
    if(!strcmp("/bus/05/mix/fader",osc->address) && osc->fCount>0)   {midiSendPitchBand(midiInfo,4,4,osc->fPar[0]); return;}
    if(!strcmp("/bus/06/mix/fader",osc->address) && osc->fCount>0)   {midiSendPitchBand(midiInfo,4,5,osc->fPar[0]); return;}
    if(!strcmp("/bus/07/mix/fader",osc->address) && osc->fCount>0)   {midiSendPitchBand(midiInfo,4,6,osc->fPar[0]); return;}
    if(!strcmp("/bus/08/mix/fader",osc->address) && osc->fCount>0)   {midiSendPitchBand(midiInfo,4,7,osc->fPar[0]); return;}
    if(!strcmp("/bus/09/mix/fader",osc->address) && osc->fCount>0)   {midiSendPitchBand(midiInfo,5,0,osc->fPar[0]); return;}
    if(!strcmp("/bus/10/mix/fader",osc->address) && osc->fCount>0)   {midiSendPitchBand(midiInfo,5,1,osc->fPar[0]); return;}
    if(!strcmp("/bus/11/mix/fader",osc->address) && osc->fCount>0)   {midiSendPitchBand(midiInfo,5,2,osc->fPar[0]); return;}
    if(!strcmp("/bus/12/mix/fader",osc->address) && osc->fCount>0)   {midiSendPitchBand(midiInfo,5,3,osc->fPar[0]); return;}
    if(!strcmp("/bus/13/mix/fader",osc->address) && osc->fCount>0)   {midiSendPitchBand(midiInfo,5,4,osc->fPar[0]); return;}
    if(!strcmp("/bus/14/mix/fader",osc->address) && osc->fCount>0)   {midiSendPitchBand(midiInfo,5,5,osc->fPar[0]); return;}
    if(!strcmp("/bus/15/mix/fader",osc->address) && osc->fCount>0)   {midiSendPitchBand(midiInfo,5,6,osc->fPar[0]); return;}
    if(!strcmp("/bus/16/mix/fader",osc->address) && osc->fCount>0)   {midiSendPitchBand(midiInfo,5,7,osc->fPar[0]); return;}
    if(!strcmp("/main/st/mix/fader",osc->address) && osc->fCount>0)  {midiSendPitchBand(midiInfo,5,8,osc->fPar[0]); return;}

    if(!strcmp("/auxin/01/mix/fader",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,6,0,osc->fPar[0]); return;}
    if(!strcmp("/auxin/02/mix/fader",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,6,1,osc->fPar[0]); return;}
    if(!strcmp("/auxin/03/mix/fader",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,6,2,osc->fPar[0]); return;}
    if(!strcmp("/auxin/04/mix/fader",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,6,3,osc->fPar[0]); return;}
    if(!strcmp("/auxin/05/mix/fader",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,6,4,osc->fPar[0]); return;}
    if(!strcmp("/auxin/06/mix/fader",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,6,5,osc->fPar[0]); return;}
    if(!strcmp("/auxin/07/mix/fader",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,6,6,osc->fPar[0]); return;}
    if(!strcmp("/auxin/08/mix/fader",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,6,7,osc->fPar[0]); return;}

    if(!strcmp("/fxrtn/01/mix/fader",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,7,0,osc->fPar[0]); return;}
    if(!strcmp("/fxrtn/02/mix/fader",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,7,1,osc->fPar[0]); return;}
    if(!strcmp("/fxrtn/03/mix/fader",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,7,2,osc->fPar[0]); return;}
    if(!strcmp("/fxrtn/04/mix/fader",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,7,3,osc->fPar[0]); return;}
    if(!strcmp("/fxrtn/05/mix/fader",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,7,4,osc->fPar[0]); return;}
    if(!strcmp("/fxrtn/06/mix/fader",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,7,5,osc->fPar[0]); return;}
    if(!strcmp("/fxrtn/07/mix/fader",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,7,6,osc->fPar[0]); return;}
    if(!strcmp("/fxrtn/08/mix/fader",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,7,7,osc->fPar[0]); return;}

    if(!strcmp("/mtx/01/mix/fader",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,8,0,osc->fPar[0]); return;}
    if(!strcmp("/mtx/02/mix/fader",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,8,1,osc->fPar[0]); return;}
    if(!strcmp("/mtx/03/mix/fader",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,8,2,osc->fPar[0]); return;}
    if(!strcmp("/mtx/04/mix/fader",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,8,3,osc->fPar[0]); return;}
    if(!strcmp("/mtx/05/mix/fader",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,8,4,osc->fPar[0]); return;}
    if(!strcmp("/mtx/06/mix/fader",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,8,5,osc->fPar[0]); return;}
    //if(!strcmp("not mapped to OSC",osc->address) && osc->fCount>0) {midiSendPan(midiInfo,8,6,osc->fPar[0]); return;}
    if(!strcmp("/main/m/mix/fader",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,8,7,osc->fPar[0]); return;}

    //map mute
    if(!strcmp("/ch/01/mix/on",osc->address) && osc->iCount>0)   {midiSendNoteOnForMute(midiInfo,0,0,0x10,osc->iPar[0]); return;}
    if(!strcmp("/ch/02/mix/on",osc->address) && osc->iCount>0)   {midiSendNoteOnForMute(midiInfo,0,0,0x11,osc->iPar[0]); return;}
    if(!strcmp("/ch/03/mix/on",osc->address) && osc->iCount>0)   {midiSendNoteOnForMute(midiInfo,0,0,0x12,osc->iPar[0]); return;}
    if(!strcmp("/ch/04/mix/on",osc->address) && osc->iCount>0)   {midiSendNoteOnForMute(midiInfo,0,0,0x13,osc->iPar[0]); return;}
    if(!strcmp("/ch/05/mix/on",osc->address) && osc->iCount>0)   {midiSendNoteOnForMute(midiInfo,0,0,0x14,osc->iPar[0]); return;}
    if(!strcmp("/ch/06/mix/on",osc->address) && osc->iCount>0)   {midiSendNoteOnForMute(midiInfo,0,0,0x15,osc->iPar[0]); return;}
    if(!strcmp("/ch/07/mix/on",osc->address) && osc->iCount>0)   {midiSendNoteOnForMute(midiInfo,0,0,0x16,osc->iPar[0]); return;}
    if(!strcmp("/ch/08/mix/on",osc->address) && osc->iCount>0)   {midiSendNoteOnForMute(midiInfo,0,0,0x17,osc->iPar[0]); return;}
    if(!strcmp("/ch/09/mix/on",osc->address) && osc->iCount>0)   {midiSendNoteOnForMute(midiInfo,1,0,0x10,osc->iPar[0]); return;}
    if(!strcmp("/ch/10/mix/on",osc->address) && osc->iCount>0)   {midiSendNoteOnForMute(midiInfo,1,0,0x11,osc->iPar[0]); return;}
    if(!strcmp("/ch/11/mix/on",osc->address) && osc->iCount>0)   {midiSendNoteOnForMute(midiInfo,1,0,0x12,osc->iPar[0]); return;}
    if(!strcmp("/ch/12/mix/on",osc->address) && osc->iCount>0)   {midiSendNoteOnForMute(midiInfo,1,0,0x13,osc->iPar[0]); return;}
    if(!strcmp("/ch/13/mix/on",osc->address) && osc->iCount>0)   {midiSendNoteOnForMute(midiInfo,1,0,0x14,osc->iPar[0]); return;}
    if(!strcmp("/ch/14/mix/on",osc->address) && osc->iCount>0)   {midiSendNoteOnForMute(midiInfo,1,0,0x15,osc->iPar[0]); return;}
    if(!strcmp("/ch/15/mix/on",osc->address) && osc->iCount>0)   {midiSendNoteOnForMute(midiInfo,1,0,0x16,osc->iPar[0]); return;}
    if(!strcmp("/ch/16/mix/on",osc->address) && osc->iCount>0)   {midiSendNoteOnForMute(midiInfo,1,0,0x17,osc->iPar[0]); return;}
    if(!strcmp("/ch/17/mix/on",osc->address) && osc->iCount>0)   {midiSendNoteOnForMute(midiInfo,2,0,0x10,osc->iPar[0]); return;}
    if(!strcmp("/ch/18/mix/on",osc->address) && osc->iCount>0)   {midiSendNoteOnForMute(midiInfo,2,0,0x11,osc->iPar[0]); return;}
    if(!strcmp("/ch/19/mix/on",osc->address) && osc->iCount>0)   {midiSendNoteOnForMute(midiInfo,2,0,0x12,osc->iPar[0]); return;}
    if(!strcmp("/ch/20/mix/on",osc->address) && osc->iCount>0)   {midiSendNoteOnForMute(midiInfo,2,0,0x13,osc->iPar[0]); return;}
    if(!strcmp("/ch/21/mix/on",osc->address) && osc->iCount>0)   {midiSendNoteOnForMute(midiInfo,2,0,0x14,osc->iPar[0]); return;}
    if(!strcmp("/ch/22/mix/on",osc->address) && osc->iCount>0)   {midiSendNoteOnForMute(midiInfo,2,0,0x15,osc->iPar[0]); return;}
    if(!strcmp("/ch/23/mix/on",osc->address) && osc->iCount>0)   {midiSendNoteOnForMute(midiInfo,2,0,0x16,osc->iPar[0]); return;}
    if(!strcmp("/ch/24/mix/on",osc->address) && osc->iCount>0)   {midiSendNoteOnForMute(midiInfo,2,0,0x17,osc->iPar[0]); return;}
    if(!strcmp("/ch/25/mix/on",osc->address) && osc->iCount>0)   {midiSendNoteOnForMute(midiInfo,3,0,0x10,osc->iPar[0]); return;}
    if(!strcmp("/ch/26/mix/on",osc->address) && osc->iCount>0)   {midiSendNoteOnForMute(midiInfo,3,0,0x11,osc->iPar[0]); return;}
    if(!strcmp("/ch/27/mix/on",osc->address) && osc->iCount>0)   {midiSendNoteOnForMute(midiInfo,3,0,0x12,osc->iPar[0]); return;}
    if(!strcmp("/ch/28/mix/on",osc->address) && osc->iCount>0)   {midiSendNoteOnForMute(midiInfo,3,0,0x13,osc->iPar[0]); return;}
    if(!strcmp("/ch/29/mix/on",osc->address) && osc->iCount>0)   {midiSendNoteOnForMute(midiInfo,3,0,0x14,osc->iPar[0]); return;}
    if(!strcmp("/ch/30/mix/on",osc->address) && osc->iCount>0)   {midiSendNoteOnForMute(midiInfo,3,0,0x15,osc->iPar[0]); return;}
    if(!strcmp("/ch/31/mix/on",osc->address) && osc->iCount>0)   {midiSendNoteOnForMute(midiInfo,3,0,0x16,osc->iPar[0]); return;}
    if(!strcmp("/ch/32/mix/on",osc->address) && osc->iCount>0)   {midiSendNoteOnForMute(midiInfo,3,0,0x17,osc->iPar[0]); return;}

    if(!strcmp("/bus/01/mix/on",osc->address) && osc->iCount>0)   {midiSendNoteOnForMute(midiInfo,4,0,0x10,osc->iPar[0]); return;}
    if(!strcmp("/bus/02/mix/on",osc->address) && osc->iCount>0)   {midiSendNoteOnForMute(midiInfo,4,0,0x11,osc->iPar[0]); return;}
    if(!strcmp("/bus/03/mix/on",osc->address) && osc->iCount>0)   {midiSendNoteOnForMute(midiInfo,4,0,0x12,osc->iPar[0]); return;}
    if(!strcmp("/bus/04/mix/on",osc->address) && osc->iCount>0)   {midiSendNoteOnForMute(midiInfo,4,0,0x13,osc->iPar[0]); return;}
    if(!strcmp("/bus/05/mix/on",osc->address) && osc->iCount>0)   {midiSendNoteOnForMute(midiInfo,4,0,0x14,osc->iPar[0]); return;}
    if(!strcmp("/bus/06/mix/on",osc->address) && osc->iCount>0)   {midiSendNoteOnForMute(midiInfo,4,0,0x15,osc->iPar[0]); return;}
    if(!strcmp("/bus/07/mix/on",osc->address) && osc->iCount>0)   {midiSendNoteOnForMute(midiInfo,4,0,0x16,osc->iPar[0]); return;}
    if(!strcmp("/bus/08/mix/on",osc->address) && osc->iCount>0)   {midiSendNoteOnForMute(midiInfo,4,0,0x17,osc->iPar[0]); return;}

    if(!strcmp("/bus/09/mix/on",osc->address) && osc->iCount>0)   {midiSendNoteOnForMute(midiInfo,5,0,0x10,osc->iPar[0]); return;}
    if(!strcmp("/bus/10/mix/on",osc->address) && osc->iCount>0)   {midiSendNoteOnForMute(midiInfo,5,0,0x11,osc->iPar[0]); return;}
    if(!strcmp("/bus/11/mix/on",osc->address) && osc->iCount>0)   {midiSendNoteOnForMute(midiInfo,5,0,0x12,osc->iPar[0]); return;}
    if(!strcmp("/bus/12/mix/on",osc->address) && osc->iCount>0)   {midiSendNoteOnForMute(midiInfo,5,0,0x13,osc->iPar[0]); return;}
    if(!strcmp("/bus/13/mix/on",osc->address) && osc->iCount>0)   {midiSendNoteOnForMute(midiInfo,5,0,0x14,osc->iPar[0]); return;}
    if(!strcmp("/bus/14/mix/on",osc->address) && osc->iCount>0)   {midiSendNoteOnForMute(midiInfo,5,0,0x15,osc->iPar[0]); return;}
    if(!strcmp("/bus/15/mix/on",osc->address) && osc->iCount>0)   {midiSendNoteOnForMute(midiInfo,5,0,0x16,osc->iPar[0]); return;}
    if(!strcmp("/bus/16/mix/on",osc->address) && osc->iCount>0)   {midiSendNoteOnForMute(midiInfo,5,0,0x17,osc->iPar[0]); return;}
    if(!strcmp("/main/st/mix/on",osc->address) && osc->iCount>0)  {midiSendNoteOnForMute(midiInfo,5,0,0x18,osc->iPar[0]); return;}

    if(!strcmp("/auxin/01/mix/on",osc->address) && osc->iCount>0) {midiSendNoteOnForMuteNoToggle(midiInfo,6,0,0x10,osc->iPar[0]); return;}
    if(!strcmp("/auxin/02/mix/on",osc->address) && osc->iCount>0) {midiSendNoteOnForMuteNoToggle(midiInfo,6,0,0x11,osc->iPar[0]); return;}
    if(!strcmp("/auxin/03/mix/on",osc->address) && osc->iCount>0) {midiSendNoteOnForMuteNoToggle(midiInfo,6,0,0x12,osc->iPar[0]); return;}
    if(!strcmp("/auxin/04/mix/on",osc->address) && osc->iCount>0) {midiSendNoteOnForMuteNoToggle(midiInfo,6,0,0x13,osc->iPar[0]); return;}
    if(!strcmp("/auxin/05/mix/on",osc->address) && osc->iCount>0) {midiSendNoteOnForMuteNoToggle(midiInfo,6,0,0x14,osc->iPar[0]); return;}
    if(!strcmp("/auxin/06/mix/on",osc->address) && osc->iCount>0) {midiSendNoteOnForMuteNoToggle(midiInfo,6,0,0x15,osc->iPar[0]); return;}
    if(!strcmp("/auxin/07/mix/on",osc->address) && osc->iCount>0) {midiSendNoteOnForMuteNoToggle(midiInfo,6,0,0x16,osc->iPar[0]); return;}
    if(!strcmp("/auxin/08/mix/on",osc->address) && osc->iCount>0) {midiSendNoteOnForMuteNoToggle(midiInfo,6,0,0x17,osc->iPar[0]); return;}

    if(!strcmp("/fxrtn/01/mix/on",osc->address) && osc->iCount>0) {midiSendNoteOnForMuteNoToggle(midiInfo,7,0,0x10,osc->iPar[0]); return;}
    if(!strcmp("/fxrtn/02/mix/on",osc->address) && osc->iCount>0) {midiSendNoteOnForMuteNoToggle(midiInfo,7,0,0x11,osc->iPar[0]); return;}
    if(!strcmp("/fxrtn/03/mix/on",osc->address) && osc->iCount>0) {midiSendNoteOnForMuteNoToggle(midiInfo,7,0,0x12,osc->iPar[0]); return;}
    if(!strcmp("/fxrtn/04/mix/on",osc->address) && osc->iCount>0) {midiSendNoteOnForMuteNoToggle(midiInfo,7,0,0x13,osc->iPar[0]); return;}
    if(!strcmp("/fxrtn/05/mix/on",osc->address) && osc->iCount>0) {midiSendNoteOnForMuteNoToggle(midiInfo,7,0,0x14,osc->iPar[0]); return;}
    if(!strcmp("/fxrtn/06/mix/on",osc->address) && osc->iCount>0) {midiSendNoteOnForMuteNoToggle(midiInfo,7,0,0x15,osc->iPar[0]); return;}
    if(!strcmp("/fxrtn/07/mix/on",osc->address) && osc->iCount>0) {midiSendNoteOnForMuteNoToggle(midiInfo,7,0,0x16,osc->iPar[0]); return;}
    if(!strcmp("/fxrtn/08/mix/on",osc->address) && osc->iCount>0) {midiSendNoteOnForMuteNoToggle(midiInfo,7,0,0x17,osc->iPar[0]); return;}

    if(!strcmp("/mtx/01/mix/on",osc->address) && osc->iCount>0) {midiSendNoteOnForMuteNoToggle(midiInfo,8,0,0x10,osc->iPar[0]); return;}
    if(!strcmp("/mtx/02/mix/on",osc->address) && osc->iCount>0) {midiSendNoteOnForMuteNoToggle(midiInfo,8,0,0x11,osc->iPar[0]); return;}
    if(!strcmp("/mtx/03/mix/on",osc->address) && osc->iCount>0) {midiSendNoteOnForMuteNoToggle(midiInfo,8,0,0x12,osc->iPar[0]); return;}
    if(!strcmp("/mtx/04/mix/on",osc->address) && osc->iCount>0) {midiSendNoteOnForMuteNoToggle(midiInfo,8,0,0x13,osc->iPar[0]); return;}
    if(!strcmp("/mtx/05/mix/on",osc->address) && osc->iCount>0) {midiSendNoteOnForMuteNoToggle(midiInfo,8,0,0x14,osc->iPar[0]); return;}
    if(!strcmp("/mtx/06/mix/on",osc->address) && osc->iCount>0) {midiSendNoteOnForMuteNoToggle(midiInfo,8,0,0x15,osc->iPar[0]); return;}
    //if(!strcmp("/fxrtn/07/mix/on",osc->address) && osc->iCount>0) {midiSendNoteOnForMuteNoToggle(midiInfo,8,0,0x16,osc->iPar[0]); return;}
    if(!strcmp("/main/m/mix/on",osc->address) && osc->iCount>0) {midiSendNoteOnForMuteNoToggle(midiInfo,8,0,0x17,osc->iPar[0]); return;}

    //pan
    if(!strcmp("/ch/01/mix/pan",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,0,0,osc->fPar[0]); return;}
    if(!strcmp("/ch/02/mix/pan",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,0,1,osc->fPar[0]); return;}
    if(!strcmp("/ch/03/mix/pan",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,0,2,osc->fPar[0]); return;}
    if(!strcmp("/ch/04/mix/pan",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,0,3,osc->fPar[0]); return;}
    if(!strcmp("/ch/05/mix/pan",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,0,4,osc->fPar[0]); return;}
    if(!strcmp("/ch/06/mix/pan",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,0,5,osc->fPar[0]); return;}
    if(!strcmp("/ch/07/mix/pan",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,0,6,osc->fPar[0]); return;}
    if(!strcmp("/ch/08/mix/pan",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,0,7,osc->fPar[0]); return;}
    if(!strcmp("/ch/09/mix/pan",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,1,0,osc->fPar[0]); return;}
    if(!strcmp("/ch/10/mix/pan",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,1,1,osc->fPar[0]); return;}
    if(!strcmp("/ch/11/mix/pan",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,1,2,osc->fPar[0]); return;}
    if(!strcmp("/ch/12/mix/pan",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,1,3,osc->fPar[0]); return;}
    if(!strcmp("/ch/13/mix/pan",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,1,4,osc->fPar[0]); return;}
    if(!strcmp("/ch/14/mix/pan",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,1,5,osc->fPar[0]); return;}
    if(!strcmp("/ch/15/mix/pan",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,1,6,osc->fPar[0]); return;}
    if(!strcmp("/ch/16/mix/pan",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,1,7,osc->fPar[0]); return;}
    if(!strcmp("/ch/17/mix/pan",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,2,0,osc->fPar[0]); return;}
    if(!strcmp("/ch/18/mix/pan",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,2,1,osc->fPar[0]); return;}
    if(!strcmp("/ch/19/mix/pan",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,2,2,osc->fPar[0]); return;}
    if(!strcmp("/ch/20/mix/pan",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,2,3,osc->fPar[0]); return;}
    if(!strcmp("/ch/21/mix/pan",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,2,4,osc->fPar[0]); return;}
    if(!strcmp("/ch/22/mix/pan",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,2,5,osc->fPar[0]); return;}
    if(!strcmp("/ch/23/mix/pan",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,2,6,osc->fPar[0]); return;}
    if(!strcmp("/ch/24/mix/pan",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,2,7,osc->fPar[0]); return;}
    if(!strcmp("/ch/25/mix/pan",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,3,0,osc->fPar[0]); return;}
    if(!strcmp("/ch/26/mix/pan",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,3,1,osc->fPar[0]); return;}
    if(!strcmp("/ch/27/mix/pan",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,3,2,osc->fPar[0]); return;}
    if(!strcmp("/ch/28/mix/pan",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,3,3,osc->fPar[0]); return;}
    if(!strcmp("/ch/29/mix/pan",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,3,4,osc->fPar[0]); return;}
    if(!strcmp("/ch/30/mix/pan",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,3,5,osc->fPar[0]); return;}
    if(!strcmp("/ch/31/mix/pan",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,3,6,osc->fPar[0]); return;}
    if(!strcmp("/ch/32/mix/pan",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,3,7,osc->fPar[0]); return;}
    if(!strcmp("/bus/01/mix/pan",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,4,0,osc->fPar[0]); return;}
    if(!strcmp("/bus/02/mix/pan",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,4,1,osc->fPar[0]); return;}
    if(!strcmp("/bus/03/mix/pan",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,4,2,osc->fPar[0]); return;}
    if(!strcmp("/bus/04/mix/pan",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,4,3,osc->fPar[0]); return;}
    if(!strcmp("/bus/05/mix/pan",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,4,4,osc->fPar[0]); return;}
    if(!strcmp("/bus/06/mix/pan",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,4,5,osc->fPar[0]); return;}
    if(!strcmp("/bus/07/mix/pan",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,4,6,osc->fPar[0]); return;}
    if(!strcmp("/bus/08/mix/pan",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,4,7,osc->fPar[0]); return;}
    if(!strcmp("/bus/09/mix/pan",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,5,0,osc->fPar[0]); return;}
    if(!strcmp("/bus/10/mix/pan",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,5,1,osc->fPar[0]); return;}
    if(!strcmp("/bus/11/mix/pan",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,5,2,osc->fPar[0]); return;}
    if(!strcmp("/bus/12/mix/pan",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,5,3,osc->fPar[0]); return;}
    if(!strcmp("/bus/13/mix/pan",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,5,4,osc->fPar[0]); return;}
    if(!strcmp("/bus/14/mix/pan",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,5,5,osc->fPar[0]); return;}
    if(!strcmp("/bus/15/mix/pan",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,5,6,osc->fPar[0]); return;}
    if(!strcmp("/bus/16/mix/pan",osc->address) && osc->fCount>0)   {midiSendPan(midiInfo,5,7,osc->fPar[0]); return;}


    //bank left/right
    //if(!strcmp("/-stat/userpar/17/value",osc->address) && osc->iCount>0 && osc->iPar[0]==0x7f)  {midiSendNoteBankSwitch(midiInfo,0,0x30,0x7f); return;}
    //if(!strcmp("/-stat/userpar/18/value",osc->address) && osc->iCount>0 && osc->iPar[0]==0x7f)  {midiSendNoteBankSwitch(midiInfo,0,0x31,0x7f); return;}
    //if(!strcmp("/-stat/userpar/19/value",osc->address) && osc->iCount>0 && osc->iPar[0]==0x7f)  {midiSendNoteBankSwitch(midiInfo,0,0x2e,0x7f); return;}
    //if(!strcmp("/-stat/userpar/20/value",osc->address) && osc->iCount>0 && osc->iPar[0]==0x7f)  {midiSendNoteBankSwitch(midiInfo,0,0x2f,0x7f); return;}

    if(!strcmp("/-stat/userpar/17/value",osc->address) && osc->iCount>0 && osc->iPar[0]==0x7f)  
    {
        for (int i=0;i<maxMidiPort;i++)
        {
            for(int j=0;j<10;j++)
            {
                //printf("a %d %d\n",i,j);
                midiSendNoteBankSwitchInit(midiInfo, i, 0, 0x2e, 0x7f);
            }
        } 
        for (int i=0;i<maxMidiPort;i++)
        {
            for(int j=i;j>0;j--)
            {
                //printf("b %d %d\n",i,j);
                midiSendNoteBankSwitchInit(midiInfo, i, 0, 0x2f, 0x7f);
            }
        } 
    }

    //solo
    if(!strncmp("/-stat/solosw/",osc->address,strlen("/-stat/solosw/")) && osc->iCount>0)
    {
        int channel;
        int channelNumber=-1;
        int midiInterface;
        int number=atoi(&osc->address[strlen("/-stat/solosw/")]);
        if(number>=1 && number<=32)
        {
            channelNumber=number-1;
        }
        else if(number>=33 && number<=48)
        {
            channelNumber=number+16-1;
        }
        else if(number>=49 && number<=64)
        {
            channelNumber=number-16-1;
        }
        else if(number>=65 && number<=72)
        {
            channelNumber=number-1;
        }

        midiInterface=channelNumber/8;
        channel=channelNumber%8;
        
        if (channelNumber>=0)
        {
            midiSendNoteOnForSolo(channelNumber,midiInfo,midiInterface,0,0x08+channel,osc->iPar[0]);
        }
    }

    //select
    if(!strncmp("/-stat/selidx",osc->address,strlen("/-stat/selidx")) && osc->iCount>0)
    {
        int channel;
        int channelNumber=-1;
        int midiInterface;
        int number=osc->iPar[0];
        
        for(int i=0;i<maxChannels-1;i++)
        {
            if(selectState[i]!=0)
            {
                selectState[i]=0;
                if(i>=0 && i<=31)
                {
                    channelNumber=i;
                    midiInterface=channelNumber/8;
                    channel=channelNumber%8;
                }
                else if(i>=32 && i<=47)
                {
                    channelNumber=i+16;
                    midiInterface=channelNumber/8;
                    channel=channelNumber%8;
                }
                else if(i>=48 && i<=63)
                {
                    channelNumber=i-16;
                    midiInterface=channelNumber/8;
                    channel=channelNumber%8;
                }
                else if(i>=64 && i<=71)
                {
                    channelNumber=i;
                    midiInterface=channelNumber/8;
                    channel=channelNumber%8;
                }
             
                if (channelNumber>=0)
                {
                    midiSendNoteOnForSelect(channelNumber,midiInfo,midiInterface,0,0x18+channel,0x00);
                }
            }
        }

        channelNumber=-1;
        if(number>=0 && number<=31)
        {
            channelNumber=number;
            midiInterface=channelNumber/8;
            channel=channelNumber%8;
        }
        else if(number>=32 && number<=47)
        {
            channelNumber=number+16;
            midiInterface=channelNumber/8;
            channel=channelNumber%8;
        }
        else if(number>=48 && number<=63)
        {
            channelNumber=number-16;
            midiInterface=channelNumber/8;
            channel=channelNumber%8;
        }
        else if(number>=64 && number<=71)
        {
            channelNumber=number;
            midiInterface=channelNumber/8;
            channel=channelNumber%8;
        }
     
        if (channelNumber>=0)
        {
            midiSendNoteOnForSelect(channelNumber,midiInfo,midiInterface,0,0x18+channel,0x7f);
        }

    }

    //unmute all
    if(!strncmp("/config/mute/6",osc->address,strlen("/config/mute/6")) && osc->iCount>0)
    {
        uint32_t intval;
        unsigned char *byteval= (unsigned char *) &intval;
        char buffer[28];
        struct sockaddr_in address;

        intval = osc->iPar[0]==1?0:1;

        for(int i=0;i<maxChannels;i++)
        {
            if(i<32)
            {
                memcpy(buffer,"/ch/00/mix/on\0\0\0,i\0\0",20);
                sprintf(&buffer[4],"%02d",i+1);
                buffer[6]='/';
                sprintf(&buffer[20],"%c%c%c%c",*(byteval+3),*(byteval+2),*(byteval+1),*(byteval+0));
            }
            else if(i>=32 && i<48)
            {
                memcpy(buffer,"/bus/00/mix/on\0\0,i\0\0",20);
                sprintf(&buffer[5],"%02d",i+1);
                buffer[7]='/';
                sprintf(&buffer[20],"%c%c%c%c",*(byteval+3),*(byteval+2),*(byteval+1),*(byteval+0));
            }
            else if(i==maxChannels)
            {
                memcpy(buffer,"/main/st/mix/on\0,i\0\0",20);
                sprintf(&buffer[20],"%c%c%c%c",*(byteval+3),*(byteval+2),*(byteval+1),*(byteval+0));
            }

            printf("\t\t[%02d] %s=%d %smute\n",i+1,buffer,intval,intval==1?"un":"");

            address.sin_family = AF_INET;
            address.sin_addr.s_addr = inet_addr(remoteIP);
            address.sin_port = htons(remoteport);
            networkSend(midiInfo[0].udpSocket, &address, (const uint8_t *) buffer, sizeof(buffer));
        }
    }

}

void midiSendPollAnswer(midiInfo_t midiInfo[])
{
    for (int j=0;j<maxMidiPort;j++)
    {
        for (int i=0;i<MAX_POLL_ANSWER;i++)
        {
            if (midiInfo[j].sendPollAnswer[i])
            {
                std::vector<unsigned char> message;
                message.clear();
                switch(i)
                {
                case 0:
                    //f0 00 00 66 14 01 01 02 03 04 05 06 07 06 06 06 06 f7
                    message.push_back(0xF0);
                    message.push_back(0x00);
                    message.push_back(0x00);
                    message.push_back(0x66);
                    message.push_back(0x14);
                    message.push_back(0x01);
                    message.push_back(0x01);
                    message.push_back(0x02);
                    message.push_back(0x03);
                    message.push_back(0x04);
                    message.push_back(0x05);
                    message.push_back(0x06);
                    message.push_back(0x07);
                    message.push_back(0x06);
                    message.push_back(0x06);
                    message.push_back(0x06);
                    message.push_back(0x06);
                    message.push_back(0xF7);
                    break;
                case 1:
                    //f0 00 00 66 14 03 01 02 03 04 05 06 07 06 f7
                    message.push_back(0xF0);
                    message.push_back(0x00);
                    message.push_back(0x00);
                    message.push_back(0x66);
                    message.push_back(0x14);
                    message.push_back(0x03);
                    message.push_back(0x01);
                    message.push_back(0x02);
                    message.push_back(0x03);
                    message.push_back(0x04);
                    message.push_back(0x05);
                    message.push_back(0x06);
                    message.push_back(0x07);
                    message.push_back(0xF7);
                    break;
                case 2:
                    //F0 00 00 66 14 14 56 41  43 2D 37 F7 (AC-7)
                    message.push_back(0xF0);
                    message.push_back(0x00);
                    message.push_back(0x00);
                    message.push_back(0x66);
                    message.push_back(0x14);
                    message.push_back(0x14);
                    message.push_back(0x56);
                    message.push_back('X');
                    message.push_back('3');
                    message.push_back('2');
                    message.push_back(' ');
                    message.push_back(0xF7);
                    break;
                default:
                    midiInfo[j].sendPollAnswer[i]=false;
                    break;
                }

                if (midiInfo[j].sendPollAnswer[i])
                {
                    try 
                    {
                        midiInfo[j].midiout->sendMessage( &message );
                        printf("pollanswer %i sent ",i);
                        for (unsigned int k=0; k<message.size(); k++)
                        {
                            printf("%02x ",message[k]);
                        }
                        printf("\n");
                    }
                    catch ( RtError &error ) 
                    {
                        error.printMessage();
                    }
                }
                midiInfo[j].sendPollAnswer[i]=false;
            }
        }
    }
}

bool ignoreMidiString(int channel, const char *key, size_t keyLen, std::vector< unsigned char > *message)
{
    const unsigned char *p =  message->data();

    if (! memcmp(key,p,keyLen))
    {
        return true;
    }
    return false;
}

bool matchMidiString(int channel, const char *key, size_t keyLen, std::vector< unsigned char > *message)
{
    const unsigned char *p =  message->data();

    if (! memcmp(key,p,keyLen))
    {
        return true;
    }
    return false;
}

bool printMidiString(int channel, const char *text, const char *key, size_t keyLen, std::vector< unsigned char > *message, midiInfo_t *midiInfo)
{
    struct sockaddr_in address;
    const unsigned char *p =  message->data();
    int midiChannel;
    char buffer[40+1];
    char *pOut=midiInfo->display;

    if (! memcmp(key,p,keyLen))
    {
        //dumpBuffer((char *)message->data(),message->size());

        int startPos= *(p+keyLen);
        keyLen++;
        pOut+=startPos;

        p+=keyLen;
        size_t messageLen=message->size()-keyLen;

        if(*message->data()==0xF0)
        {
            messageLen--;
        }

        //prepare display
        for ( unsigned int i=0; i<messageLen; i++ )
        {
            *pOut++=*p++;
        }

#if 0
        //print display
        pOut=midiInfo->display;
        printf("[xx][%d] %s ",channel , text);
        for ( unsigned int i=0; i<sizeof(midiInfo->display)/2; i++ )
        {
            printf("%c", *pOut++);
        }
        printf("\n");
        printf("[xx][%d] %s ",channel , text);
        for ( unsigned int i=sizeof(midiInfo->display)/2; i<sizeof(midiInfo->display); i++ )
        {
            printf("%c", *pOut++);
        }
        printf("\n");
#endif

        p = (const unsigned char *) midiInfo->display;        
        for (midiChannel=0;midiChannel<8;midiChannel++)
        {
            if (channel>=5)
            {
                memcpy(buffer,"/bus/00/config/name\0,s\0\0            \0\0\0\0",40);
                sprintf(&buffer[5],"%02d",midiChannel+1+((channel-5)*8));
                buffer[7]='/';
            }
            else
            {
                memcpy(buffer,"/ch/00/config/name\0\0,s\0\0            \0\0\0\0",40);
                sprintf(&buffer[4],"%02d",midiChannel+1+((channel-1)*8));
                buffer[6]='/';
            }

            memcpy(&buffer[24],p,6);
            memcpy(&buffer[30],p+(8*7),6);

            //dumpBuffer(buffer,40);
            address.sin_family = AF_INET;
            address.sin_addr.s_addr = inet_addr(remoteIP);
            address.sin_port = htons(remoteport);
            networkSend(midiInfo->udpSocket, &address, (const uint8_t *) buffer, 40);
    
            p+=7;
        }
        return true;

    }
    return false;
}

bool printMidiByte(int channel, const char *text, const char *key, size_t keyLen, std::vector< unsigned char > *message)
{
    const unsigned char *p =  message->data();

    if (! memcmp(key,p,keyLen))
    {
        printf("[%d] %s ",channel , text);
        p+=keyLen;
        printf("%02x\n", *p);
        return true;
    }
    return false;
}

bool printMidiNibble(int channel, const char *text, const char *key, std::vector< unsigned char > *message)
{
    const unsigned char *p =  message->data();

    if (((*key)&0xf0)==((*p)&0xf0))
    {
        printf("[%d] %s channel:%d ",channel , text, (*p)&0x0f);
        printf("%02x\n", *(p+1));
        return true;
    }
    return false;
}

bool handleMidiPitchBand(int channel, const char *text, const char *key, std::vector< unsigned char > *message, int udpSocket)
{
    const unsigned char *p =  message->data();
    int16_t intval;
    float floatval;
    unsigned char *byteval= (unsigned char *) &floatval;
    char buffer[28+1];

    struct sockaddr_in address;

    if (((*key)&0xf0)==((*p)&0xf0))
    {
        int channelNumber = ((*p)&0x0f)+((channel-1)*8);

        if (lockChannel(channelNumber,MIDI_LOCK))
        {
            intval = (((*(p+2))&0x7f)<<7) + ((*(p+1))&0x7f);
            floatval = intval;

            //floatval /= 0x3fff;
            floatval /= 0x397b;

            if(((*p)&0x0f)==0x08 && channel==6)
            {
                memcpy(buffer,"/main/st/mix/fader\0\0,f\0\0",24);
                sprintf(&buffer[24],"%c%c%c%c",*(byteval+3),*(byteval+2),*(byteval+1),*(byteval+0));
            }
            else if (((*p)&0x0f)<0x08 && channel>=5)
            {
                memcpy(buffer,"/bus/00/mix/fader\0\0\0,f\0\0",24);
                sprintf(&buffer[5],"%02d",((*p)&0x0f)+1+((channel-5)*8));
                buffer[7]='/';
                sprintf(&buffer[24],"%c%c%c%c",*(byteval+3),*(byteval+2),*(byteval+1),*(byteval+0));

            }
            else if(((*p)&0x0f)<0x08)
            {
                memcpy(buffer,"/ch/00/mix/fader\0\0\0\0,f\0\0",24);
                sprintf(&buffer[4],"%02d",((*p)&0x0f)+1+((channel-1)*8));
                buffer[6]='/';
                sprintf(&buffer[24],"%c%c%c%c",*(byteval+3),*(byteval+2),*(byteval+1),*(byteval+0));

            }
            else
            {
                return false;
            }

            address.sin_family = AF_INET;
            address.sin_addr.s_addr = inet_addr(remoteIP);
            address.sin_port = htons(remoteport);
            networkSend(udpSocket, &address, (const uint8_t *) buffer, 28);

            printf("[%02d][%d] %s pitchband:%d ",channelNumber+1,channel , text, (*p)&0x0f);
            printf("(%f, %5d, 0x%02x%02x) --> '%s'\n", floatval,intval,*(p+2),*(p+1),buffer);

            return true;
        }
        else
        {
            if(debugLock) printf("\t\t[%02d] LOCKED from OSC\n",channelNumber+1);
        }
    }
    return false;
}

bool handleMidiNoteOn(int channel, const char *text, const char *key, std::vector< unsigned char > *message, int udpSocket)
{
    const unsigned char *p =  message->data();
    uint32_t intval;
    unsigned char *byteval= (unsigned char *) &intval;
    char buffer[28+1];

    struct sockaddr_in address;

    if (((*key)&0xf0)==((*p)&0xf0))
    {

        //handle mute
        if ((*(p+1)&0xf8)==0x10) 
        {
            int midiChannel=(*(p+1)&0x0f);
            int channelNumber = midiChannel+((channel-1)*8);
            if (channelNumber>=maxChannels)
            {
                return false;
            }

            intval = ((*(p+2))&0x7f);

            if(noToggle) intval=intval?1:0; //Logic changes meaning due to config in midi mapping from alternate to values
            else intval=intval?0:1;

            muteState[channelNumber]=intval;

            if(((*(p+1))&0x0f)==0x08 && channel==6)
            {
                memcpy(buffer,"/main/st/mix/on\0,i\0\0",20);
                sprintf(&buffer[20],"%c%c%c%c",*(byteval+3),*(byteval+2),*(byteval+1),*(byteval+0));
            }
            else if (((*(p+1))&0x0f)<0x08 && channel>=5)
            {
                memcpy(buffer,"/bus/00/mix/on\0\0,i\0\0",20);
                sprintf(&buffer[5],"%02d",midiChannel+1+((channel-5)*8));
                buffer[7]='/';
                sprintf(&buffer[20],"%c%c%c%c",*(byteval+3),*(byteval+2),*(byteval+1),*(byteval+0));
            }
            else if(((*(p+1))&0x0f)<0x08)
            {
                memcpy(buffer,"/ch/00/mix/on\0\0\0,i\0\0",20);
                sprintf(&buffer[4],"%02d",midiChannel+1+((channel-1)*8));
                buffer[6]='/';
                sprintf(&buffer[20],"%c%c%c%c",*(byteval+3),*(byteval+2),*(byteval+1),*(byteval+0));
            }
            else
            {
                return false;
            }


            address.sin_family = AF_INET;
            address.sin_addr.s_addr = inet_addr(remoteIP);
            address.sin_port = htons(remoteport);
            networkSend(udpSocket, &address, (const uint8_t *) buffer, 24);

            printf("[%02d][%d] %s %s ,i %d %smute\n",channelNumber+1,channel, text, buffer, intval, intval==1?"un":"");
        }

        //handle Solo
        else if ((*(p+1)&0xf8)==0x08) 
        {
            int midiChannel=(*(p+1)&0x07);
            int channelNumber = midiChannel+((channel-1)*8);
            if (channelNumber>=maxChannels)
            {
                return false;
            }

            intval = ((*(p+2))&0x7f);

            intval=intval?1:0;

            soloState[channelNumber]=intval;

            if (((*(p+1))&0x07)<0x08 && channel>=5)
            {
                memcpy(buffer,"/-stat/solosw/00\0\0\0\0,i\0\0",24);
                sprintf(&buffer[14],"%02d",midiChannel+49+((channel-5)*8));
                sprintf(&buffer[24],"%c%c%c%c",*(byteval+3),*(byteval+2),*(byteval+1),*(byteval+0));
            }
            else if(((*(p+1))&0x07)<0x08)
            {
                memcpy(buffer,"/-stat/solosw/00\0\0\0\0,i\0\0",24);
                sprintf(&buffer[14],"%02d",midiChannel+1+((channel-1)*8));
                sprintf(&buffer[24],"%c%c%c%c",*(byteval+3),*(byteval+2),*(byteval+1),*(byteval+0));
            }
            else
            {
                return false;
            }

            printf("[%02d][%d] %s note on:%d ",channelNumber+1,channel , text, (*(p+1))&0x07);
            printf("%d --> '%s'\n", intval,buffer);
            //dumpBuffer(buffer,sizeof(buffer));

            address.sin_family = AF_INET;
            address.sin_addr.s_addr = inet_addr(remoteIP);
            address.sin_port = htons(remoteport);
            networkSend(udpSocket, &address, (const uint8_t *) buffer, 28);
        }
        
        //handle select
        else if ((*(p+1)&0xf8)==0x18) 
        {
            int midiChannel=(*(p+1)&0x07);
            int channelNumber = midiChannel+((channel-1)*8);
            if (channelNumber>=maxChannels)
            {
                return false;
            }

            intval = ((*(p+2))&0x7f);
            selectState[channelNumber]=intval;
            printf("\t\tselectState[%d] is set to %02x\n",channelNumber,selectState[channelNumber]);

            if(((*(p+2))&0x7f)==0)
            {
                return true;
            }

            if (((*(p+1))&0x07)<0x08 && channel>=5)
            {
                memcpy(buffer,"/-stat/selidx\0\0\0,i\0\0",20);
                intval=midiChannel+48+((channel-5)*8);
                sprintf(&buffer[20],"%c%c%c%c",*(byteval+3),*(byteval+2),*(byteval+1),*(byteval+0));
            }
            else if(((*(p+1))&0x07)<0x08)
            {
                memcpy(buffer,"/-stat/selidx\0\0\0,i\0\0",20);
                intval=midiChannel+((channel-1)*8);
                sprintf(&buffer[20],"%c%c%c%c",*(byteval+3),*(byteval+2),*(byteval+1),*(byteval+0));
            }
            else
            {
                return false;
            }

#if 1
            printf("[%02d][%d] %s note on:%d ",channelNumber+1,channel , text, (*(p+1))&0x07);
            printf("%d --> '%s'\n", intval,buffer);
            //dumpBuffer(buffer,sizeof(buffer)-4);

            address.sin_family = AF_INET;
            address.sin_addr.s_addr = inet_addr(remoteIP);
            address.sin_port = htons(remoteport);
            networkSend(udpSocket, &address, (const uint8_t *) buffer, 24);
#endif
        }

        return true;

    }
    return false;
}

void midiCallback( double deltatime, std::vector< unsigned char > *message, void *userData )
{
    midiInfo_t *midiInfo = (midiInfo_t *) userData;

    //if(ignoreMidiString(midiInfo->channel,"\xF0\x00\x00\x66\x14\x12",6,message)) return;
    if(printMidiString(midiInfo->channel,"display","\xF0\x00\x00\x66\x14\x12",6,message,midiInfo)) return;

    if(ignoreMidiString(midiInfo->channel,"\xB0",1,message)) return;

    if(ignoreMidiString(midiInfo->channel,"\xD0",1,message)) return;
    //if(printMidiNibble(midiInfo->channel,"level","\xD0",message)) return;

    if(handleMidiPitchBand(midiInfo->channel,"fader","\xE0",message,midiInfo->udpSocket)) return;


    printf("[xx][%d] ",midiInfo->channel);
    for ( unsigned int i=0; i<message->size(); i++ )
    {
        printf("%02X ", (int)message->at(i));
    }
    if ( message->size() > 0 )
    {
        printf("stamp = %e\n",deltatime);
    }

    if(handleMidiNoteOn(midiInfo->channel,"mute/solo/select","\x90",message,midiInfo->udpSocket)) return;

    //no autodetect for aditional channels
    if (midiInfo->channel<maxMidiPort)
    {
        if(matchMidiString(midiInfo->channel,"\xF0\x7e\x00\x06\x01\xf7",6,message)) //some kind of poll request for autodetect
        {
            midiInfo->sendPollAnswer[0]=true;
            return;  
        } 

        if(matchMidiString(midiInfo->channel,"\xF0\x00\x00\x66\x14\x01\x01\x02\x03\x04\x05\x06\x07\x06\x06\x06\x06\xf7",0x12,message)) //some kind of poll request for autodetect
        {
            midiInfo->sendPollAnswer[0]=true;
            return;  
        } 

        if(matchMidiString(midiInfo->channel,"\xF0\x00\x00\x66\x14\x13\x00\xf7",8,message)) //some kind of poll request for autodetect
        {
            midiInfo->sendPollAnswer[1]=true;
            return;  
        } 

        if(matchMidiString(midiInfo->channel,"\xF0\x00\x00\x66\x14\x02\x01\x02\x03\x04\x05\x06\x07\x0c\x0c\x68\x10\xf7",0x12,message)) //some kind of poll request for autodetect
        {
            midiInfo->sendPollAnswer[2]=true;
            return;  
        } 
    }

    //dumpBuffer((char *)message->data(),message->size());
}

void initScribbleScripts(midiInfo_t *midiInfo)
{
    struct sockaddr_in address;
    int channel;
    int midiChannel;

    char buffer[40+1];

    for (channel=1;channel<=maxMidiPort;channel++)
    {
        for (midiChannel=0;midiChannel<8;midiChannel++)
        {
            uint32_t intval;
            unsigned char *byteval= (unsigned char *) &intval;
            if (channel>=5)
            {
                intval=71;
                memcpy(buffer,"/bus/00/config/icon\0,i\0\0",24);
                sprintf(&buffer[5],"%02d",midiChannel+1+((channel-5)*8));
                buffer[7]='/';
                sprintf(&buffer[24],"%c%c%c%c",*(byteval+3),*(byteval+2),*(byteval+1),*(byteval+0));
            }
            else
            {
                intval=74;
                memcpy(buffer,"/ch/00/config/icon\0\0,i\0\0",24);
                sprintf(&buffer[4],"%02d",midiChannel+1+((channel-1)*8));
                buffer[6]='/';
                sprintf(&buffer[24],"%c%c%c%c",*(byteval+3),*(byteval+2),*(byteval+1),*(byteval+0));
            }

            dumpBuffer(buffer,28);
            address.sin_family = AF_INET;
            address.sin_addr.s_addr = inet_addr(remoteIP);
            address.sin_port = htons(remoteport);
            networkSend(midiInfo->udpSocket, &address, (const uint8_t *) buffer, 28);
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
//main
///////////////////////////////////////////////////////////////////////////////

/**
 * Arguments:
 *  - 1. Local port at which the client listens.
 *  - 2. Remote port at which the remote server listens.
 *  - 3. Remote IP at which the remote server listens.
  */
int main(int argc, char *argv[])
{
	uint8_t rxBuffer[MAX_RX_UDP_PACKET];
	struct sockaddr_in address;
	int udpSocket;
	int count;
	int initCount=10;
	int d = 70;
	std::string portName;

	strcpy(remoteIP, "172.17.100.2");

	midiInfo_t midiInfo[MAX_MIDI_PORT];

	memset(midiInfo,0,sizeof(midiInfo));
	memset(muteState,0,sizeof(muteState));

	lockChannelClear();

	registerSignalHandler();

    //process arguments
    if (argc > 1)
        localport = atoi(argv[1]);
    if (argc > 2)
        remoteport = atoi(argv[2]);
    if (argc > 3)
        strcpy(remoteIP, argv[3]);

	if (argc > 1 && !strncmp(argv[1],"--list",6))
	{
		RtMidiIn  *midiin = 0;
		RtMidiOut *midiout = 0;

		// RtMidiIn constructor
		try 
		{
			midiin = new RtMidiIn();
		}
		catch ( RtError &error ) 
		{
			error.printMessage();
			exit( EXIT_FAILURE );
		}

		// Check inputs.
		unsigned int nPorts = midiin->getPortCount();
		std::cout << "\nThere are " << nPorts << " MIDI input sources available.\n";
		for ( unsigned int i=0; i<nPorts; i++ ) 
		{
			try 
			{
				portName = midiin->getPortName(i);
			}
			catch ( RtError &error ) 
			{
				error.printMessage();
                exit( EXIT_FAILURE );
			}
			printf("\t\t Input Port %-3d: '%s'\n",i,portName.c_str());
		}

		// RtMidiOut constructor
		try 
		{
			midiout = new RtMidiOut();
		}
		catch ( RtError &error ) 
		{
			error.printMessage();
			exit( EXIT_FAILURE );
		}

		// Check outputs.
		nPorts = midiout->getPortCount();
		std::cout << "\nThere are " << nPorts << " MIDI output ports available.\n";
		for ( unsigned int i=0; i<nPorts; i++ ) 
		{
			try 
			{
				portName = midiout->getPortName(i);
			}
			catch (RtError &error) {
				error.printMessage();
                exit( EXIT_FAILURE );
			}
			printf("\t\tOutput Port %-3d: '%s'\n",i,portName.c_str());
		}
		std::cout << '\n';

		// Clean up
		delete midiin;
		delete midiout;

		return 0;
	}

    if (argc > 1 && !strncmp(argv[1],"--help",6))
    {
        printf("%s",CLIENT_HELP_STR);
        return 0;
    }
    if (argc<4)
    {
        printf("%s",CLIENT_HELP_STR);
        return -1;
    }


    if (argc>4 && argc < 16)
	{
        printf("%s",CLIENT_HELP_STR);
		return -1;
	}

    if (argc>22)
    {
        printf("%s",CLIENT_HELP_STR);
        return -1;
    }

    if (argc<=16)
    {
        maxMidiPort-=3;
    }

    printf("Setting up network layer. local port:%d remote port:%d remote IP:%s\n",localport,remoteport,remoteIP);

    udpSocket = networkInit(localport);
    if ( udpSocket < 0 )
    {
        printf("Exit.\n");
        return -1;
    }

    for (int i=0;i<maxMidiPort;i++)    
    {
		midiInfo[i].channel=i+1;
    
		// RtMidiOut constructor                                                                                                                                           
        try 
        {
            midiInfo[i].midiin = new RtMidiIn();
        }
        catch ( RtError &error ) 
        {
            error.printMessage();
            exit( EXIT_FAILURE );
        }

        // RtMidiOut constructor                                                                                                                                           
        try 
        {
            midiInfo[i].midiout = new RtMidiOut();
        }
        catch ( RtError &error ) 
        {
            error.printMessage();
            exit( EXIT_FAILURE );
        }

        try 
        {
			if (argc > 4 && atoi(argv[4+i])>=0)
			{
				try 
				{
					portName = midiInfo[i].midiin->getPortName(atoi(argv[4+i]));
					printf("\t\t [%02d]  Input Port: '%s'\n",i+1,portName.c_str());
				}
				catch ( RtError &error ) 
				{
					error.printMessage();
					exit( EXIT_FAILURE );
 				}
				midiInfo[i].midiin->openPort(atoi(argv[4+i]),channelNameIn[i]);
			}
			else
			{
				midiInfo[i].midiin->openVirtualPort(channelNameIn[i],0x47494401+i);
			}
        }
        catch ( RtError &error ) {
            error.printMessage();
            exit( EXIT_FAILURE );
        }

        try 
        {
			if (argc > 4 && atoi(argv[maxMidiPort+4+i])>=0)
			{
				try 
				{
					portName = midiInfo[i].midiout->getPortName(atoi(argv[maxMidiPort+4+i]));
					printf("\t\t [%02d] Output Port: '%s'\n\n",i+1,portName.c_str());
				}
				catch ( RtError &error ) 
				{
					error.printMessage();
                    exit( EXIT_FAILURE );
				}
				midiInfo[i].midiout->openPort(atoi(argv[maxMidiPort+4+i]),channelNameOut[i]);
			}
			else
			{
	            midiInfo[i].midiout->openVirtualPort(channelNameOut[i],0x4F444901+i);
			}
        }
        catch ( RtError &error ) {
            error.printMessage();
            exit( EXIT_FAILURE );
        }

        // Don't ignore sysex, timing, or active sensing messages.
        midiInfo[i].midiin->ignoreTypes( false, false, false );    

        midiInfo[i].udpSocket=udpSocket;

        midiInfo[i].channel=i+1;

        midiInfo[i].midiin->setCallback( &midiCallback, &(midiInfo[i]) );
    }

    //initScribbleScripts(midiInfo);

    while (!doExit && initCount>0)
    {

        SLEEP(100);

        lockChannelHandler();

        if (d%70==0)
        {
            address.sin_family = AF_INET;
            address.sin_addr.s_addr = inet_addr(remoteIP);
            address.sin_port = htons(remoteport);
            networkSend(udpSocket, &address, (const uint8_t *) "/xremote", 8);
        }

        do
        {
            count = networkReceive(udpSocket,rxBuffer);
        } while (count>0);

        midiSendPollAnswer(midiInfo);

        d++;
        initCount --;
    }

    if (argc>15)
    {
        maxMidiPort-=3;
        useAllPorts=true;
    }

    maxChannels = (((maxMidiPort)*8)+1);

    printf("Init done\n");

    while (!doExit)
    {

        SLEEP(100);

        lockChannelHandler();

        if (d%70==0)
        {
            address.sin_family = AF_INET;
            address.sin_addr.s_addr = inet_addr(remoteIP);
            address.sin_port = htons(remoteport);
            networkSend(udpSocket, &address, (const uint8_t *) "/xremote", 8);
        }

        do
        {
            count = networkReceive(udpSocket,rxBuffer);
            if (count>0)
            {
                OSCSTRUCT osc;
                char debugString[4096];
                decodeOsc(rxBuffer, count, &osc, debugString, sizeof(debugString));
                printf("\t\t%s\n",debugString);

                //printOSC(&osc);
                mapOSC(&osc,midiInfo);
            }
        } while (count>0);


        midiSendPollAnswer(midiInfo);

        d++;
    }

    if (argc>15)
    {
        maxMidiPort+=2;
    }

    for (int i=0;i<maxMidiPort;i++)    
    {
        midiInfo[i].midiin->closePort();
        delete midiInfo[i].midiin;
        midiInfo[i].midiout->closePort();
        delete midiInfo[i].midiout;
    }

    //stop network layer
    networkHalt(udpSocket);

    printf("Exiting. Bye.\n");
    return 0;
}
