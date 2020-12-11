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
#include <sys/sem.h>
#include <semaphore.h>

#include "nvram.h"
#include "i2s_ctrl.h"
#include "linux/autoconf.h"

#include "mct_gadget.h"
#include "uvcdev.h"

sem_t video_write_mutex;

/*  I2S device  */
int i2s_fd = -1;

/*  tcp  */
int tcp_ctrl_socket = -1;
int tcp_mic_socket = -1;
int tcp_spk_socket = -1;
#ifdef  SUPPORT_H264_FOR_TCP
int tcp_h264_socket = -1;
#endif
int tcp_control_clnsd = 0;
static int client_connected = 0;

/*  udp  */
int udp_broadcast_socket = -1;
int udp_video_socket = -1;

/*  thread  */
pthread_t pthr_video_control;
int video_control_go = 0;
pthread_t pthr_broadcast;
int broadcast_go = 0;
pthread_t pthr_video;
int video_go = 0;
pthread_t pthr_video_write;
int video_write_go = 0;
pthread_t pthr_video_status;
int video_status_go = 0;
pthread_t pthr_mic;
int mic_go = 0;
pthread_t pthr_spk;
int spk_go = 0;
#ifdef  SUPPORT_H264_FOR_TCP
pthread_t pthr_h264;
int h264_go = 0;
#endif

static int is_yuv = 0;

void *shtxbuf[MAX_I2S_PAGE];
void *shrxbuf[MAX_I2S_PAGE];
unsigned char* sbuf;
struct stat i2s_stat;
#if defined(CONFIG_I2S_MMAP)
/*  not support right now  */
#else
char txbuffer[I2S_PAGE_SIZE+(I2S_PAGE_SIZE>>1)];
char rxbuffer[I2S_PAGE_SIZE];
#endif

static int exited = 0;
static char video_buffer[RAING_NUM][JUVC_MAX_VIDEO_PACKET_SIZE];
static int total_size[RAING_NUM] = {0, 0, 0, 0};
static int copying = 0;
static int done = 0;

extern struct uvc_state mct_uvc_data;
extern unsigned char gSetDirBuff[MCT_UVC_NOTIFY_SIZE];
extern unsigned char gGetDirBuff[MCT_UVC_NOTIFY_SIZE];

int write_raw(int type, char * buf, int len)
{
	int ret = 0;
	FILE * fp = NULL;
	if (type == 1) {
		fp = fopen("/etc/manufactorer", "w");
	} else if (type == 2) {
		fp = fopen("/etc/product", "w");
	} else if (type == 3) {
		fp = fopen("/etc/vc_desc", "w");
	}

	if (fp) {
		ret = fwrite(buf, len, 1, fp);
		fclose(fp);
	}

	return ret;
}

static void load_bg(void)
{
	const char * lang =  nvram_get(RT2860_NVRAM, "lang");
	char full_name[256];
	FILE * fp = NULL;

	if (strlen(lang) > 0) {
		if (mct_uvc_data.status == 1) {
			if (mct_uvc_data.prob_ctrl[2] >= 3) {
				sprintf(full_name, "/home/bg/%s/%s", lang, BGYUV_FILENAME240P);
			} else {
				if (mct_uvc_data.prob_ctrl[3] == 4)
					sprintf(full_name, "/home/bg/%s/%s", lang, BG_FILENAME1080P);
				else if (mct_uvc_data.prob_ctrl[3] == 3)
					sprintf(full_name, "/home/bg/%s/%s", lang, BG_FILENAME720P);
				else if (mct_uvc_data.prob_ctrl[3] == 2)
					sprintf(full_name, "/home/bg/%s/%s", lang, BG_FILENAME480P);
				else if (mct_uvc_data.prob_ctrl[3] == 1)
					sprintf(full_name, "/home/bg/%s/%s", lang, BG_FILENAME240P);
				else
					sprintf(full_name, "/home/bg/%s/%s", lang, BG_FILENAME720P);
			}
		} else {
			total_size[0] = 0;
			return;
		}
	} else {
		/*  open the default folder  */
		if (mct_uvc_data.status == 1) {
			if (mct_uvc_data.prob_ctrl[2] >= 3) {
				sprintf(full_name, "/home/bg/en-us/%s", BGYUV_FILENAME240P);
			} else {
				if (mct_uvc_data.prob_ctrl[3] == 4)
					sprintf(full_name, "/home/bg/en-us/%s", BG_FILENAME1080P);
				else if (mct_uvc_data.prob_ctrl[3] == 3)
					sprintf(full_name, "/home/bg/en-us/%s", BG_FILENAME720P);
				else if (mct_uvc_data.prob_ctrl[3] == 2)
					sprintf(full_name, "/home/bg/en-us/%s", BG_FILENAME480P);
				else if (mct_uvc_data.prob_ctrl[3] == 1)
					sprintf(full_name, "/home/bg/en-us/%s", BG_FILENAME240P);
				else
					sprintf(full_name, "/home/bg/en-us/%s", BG_FILENAME720P);
			}
		} else {
			total_size[0] = 0;
			return;
		}
	}

	fp = fopen(full_name, "r");
	if (fp) {
		total_size[0] = fread(video_buffer[0], 1, JUVC_MAX_VIDEO_PACKET_SIZE, fp);
		fclose(fp);
	} else {
		total_size[0] = 0;
	}
}

static void close_socket()
{
	DBG_MSG("mct_gadget: Close socket\n");

	if (tcp_ctrl_socket > -1) {
		close(tcp_ctrl_socket);
		tcp_ctrl_socket = -1;
	}
	if (tcp_mic_socket > -1) {
		close(tcp_mic_socket);
		tcp_mic_socket = -1;
	}
	if (tcp_spk_socket > -1) {
		close(tcp_spk_socket);
		tcp_spk_socket = -1;
	}
#ifdef  SUPPORT_H264_FOR_TCP
	if (tcp_h264_socket > -1) {
		close(tcp_h264_socket);
		tcp_h264_socket = -1;
	}
#endif
	if (udp_broadcast_socket > -1) {
		close(udp_broadcast_socket);
		udp_broadcast_socket = -1;
	}
	if (udp_video_socket > -1) {
		close(udp_video_socket);
		udp_video_socket = -1;
	}

}

static int init_tcp_socket(unsigned short port)
{
	int on = 1;
	struct sockaddr_in serveraddr;
	int tcp_socket = -1;

	//struct timeval tv;
	//memset(&tv, '\0', sizeof(tv));
	//tv.tv_sec = TCP_TIME_OUT;

	/* create sockett */
	tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (tcp_socket < 0) {
		DBG_MSG("ERROR opening tcp socket");
		return -1;
	}

	//setsockopt(tcp_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

	if(setsockopt(tcp_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) < 0) {
		DBG_MSG("Server-setsockopt() error");
		close(tcp_socket);
		tcp_socket = -1;
		return -1;
	}

	/* initialize structure serveraddr */
	bzero(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(port);
	serveraddr.sin_addr.s_addr = INADDR_ANY;

	/* Assign a port number to socket */
	if (bind(tcp_socket, (struct sockaddr*)&serveraddr, sizeof(serveraddr)) < 0) {
		DBG_MSG("ERROR on tcp binding");
		return -1;
	}

	/* make it listen to socket with max connections */
	listen(tcp_socket, JUVC_MAX_TCPCLIENT);

	return tcp_socket;
}

static int init_udp_socket(unsigned short port, int broadcast)
{
	int optval;			/* flag value for setsockopt */
	struct sockaddr_in serveraddr;	/* server's addr */
	struct timeval tv;
	int udp_sock = -1;

	memset(&tv, '\0', sizeof(tv));
	tv.tv_sec = UDP_TIME_OUT;

	udp_sock = socket(AF_INET, SOCK_DGRAM, 0);

	if (udp_sock < 0) {
		perror("ERROR opening udp socket");
		return -1;
	}

	setsockopt(udp_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	optval = 1;
	setsockopt(udp_sock, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int));

	optval = JUVC_MAX_MCAST_PACKET_SIZE;
	setsockopt(udp_sock,SOL_SOCKET,SO_RCVBUF,(const char*)&optval,sizeof(int));

	if (broadcast == 1) {
		/*  make sure send Packet have broadcast attribute  */
		bool brdcast = true;
		setsockopt(udp_sock, SOL_SOCKET, SO_BROADCAST, (const char*)&brdcast, sizeof(bool));
	}

	/*
	 * build the server's Internet address
	 */
	bzero((char *)&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = INADDR_ANY;
	serveraddr.sin_port = htons(port);

	/*
	 * bind: associate the socket with a port
	 */
	if (bind(udp_sock, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0) {
		DBG_MSG("ERROR on udp binding");
		return -1;
	}

	return udp_sock;
}

static void signal_handler(int sig)
{
	if (sig == SIGINT) {
		DBG_MSG("mct_gadget: SIGINT!\n");
		exited = 1;
	} else if (sig == SIGALRM) {
		DBG_MSG("mct_gadget: SIGALRM!\n");
	} else if (sig == SIGUSR1) {
		DBG_MSG("mct_gadget: SIGUSR1!\n");
	} else if (sig == SIGUSR2) {
		DBG_MSG("mct_gadget: SIGUSR2!\n");
	}
}

static int check_exist_inst(void)
{
	int fd_pid = open(PID_FILE, O_WRONLY | O_CREAT | O_EXCL, 0660);

	if (fd_pid < 0) {
		if (errno == EEXIST) {
			DBG_MSG("mct_gadget: Program is already running.\n");
		} else {
			DBG_MSG("mct_gadget: File %s: %m", PID_FILE);
		}
		return -1;
	}
	return 0;
}

static int init_i2s_device(void)
{
	i2s_fd = open(I2S_DEV, O_RDWR|O_SYNC);
	if (i2s_fd < 0) {
		DBG_MSG("mct_gadget: Can't open i2s device\n");
		return -1;
	}

	/*  initial i2s  */
	ioctl(i2s_fd, I2S_RESET, 0);
	ioctl(i2s_fd, I2S_SRATE, 48000);
	ioctl(i2s_fd, I2S_TX_VOL, 100);
	ioctl(i2s_fd, I2S_RX_VOL, 100);
	/*  set to slave  */
	ioctl(i2s_fd, I2S_MS_MODE_CTRL, 1);
	ioctl(i2s_fd, I2S_TXRX_COEXIST, 1);
	/*  NOTE
	 *  if we doing this in here, this program will be stuck here until the user plugs the USB cable into pc.  */
	//ioctl(i2s_fd, I2S_RX_ENABLE, 0);
	//ioctl(i2s_fd, I2S_TX_ENABLE, 0);

	return 0;
}

static void set_signal_handle(void)
{
	(void) signal(SIGINT, signal_handler);
	(void) signal(SIGALRM, signal_handler);
	(void) signal(SIGUSR1, signal_handler);
	(void) signal(SIGUSR2, signal_handler);

	/*  NOTE
	 *  when socket write failed, the system will send SIGPIPE to the caller, this will terminate the caller,
	 *  so we need to ignore this signal to avoid this issue.  */
	(void) signal(SIGPIPE, SIG_IGN);
}

static void save_pid(void)
{
	FILE * fp = NULL;
	fp = fopen(PID_FILE, "w");
	if (fp) {
		fprintf(fp, "%d", getpid());
		fclose(fp);
	}
}

static void close_i2s(void)
{
	DBG_MSG("mct_gadget: Close I2S\n");

	if (i2s_fd > 0) {
		ioctl(i2s_fd, I2S_TXRX_COEXIST, 0);
		/*  *************************
		 *  NOTE: if the upstream not connect to PC, this function can't return due to
		 *  the i2s will stuck on ioctl of the i2s driver
		 *  *************************  */
		//ioctl(i2s_fd, I2S_TX_DISABLE, 0);
		//ioctl(i2s_fd, I2S_RX_DISABLE, 0);
		close(i2s_fd);
		i2s_fd = -1;
	}
}

void hex_dump(char * buf, int len)
{
	while (len--)
		DBG_MSG("%02x,", (*buf++)&0xff);
	DBG_MSG("\n");
}

int send_setcommand(char * data, int size)
{
	int ret = 0;
	JUVCHDR juvchdr;
	int l = sizeof(JUVCHDR);

	if (tcp_control_clnsd <= -1) {
		return -1;
	}

	juvchdr.Tag = JUVC_TAG;
	juvchdr.XactType = JUVC_TYPE_CONTROL;
	juvchdr.HdrSize = l;
	juvchdr.XactId = 0;
	juvchdr.Flags = JUVC_CONTROL_SET;
	juvchdr.PayloadLength = size;
	juvchdr.TotalLength = size;
	juvchdr.XactOffset = 0;
	memcpy(juvchdr.ControlHeader, data, 8);

	ret = send(tcp_control_clnsd, &juvchdr, l, 0);
	DBG_MSG("%s: ret = %d\n", __FUNCTION__, ret);
	ret = send(tcp_control_clnsd, &data[8], size, 0);
	DBG_MSG("%s: ret = %d\n", __FUNCTION__, ret);
	return ret;
}

int send_getcommand(char * data, int size)
{
	int ret = 0;
	JUVCHDR juvchdr;
	int l = sizeof(JUVCHDR);

	if (tcp_control_clnsd <= -1) {
		return -1;
	}

	juvchdr.Tag = JUVC_TAG;
	juvchdr.XactType = JUVC_TYPE_CONTROL;
	juvchdr.HdrSize = l;
	juvchdr.XactId = 0;
	juvchdr.Flags = JUVC_CONTROL_GET;
	juvchdr.PayloadLength = size;
	juvchdr.TotalLength = size;
	juvchdr.XactOffset = 0;
	memcpy(juvchdr.ControlHeader, data, 8);

	ret = send(tcp_control_clnsd, &juvchdr, l, 0);

	DBG_MSG("%s: ret = %d\n", __FUNCTION__, ret);
	return 0;
}

int send_command(unsigned char cmd, unsigned char format, unsigned char frame, unsigned int camera, unsigned int process)
{
	int ret = 0;
	JUVCHDR juvchdr;
	int l = sizeof(JUVCHDR);

	if (tcp_control_clnsd <= -1) {
		return -1;
	}

	juvchdr.Tag = JUVC_TAG;
	juvchdr.XactType = JUVC_TYPE_CONTROL;
	juvchdr.HdrSize = l;
	juvchdr.XactId = 0;
	juvchdr.Flags = cmd;
	juvchdr.PayloadLength = l;
	juvchdr.TotalLength = l;
	juvchdr.XactOffset = 0;
	if (cmd == JUVC_CONTROL_CAMERACTRL) {
		juvchdr.CamaraCtrl.formatidx = format;
		juvchdr.CamaraCtrl.frameidx = frame;
		juvchdr.CamaraCtrl.camera = camera;
		juvchdr.CamaraCtrl.process = process;
	}

	ret = send(tcp_control_clnsd, &juvchdr, l, 0);

	DBG_MSG("%s: ret = %d\n", __FUNCTION__, ret);
	return ret;
}

extern int cnt;
void * video_status_thread(void * arg)
{
	DBG_MSG("mct_gadget: %s go\n", __FUNCTION__);

	while(exited == 0) {
		if (get_status() == 0) {
			DBG_MSG("mct_gadget: mct_uvc_data.status = %d\n", mct_uvc_data.status);
#if MCT_PROTORCOL_VER_2
			if (mct_uvc_data.direct_control.wIndex == 0 &&
			    mct_uvc_data.direct_control.bmRequestType == 0 &&
			    mct_uvc_data.direct_control.bRequest == 0 &&
			    mct_uvc_data.direct_control.wValue == 0 &&
			    mct_uvc_data.direct_control.wLength == 0) {  /*  for video stream  */
#endif
				/*  if the status equal to 1 or 0 we need to load background image again  */
				if (mct_uvc_data.status < 2) {
					load_bg();
					if (mct_uvc_data.prob_ctrl[2] >= 3)
						is_yuv = 1;
					else
						is_yuv = 0;
					send_command(JUVC_CONTROL_CAMERACTRL, mct_uvc_data.prob_ctrl[2], mct_uvc_data.prob_ctrl[3], 0, 0);
				}
#if MCT_PROTORCOL_VER_2
			} else {  /*  for video control  */
				if (mct_uvc_data.direct_control.bmRequestType&0x20 == 0x20) {  /*  CLASS  */
					if (mct_uvc_data.direct_control.wIndex != 0) {
						struct DIRECT_CONTROL * ptr;
						if (mct_uvc_data.direct_control.bmRequestType & 0x80) {
							/*  GET  */
							ptr = (struct DIRECT_CONTROL *)gGetDirBuff;
							send_getcommand(gGetDirBuff, ptr->wLength);
						} else {
							/*  SET  */
							ptr = (struct DIRECT_CONTROL *)gSetDirBuff;
							send_setcommand(gSetDirBuff, ptr->wLength);
						}
					} else if (mct_uvc_data.direct_control.bmRequestType&0x20 == 0x20) {
						char data[256];
						data[0] = 1;
						data[1] = 0;
						ioctl_uvc(CTRLEP_WRITE, data);
					}
				}
			}
#endif
		}
	}

	DBG_MSG("mct_gadget: %s exit\n", __FUNCTION__);
}

#define UVC_SETCONF_DONE                "/etc/uvc_setconf_done"
int wait_setting_done()
{
	FILE * fp = fopen(UVC_SETCONF_DONE, "r");
	if (fp) {
		fclose(fp);
		return 1;
	}
	return 0;
}

static void * video_control_thread(void * arg)
{
	int ret = -1;
	struct sockaddr_in from;
	socklen_t len;
	JUVCHDR juvchdr;
	int l = sizeof(JUVCHDR);
	int optval = 1;
	socklen_t optlen = sizeof(optval);
	UVC_CAPABILITIES_DATA cap_data;
	char data[256];

	DBG_MSG("mct_gadget: %s go\n", __FUNCTION__);

	while(exited == 0) {
		if (tcp_ctrl_socket <= -1) {
			sleep(1);
			continue;
		}

		memset(&from, '\0', sizeof(struct sockaddr_in));
		tcp_control_clnsd = accept(tcp_ctrl_socket, (struct sockaddr*)&from, &len);

		if (tcp_control_clnsd < 0) {
			sleep(1);
			continue;
		}

		/*  maybe we don't need to wait for this, anyway, just check it out  */
		if (!wait_setting_done()) {
			sleep(1);
			continue;
		}

		PRINT_MSG("mct_gadget: client connected\n");
		client_connected = 1;
		sleep(1);
		setsockopt(tcp_control_clnsd, SOL_SOCKET, SO_KEEPALIVE, &optval, optlen);
		ret = send_command(JUVC_CONTROL_CAMERAINFO, mct_uvc_data.prob_ctrl[2], mct_uvc_data.prob_ctrl[3], 0, 0);

		while(exited == 0) {
			ret = read(tcp_control_clnsd, &juvchdr, l);
			if (ret == 0) {
				/*  close by the client side, we need to jump out this loop  */
				close(tcp_control_clnsd);
				tcp_control_clnsd = 0;
				break;
			}
			DBG_MSG("mct_gadget: %d, ret = %d\n" , __LINE__, ret);
			DBG_MSG("mct_gadget: juvchdr.XactType = %x\n", juvchdr.XactType);
			DBG_MSG("mct_gadget: juvchdr.Flags = %x\n", juvchdr.Flags);
			/*  check packet  */
			if (juvchdr.Tag == JUVC_TAG) {
				/*  process command  */
				if (juvchdr.XactType == JUVC_TYPE_CONTROL) {
					if (juvchdr.Flags == JUVC_CONTROL_CAMERAINFO) {

						memset(&cap_data, '\0', sizeof(UVC_CAPABILITIES_DATA));
						cap_data.mjpeg = juvchdr.CamaraInfo.mjpeg_res_bitfield;
						cap_data.h264 = juvchdr.CamaraInfo.h264_res_bitfield;
						cap_data.yuv = juvchdr.CamaraInfo.yuv_res_bitfield;
						cap_data.nv12 = juvchdr.CamaraInfo.nv12_res_bitfield;
						cap_data.i420 = juvchdr.CamaraInfo.i420_res_bitfield;
						cap_data.m420 = juvchdr.CamaraInfo.m420_res_bitfield;
						/*  If we want to disable all of the control capability of the camera, 
						 *  just commented out the following two lines.  */
						memcpy(&cap_data.camera, juvchdr.CamaraInfo.camera, 3);
						memcpy(&cap_data.process, juvchdr.CamaraInfo.process, 3);
						ioctl_uvc(CAMERA_STS_CHANGE, &cap_data);
						//DBG_MSG("mjpeg : %x\n", juvchdr.CamaraInfo.mjpeg_res_bitfield);
						//DBG_MSG("h264 : %x\n", juvchdr.CamaraInfo.h264_res_bitfield);
						//DBG_MSG("yuv : %x\n", juvchdr.CamaraInfo.yuv_res_bitfield);
						//DBG_MSG("nv12 : %x\n", juvchdr.CamaraInfo.nv12_res_bitfield);
						//DBG_MSG("i420 : %x\n", juvchdr.CamaraInfo.i420_res_bitfield);
						//DBG_MSG("m420 : %x\n",  juvchdr.CamaraInfo.m420_res_bitfield);
						//DBG_MSG("camera : %02x %02x %02x\n", juvchdr.CamaraInfo.camera[0]&0xff, juvchdr.CamaraInfo.camera[1]&0xff, juvchdr.CamaraInfo.camera[2]&0xff);
						//DBG_MSG("process : %02x %02x %02x\n", juvchdr.CamaraInfo.process[0]&0xff, juvchdr.CamaraInfo.process[1]&0xff, juvchdr.CamaraInfo.process[2]&0xff);
					} else if (juvchdr.Flags == JUVC_CONTROL_MANUFACTURER) {
						char buffer[256];
						read(tcp_control_clnsd, buffer, juvchdr.TotalLength);
						write_raw(1, buffer, juvchdr.TotalLength);
						ioctl_uvc(UVC_SET_MANUFACTURER, 0);
					} else if (juvchdr.Flags == JUVC_CONTROL_PRODUCT) {
						char buffer[256];
						read(tcp_control_clnsd, buffer, juvchdr.TotalLength);
						write_raw(2, buffer, juvchdr.TotalLength);
						ioctl_uvc(UVC_SET_PRODUCT_NAME, 0);
					} else if (juvchdr.Flags == JUVC_CONTROL_GETVC) {
						char buffer[2048];
						int len = read(tcp_control_clnsd, buffer, juvchdr.TotalLength);
						write_raw(3, buffer, juvchdr.TotalLength);
						ioctl_uvc(UVC_SET_VC_DESC, 0);
						ioctl_uvc(UVC_GET_UVC_VER, 0);
					} else if (juvchdr.Flags == JUVC_CONTROL_DESCRIPTOR) {
						unsigned int pidvid = (juvchdr.CamaraDescriptor.idVendor << 16);
						pidvid |= juvchdr.CamaraDescriptor.idProduct;
						ioctl_uvc(UVC_SET_PIDVID, &pidvid);
					} else if (juvchdr.Flags == JUVC_CONTROL_GET) {
						if (juvchdr.TotalLength > 0) {
							data[0] = juvchdr.TotalLength;
							read(tcp_control_clnsd, &data[1], juvchdr.TotalLength);
							ioctl_uvc(CTRLEP_WRITE, data);
						} else {
							/*  set UVC to STALL  */
							ioctl_uvc(UVC_EP0_SET_HALT, NULL);
						}
					}
				}
			}
		}
		ioctl_uvc(UVC_CLIENT_LOST, NULL);
		PRINT_MSG("mct_gadget: client disconnected\n");
		tcp_control_clnsd = -1;
		client_connected = 0;
	}

	DBG_MSG("mct_gadget: %s exit\n", __FUNCTION__);
}

void * broadcast_thread(void * arg)
{
	int ret = 0;
	struct sockaddr_in from;
	socklen_t len = sizeof(struct sockaddr_in);
	JUVCHDR juvchdr;
	int l = sizeof(JUVCHDR);

	DBG_MSG("mct_gadget: %s go\n", __FUNCTION__);

	while(exited == 0) {
		if (udp_broadcast_socket <= -1) {
			sleep(1);
			continue;
		}
		/*  read from socket;  */
		memset(&from, '\0', sizeof(struct sockaddr_in));
	 	ret = recvfrom(udp_broadcast_socket, &juvchdr, l, 0, (struct sockaddr*)&from, &len);
		if (ret <= 0) {
			continue;
		}

		/*  check packet  */
		if (juvchdr.Tag == JUVC_TAG) {
			if (juvchdr.XactType == JUVC_TYPE_CONTROL) {
				int n = juvchdr.PayloadLength-l;
				ret = recvfrom(udp_broadcast_socket, &juvchdr, n, 0, (struct sockaddr*)&from, &len);
				if (ret <= 0) {
					/*  wrong packet  */
					continue;
				}
			}
		}
	}

	DBG_MSG("mct_gadget: %s exit\n", __FUNCTION__);
}

#define FOR_TEST  0

#if FOR_TEST
int test_image(char * buf, int max)
{
	FILE * fp = NULL;
	int n = 0;
	fp = fopen("/bin/1280x720.jpg", "r");
	if (fp) {
		n = fread(buf, 1, max, fp);
		fclose(fp);
	}
	return n;
}
#endif

void * video_write_thread(void * arg)
{
	DBG_MSG("mct_gadget: %s go\n", __FUNCTION__);

	while(exited == 0) {
		if (udp_video_socket <= -1) {
			sleep(1);
			continue;
		}
		sem_wait(&video_write_mutex);
		if (is_yuv == 0)
			write_uvc(video_buffer[done], total_size[done]);
		else
			write_uvc4yuv(video_buffer[done], total_size[done]);
	}

	DBG_MSG("mct_gadget: %s exit\n", __FUNCTION__);
}

#define COUNTING_FPS					0
#if COUNTING_FPS
struct timeval tv;					/*  for time calculation  */
unsigned long long start_t =0, end_t =0, period=0, pre_period=0;
int count_num = 0;					/*  Encoded frame count  */
int real_fps = 0;					/*  real_fps frame count  */
long long unsigned int data_size = 0;			/*  Count the total datasize  */
#endif

void * video_thread(void * arg)
{
	int n = 0;
	struct sockaddr_in from;
	socklen_t len = sizeof(struct sockaddr_in);
	JUVCHDR juvchdr;
	int l = sizeof(JUVCHDR);
	PJUVCHDR header = NULL;
	int first_time = 1;
	char buffer[JUVC_UDP_PKT_SIZE];
	unsigned char xid = 0;

	DBG_MSG("mct_gadget: %s go\n", __FUNCTION__);

#if FOR_TEST
	total_size[0] = test_image(video_buffer[0], JUVC_MAX_VIDEO_PACKET_SIZE);
	PRINT_MSG("total_size = %d\n", total_size[0]);
#endif

	while(exited == 0) {
#if FOR_TEST
		if (total_size[0] > 0) {
			if (write_uvc(video_buffer[0], total_size[0]) > 0) {
				sleep(1);
				continue;
			}
		}
#else
		if (udp_video_socket <= -1) {
			sleep(1);
			continue;
		}

		if (client_connected == 1) {
			/*  read from socket;  */
			memset(&from, '\0', sizeof(struct sockaddr_in));
			n = recvfrom(udp_video_socket, buffer, JUVC_UDP_PKT_SIZE, 0, (struct sockaddr*)&from, &len);
			if (n <= 0) {
				continue;
			}
#if COUNTING_FPS
			data_size += n;
#endif
			header = (PJUVCHDR)buffer;
			if (first_time == 1) {
				if (header->XactOffset != 0) {
					continue;  /*  drop this packet  */
				} else {
					first_time = 0;
					memcpy(video_buffer[copying], buffer+sizeof(JUVCHDR), header->PayloadLength);
					total_size[copying] = header->PayloadLength;
					xid = header->XactId;
				}
			} else {
				if (header->XactOffset == 0) {
					memcpy(video_buffer[copying], buffer+sizeof(JUVCHDR), header->PayloadLength);
					total_size[copying] = header->PayloadLength;
					xid = header->XactId;
				} else {
					if (xid != header->XactId) {
						DBG_MSG("packet failed: total_size = (%d/%d), offset = %d, xid = (%d/%d)\n", total_size[copying], header->TotalLength, header->XactOffset, xid, (unsigned int)header->XactId);
						continue;
					}
					memcpy(video_buffer[copying]+header->XactOffset, buffer+sizeof(JUVCHDR), header->PayloadLength);
					total_size[copying] += header->PayloadLength;
				}
			}
			//DBG_MSG("total_size[%d] = (%d/%d), offset = %d, xid = %d\n", copying, total_size[copying], header->TotalLength, header->XactOffset, (unsigned int)header->XactId);
			if (total_size[copying] >= header->TotalLength) {
#if COUNTING_FPS
				count_num ++;
				real_fps ++;
				gettimeofday(&tv,NULL);
				end_t = tv.tv_sec * 1000000 + tv.tv_usec;
				period = end_t - start_t;

				if (period - pre_period >= 1000000) {
					pre_period = period;
					DBG_MSG("fps=%d (frames= %d), bps=%lld Kbps\n" \
					, real_fps \
					, count_num \
					, (data_size>>7));  /*  this mean size/1024*8  */

					real_fps = 0;
					data_size = 0;
				}
#endif
				done = copying;
				copying = ++copying%RAING_NUM;
				sem_post(&video_write_mutex);
			}
		} else {
			//DBG_MSG("total_size[0] = %d\n", total_size[0]);
			if (total_size[0] > 0) {
				if (is_yuv == 0) {
					if (write_uvc(video_buffer[0], total_size[0]) > 0) {
						sleep(1);
					}
				} else {
					if (write_uvc4yuv(video_buffer[0], total_size[0]) > 0) {
						sleep(1);
					}
				}
			} else {
				/*  failed to get the bg  */
				sleep(1);
			}
		}
#endif
	}

	DBG_MSG("mct_gadget: %s exit\n", __FUNCTION__);
}

void * mic_thread(void * arg)
{
	int ret = 0;
	struct sockaddr_in from;
	int len = sizeof(from);
	int tcp_mic_clnsd = 0;
	char mic_buffer[I2S_PAGE_SIZE];
	int retry = 0;

	DBG_MSG("mct_gadget: %s go\n", __FUNCTION__);

	ioctl(i2s_fd, I2S_TX_ENABLE, 0);

	while(exited == 0) {
		if (tcp_mic_socket <= -1) {
			sleep(1);
			continue;
		}

		if (client_connected == 1) {
			/*  waiting for client  */
			//DBG_MSG("waiting for client\n");
			memset(&from, '\0', sizeof(struct sockaddr_in));
			tcp_mic_clnsd = accept(tcp_mic_socket, (struct sockaddr*)&from, &len);

			if (tcp_mic_clnsd < 0) {
				sleep(1);
				continue;
			}

			while(exited == 0) {
				/*  read from socket;  */
				ret = read(tcp_mic_clnsd, mic_buffer, I2S_PAGE_SIZE);
				if (ret < 0) {
					sleep(1);
					/*  is timeout maybe we need to read again  */
					if (++retry > 10) {
						/*  retry too many times  */
						retry = 0;
						break;
					}
					continue;
				} else if (ret == 0) {
					/*  close by the client side, we need to jump out this loop  */
					close(tcp_mic_clnsd);
					tcp_mic_clnsd = 0;
					break;
				}

				/*  write to i2s  */
				if (i2s_fd > 0) {
					ioctl(i2s_fd, I2S_PUT_AUDIO, mic_buffer);
				} else {
					sleep(1);
					continue;
				}
			}

			if (tcp_mic_clnsd > 0) {
				close(tcp_mic_clnsd);
				tcp_mic_clnsd = 0;
			}
		}
	}
	/*  NOTE
	 *  before we leave this thread, we have to stop the TX, otherwise, the system will be crash.  */
	ioctl(i2s_fd, I2S_TX_DISABLE, 0);

	DBG_MSG("mct_gadget: %s exit\n", __FUNCTION__);
}

void * spk_thread(void * arg)
{
	int ret = 0;
	struct sockaddr_in client_addr;
	int addrlen = sizeof(client_addr);
	int tcp_spk_clnsd = 0;
	char spk_buffer[I2S_PAGE_SIZE];

	DBG_MSG("mct_gadget: %s go\n", __FUNCTION__);

	ioctl(i2s_fd, I2S_RX_ENABLE, 0);

	while(exited == 0) {
		if (tcp_spk_socket <= -1) {
			sleep(1);
			continue;
		}

		if (client_connected == 1) {
			/*  waiting for client  */
			//DBG_MSG("waiting for client\n");
			tcp_spk_clnsd = accept(tcp_spk_socket, (struct sockaddr*)&client_addr, &addrlen);

			if (tcp_spk_clnsd < 0) {
				sleep(1);
				continue;
			}

			while(exited == 0) {
				/*  read from i2s  */
				if (i2s_fd > 0) {
					ioctl(i2s_fd, I2S_GET_AUDIO, spk_buffer);
				} else {
					sleep(1);
					continue;
				}

				/*  write to socket;  */
				ret = write(tcp_spk_clnsd, spk_buffer, I2S_PAGE_SIZE);
				if (ret <= 0) {
					sleep(1);
					continue;
				}
			}

			if (tcp_spk_clnsd > 0) {
				close(tcp_spk_clnsd);
				tcp_spk_clnsd = 0;
			}
		}
	}
	/*  NOTE
	 *  before we leave this thread, we have to stop the RX, otherwise, the system will be crash.  */
	ioctl(i2s_fd, I2S_RX_DISABLE, 0);

	DBG_MSG("mct_gadget: %s exit\n", __FUNCTION__);
}

#ifdef  SUPPORT_H264_FOR_TCP
void * h264_thread(void * arg)
{
	int ret = 0;
	struct sockaddr_in from;
	int len = sizeof(from);
	int tcp_h264_clnsd = 0;
	char h264_buffer[JUVC_MAX_VIDEO_PACKET_SIZE];
	int retry = 0;
	int h264_len = 0;
	int first_time = 1;
	int n = 0;

	DBG_MSG("mct_gadget: %s go\n", __FUNCTION__);

	while(exited == 0) {
		if (tcp_h264_socket <= -1) {
			sleep(1);
			continue;
		}

		if (client_connected == 1) {
			/*  waiting for client  */
			//DBG_MSG("waiting for client\n");
			memset(&from, '\0', sizeof(struct sockaddr_in));
			tcp_h264_clnsd = accept(tcp_h264_socket, (struct sockaddr*)&from, &len);

			if (tcp_h264_clnsd < 0) {
				sleep(1);
				continue;
			}

			first_time = 1;
			while(exited == 0) {
				/*  read from socket;  */
				ret = read(tcp_h264_clnsd, &h264_len, sizeof(int));
				if (ret < 0) {
					sleep(1);
					/*  is timeout maybe we need to read again  */
					if (++retry > 10) {
						/*  retry too many times  */
						retry = 0;
						break;
					}
					continue;
				} else if (ret == 0) {
					retry = 0;
					/*  close by the client side, we need to jump out this loop  */
					close(tcp_h264_clnsd);
					tcp_h264_clnsd = 0;
					break;
				}
				retry = 0;

				n = 0;
				while (n < h264_len) {
					if (h264_len-n > JUVC_H264_PACKET_SIZE) {
						ret = read(tcp_h264_clnsd, h264_buffer+n, JUVC_H264_PACKET_SIZE);
					} else {
						ret = read(tcp_h264_clnsd, h264_buffer+n, h264_len-n);
					}

					if (ret < 0) {
						sleep(1);
						/*  is timeout maybe we need to read again  */
						if (++retry > 10) {
							/*  retry too many times  */
							retry = 0;
							break;
						}
					} else if (ret == 0) {
						retry = 0;
						/*  close by the client side, we need to jump out this loop  */
						close(tcp_h264_clnsd);
						tcp_h264_clnsd = 0;
						break;
					}
					retry = 0;
					n += ret;
				}

				write_uvc(h264_buffer, h264_len);
			}

			if (tcp_h264_clnsd > 0) {
				close(tcp_h264_clnsd);
				tcp_h264_clnsd = 0;
			}
		}
	}
	/*  NOTE
	 *  before we leave this thread, we have to stop the RX, otherwise, the system will be crash.  */
	ioctl(i2s_fd, I2S_RX_DISABLE, 0);

	DBG_MSG("mct_gadget: %s exit\n", __FUNCTION__);
}
#endif

static int create_thread()
{
	/*  control thread  */
	if (pthread_create(&pthr_video_control, NULL, video_control_thread, (void*)NULL) != 0) {
		PRINT_MSG("mct_gadget: Error create control thread\n");
		return -1;
	}
	video_control_go = 1;

	/*  we DO NOT use this function right now. maybe next version.  */
	//if (pthread_create(&pthr_broadcast, NULL, broadcast_thread,(void*) NULL) != 0) {
	//	PRINT_MSG("mct_gadget: Error create broadcast thread\n");
	//	return -1;
	//}
	//broadcast_go = 1;

	if (pthread_create(&pthr_video, NULL, video_thread, (void*)NULL) != 0) {
		PRINT_MSG("mct_gadget: Error create video thread\n");
		return -1;
	}
	video_go = 1;

	if (pthread_create(&pthr_video_write, NULL, video_write_thread, (void*)NULL) != 0) {
		PRINT_MSG("mct_gadget: Error create video write thread\n");
		return -1;
	}
	video_write_go = 1;

	if (pthread_create(&pthr_video_status, NULL, video_status_thread, (void*)NULL) != 0) {
		PRINT_MSG("mct_gadget: Error create video control thread\n");
		return -1;
	}
	video_status_go = 1;

	if (pthread_create(&pthr_mic, NULL, mic_thread, (void*)NULL) != 0) {
		PRINT_MSG("mct_gadget: Error create audio thread\n");
		return -1;
	}
	mic_go = 1;

	if (pthread_create(&pthr_spk, NULL, spk_thread, (void*)NULL) != 0) {
		PRINT_MSG("mct_gadget: Error create audio thread\n");
		return -1;
	}
	spk_go = 1;

#ifdef  SUPPORT_H264_FOR_TCP
	if (pthread_create(&pthr_h264, NULL, h264_thread, (void*)NULL) != 0) {
		PRINT_MSG("mct_gadget: Error create audio thread\n");
		return -1;
	}
	h264_go = 1;
#endif

	return 0;
}

static int initialize_procedure(int argc, char **argv)
{
	if (check_exist_inst() < 0)
		return -1;

	load_bg();

	if (init_uvc_dev() < 0)
		return -1;

	if (init_i2s_device() < 0)
		return -1;

	/*  assigned signal handler  */
	set_signal_handle();

	/*  create all socket which we need  */
	if ((tcp_ctrl_socket=init_tcp_socket((unsigned short)GADGET_CONTROL_PORT)) < 0) {
		return -1;
	}

	if ((tcp_mic_socket=init_tcp_socket((unsigned short)GADGET_MIC_PORT)) < 0) {
		close_socket();
		return -1;
	}

	if ((tcp_spk_socket=init_tcp_socket((unsigned short)GADGET_SPK_PORT)) < 0) {
		close_socket();
		return -1;
	}
#ifdef  SUPPORT_H264_FOR_TCP
	if ((tcp_h264_socket=init_tcp_socket((unsigned short)GADGET_H264_PORT)) < 0) {
		close_socket();
		return -1;
	}
#endif
	if ((udp_broadcast_socket=init_udp_socket((unsigned short)GADGET_BROADCAST_PORT, 1)) < 0) {
		close_socket();
		return -1;
	}

	if ((udp_video_socket=init_udp_socket((unsigned short)GADGET_CAMERA_PORT, 0)) < 0) {
		close_socket();
		return -1;
	}

	/*  save our pid  */
	save_pid();

	/*  semaphore for video display  */
	sem_init(&video_write_mutex, 0, 0);

	return 0;
}

static int main_loop(int argc, char **argv)
{
	PRINT_MSG("mct_gadget: here we go\n");
	if (create_thread() < 0) {
		exited = 1;
		return -1;
	}

	if (video_control_go)
		pthread_join(pthr_video_control, NULL);
	if (broadcast_go)
		pthread_join(pthr_broadcast, NULL);
	if (video_go)
		pthread_join(pthr_video, NULL);
	if (video_write_go)
		pthread_join(pthr_video_write, NULL);
	if (video_status_go)
		pthread_join(pthr_video_status, NULL);
	if (mic_go)
		pthread_join(pthr_mic, NULL);
	if (spk_go)
		pthread_join(pthr_spk, NULL);
#ifdef  SUPPORT_H264_FOR_TCP
	if (h264_go)
		pthread_join(pthr_h264, NULL);
#endif
	return 0;
}

static void close_procedure(void)
{
	unlink(PID_FILE);

	close_socket();

	close_uvc();

	close_i2s();

	sem_destroy(&video_write_mutex);
}

int main(int argc, char ** argv)
{
	pid_t child = 0, sid = 0;

	int bgmode = 1;

	if(argc > 1) {
		if(strcasecmp(argv[1], "-f") == 0)  /*  run as foreground  */
			bgmode = 0;
	}

	if(bgmode == 1) {
		/*  run in background  */
		child = fork();
		if(child < 0) {
			PRINT_MSG("mct_gadget: Execute %s failed\n", argv[0]);
			exit(-1);
		}

		if(child > 0) {
			/*  parent process  */
			exit(0);
		}
	}

	if (initialize_procedure(argc, argv) < 0) {
		exit(EXIT_FAILURE);
	}

	if (main_loop(argc, argv) < 0) {
		perror("mct_gadget: Create thread failed");
	}

	close_procedure();

	PRINT_MSG("mct_gadget: terminated\n");

	/*  NOTE
	 *  just a workaround, due to we can't terminal the main process when the child process ends.
	 *  cause by the bug of libpthread of uClibc-0.9.33.2.  */
	kill(getpid(), SIGTERM);

	return 0;
}