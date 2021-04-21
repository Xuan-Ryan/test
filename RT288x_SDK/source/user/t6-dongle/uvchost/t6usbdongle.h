#ifndef _T6USBDONGLE_H_
#define _T6USBDONGLE_H_

#include "libusb/libusb.h"
#include "t6.h"
#include "t6auddef.h"
#include "t6bulkdef.h"



int  t6_libusb_get_displaysectionheader();
int  t6_libusb_get_ram_size();
int  t6_libusb_get_monitorstatus(char view);
int  t6_libusb_set_monitor_power(char on);
void t6_libusb_donglereset();
int  t6_libusb_get_edid(char *edid);
int  t6_libusb_set_AudioEngineStatus();
int   t6_libusb_set_softready();
int  t6_libusb_set_resolution( int w,int h);
int  t6_libusb_FilpJpegFrame(char *jpgimage ,int jpgsize,int flag ,int cmdAddr ,int fbAddr,int Width ,int Height);
int  t6_libusb_FilpJpegFrame422(char *jpgimage ,int jpgsize,int flag ,int cmdAddr ,int fbAddr,int Width ,int Height);
int t6_libusb_FilpJpegFrame444(char *jpgimage ,int jpgsize,int flag ,int cmdAddr ,int fbAddr,int Width ,int Height);

int  t6_libusb_SendAudio(char * data , int len  );

int  t6_libusb_dongle_reset();



#endif

