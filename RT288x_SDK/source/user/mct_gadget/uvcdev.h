#ifndef __MCT_UVCDEV__
#define __MCT_UVCDEV__

/*  ----------------  UVC  ----------------  */
#define UVC_DEV_NAME                            "/dev/Web_UVC0"
#define JUVC_PAGE_SIZE                          512

#define MCT_UVC_NOTIFY_SIZE                     256

#define MCT_PROTORCOL_VER_2                     1

#define CY_FX_UVC_MAX_PROBE_SETTING             34
#if MCT_PROTORCOL_VER_2
#define MCT_UVC_DATA_SIZE                       CY_FX_UVC_MAX_PROBE_SETTING+4
#else
#define MCT_UVC_DATA_SIZE                       CY_FX_UVC_MAX_PROBE_SETTING+12
#endif


struct uvc_state {
#if MCT_PROTORCOL_VER_2
	union {
		unsigned char setuptoken_8byte[8];
		struct DIRECT_CONTROL {
			unsigned char bmRequestType;
			unsigned char bRequest;
			unsigned short wValue;
			unsigned short wIndex;
			unsigned short wLength;
		} __attribute__ ((packed)) direct_control;
	};
#endif
	unsigned int status;
	unsigned char prob_ctrl[CY_FX_UVC_MAX_PROBE_SETTING];
} __attribute__ ((packed)) ;

typedef struct _uvc_capabilities_data {
	unsigned int mjpeg;
	unsigned int h264;
	unsigned int yuv;
	unsigned int nv12;
	unsigned int i420;
	unsigned int m420;
	unsigned int camera;
	unsigned int process;
}UVC_CAPABILITIES_DATA;

#define UVC_GET_STAT                            0x88
#define UVC_CLIENT_LOST                         0x89
#define QUERY_SETUP_DATA                        0xF0
#define CAMERA_STS_CHANGE                       0xF1
#define APPLY_DESC_CHANGE                       0xF2
#define INTREP_WRIT                             0xF3
#define UVC_SET_MANUFACTURER                    0xF4
#define UVC_SET_PRODUCT_NAME                    0xF5
#define UVC_SET_PIDVID                          0xF6
#define CTRLEP_WRITE                            0xF7
#define UVC_EP0_SET_HALT                        0xF8
#define UVC_EP0_CLEAR_HALT                      0xF9
#define UVC_SET_VC_DESC                         0xFA
#define UVC_GET_UVC_VER                         0xFB
#define UVC_RESET_TO_DEFAULT                    0xFC


int init_uvc_dev(void);
int ioctl_uvc(int ioctrl_cmd, void * data);
int get_status(void);
int write_uvc(char * buffer, int size);
int write_uvc4yuv(char * buffer, int size);
int close_uvc(void);

#endif
