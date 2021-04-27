#ifndef __UVCHOST_H
#define __UVCHOST_H


#pragma pack(1)

#define GADGET_BROADCAST_PORT                   42776
#define GADGET_CONTROL_PORT                     42777
#define GADGET_CAMERA_PORT                      42800

#define GADGET_MIC_PORT                         42801
#define GADGET_SPK_PORT                         42802
#define GADGET_H264_PORT                        42803


#define FMT_MJPEG     0x47504a4d
#define FMT_H264      0x34363248
#define FMT_YUY2      0x56595559
#define FMT_NV12      0x3231564e

#define M_FMT_MJPEG     1
#define M_FMT_H264      2
#define M_FMT_YUY2      3
#define M_FMT_NV12      4

#define M_RES_4K        5
#define M_RES_1920X1080 4
#define M_RES_1280X720  3
#define M_RES_640X480   2
#define M_RES_320X240   1
#define T6_EN           0



#define CLEAR(x) memset(&(x), 0, sizeof(x))

#ifndef V4L2_PIX_FMT_H264
#define V4L2_PIX_FMT_H264     v4l2_fourcc('H', '2', '6', '4') /* H264 with start codes */
#endif

enum io_method {
        IO_METHOD_READ,
        IO_METHOD_MMAP,
        IO_METHOD_USERPTR,
};

struct buffer {
        void   *start;
        size_t  length;
};


struct uvcdev{
	int fd;
	int w; 
	int h;
	int format ;
	int force_format;
	struct buffer    *buffers;
    unsigned int     n_buffers;
	enum io_method   io;
	int socket;
	int hsocket;
	int cmd_socket;
	int udpsocket;
	int audio_webcam_cap_run;
	int audio_webcam_cap_active;
	int audio_webcam_play_run;
	int audio_webcam_play_active;
	int audio_dev_cap_run;
	int audio_dev_cap_active;
	int audio_dev_play_run;
	int audio_dev_play_active;
	int audio_thread_run;
	int audio_active;
	int video_thread_run;
	int video_active;
	int cmd_thread_run;
	int cmd_active;
	int uvc_detcet_run ;
	int tcp_detcet_run ;
	char video_id;
	unsigned short vid;
	unsigned short pid;
	unsigned short uvcver;

};

struct InputTerminal
{
   unsigned int Scanning_mode : 1;         
   unsigned int Auto_Exposure_mode : 1;
   unsigned int Auto_Exposure_Priority : 1;
   unsigned int Exposure_Time_Absolute : 1; 
   unsigned int Exposure_Time_Relative : 1;          // 注意: type 必須為整數(signed or unsigned皆可)
   unsigned int Focus_Absolute: 1;
   unsigned int Focus_Relative : 1;
   unsigned int Iris_Absolute : 1;
   unsigned int Iris_Relative : 1;
   unsigned int Zoom_Absolute : 1;
   unsigned int Zoom_Relative : 1;
   unsigned int PanTilt_Absolute : 1;
   unsigned int PanTilt_Relative : 1;
   unsigned int Roll_Absolute : 1;
   unsigned int Roll_Relative : 1;
   unsigned int Reserved1 : 1;
   unsigned int Reserved2 : 1;
   unsigned int Focus_Auto : 1;
   unsigned int Privacy : 1;
   unsigned int Reserved : 5;
};


union Vc_Input{
	struct InputTerminal Input;
	unsigned char bmControl[3];	
};

struct ProcessingUnit
{
	unsigned int Brightness:1;
	unsigned int Contrast:1;
	unsigned int Hue:1;
	unsigned int Saturation:1;
	unsigned int Sharpness:1;
	unsigned int Gamma:1;
	unsigned int White_Balance_Temperature:1;
	unsigned int White_Balance_Component:1;
	unsigned int Backlight_Compensation:1;
	unsigned int Gain:1;
	unsigned int Power_line_Frequency:1;
	unsigned int Hue_Auto:1;
	unsigned int White_Balance_Temperature_Auto:1;
	unsigned int White_Balance_Component_Auto:1;
	unsigned int Digital_Multiplier:1;
	unsigned int Digital_Multiplier_limilt:1;
	unsigned int Analog_Video_Standard:1;	
	unsigned int Analog_Video_Lock_Status:1;
	unsigned int Reserved : 6;	
};

union Vc_Processing{
	struct ProcessingUnit Processing;
	unsigned char bmControl[3];	
};


struct uvc_ctrl_info{
	unsigned int    Vc_Interface;
	unsigned char   Input_ID;
	unsigned char   Processing_ID;
	union Vc_Input          vci;
	union Vc_Processing  	vcp;
};


struct Resolution
{
	unsigned short Width;
	unsigned short Hight;
	unsigned char  framerate;
};

struct Resolution_list
{
	int format;
	int res_number;
	struct Resolution res[24];
};

struct format_list
{
	int index;
    struct Resolution_list res_list[6];
};


struct uvc_ctrl_cmd{
	unsigned char   cmd;
	unsigned short  len;
	unsigned char   data[9]; 
};

union ctrl_select{
	unsigned char   byte[2];
	unsigned short  cs;
};


struct uvc_ctrl_list{
	union ctrl_select    ctrl_sel;
	unsigned short       cmd_index;
	struct uvc_ctrl_cmd* pcmd_buf;
};

#pragma pack(pop)



#endif

