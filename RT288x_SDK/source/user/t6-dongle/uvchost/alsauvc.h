#ifndef __ALSAUVC_H
#define __ALSAUVC_H

//#define WRITE_FILE
#define CAP   1
#define PLK   0

#define AUDIO_CARD1_DEV  "hw:1,0"
#define AUDIO_CARD2_DEV  "hw:2,0"

#define AUDIO_CARD1_USBID  "/proc/asound/card1/usbid"
#define AUDIO_CARD2_USBID  "/proc/asound/card2/usbid"
#define AUDIO_CARD1_STREAM "/proc/asound/card1/stream0"
#define AUDIO_CARD2_STREAM "/proc/asound/card2/stream0"
#define AUDIO_CARD1_CAPTURE   "/proc/asound/card1/pcm0c/info"
#define AUDIO_CARD1_PLAYBACK  "/proc/asound/card1/pcm0p/info"
#define AUDIO_CARD2_CAPTURE   "/proc/asound/card2/pcm0c/info"
#define AUDIO_CARD2_PLAYBACK  "/proc/asound/card2/pcm0p/info"

struct audio_para{
    int mode;
	char dev[8];
	int format;
	int ch;
	int rateindex;
    int rate[16]; 
	int page_size;
	int socket;
	int *run;
	int *run2;
	void* resampleEngine; 
};


void DumpAudioParameter(struct audio_para *par);
int  GetAudioStream(char* filename, struct audio_para *par ,int mode);
void SendAudioCapture(struct audio_para *par,int card);
void PlayAudio(struct audio_para *par ,int card);

void RunAudioCapture(struct audio_para *par);
int     GetAudioUsbid(char* filename,uint16_t *vid,uint16_t *pid);
uint8_t GetAudioDevices();





#endif

