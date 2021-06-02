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
#include <netinet/tcp.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/time.h>
#include <syslog.h>
#include <pthread.h>
#include <sys/sem.h>
#include <semaphore.h>

#include "nvram.h"
#include "i2s_ctrl.h"
#include "linux/autoconf.h"

#include "mct_gadget.h"
#include "uvcdev.h"
#ifndef SUPPORT_RING_ELEMENT
#include "queue.h"
#endif

//  Added by Tiger
#include "ralink_gpio.h"
#if 0
#define GPIO_DEV	"/dev/gpio"
#endif

sem_t video_write_mutex;

/*  I2S device  */
int i2s_fd = -1;

/*  tcp  */
int tcp_ctrl_socket = -1;
#ifdef  SUPPORT_H264_FOR_TCP
int tcp_h264_socket = -1;
#endif
int tcp_control_clnsd = 0;
static int client_connected = 0;

/*  udp  */
int udp_broadcast_socket = -1;
int udp_video_socket = -1;
#ifdef AUDIO_USES_TCP
int tcp_mic_socket = -1;
int tcp_spk_socket = -1;
#else
int udp_mic_socket = -1;
int udp_spk_socket = -1;
#endif
sem_t mic_write_mutex;

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
pthread_t pthr_mic_write;
int mic_write_go = 0;
pthread_t pthr_spk;
int spk_go = 0;
#ifdef  SUPPORT_H264_FOR_TCP
pthread_t pthr_h264;
int h264_go = 0;
#endif

static int is_yuv = 0;
static int playing = 0;

static int cur_formatidx = FORMAT_MJPG_1080;

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

#ifdef SUPPORT_RING_ELEMENT
static char video_buffer[RING_NUM][JUVC_MAX_VIDEO_PACKET_SIZE];
static int total_size[RING_NUM];
static int copying = 0;
static int done = 0;
#else
static char video_buffer[JUVC_MAX_VIDEO_PACKET_SIZE];
static int total_size = 0;

queue_t * queue = NULL;

#endif

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
#ifdef SUPPORT_RING_ELEMENT
			total_size[0] = 0;
#else
			total_size = 0;
#endif
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
#ifdef SUPPORT_RING_ELEMENT
			total_size[0] = 0;
#else
			total_size = 0;
#endif
			return;
		}
	}

	fp = fopen(full_name, "r");
	if (fp) {
#ifdef SUPPORT_RING_ELEMENT
		total_size[0] = fread(video_buffer[0], 1, JUVC_MAX_VIDEO_PACKET_SIZE, fp);
#else
		total_size = fread(video_buffer, 1, JUVC_MAX_VIDEO_PACKET_SIZE, fp);
#endif
		fclose(fp);
	} else {
#ifdef SUPPORT_RING_ELEMENT
		total_size[0] = 0;
#else
		total_size = 0;
#endif
	}
}

static void close_socket()
{
	DBG_MSG("mct_gadget: Close socket\n");

	if (tcp_ctrl_socket > -1) {
		close(tcp_ctrl_socket);
		tcp_ctrl_socket = -1;
	}
#ifdef AUDIO_USES_TCP
	if (tcp_mic_socket > -1) {
		close(tcp_mic_socket);
		tcp_mic_socket = -1;
	}
	if (tcp_spk_socket > -1) {
		close(tcp_spk_socket);
		tcp_spk_socket = -1;
	}
#else
	if (udp_mic_socket > -1) {
		close(udp_mic_socket);
		udp_mic_socket = -1;
	}
	if (udp_spk_socket > -1) {
		close(udp_spk_socket);
		udp_spk_socket = -1;
	}
#endif
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
	int optval = 1;
	struct sockaddr_in serveraddr;
	int tcp_socket = -1;
	struct timeval tv;

	/* create sockett */
	tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (tcp_socket < 0) {
		DBG_MSG("ERROR opening tcp socket");
		return -1;
	}

	memset(&tv, '\0', sizeof(tv));
	tv.tv_sec = TCP_TIME_OUT;
	setsockopt(tcp_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	setsockopt(tcp_socket, IPPROTO_TCP, TCP_NODELAY, (char *)&optval, sizeof(optval));
	setsockopt(tcp_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&optval, sizeof(optval));
	setsockopt(tcp_socket, SOL_SOCKET, SO_KEEPALIVE, (char *)&optval, sizeof(optval));
	optval = 3;  // when idling over 3s then sending keepalive probes.(the default value is 2h)
	setsockopt(tcp_socket, IPPROTO_TCP, TCP_KEEPIDLE, (char *)&optval, sizeof(optval));
	optval = 3;  // every 3 sec between individual keepalive probes.(the default value is 75s)
        setsockopt(tcp_socket, IPPROTO_TCP, TCP_KEEPINTVL, (char *)&optval, sizeof(optval));
	optval = 5;  // 5 of the keepalive probes TCP should send before dropping the connection. .(the default value is 9)
        setsockopt(tcp_socket, IPPROTO_TCP, TCP_KEEPCNT, (char *)&optval, sizeof(optval));

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

int send_command(unsigned char cmd, unsigned char format, unsigned char frame, unsigned int camera, unsigned int process);
static int init_i2s_device(void);
static int i2s_stop = 1;
static int hub_enable = 0;

static void signal_handler(int sig)
{
	if (sig == SIGINT) {
		DBG_MSG("mct_gadget: SIGINT!\n");
		exited = 1;
	} else if (sig == SIGALRM) {
		DBG_MSG("mct_gadget: SIGALRM!\n");
	} else if (sig == SIGUSR1) {
		DBG_MSG("mct_gadget: Stop/Start play(SIGUSR1)!\n");
		send_command(JUVC_CONTROL_STOP_START, 0, 0, 0, 0);
		playing = !playing;
		if (playing)
			PRINT_MSG("Start playing\n");
		else
			PRINT_MSG("Stop playing\n");
	} else if (sig == SIGUSR2) {
		DBG_MSG("mct_gadget: SIGUSR2!\n");
	}
}

static int connected_status(int connect)
{
	FILE * fp = NULL;
	char buf[] = "connected";
	if (connect) {
		fp = fopen(CONNECTED_FILE, "w");
		fwrite(buf, 1, strlen(buf), fp);
		fclose(fp);
	} else {
		unlink(CONNECTED_FILE);
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

int counvert_formatidx(char format, char frame)
{
	int idx = 0;
	switch(format) {
	case 1:
		switch(frame) {
		case 1: idx = FORMAT_MJPG_320; break;
		case 2: idx = FORMAT_MJPG_480; break;
		case 3: idx = FORMAT_MJPG_720; break;
		case 4: idx = FORMAT_MJPG_1080; break;
		}
		break;
	case 2:
		switch(frame) {
		case 1: idx = FORMAT_H264_320; break;
		case 2: idx = FORMAT_H264_480; break;
		case 3: idx = FORMAT_H264_720; break;
		case 4: idx = FORMAT_H264_1080; break;
		case 5: idx = FORMAT_H264_4K; break;
		}
		break;
	case 3:
		switch(frame) {
		case 1: idx = FORMAT_YUV_320; break;
		}
		break;
	}
	return idx;
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
					cur_formatidx = counvert_formatidx(mct_uvc_data.prob_ctrl[2], mct_uvc_data.prob_ctrl[3]);
					send_command(JUVC_CONTROL_CAMERACTRL, mct_uvc_data.prob_ctrl[2], mct_uvc_data.prob_ctrl[3], 0, 0);
					i2s_stop = 0;
				} else if (mct_uvc_data.status == 2) {
					//  stop streaming
					i2s_stop = 1;
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

enum {
	gpio_in,
	gpio_out,
};

enum {
	gpio2300,
	gpio3924,
	gpio7140,
	gpio72
};

void cp2615_reset(int fd, int plugin)
{
	int value;
	//  pin 11
	//  set direction to out
	ioctl(fd, RALINK_GPIO_SET_DIR_OUT, (1 << 11));
	//  read
	ioctl(fd, RALINK_GPIO_READ, &value);
	//  push hi to disable CP2615
	value |= (1 << 11);
	ioctl(fd, RALINK_GPIO_WRITE, value);
	if (plugin == 1) {
		sleep(2);
		//  pull low to enable CP2615
		value &= ~(1 << 11);
		ioctl(fd, RALINK_GPIO_WRITE, value);
	}
}
#if 0
void usb_hub_reset(int plugin)
{
	int fd = 0;
	int gpio_num = 53;
	int action = 0;
	int dir = gpio_out;
	int value = 0;

	fd = open(GPIO_DEV, O_RDONLY);
	if (fd < 0) {
		return;
	}

	gpio_num = gpio_num - 40; 

	//  set direction
	ioctl(fd, RALINK_GPIO7140_SET_DIR_OUT, (1 << gpio_num));
	
	//  read
	ioctl(fd, RALINK_GPIO7140_READ, &value);
	//  pull low
	value &= ~(1 << gpio_num);
	ioctl(fd, RALINK_GPIO7140_WRITE, value);
	//  7's????
	sleep(2);
	//  push hi
	value |= (1 << gpio_num);
	ioctl(fd, RALINK_GPIO7140_WRITE, value);

	//cp2615_reset(fd, plugin);

	close(fd);
	PRINT_MSG("USB HUB Disabled\n");
	hub_enable = 0;
}
#else
void usb_hub_reset(int plugin)
{
	/*if (plugin == 1) {
		system("web gpio 11 0");  //  pull low to enable cp2615
	}*/

	system("web gpio 53 0");  //  pull low to disable USB hub

	/*if (plugin == 1)
		sleep(2);
	else
		sleep(1);*/
	usleep(100000);

	system("web gpio 53 1");  //  pull high to enable USB hub
	/*if (plugin == 0) {
		system("web gpio 11 1");  //  pull high to disable cp2615
	}*/
}
#endif

enum {
	RED_LED,
	GREEN_LED,
	ORANGE_LED,
	FLASH_LED
};

int get_led_state()
{
	FILE * fp = NULL;
	char buf[16];
	int ret = 0;

	fp = fopen("/tmp/led", "r");
	if (fp) {
		fread(buf, 1, sizeof(buf), fp);
		if (strcmp(buf, "red") == 0)
			ret = RED_LED;
		else if (strcmp(buf, "green") == 0)
			ret = GREEN_LED;
		else if (strcmp(buf, "orange") == 0)
			ret = ORANGE_LED;
		else if (strcmp(buf, "flash") == 0)
			ret = FLASH_LED;
		fclose(fp);
	}
	return ret;
}

void keep_led_state(char * color)
{
	FILE * fp = fopen("/tmp/led", "w");

	if (fp) {
		fwrite(color, 1, strlen(color), fp);
		fclose(fp);
	}
}

void LED_control(int type)
{
	switch(type) {
	case 0:
		//  show orange
		if (get_led_state() != ORANGE_LED) {
			system("gpio l 14 4000 0 1 0 4000");
			system("gpio l 52 4000 0 1 0 4000");
			keep_led_state("orange");
		}
		break;
	case 1:
		//  show green
		if (get_led_state() != GREEN_LED) {
			system("gpio l 14 0 4000 0 1 4000");
			system("gpio l 52 4000 0 1 0 4000");
			keep_led_state("green");
		}
		break;
	}
}

int setTimer(int sec)
{
	struct itimerval value, ovalue;

	value.it_value.tv_sec = sec;
	value.it_value.tv_usec = 0;
	value.it_interval.tv_sec = sec;
	value.it_interval.tv_usec = 0;
	return setitimer(ITIMER_REAL, &value, &ovalue);
}

void stopTimer(void)
{
	struct itimerval value, ovalue;

	value.it_value.tv_sec = 0;
	value.it_value.tv_usec = 0;
	value.it_interval.tv_sec = 0;
	value.it_interval.tv_usec = 0;
	setitimer(ITIMER_REAL, &value, &ovalue);
}

static void * video_control_thread(void * arg)
{
	int ret = -1;
	struct sockaddr_in from;
	socklen_t len = sizeof(from);
	JUVCHDR juvchdr;
	int l = sizeof(JUVCHDR);
	int optval = 1;
	UVC_CAPABILITIES_DATA cap_data;
	char data[256];

	DBG_MSG("mct_gadget: %s go\n", __FUNCTION__);

	while(exited == 0) {
		if (tcp_ctrl_socket <= -1) {
			sleep(1);
			continue;
		}

		DBG_MSG("%s, waiting for client\n", __FUNCTION__);

		memset(&from, '\0', sizeof(struct sockaddr_in));
		tcp_control_clnsd = accept(tcp_ctrl_socket, (struct sockaddr*)&from, &len);

		if (tcp_control_clnsd < 0) {
			DBG_MSG("%s: accept failed\n", __FUNCTION__);
			if (client_connected == 1) {
				ioctl_uvc(UVC_CLIENT_LOST, NULL);
				PRINT_MSG("mct_gadget: client disconnected\n");
				client_connected = 0;
				i2s_stop = 1;
				playing = 0;
				ioctl_uvc(UVC_RESET_TO_DEFAULT, NULL);
				usb_hub_reset(0);
				LED_control(0);
				connected_status(0);
#ifndef SUPPORT_RING_ELEMENT
				//  clear queue
				releses_queue(queue);
#else
				memset(total_size, '\0', sizeof(total_size));
				copying = 0;
				done = 0;
#endif
			}
			sleep(1);
			continue;
		}
		DBG_MSG("%s, accept success\n", __FUNCTION__);
		optval = 1;
		setsockopt(tcp_control_clnsd, SOL_SOCKET, SO_KEEPALIVE, (char *)&optval, sizeof(optval));
		optval = 3;
		setsockopt(tcp_control_clnsd, IPPROTO_TCP, TCP_KEEPIDLE, (char *)&optval, sizeof(optval));
		optval = 1;
		setsockopt(tcp_control_clnsd, IPPROTO_TCP, TCP_KEEPINTVL, (char *)&optval, sizeof(optval));
		optval = 5;
		setsockopt(tcp_control_clnsd, IPPROTO_TCP, TCP_KEEPCNT, (char *)&optval, sizeof(optval));

		/*  maybe we don't need to wait for this, anyway, just check it out  */
		if (!wait_setting_done()) {
			PRINT_MSG("**** UVC device not ready yet ****\n");
			close(tcp_control_clnsd);
			tcp_control_clnsd = -1;
			sleep(1);
			continue;
		}

		if (client_connected == 0) {
			PRINT_MSG("mct_gadget: client connected\n");
			sleep(1);
			cur_formatidx = counvert_formatidx(mct_uvc_data.prob_ctrl[2], mct_uvc_data.prob_ctrl[3]);
			ret = send_command(JUVC_CONTROL_CAMERAINFO, mct_uvc_data.prob_ctrl[2], mct_uvc_data.prob_ctrl[3], 0, 0);
		} else {
			cur_formatidx = counvert_formatidx(mct_uvc_data.prob_ctrl[2], mct_uvc_data.prob_ctrl[3]);
			ret = send_command(JUVC_CONTROL_CAMERACTRL, mct_uvc_data.prob_ctrl[2], mct_uvc_data.prob_ctrl[3], 0, 0);
		}

		while(exited == 0) {
			ret = read(tcp_control_clnsd, &juvchdr, l);
			if (ret == 0) {
				/*  close by the client side, we need to jump out this loop  */
				DBG_MSG("%s, read ret = 0\n", __FUNCTION__);
				close(tcp_control_clnsd);
				tcp_control_clnsd = -1;
				break;
			} if (ret == -1) {
				continue;
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
						usb_hub_reset(1);
						LED_control(1);
						connected_status(1);
						client_connected = 1;
						playing = 1;
					} else if (juvchdr.Flags == JUVC_CONTROL_MOBILE_DEV) {
						i2s_stop = 0;
						client_connected = 1;
						playing = 1;
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
					} else if (juvchdr.Flags == JUVC_CONTROL_STOP_START) {
						playing = !playing;
						if (playing)
							PRINT_MSG("Start playing\n");
						else
							PRINT_MSG("Stop playing\n");
						
					} else if (juvchdr.Flags == JUVC_CAMERA_DISCONNECT) {
						ioctl_uvc(UVC_CLIENT_LOST, NULL);
						PRINT_MSG("mct_gadget: client disconnected\n");
						client_connected = 0;
						i2s_stop = 1;
						playing = 0;
						ioctl_uvc(UVC_RESET_TO_DEFAULT, NULL);
						usb_hub_reset(0);
						LED_control(0);
						connected_status(0);
#ifndef SUPPORT_RING_ELEMENT
						//  clear queue
						releses_queue(queue);
#else
						memset(total_size, '\0', sizeof(total_size));
						copying = 0;
						done = 0;
#endif
						break;
					}
				}
			}
		}
		/*ioctl_uvc(UVC_CLIENT_LOST, NULL);
		PRINT_MSG("mct_gadget: client disconnected\n");
		client_connected = 0;
		i2s_stop = 1;
		playing = 0;
		ioctl_uvc(UVC_RESET_TO_DEFAULT, NULL);
		usb_hub_reset(0);
		LED_control(0);
		connected_status(0);
#ifndef SUPPORT_RING_ELEMENT
		//  clear queue
		releses_queue(queue);
#else
		memset(total_size, '\0', sizeof(total_size));
		copying = 0;
		done = 0;
#endif*/
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

void * video_write_thread(void * arg)
{
	DBG_MSG("mct_gadget: %s go\n", __FUNCTION__);
#ifndef SUPPORT_RING_ELEMENT
	PVIDEO_FRAME frame = NULL;
#endif

	while(exited == 0) {
		if (udp_video_socket <= -1) {
			sleep(1);
			continue;
		}
#ifdef SUPPORT_RING_ELEMENT
		sem_wait(&video_write_mutex);
#else
		if (queue_length(queue) <= 0){
			usleep(1000);
			continue;
		}
		frame = (PVIDEO_FRAME)queue_remove(queue);
#endif
		if (playing == 1) {
			if (is_yuv == 0) {
#ifdef SUPPORT_RING_ELEMENT
				write_uvc(video_buffer[done], total_size[done]);
#else
				if (frame)
					write_uvc(frame->buf, frame->length);
#endif
			} else {
#ifdef SUPPORT_RING_ELEMENT
				write_uvc4yuv(video_buffer[done], total_size[done]);
#else
				if (frame)
					write_uvc4yuv(frame->buf, frame->length);
#endif
			}
		} else {
			write_uvc(video_buffer, total_size);
		}
#ifndef SUPPORT_RING_ELEMENT
		if (frame) {
			free_video_frame(frame);
			frame = NULL;
		}
#endif
	}

	DBG_MSG("mct_gadget: %s exit\n", __FUNCTION__);
}

//#define COUNTING_FPS					1
#ifdef COUNTING_FPS
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
#ifndef SUPPORT_RING_ELEMENT
	int frame_size = 0;
	PVIDEO_FRAME frame = NULL;
#endif

	DBG_MSG("mct_gadget: %s go\n", __FUNCTION__);

	while(exited == 0) {
		if (udp_video_socket <= -1) {
			sleep(1);
			continue;
		}

		if (client_connected == 1) {
			/*  read from socket;  */
			memset(&from, '\0', sizeof(struct sockaddr_in));
			n = recvfrom(udp_video_socket, buffer, JUVC_UDP_PKT_SIZE, 0, (struct sockaddr*)&from, &len);
 
			if (n <= 0) {
#ifndef SUPPORT_RING_ELEMENT
				if (frame) {
					free_video_frame(frame);
					frame = NULL;
				}
#endif
				continue;
			}
#ifdef COUNTING_FPS
			data_size += n;
#endif
			header = (PJUVCHDR)buffer;
			if (cur_formatidx != header->formatidx) {
#ifndef SUPPORT_RING_ELEMENT
				if (frame) {
					free_video_frame(frame);
					frame = NULL;
				}
#endif
				continue;
			}
			if (first_time == 1) {
				if (header->XactOffset != 0) {
#ifndef SUPPORT_RING_ELEMENT
					if (frame) {
						free_video_frame(frame);
						frame = NULL;
					}
#endif
					continue;  /*  drop this packet  */
				} else {
					first_time = 0;
#ifdef SUPPORT_RING_ELEMENT
					memcpy(video_buffer[copying], buffer+sizeof(JUVCHDR), header->PayloadLength);
					total_size[copying] = header->PayloadLength;
#else
					if (frame) {
						free_video_frame(frame);
						frame = NULL;
					}
					frame = allocate_video_frame(header->TotalLength);
					if (frame == NULL)
						continue;
					memcpy(frame->buf, buffer+sizeof(JUVCHDR), header->PayloadLength);
					frame_size = header->PayloadLength;
#endif
					xid = header->XactId;
				}
			} else {
				if (header->XactOffset == 0) {
#ifdef SUPPORT_RING_ELEMENT
					memcpy(video_buffer[copying], buffer+sizeof(JUVCHDR), header->PayloadLength);
					total_size[copying] = header->PayloadLength;
#else
					if (frame) {
						free_video_frame(frame);
						frame = NULL;
					}
					frame = allocate_video_frame(header->TotalLength);
					if (frame == NULL)
						continue;
 					memcpy(frame->buf, buffer+sizeof(JUVCHDR), header->PayloadLength);
					frame_size = header->PayloadLength;
#endif
					xid = header->XactId;
				} else {
					if (xid != header->XactId) {
#ifdef SUPPORT_RING_ELEMENT
						DBG_MSG("packet failed: total_size = (%d/%d), offset = %d, xid = (%d/%d)\n", total_size[copying], header->TotalLength, header->XactOffset, xid, (unsigned int)header->XactId);
#else
						DBG_MSG("packet failed: total_size = (%d/%d), offset = %d, xid = (%d/%d)\n", frame_size, header->TotalLength, header->XactOffset, xid, (unsigned int)header->XactId);

						if (frame) {
							free_video_frame(frame);
							frame = NULL;
						}
#endif
						continue;
					}
#ifdef SUPPORT_RING_ELEMENT
					memcpy(video_buffer[copying]+header->XactOffset, buffer+sizeof(JUVCHDR), header->PayloadLength);
					total_size[copying] += header->PayloadLength;
#else
					memcpy(frame->buf+header->XactOffset, buffer+sizeof(JUVCHDR), header->PayloadLength);
					frame_size += header->PayloadLength;
#endif
				}
			}

#ifdef SUPPORT_RING_ELEMENT
			//DBG_MSG("total_size[%d] = (%d/%d), offset = %d, xid = %d\n", copying, total_size[copying], header->TotalLength, header->XactOffset, (unsigned int)header->XactId);
			if (total_size[copying] >= header->TotalLength) {
 #ifdef COUNTING_FPS
				count_num ++;
				real_fps ++;
				gettimeofday(&tv,NULL);
				end_t = tv.tv_sec * 1000000 + tv.tv_usec;
				period = end_t - start_t;

				if (period - pre_period >= 1000000) {
					pre_period = period;
					PRINT_MSG("fps=%d (frames= %d), bps=%lld Kbps\n" \
					, real_fps \
					, count_num \
					, (data_size>>7));  /*  this mean size/1024*8  */

					real_fps = 0;
					data_size = 0;
				}
 #endif
				done = copying;
				copying = ++copying%RING_NUM;
				sem_post(&video_write_mutex);
			}
#else
			if (frame_size >= header->TotalLength) {
 #ifdef COUNTING_FPS
				count_num ++;
				real_fps ++;
				gettimeofday(&tv,NULL);
				end_t = tv.tv_sec * 1000000 + tv.tv_usec;
				period = end_t - start_t;

				if (period - pre_period >= 1000000) {
					pre_period = period;
					PRINT_MSG("fps=%d (frames= %d), bps=%lld Kbps\n" \
					, real_fps \
					, count_num \
					, (data_size>>7));  /*  this mean size/1024*8  */

					real_fps = 0;
					data_size = 0;
				}
 #endif				
				if(queue_length(queue) <= QUEUE_NUM) {
					queue_add(queue, (void*)frame);
				} else {
					free_video_frame(frame);
				}
				frame = NULL;
			}
#endif
		} else {
#ifdef SUPPORT_RING_ELEMENT
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
#else
			if (total_size > 0) {
				if (is_yuv == 0) {
					if (write_uvc(video_buffer, total_size) > 0) {
						sleep(1);
					}
				} else {
					if (write_uvc4yuv(video_buffer, total_size) > 0) {
						sleep(1);
					}
				}
			} else {
				/*  failed to get the bg  */
				sleep(1);
			}
#endif
		}
	}

	DBG_MSG("mct_gadget: %s exit\n", __FUNCTION__);
}

typedef struct circular_buffer {
	char * buffer;
	char * buffer_end;
	pthread_mutex_t mutex;
	size_t capacity;
	size_t count;
	size_t sz;
	char * head;
	char * tail;
} circular_buffer;

circular_buffer mic_cb;
int init_audio_cb(circular_buffer * cb, size_t capacity, size_t sz)
{
	cb->buffer = malloc(capacity * sz);
	if (!cb->buffer) {
		cb->buffer_end = NULL;
		cb->capacity = 0;
		cb->count = 0;
		cb->sz = 0;
		cb->head = NULL;
		cb->tail = NULL;
		return -1;
	}
	pthread_mutex_init(&cb->mutex, NULL);
	cb->buffer_end = cb->buffer + capacity * sz;
	cb->capacity = capacity;
	cb->count = 0;
	cb->sz = sz;
	cb->head = cb->buffer;
	cb->tail = cb->buffer;
	return 0;
}

void free_cb(circular_buffer * cb)
{
	pthread_mutex_lock(&cb->mutex);
	if (cb->buffer) {
		free(cb->buffer);
	}
	cb->buffer = NULL;
	cb->buffer_end = NULL;
	cb->capacity = 0;
	cb->count = 0;
	cb->sz = 0;
	cb->head = NULL;
	cb->tail = NULL;
	pthread_mutex_unlock(&cb->mutex);
	pthread_mutex_destroy(&cb->mutex);
}

int cb_push_back(circular_buffer * cb, char * buffer)
{
	pthread_mutex_lock(&cb->mutex);
	if (cb->count == cb->capacity) {
		//  circular buffer is full, just drop this buffer
		pthread_mutex_unlock(&cb->mutex);
		return -1;
	}

	memcpy(cb->head, buffer, cb->sz);
	cb->head = cb->head + cb->sz;
	if (cb->head == cb->buffer_end)
		cb->head = cb->buffer;
	cb->count++;
	pthread_mutex_unlock(&cb->mutex);
	return 0;
}

int cb_pop_front(circular_buffer * cb, char * buffer)
{
	pthread_mutex_lock(&cb->mutex);
	if (cb->count == 0) {
		//  circular buffer is embpy
		pthread_mutex_unlock(&cb->mutex);
		memset(buffer, '\0', cb->sz);
		return -1;
	}

	memcpy(buffer, cb->tail, cb->sz);
	cb->tail = cb->tail + cb->sz;
	if (cb->tail == cb->buffer_end)
		cb->tail = cb->buffer;
	cb->count--;
	pthread_mutex_unlock(&cb->mutex);
	return 0;
}

void * mic_write_thread(void * arg)
{
	char buffer[I2S_PAGE_SIZE];
	DBG_MSG("mct_gadget: %s go\n", __FUNCTION__);

	while(exited == 0) {
#ifdef AUDIO_USES_TCP
		if (tcp_mic_socket <= -1) {
#else
		if (udp_mic_socket <= -1) {
#endif
			sleep(1);
			continue;
		}

		sem_wait(&mic_write_mutex);

		if (cb_pop_front(&mic_cb, buffer) == 0) {
			if (i2s_stop == 0 && playing == 1)
				ioctl(i2s_fd, I2S_PUT_AUDIO, buffer);
		}
	}

	DBG_MSG("mct_gadget: %s exit\n", __FUNCTION__);
}

//  for tcp
#ifdef AUDIO_USES_TCP
void * mic_thread(void * arg)
{
	int ret = 0;
	int offset = 0;
	int readbyte = 0;
	struct sockaddr_in from;
	int len = sizeof(from);
	int tcp_mic_clnsd = 0;
	char mic_buffer[I2S_PAGE_SIZE];
	int retry = 0;

	DBG_MSG("mct_gadget: %s go\n", __FUNCTION__);

	if (i2s_fd > 0) {
		ioctl(i2s_fd, I2S_TX_ENABLE, 0);
	}

	while(exited == 0) {
		if (tcp_mic_socket <= -1) {
			sleep(1);
			continue;
		}

		/*  waiting for client  */
		DBG_MSG("%s, waiting for client\n", __FUNCTION__);
		memset(&from, '\0', sizeof(struct sockaddr_in));
		tcp_mic_clnsd = accept(tcp_mic_socket, (struct sockaddr*)&from, &len);

		if (tcp_mic_clnsd < 0) {
			DBG_MSG("%s: accept failed\n", __FUNCTION__);
			sleep(1);
			continue;
		}
		DBG_MSG("%s, accept success\n", __FUNCTION__);
		while(exited == 0) {
			/*  read from socket;  */
			ret = 0;
			offset = 0;
			readbyte = 0;
			do {
				ret = read(tcp_mic_clnsd, mic_buffer+offset, I2S_PAGE_SIZE-offset);
				if (ret <= 0)
					break;
				readbyte += ret;
				offset += ret;
			} while(readbyte < I2S_PAGE_SIZE);
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
				DBG_MSG("%s, read ret = 0\n", __FUNCTION__);
				break;
			}
			retry = 0;

			if (client_connected == 1) {
				/*  write to i2s  */
				if (i2s_fd > 0) {
					if (i2s_stop == 0 && readbyte == I2S_PAGE_SIZE) {
						if (cb_push_back(&mic_cb, mic_buffer) == 0) {
							sem_post(&mic_write_mutex);
						}
					}
				}
			}
		}

		if (tcp_mic_clnsd > 0) {
			close(tcp_mic_clnsd);
			tcp_mic_clnsd = 0;
		}
	}
	/*  NOTE
	 *  before we leave this thread, we have to stop the TX, otherwise, the system will be crash.  */
	if (i2s_fd > 0) {
		ioctl(i2s_fd, I2S_TX_DISABLE, 0);
	}

	DBG_MSG("mct_gadget: %s exit\n", __FUNCTION__);
}

void * spk_thread(void * arg)
{
	int ret = 0;
	struct sockaddr_in from;
	int len = sizeof(from);
	int tcp_spk_clnsd = 0;
	char spk_buffer[I2S_PAGE_SIZE];
	int retry = 0;

	DBG_MSG("mct_gadget: %s go\n", __FUNCTION__);

	if (i2s_fd > 0) {
		ioctl(i2s_fd, I2S_RX_ENABLE, 0);
	}

	while(exited == 0) {
		if (tcp_spk_socket <= -1) {
			sleep(1);
			continue;
		}

		/*  waiting for client  */
		DBG_MSG("%s: waiting for client\n", __FUNCTION__);
		memset(&from, '\0', sizeof(struct sockaddr_in));
		tcp_spk_clnsd = accept(tcp_spk_socket, (struct sockaddr*)&from, &len);

		if (tcp_spk_clnsd < 0) {
			DBG_MSG("%s: accept failed\n", __FUNCTION__);
			sleep(1);
			continue;
		}
		DBG_MSG("%s, accept success\n", __FUNCTION__);
		while(exited == 0) {
			/*  read from i2s  */
			if (i2s_fd > 0) {
				ioctl(i2s_fd, I2S_GET_AUDIO, spk_buffer);
			} else {
				sleep(1);
				continue;
			}

			if (client_connected == 1) {
				if (i2s_stop == 0 && playing == 1) {
					/*  write to socket;  */
					ret = write(tcp_spk_clnsd, spk_buffer, I2S_PAGE_SIZE);
					if (ret <= 0) {
						sleep(1);
						if (++retry > 10) {
							retry = 0;
							break;
						}
						continue;
					} else {
						retry = 0;
					}
				}
			}
		}

		if (tcp_spk_clnsd > 0) {
			close(tcp_spk_clnsd);
			tcp_spk_clnsd = 0;
		}
	}
	/*  NOTE
	 *  before we leave this thread, we have to stop the RX, otherwise, the system will be crash.  */
	if (i2s_fd > 0) {
		ioctl(i2s_fd, I2S_RX_DISABLE, 0);
	}

	DBG_MSG("mct_gadget: %s exit\n", __FUNCTION__);
}
#else
void * mic_thread(void * arg)
{
	int n = 0;
	struct sockaddr_in from;
	int len = sizeof(from);
	char mic_buffer[I2S_PAGE_SIZE];
	int retry = 0;

	DBG_MSG("mct_gadget: %s go\n", __FUNCTION__);

	if (i2s_fd > 0) {
		ioctl(i2s_fd, I2S_TX_ENABLE, 0);
	}

	while(exited == 0) {
		if (udp_mic_socket <= -1) {
			sleep(1);
			continue;
		}

		if (client_connected == 1) {
			/*  read form client  */
			//DBG_MSG("read from client\n");
			memset(&from, '\0', sizeof(struct sockaddr_in));

			n = recvfrom(udp_mic_socket, mic_buffer, I2S_PAGE_SIZE, 0, (struct sockaddr*)&from, &len);

			if (n <= 0) {
				sleep(1);
				if (++retry > 10) {
					retry = 0;
					break;
				}
				continue;
			} else {
				retry = 0;
			}

			if (i2s_fd > 0) {
				if (i2s_stop == 0) {
					if (cb_push_back(&mic_cb, mic_buffer) == 0) {
						sem_post(&mic_write_mutex);
					}
				}
			}
		}
	}
	/*  NOTE
	 *  before we leave this thread, we have to stop the TX, otherwise, the system will be crash.  */
	if (i2s_fd > 0) {
		ioctl(i2s_fd, I2S_TX_DISABLE, 0);
	}

	DBG_MSG("mct_gadget: %s exit\n", __FUNCTION__);
}

void * spk_thread(void * arg)
{
	int n = 0;
	struct sockaddr_in from;
	int len = sizeof(from);
	int tcp_spk_clnsd = 0;
	char spk_buffer[I2S_PAGE_SIZE];
	int retry = 0;

	DBG_MSG("mct_gadget: %s go\n", __FUNCTION__);

	ioctl(i2s_fd, I2S_RX_ENABLE, 0);

	while(exited == 0) {
		if (udp_spk_socket <= -1) {
			sleep(1);
			continue;
		}

		if (client_connected == 1) {
			/*  read from i2s  */
			memset(spk_buffer, '\0', sizeof(spk_buffer));
			if (i2s_fd > 0) {
				ioctl(i2s_fd, I2S_GET_AUDIO, spk_buffer);
			} else {
				sleep(1);
				continue;
			}

			if (i2s_stop == 0 && playing == 1) {
				/*  write to socket;  */
				n = sendto(udp_spk_socket, spk_buffer, I2S_PAGE_SIZE, 0, (struct sockaddr *)&from, len);
				if (n <= 0) {
					sleep(1);
					if (+++retry > 10) {
						retry = 0;
						break;
					}
					continue;
				} else {
					retry = 0;
				}
			}
		}
	}
	/*  NOTE
	 *  before we leave this thread, we have to stop the RX, otherwise, the system will be crash.  */
	ioctl(i2s_fd, I2S_RX_DISABLE, 0);

	DBG_MSG("mct_gadget: %s exit\n", __FUNCTION__);
}
#endif

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

	if (pthread_create(&pthr_mic_write, NULL, mic_write_thread, (void*)NULL) != 0) {
		PRINT_MSG("mct_gadget: Error create microphone write thread\n");
		return -1;
	}
	mic_write_go = 1;

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
	int optval = 0;
	if (check_exist_inst() < 0)
		return -1;

#ifdef SUPPORT_RING_ELEMENT
	memset(total_size, '\0', sizeof(total_size));
	load_bg();
#else
	load_bg();
	queue = queue_create();
#endif
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
#ifdef AUDIO_USES_TCP
	if ((tcp_mic_socket=init_tcp_socket((unsigned short)GADGET_MIC_PORT)) < 0) {
		close_socket();
		return -1;
	}

	if ((tcp_spk_socket=init_tcp_socket((unsigned short)GADGET_SPK_PORT)) < 0) {
		close_socket();
		return -1;
	}
#else
	if ((udp_mic_socket=init_udp_socket((unsigned short)GADGET_MIC_PORT, 0)) < 0) {
		close_socket();
		return -1;
	}
	/*  set the buffer to 3072*3  */
	optval = I2S_PAGE_SIZE*3 ;
	if (setsockopt(udp_mic_socket, SOL_SOCKET, SO_RCVBUF, &optval, sizeof(optval)) == -1) {
		close_socket();
		return -1;
        }

	if ((udp_spk_socket=init_udp_socket((unsigned short)GADGET_SPK_PORT, 0)) < 0) {
		close_socket();
		return -1;
	}
#endif
	sem_init(&mic_write_mutex, 0, 1);
	init_audio_cb(&mic_cb, MIC_CB_SIZE, I2S_PAGE_SIZE);
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
	if (mic_write_go)
		pthread_join(pthr_mic_write, NULL);
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

	unlink(CONNECTED_FILE);

	close_socket();

	close_uvc();

	close_i2s();
#ifndef SUPPORT_RING_ELEMENT
	releses_queue(queue);
	queue_destroy(queue);
#endif

	sem_destroy(&video_write_mutex);
	sem_destroy(&mic_write_mutex);
	free_cb(&mic_cb);
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
