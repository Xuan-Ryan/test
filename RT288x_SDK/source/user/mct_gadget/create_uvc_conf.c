#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <syslog.h>
#include <pthread.h>

#include "i2s_ctrl.h"
#include "linux/autoconf.h" //kernel config

#include "mct_gadget.h"
#include "uvcdev.h"

#define UVC_DEV_NAME                                    "/dev/Web_UVC0"

#define UVC_GET_STAT                                    0x88
#define QUERY_SETUP_DATA                                0xF0
#define CAMERA_STS_CHANGE                               0xF1
#define APPLY_DESC_CHANGE                               0xF2
#define INTREP_WRIT                                     0xF3
#define UVC_SET_MANUFACTURER                            0xF4
#define UVC_SET_PRODUCT_NAME                            0xF5

int main(int argc, char ** argv)
{
	int idx = 0;
	int fd = -1;
	UVC_CAPABILITIES_DATA cap_data;
	memset(&cap_data, '\0', sizeof(UVC_CAPABILITIES_DATA));
	int mjpeg;
	int h264;
	int yuv;
	int nv12;
	int i420;
	int m420;

	if (argc = 2) {
		mjpeg = atoi(argv[1]);
		cap_data.mjpeg = mjpeg;
	}
	if (argc = 3) {
		h264 = atoi(argv[2]);
		cap_data.h264 = h264;
	}
	if (argc = 4) {
		yuv = atoi(argv[3]);
		cap_data.yuv = yuv;
	}
	if (argc = 5) {
		nv12 = atoi(argv[4]);
		cap_data.nv12 = nv12;
	}
	if (argc = 6) {
		i420 = atoi(argv[5]);
		cap_data.i420 = i420;
	}
	if (argc = 7) {
		m420 = atoi(argv[6]);
		cap_data.m420 = m420;
	}
/*
	switch(idx) {
	case 0:
		cap_data.mjpeg = 0x08;
		cap_data.h264 = 0x08;
		break;
	case 1:
		cap_data.mjpeg = 0x0c;
		cap_data.h264 = 0x0c;
		break;
	case 2:
		cap_data.mjpeg = 0x0e;
		cap_data.h264 = 0x0e;
		break;
	case 3:
		cap_data.mjpeg = 0x0f;
		cap_data.h264 = 0x0f;
		break;
	case 4:
		cap_data.mjpeg = 0x0f;
		cap_data.h264 = 0x0f;
		cap_data.yuv = 0x01;
		cap_data.nv12 = 0x01;
		break;
	case 5:
		cap_data.mjpeg = 0x01;
		cap_data.h264 = 0x01;
		cap_data.yuv = 0x01;
		cap_data.nv12 = 0x01;
		cap_data.i420 = 0x01;
		cap_data.m420 = 0x01;
		break;
	case 6:
		cap_data.yuv = 0x01;
		cap_data.nv12 = 0x01;
		cap_data.i420 = 0x01;
		cap_data.m420 = 0x01;
		break;
	case 15:
		cap_data.mjpeg = 0x0f;
		cap_data.h264 = 0x0f;
		cap_data.yuv = 0x0f;
		cap_data.nv12 = 0x0f;
		cap_data.i420 = 0x0f;
		cap_data.m420 = 0x0f;
		break;
	default:
		break;
	}*/
	printf("mjpeg = %x\n", cap_data.mjpeg);
	printf("h264 = %x\n", cap_data.h264);
	printf("yuv = %x\n", cap_data.yuv);
	printf("nv12 = %x\n", cap_data.nv12);
	printf("i420 = %x\n", cap_data.i420);
	printf("m420 = %x\n", cap_data.m420);

	fd = open(UVC_DEV_NAME, O_RDWR);
	if (fd) {
	/*	if (idx == 16) {
			ioctl(fd, UVC_SET_MANUFACTURER, "Tiger Corp");
			ioctl(fd, UVC_SET_PRODUCT_NAME, "Tiger Camera");
		} else {*/
			ioctl(fd, CAMERA_STS_CHANGE, &cap_data);
			ioctl(fd, APPLY_DESC_CHANGE, NULL);
	//	}
		close(fd);
	}
	return 0;
}

#if 0
#define UVC_CONF_FILE                   "/etc/uvc.conf"
#define CY_FX_UVC_MAX_PROBE_SETTING     34

unsigned char glProbeCtrl[CY_FX_UVC_MAX_PROBE_SETTING] = {
    0x00,0x00,                       /* bmHint : No fixed parameters */
    0x01,                            /* Use 1st Video format index */
    0x01,                            /* Use 1st Video frame index */
    0x15,0x16,0x05,0x00,             /* Desired frame interval in 100ns */
    0x00,0x00,                       /* Key frame rate in key frame/video frame units */
    0x00,0x00,                       /* PFrame rate in PFrame / key frame units */
    0x00,0x00,                       /* Compression quality control */
    0x00,0x00,                       /* Window size for average bit rate */
    0x00,0x00,                       /* Internal video streaming i/f latency in ms */
    0x00,0x30,0x2A,0x00,             /* Max video frame size in bytes */
    0x00,0x30,0x2A,0x00,             /* No. of bytes device can transmit in single payload */
    0x00,0x6C,0xDC,0x02,             /* The device clock frequency in Hz for the specified format. 48M*/
    0x03,                            /* bmFramingInfo */
    0x01,                            /* bPreferedVersion */
    0x01,                            /* bMinVersion based on bFormatIndex */
    0x02,                            /* bMaxVersion based on bFormatIndex */
};

int main(int argc, char ** argv)
{
	FILE * fp = NULL;

	fp = fopen(UVC_CONF_FILE, "w");
	if (fp) {
		fwrite(glProbeCtrl, CY_FX_UVC_MAX_PROBE_SETTING, 1, fp);
		fclose(fp);
	}
	return 0;
}
#endif