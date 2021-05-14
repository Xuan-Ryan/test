#ifndef __MCT_GADGET__
#define __MCT_GADGET__

//#define SUPPORT_H264_FOR_TCP                    1
//#define SUPPORT_RING_ELEMENT                    1
#define AUDIO_USES_TCP                          1
#define MIC_CB_SIZE                             8

#define GADGET_BROADCAST_PORT                   42776
#define GADGET_CONTROL_PORT                     42777
#define GADGET_CAMERA_PORT                      42800
#define GADGET_MIC_PORT                         42801
#define GADGET_SPK_PORT                         42802
#ifdef  SUPPORT_H264_FOR_TCP
#define GADGET_H264_PORT                        42803
#endif

#define UDP_TIME_OUT                            5
#define TCP_TIME_OUT                            3

#define PID_FILE                                "/var/run/mct_gadget.pid"
#define CONNECTED_FILE                          "/tmp/mctgadget_connected"

#define LOGFILE                                 "/dev/console"

#define JUVC_MAX_MCAST_PACKET_SIZE              8388608
#define JUVC_MAX_VIDEO_PACKET_SIZE              1048576
#define JUVC_H264_PACKET_SIZE                   1048576
#define JUVC_UDP_PKT_SIZE                       32768
#define JUVC_MAX_TCPCLIENT                      20

#ifdef SUPPORT_RING_ELEMENT
 #define RING_NUM                               4
#else
 #define QUEUE_NUM                              8
#endif
#define BG_FILENAME240P                         "bg-240p.jpg"
#define BG_FILENAME480P                         "bg-480p.jpg"
#define BG_FILENAME720P                         "bg-720p.jpg"
#define BG_FILENAME1080P                        "bg-1080p.jpg"
#define BGYUV_FILENAME240P                      "bg-240p.yuyv"

/*  ----------------  i2s  ----------------  */
#define I2S_DEV                                 "/dev/i2s0"

/*  ----------------  printf  ----------------  */
#if 1
 #define PRINT_MSG(fmt, arg...)                   do { FILE *log_fp = fopen(LOGFILE, "w+"); \
                                                     fprintf(log_fp, fmt , ##arg); \
                                                     fclose(log_fp); \
                                                   } while(0)

 //#define GADGET_DEBUG                            1
 #ifdef GADGET_DEBUG
  #define DBG_MSG(fmt, arg...)                    do { FILE *log_fp = fopen(LOGFILE, "w+"); \
                                                     fprintf(log_fp, fmt , ##arg); \
                                                     fclose(log_fp); \
                                                   } while(0)
 #else
  #define DBG_MSG(fmt, arg...)                    { }
 #endif
 //#define GADGET_DEEP_DEBUG                       1
 #ifdef GADGET_DEEP_DEBUG
  #define DEEP_MSG(fmt, arg...)                    do { FILE *log_fp = fopen(LOGFILE, "w+"); \
                                                     fprintf(log_fp, fmt , ##arg); \
                                                     fclose(log_fp); \
                                                   } while(0)
 #else
  #define DEEP_MSG(fmt, arg...)                    { }
 #endif
#else
 #define PRINT_MSG(fmt, arg...)                   {}
 #define DBG_MSG(fmt, arg...)                    { }
#endif

/*  ----------------  protocol  ----------------  */
#define JUVC_TAG                                1247106627 /* JUVC  */

/*  JUVC_TYPE_XXX  */
#define JUVC_TYPE_NONE                          0
#define JUVC_TYPE_CONTROL                       1
#define JUVC_TYPE_CAMERA                        2
#define JUVC_TYPE_MIC                           3
#define JUVC_TYPE_SPK                           4
#define JUVC_TYPE_ALIVE                         255

/*  JUVC_FLAGS_XXX  */
#define JUVC_FLAG_NONE                          0

/*  header size is 32 bytes  */
typedef struct juvc_hdr_packet {
	unsigned int                    Tag;           /*  JUVC  */
	unsigned char                   XactType;      /*  JUVC_TYPE_XXX  */
	unsigned char                   HdrSize;       /*  should be 32 always  */
	unsigned char                   XactId;        
	unsigned char                   Flags;         /*  JUVC_FLAGS_XXX or JUVC_CONTROL_XXX  */
	unsigned int                    PayloadLength;
	unsigned int                    TotalLength;
	unsigned int                    XactOffset;
	union {
		struct cameractrl {  /*  for JUVC_CONTROL_CAMERACTRL  */
			unsigned char   formatidx;
			unsigned char   frameidx;
			unsigned int    camera;
			unsigned int    process;
		} __attribute__ ((packed)) CamaraCtrl;
		struct camerainfo {  /*  for JUVC_CONTROL_CAMERAINFO  */
			unsigned char   mjpeg_res_bitfield;
			unsigned char   h264_res_bitfield;
			unsigned char   yuv_res_bitfield;
			unsigned char   nv12_res_bitfield;
			unsigned char   i420_res_bitfield;
			unsigned char   m420_res_bitfield;
			unsigned char   camera[3];
			unsigned char   process[3];
		} __attribute__ ((packed)) CamaraInfo;
		struct cameradescriptor {  /*  for JUVC_CONTROL_DESCRIPTOR  */
			unsigned short  idVendor;
			unsigned short  idProduct;
		} __attribute__ ((packed)) CamaraDescriptor;
		char ControlHeader[8];
		char Rsvd[12];
	};
} __attribute__ ((packed)) JUVCHDR, *PJUVCHDR;

/*  JUVC_CONTROL_XXX  */
#define JUVC_CONTROL_NONE                       0
#define JUVC_CONTROL_LOOKUP                     1
#define JUVC_CONTROL_CLNTINFO                   2
#define JUVC_CONTROL_CAMERAINFO                 3
#define JUVC_CONTROL_CAMERACTRL                 4
#define JUVC_CONTROL_MANUFACTURER               5
#define JUVC_CONTROL_PRODUCT                    6
#define JUVC_CONTROL_DESCRIPTOR                 7
#define JUVC_CONTROL_SET                        8
#define JUVC_CONTROL_GET                        9
#define JUVC_CONTROL_GETVC                      10
#define JUVC_CONTROL_STOP_START                 11
#define JUVC_CONTROL_MOBILE_DEV                 12
#define JUVC_CAMERA_DISCONNECT                  13

/*  JUVC_RES_XXX  */
#define JUVC_RESPONSE_ACK                       1

/*  JUVC_CONTROL_CLNTINFO  */
/*  not ready now  */
typedef struct juvc_client_info {
	unsigned char           mac_addr[18];    /*  mac address 00:05:1B:A0:00:00  */
} JUVCCLNTINFO, *PJUVCCLNTINFO;
#endif
