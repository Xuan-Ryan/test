/*****************************************************************************
 * updater_device.c    "device Process"
 ***************************************************************************** 
 * second broadcast when receive PC broadcast
 * Receive head {TAG、TotalLength、XactType、HdrSize、Flags、Payloadlength、CRC}
 * Receive packet 、calculate current packet 、rest packet and write each packet 
 * Check CRC and send CRC result
 * Write progress 、read Program process and send
 * Finish
*/
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <arpa/inet.h> //inet_ntoa
#include <sys/types.h> /* open socket */
#include <unistd.h>	/* open */
#include <sys/stat.h>  /* open */
#include <fcntl.h>	 /* open */
#include <sys/ioctl.h> /* ioctl */
#include <signal.h>	/* SIGPIPE */
#include <errno.h>
#include <pthread.h>
#include <net/if.h>

#include "nvram.h" //catch device infomation
#include "updater.h"
#include "CRC.h"

#define KGRN "\033[0;32;32m"
#define RESET "\033[0m"
#define FILE_NAME "/var/tmpFW" //   UVC-j5-RX-0.0.0.1.bin
#define PACKET_LENGTH_MAX 32 * 1024
#define TCP_PORT 55551
#define BROADCAST_IP "255.255.255.255"
#define BROADCAST_PORT 55550

int tcp_sd, broadcastsd, progress, kill_rc, role;
unsigned int tcptotalget;
struct sockaddr_in server; //br receform
struct sockaddr_in form;   //br sendto

void ledcolor(char *color)
{
	char led[8] = {0};
	FILE *fp;

	fp = fopen("/tmp/led", "r");
	if (fp == NULL)
	{
		printf("open file error !......\n");
		return;
	}
	fread(led, 1, sizeof(led), fp);
	printf("led = %s\n", led);
	if (strcmp(color, "red") == 0)
		if (strncmp(led, color, 3))
		{
			system("gpio l 52 0 4000 0 1 4000");
			system("gpio l 14 4000 0 1 0 4000");
			system("echo 'red' > /tmp/led");
		}
	if (strcmp(color, "green") == 0)
		if (strncmp(led, color, 5))
		{
			system("gpio l 14 0 4000 0 1 3000");
			system("gpio l 52 4000 0 1 0 4000");
			system("echo 'green' > /tmp/led");
		}
	if (strcmp(color, "flash") == 0)
		if (strncmp(led, color, 5))
		{
			system("gpio l 52 0 4000 0 1 4000");
			system("gpio l 14 1 1 1 1 4000");
			system("echo 'flash' > /tmp/led");
		}
	fclose(fp);
}
char *get_macaddr(char *ifname)
{
	struct ifreq ifr;
	//char *ptr;
	int skfd;
	static char if_hw[7] = {0};
	if ((skfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		fprintf(stderr, "%s: open socket error\n", __func__);
		return NULL;
	}

	strncpy(ifr.ifr_name, ifname, IF_NAMESIZE);

	if (ioctl(skfd, SIOCGIFHWADDR, &ifr) < 0)
	{
		close(skfd);
		fprintf(stderr, "%s: ioctl fail\n", __func__);
		return NULL;
	}
	//ptr = (char *)&ifr.ifr_addr.sa_data;
	int i = 0;
	while (i < 6)
	{
		if_hw[i] = (long int)ifr.ifr_addr.sa_data[i] & 0xff;
		printf("%02X\n", if_hw[i]);
		i++;
	}
	if_hw[i] = '\0';
	/*
	sprintf(if_hw, "%02X:%02X:%02X:%02X:%02X:%02X",
			(ptr[0] & 0377), (ptr[1] & 0377), (ptr[2] & 0377),
			(ptr[3] & 0377), (ptr[4] & 0377), (ptr[5] & 0377));
			*/

	close(skfd);
	return if_hw;
}
void close_socket(int acceptfd, int tcp_sd, int broadcastsd)
{
	if (acceptfd > 0)
	{
		close(acceptfd);
		acceptfd = 0;
	}
	if (tcp_sd > 0)
	{
		close(tcp_sd);
		tcp_sd = 0;
	}
	if (broadcastsd > 0)
	{
		close(broadcastsd);
		broadcastsd = 0;
	}
}
void printmessage(PJUVCHDR t6_head, PDEVICEINFO t6_info) //print head message
{
	printf("\n thread !!!!!!!!!!!!!!!!!\n");
	if (t6_head != NULL)
	{
		printf("t6_head->Tag = %u\n", t6_head->Tag);
		printf("t6_head->XactType = %d\n", t6_head->XactType);
		printf("t6_head->Role = %u\n", t6_head->Role);
		printf("t6_head->XactId = %u\n", t6_head->XactId);
		printf("t6_head->TotalLength = %u\n", t6_head->TotalLength);
		printf("t6_head->HdrSize = %d\n", t6_head->HdrSize);
	}
	if (t6_info != NULL)
	{
		printf("t6_info->DeviceName = %s\n", t6_info->DeviceName);
		printf("t6_info->Version = %s\n", t6_info->Version);
		printf("t6_info->ModelName = %s\n", t6_info->ModelName);
		printf("t6_info->Port = %d\n", t6_info->Port);
		printf("t6_info->Manufacture = %d\n", t6_info->Manufacture);
		printf("t6_info->MacAddress = %02X", t6_info->MacAddress[0]);
		printf(":%02X", t6_info->MacAddress[1]);
		printf(":%02X", t6_info->MacAddress[2]);
		printf(":%02X", t6_info->MacAddress[3]);
		printf(":%02X", t6_info->MacAddress[4]);
		printf(":%02X\n", t6_info->MacAddress[5]);
	}
	printf("=========================================================\n");
}
void broadcast_create()
{
	//=================================Broadcast Create============================================
	int flag, ret;
	struct timeval tv;
	tv.tv_sec = 3;
	tv.tv_usec = 0;

	do
	{
		broadcastsd = socket(PF_INET, SOCK_DGRAM, 0);
		if (broadcastsd > 0)
			break;
		printf("Broadcast Socket Fail... %d\n", broadcastsd);
		sleep(1);
	} while (1);

	//ret = setsockopt(broadcastsd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
	flag = 1;
	ret = setsockopt(broadcastsd, SOL_SOCKET, SO_REUSEADDR, (const char *)&flag, sizeof(flag));
	ret = setsockopt(broadcastsd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	ret = setsockopt(broadcastsd, SOL_SOCKET, SO_BROADCAST, &flag, sizeof(flag));

	bzero(&server, sizeof(struct sockaddr_in));
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = 0x00; //INADDR_ANY = 0
	server.sin_port = htons(BROADCAST_PORT);

	form.sin_family = AF_INET;
	form.sin_addr.s_addr = htonl(INADDR_BROADCAST); //255.255.255.255  send BROADCAST setting (INADDR_BROADCAST)
	form.sin_port = htons(55550);
	flag = 0;
	do
	{
		ret = bind(broadcastsd, (struct sockaddr *)&server, sizeof(struct sockaddr_in));
		if (ret == 0)
			break;
		printf("Broadcast bind Fail...  %d\n", ret);
		sleep(1);
	} while (1);
}
void tcp_create()
{
	int ret, flag;
	struct timeval tv;
	tv.tv_sec = 4;
	tv.tv_usec = 0;

	tcp_sd = socket(AF_INET, SOCK_STREAM, 0);

	do
	{
		tcp_sd = socket(AF_INET, SOCK_STREAM, 0);
		if (tcp_sd > 0)
			break;
		printf("TCP Socket Fail\n");
		sleep(1);
	} while (1);

	printf("test cursor socket tcp_sd = %d ip = %s port = %d \n", tcp_sd, "INADDR_ANY", TCP_PORT);
	flag = 1;
	setsockopt(tcp_sd, SOL_SOCKET, SO_REUSEADDR, (const char *)&flag, sizeof(flag));
	setsockopt(tcp_sd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

	struct sockaddr_in tcp_addr;
	tcp_addr.sin_family = AF_INET;
	tcp_addr.sin_port = htons(TCP_PORT);
	tcp_addr.sin_addr.s_addr = INADDR_ANY; //inet_addr(ip[1]);

	do
	{
		ret = bind(tcp_sd, (struct sockaddr *)&tcp_addr, sizeof(tcp_addr));
		if (ret == 0)
			break;
		printf("TCP bind Fail..%d\n", ret);
		sleep(1);
	} while (1);

	printf("test cursor Bind successfully.\n");

	do
	{
		ret = listen(tcp_sd, 5);
		if (ret >= 0)
			break;
		printf("TCP listen Fail... %d\n", ret);
		sleep(1);
	} while (1);

	printf("Server-Ready for client connection...\n");
}
int receive_data(int sd, char *modelname, char *version, char *devicename)
{
	int ret, calculate_file;
	unsigned char buf[128] = {0};
	unsigned char *rom_buf;
	unsigned int write_length, percentage, tcptotallength;

	PJUVCHDR t6_head;
	PDEVICEINFO t6_info;
	FILE *fp;

	//==============================Receive head===================================
	fp = fopen(FILE_NAME, "w");
	if (fp == NULL)
	{
		printf("File open fail !....\n");
		return -1;
	}
	tcptotallength = 0;
	bzero(buf, sizeof(buf));
	ret = recv(sd, buf, 32, 0);
	if (ret <= 0) //  other side disconnected
	{
		printf("Receive head Error = %d \n", ret);
		printf("Re-receive Broadcast !...........\n");
		fclose(fp);
		return -1;
	}
	t6_head = (PJUVCHDR)buf;
	printf("rom recvfrom length ret = %d \n", ret);
	printmessage(t6_head, NULL);
	if (t6_head->Tag != JUVC_TAG) //JUVC   1247106627
	{
		printf("Tag Error = %d\n", t6_head->Tag);
		printf("Re-receive Broadcast and check Tag !...........\n");
		fclose(fp);
		return -1;
	}
	t6_info = (PDEVICEINFO)(buf + 32);
	if (t6_head->XactType == JUVC_TYPE_READY) // 6 ready
	{
		t6_head->XactType == JUVC_TYPE_READY;
		t6_head->Role = role;
		t6_head->TotalLength = 96;
		t6_info->Manufacture = 1; //MCT
		strcpy(t6_info->DeviceName, devicename);
		strcpy(t6_info->ModelName, modelname);
		strcpy(t6_info->Version, version);
		ret = send(sd, buf, t6_head->TotalLength, 0);
		printf("send ret = %d\n", ret);
	}
	else
	{
		return -1;
	}

	bzero(buf, sizeof(buf));
	ret = recv(sd, buf, 32, 0);
	if (ret <= 0) //  other side disconnected
	{
		printf("Receive head Error = %d \n", ret);
		printf("Re-receive Broadcast !...........\n");
		fclose(fp);
		return -1;
	}

	if (t6_head->XactType == JUVC_TYPE_UPDATE_DATA) //need update
	{
		tcptotallength = t6_head->TotalLength - 32;
		if (tcptotallength > (15 * 1024 * 1024) || tcptotallength <= 32) //JUVC
		{
			//printf("File size Error = %d\n", tcptotallength);
			printf("Re-receive Broadcast and check File !...........\n");
			fclose(fp);
			return -1;
		}
		/*=====Receive packet 、calculate current packet 、rest packet and write each packet=====*/

		rom_buf = malloc(PACKET_LENGTH_MAX);
		tcptotalget = 0;
		calculate_file = tcptotallength;
		printf("Receive head Error = %d \n", ret);
		printf("Re-receive Broadcast !...........\n");
		while (tcptotallength)
		{
			bzero(rom_buf, PACKET_LENGTH_MAX);
			ret = recv(sd, rom_buf, PACKET_LENGTH_MAX, 0);
			if (ret <= 0) //disable = 0 ERROR = -1
			{
				printf("Recvfrom  Error = %d \n", ret);
				printf("Re-receive Broadcast !..........\n");
				break;
			}
			write_length = fwrite(rom_buf, 1, ret, fp);
			printf("write_length = %d\n", write_length);
			if (write_length < ret)
			{
				printf("File:\t%s Write Failed!\n", FILE_NAME);
				printf("Re-receive Broadcast !..........\n");
				break;
			}
			printf("rom recvfrom ret = %d \n", ret);
			tcptotalget += ret;
			tcptotallength -= ret;
			percentage = ((float)tcptotalget / calculate_file) * 100;
			printf("percentage = %d\n", percentage);
			printf("tcptotalget =  %d\n", tcptotalget);
			printf("tcptotallength REST =  %d\n", tcptotallength);
			bzero(buf, sizeof(buf));
			t6_head = (PJUVCHDR)buf;
			t6_head->Tag = 1247106627;
			t6_head->XactType = 7; //XactType = 7 (receive status)
			t6_head->HdrSize = 32;
			t6_head->TotalLength = 64;
			t6_info->Manufacture = 1; //MCT
			memcpy(buf + 32, &percentage, sizeof(percentage));
			ret = send(sd, buf, t6_head->TotalLength, 0);
			printf("send ret = %d\n", ret);
			usleep(10000);
		}
	}
	else
		return -1;
	if (write_length < ret || ret <= 0)
	{
		free(rom_buf);
		fclose(fp);
		return -1;
	}
	free(rom_buf);
	fclose(fp);
	printf(KGRN "File:\t%s Receive Finished!", FILE_NAME);
	printf("\n" RESET);
	return 0;
}
int crc_checksum(int sd)
{
	int calculate_crc, receive_crc, ret;
	char *file_buffer;
	unsigned char buf[64] = {0};
	char *ih_name;
	PDEVICEINFO t6_info;
	PJUVCHDR t6_head;
	FILE *fp;
	image_header_t head;
	//====================Check CRC and send CRC result==========================
	file_buffer = (char *)malloc(tcptotalget);

	fp = fopen(FILE_NAME, "r");
	if (fp == NULL)
	{
		printf("open file error !......\n");
		return -1;
	}
	printf("file_size = %d\n", tcptotalget);
	bzero(file_buffer, tcptotalget);
	fread(file_buffer, 1, tcptotalget, fp);
	t6_info = (PDEVICEINFO)(buf + 32);
	calculate_crc = crc32(0, (const unsigned char *)file_buffer + sizeof(image_header_t), tcptotalget - sizeof(image_header_t)); //32 file crc
	fclose(fp);

	fp = fopen(FILE_NAME, "r");
	if (fp == NULL)
	{
		printf("open file error !......\n");
		return -1;
	}
	fread(&head, 1, sizeof(image_header_t), fp); //read data CRC
	receive_crc = ntohl(head.ih_dcrc);
	ih_name = strstr(head.ih_name, "1:");

	printf("ih_magic = %u\n", ntohl(head.ih_magic));
	printf("ih_hcrc = %u\n", ntohl(head.ih_hcrc));
	printf("ih_time = %u\n", ntohl(head.ih_time));
	printf("ih_size = %u\n", ntohl(head.ih_size));
	printf("ih_load = %u\n", ntohl(head.ih_load));
	printf("ih_ep = %u\n", ntohl(head.ih_ep));
	printf("ih_dcrc = %u\n", ntohl(head.ih_dcrc));
	printf("ih_os = %X\n", head.ih_os);
	printf("ih_arch = %X\n", head.ih_arch);
	printf("ih_type = %X\n", head.ih_type);
	printf("ih_comp = %X\n", head.ih_comp);
	printf("ih_name = %s\n", head.ih_name);
	printf("ih_name = %s\n", ih_name);
	fclose(fp);

	bzero(buf, sizeof(buf));
	t6_head = (PJUVCHDR)buf;
	t6_head->Tag = 1247106627;
	t6_head->XactType = 4; //XactType = 4 (CRC)
	t6_head->HdrSize = 32;
	t6_head->TotalLength = 64;

	printf("ih_name[5] = %c\n", ih_name[5]);
	if (ih_name[5] == '1') //MCT
	{
		memcpy(buf + 32, "MCT", 4);
		ret = send(sd, buf, t6_head->TotalLength, 0);
		printf("send ret = %d\n", ret);
		printf("ih_name[5] = %c\n", ih_name[5]);
	}
	if (ih_name[5] == '2') //J5
	{
		memcpy(buf + 32, "j5", 3);
		ret = send(sd, buf, t6_head->TotalLength, 0);
		printf("send ret = %d\n", ret);
		printf("ih_name[5] = %c\n", ih_name[5]);
		return -1;
	}
	sleep(3);
	if (ih_name[2] == 'T' && role == JUVC_DEVICE_TYPE_TX) //MCT
	{
		memcpy(buf + 32, "TX", 3);
		ret = send(sd, buf, t6_head->TotalLength, 0);
		printf("send ret = %d\n", ret);
		printf("ih_name[2] = %c\n", ih_name[2]);
	}
	else if (ih_name[2] == 'R' && role == JUVC_DEVICE_TYPE_RX) //MCT
	{
		memcpy(buf + 32, "RX", 3);
		ret = send(sd, buf, t6_head->TotalLength, 0);
		printf("send ret = %d\n", ret);
		printf("ih_name[2] = %c\n", ih_name[2]);
	}
	else
	{
		memcpy(buf + 32, "Type Error", 11);
		ret = send(sd, buf, t6_head->TotalLength, 0);
		printf("send ret = %d\n", ret);
		printf("ih_name[2] = %c\n", ih_name[2]);
		free(file_buffer);
		return -1;
	}
	sleep(2);
	if (calculate_crc != receive_crc)
	{
		printf("Accept File Content is Error !.........\n");
		printf("calculation CRC32 = %u\n", calculate_crc);
		printf("Recive t6_head->DeviceInfo.crc32 = %u\n", receive_crc);
		memcpy(buf + 32, "File Content is Error !", 24);
		ret = send(sd, buf, t6_head->TotalLength, 0);
		printf("send ret = %d\n", ret);
		free(file_buffer);
		return -1;
	}
	else
	{
		printf("calculation CRC32 = %u\n", calculate_crc);
		printf("Recive t6_head->DeviceInfo.crc32 = %u\n", receive_crc);
		printf("Accept Content is Complete !.........\n");
		//memcpy(buf + 32, "Content is Complete", 20);
		memcpy(buf + 32, "Waiting to Burn in", 19);
		ret = send(sd, buf, sizeof(buf), 0);
		printf("send ret = %d\n", ret);
	}
	sleep(2);
	free(file_buffer);
	return 0;
}
int write_progress(int sd, int alive)
{
	/*============Write progress 、read Program process and send=================*/
	int fpeng;
	unsigned char writebuf[4] = {0};
	unsigned char cmd[512] = {0};
	unsigned char buf[64] = {0};
	PJUVCHDR t6_head;
	FILE *fp_progress;
	int ret = 0;

	printf("File_TotalLength:%d\n", tcptotalget);
	printf("alive = %d\n", alive);
	if (alive != 3)
	{
		snprintf(cmd, sizeof(cmd), "mtd_write -o %d -l %d write %s Kernel &", 0, tcptotalget, FILE_NAME); //FILE_NAME
		//snprintf(cmd, sizeof(cmd), "/home/ryan/samba/updater/mtd_write -o %d -l %d write %s Kernel &", 0, tcptotalget, "/home/ryan/samba/updater/RusbFW");
		system(cmd);
		progress = 0;
	}

	while (progress < 100)
	{
		fp_progress = fopen("/var/mtd_write.log", "r"); // /var/mtd_write.log  /home/ryan/samba/updater/write_file.txt
		if (fp_progress == NULL)
		{
			printf("File open fail, %d\n", errno);
			sleep(1);
			continue;
		}
		fread(writebuf, 1, sizeof(writebuf), fp_progress);
		sscanf(writebuf, "%d", &progress);
		printf("progress : %d\n", progress);
		t6_head = (PJUVCHDR)buf;
		t6_head->Tag = JUVC_TAG;
		t6_head->XactType = JUVC_TYPE_UPDATE_STATUS; //XactType = 3 (write status)
		t6_head->HdrSize = 32;
		t6_head->TotalLength = 64;
		printf("progress : %d   %d\n", progress, __LINE__);
		memcpy(buf + 32, &progress, sizeof(progress));
		ret = send(sd, buf, t6_head->TotalLength, 0);
		printf("ret = %d\n", ret);
		fclose(fp_progress);
		usleep(500000);
		if (ret == -1)
			return -1; //TCP disconnect
	}
	return 0;
}
void *thread_fun(void *data)
{
	pthread_detach(pthread_self());
	PJUVCHDR t6_head;
	PDEVICEINFO t6_info;
	PTHREADINFO t6_thread;
	int ret;
	char buf[128] = {0};
	FILE *fp;

	t6_thread = *(PTHREADINFO *)data;
	t6_head = (PJUVCHDR)buf;
	t6_info = (PDEVICEINFO)(buf + 32);
	printf("t6_thread->sd = %d\n", t6_thread->sd);
	printf("t6_thread->flag = %d\n", t6_thread->flag);
	printf("t6_thread->alive = %d\n", t6_thread->alive);
	printf("t6_thread->Version = %s\n", t6_thread->Version);
	printf("t6_thread->ModelName = %s\n", t6_thread->ModelName);
	printf("t6_thread->DeviceName = %s\n", t6_thread->DeviceName);
	if (t6_thread->flag == 0)
	{
		if ((ret = receive_data(t6_thread->sd, t6_thread->ModelName, t6_thread->Version, t6_thread->DeviceName)) == -1)
		{

			t6_head->XactType = 4; //XactType = 4 (CRC status)
			memcpy(buf + 32, "receive_data Error !", 21);
			send(t6_thread->sd, buf, 53, 0);
			ledcolor("red");
			close_socket(t6_thread->sd, tcp_sd, broadcastsd);
			printf("receive_data = %d\n", ret);
			kill_rc = 3;
			sleep(1);
			pthread_exit(NULL);
		}
		t6_thread->flag++;
	}

	if (t6_thread->flag == 1)
	{
		if ((ret = crc_checksum(t6_thread->sd)) == -1)
		{
			ledcolor("red");
			sleep(1);
			kill_rc = 3;
			close_socket(t6_thread->sd, tcp_sd, broadcastsd);
			printf("crc_checksum = %d\n", ret);
			system("rm /var/tmpFW");
			pthread_exit(NULL);
		}
		t6_thread->flag++;
	}

	if (t6_thread->flag == 2)
	{
		if ((ret = write_progress(t6_thread->sd, t6_thread->alive)) == -1)
		{
			//ledcolor("red");
			printf("write_progress = %d\n", ret);
			sleep(1);
			kill_rc = 3;
			pthread_exit(NULL);
		}
	}
	close_socket(t6_thread->sd, tcp_sd, broadcastsd);
	printf(KGRN "Update Complete!");
	printf("\n" RESET);
	ledcolor("green");
	system("killall -SIGTTIN nvram_daemon");
	//system("reboot");
	pthread_exit(NULL);
}
int main(int argc, char *avg[])
{
	int recvbytes, sendBytes, fromlen, ret, acceptfd, manuf;
	unsigned int sin_size, write_length, maxfd;
	char buf[128] = {0};
	char thread_data[64] = {0};
	char macaddr[7] = {0};
	char type[3] = {0};
	char modelname[32], version[32], devicename[32], manufac[32];
	char writebuf[4] = {0};
	pthread_t thread_1, thread_2;
	PJUVCHDR t6_head;
	PDEVICEINFO t6_info;
	PTHREADINFO t6_thread;
	struct timeval tv;
	struct sockaddr_in their_addr;
	FILE *fp, *fp_progress;

	ret = 0;
	kill_rc = 3; //any
	acceptfd = 0;
	tcp_create();
	broadcast_create();
	fd_set read_sd;
	FD_ZERO(&read_sd);
	signal(SIGPIPE, SIG_IGN);
	memcpy(macaddr, get_macaddr("rai0"), 6);
	strcpy(modelname, nvram_get(RT2860_NVRAM, "ModelName")); //catch device ModelName
	strcpy(version, nvram_get(RT2860_NVRAM, "Version"));
	strcpy(devicename, nvram_get(RT2860_NVRAM, "HostName"));
	strcpy(manufac, nvram_get(RT2860_NVRAM, "ManufactureCode"));
	strcpy(type, nvram_get(RT2860_NVRAM, "Type"));
	if (strncmp(type, "TX", 1) == 0)
		role = JUVC_DEVICE_TYPE_TX;
	else
		role = JUVC_DEVICE_TYPE_RX;
	sscanf(manufac, "%d", &manuf);
	system("rm /var/mtd_write.log");

	while (1)
	{
		FD_SET(tcp_sd, &read_sd);
		FD_SET(broadcastsd, &read_sd);
		tv.tv_sec = 7;
		tv.tv_usec = 0;
		maxfd = (tcp_sd > broadcastsd) ? (tcp_sd + 1) : (broadcastsd + 1); //which max+1
		ret = select(maxfd + 1, &read_sd, 0, 0, &tv);

		if (ret == 0) //timeout
		{
			printf("Waiting Receive BroadCast !.......  ret = %d\n", ret);
			fp_progress = fopen("/var/mtd_write.log", "r"); // /var/mtd_write.log  /home/ryan/samba/updater/write_file.txt
			if (fp_progress == NULL)
			{
				printf("File open fail, %d\n", errno);
				sleep(1);
				continue;
			}
			fread(writebuf, 1, sizeof(writebuf), fp_progress);
			sscanf(writebuf, "%d", &progress);
			if (progress == 100)
			{
				ledcolor("green");
				system("killall -SIGTTIN nvram_daemon");
			}
			fclose(fp_progress);
			printf("progress = %d\n", progress);
		}

		else if (ret == -1)
		{ //fail
			printf("Select Error Re-receive Broadcast !...... ret = %d\n", ret);
			broadcast_create();
			tcp_create();
			FD_ZERO(&read_sd);
			kill_rc = 3;
			ret = 0;
		}
		// add tcp connect
		else if (FD_ISSET(tcp_sd, &read_sd))
		{
			sin_size = sizeof(struct sockaddr_in);
			acceptfd = accept(tcp_sd, (struct sockaddr *)&their_addr, &sin_size);
			if (acceptfd < 0)
			{
				printf("Server-accept() Error Re-receive Broadcast !......\n");
				close_socket(0, tcp_sd, 0);
				acceptfd = 0;
				continue;
			}
			printf("Server-accept() is OK\n");
			printf("Server-new socket, acceptfd is OK...\n");
			printf("Got connection from the client: %s\n", inet_ntoa(their_addr.sin_addr)); // *client IP

			FD_SET(acceptfd, &read_sd); // add master set
			if (acceptfd >= maxfd)
			{ // max fd
				maxfd = acceptfd;
			}

			ledcolor("flash");

			bzero(thread_data, sizeof(thread_data));
			t6_thread = (PTHREADINFO)thread_data;
			t6_thread->sd = acceptfd;
			t6_thread->flag = 2;
			strncpy(t6_thread->DeviceName, devicename, strlen(devicename) - 1);
			strncpy(t6_thread->ModelName, modelname, strlen(modelname) - 1);
			strncpy(t6_thread->Version, version, strlen(version) - 1);

			fp = fopen("/var/mtd_write.log", "r"); // /var/mtd_write.log  /home/ryan/samba/updater/write_file.txt

			if (fp == NULL)
			{
				t6_thread->flag = 0;
				t6_thread->alive = 1;
			}
			else
			{
				t6_thread->flag = 2;
				t6_thread->alive = 3;
				fclose(fp);
			}
			ret = pthread_create(&thread_1, NULL, thread_fun, &t6_thread);
			printf("ret = %d\n", ret);
			kill_rc = pthread_kill(thread_1, 0);
			printf("kill_rc = %d    %d\n", kill_rc, __LINE__);
		}
		// *Broadcast
		else if (FD_ISSET(broadcastsd, &read_sd) && FD_ISSET(tcp_sd, &read_sd) != 1)
		{
			fromlen = sizeof(struct sockaddr_in);
			bzero(buf, sizeof(buf));
			recvfrom(broadcastsd, buf, 32, 0, (struct sockaddr *)&server, &fromlen);
			t6_head = (PJUVCHDR)buf;
			kill_rc = pthread_kill(thread_1, 0);

			if (t6_head->XactType == JUVC_TYPE_DISCOVER && kill_rc != 0) //JUVC_TYPE_DISCOVER = 5
			{
				system("killall apclient_chkconn.sh");
				t6_head = (PJUVCHDR)buf;
				t6_info = (PDEVICEINFO)(buf + 32);
				t6_head->Tag = JUVC_TAG;
				t6_head->XactType = JUVC_TYPE_INFO;
				t6_head->Role = role; //TX or RX select
				t6_head->XactId = 204;
				t6_head->HdrSize = 32;
				t6_head->TotalLength = 96;
				t6_info->Port = TCP_PORT;
				t6_info->Manufacture = manuf; //MCT
				memcpy(t6_info->MacAddress, macaddr, 6);
				strncpy(t6_info->ModelName, modelname, strlen(modelname));
				strncpy(t6_info->Version, version, strlen(version));
				strncpy(t6_info->DeviceName, devicename, strlen(devicename));
				sendBytes = sendto(broadcastsd, buf, t6_head->TotalLength, 0, (struct sockaddr *)&form, fromlen);
				printmessage(t6_head, t6_info);
			}
		}
	}
}
