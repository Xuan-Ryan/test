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
#include <time.h>
#include <pthread.h>
#include <arpa/inet.h> //inet_ntoa
#include <sys/types.h> /* open socket */
#include <unistd.h>	   /* open */
#include <sys/stat.h>  /* open */
#include <fcntl.h>	   /* open */
#include <sys/ioctl.h> /* ioctl */
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <stdbool.h>
#include <linux/types.h>
#include <linux/input.h>
#include <linux/hidraw.h>
#include <errno.h>
//#include <linux/autoconf.h>

#include "mnspdef.h"   //PWIFIDONGLEROMHDR
#include "t6bulkdef.h" //PT6BULKDMAHDR
#include "queue.h"
#include "t6usbdongle.h"
#include "displayserver.h"
#include "jwr2100_tcp_cmddef.h"
#include "CRC.h"

#define KGRN "\033[0;32;32m"
#define RESET "\033[0m"
#define FILE_NAME "UVC-j5-RX-0.0.0.1.bin" //254
#define PACKET_LENGTH 32 * 1024
#define DEVICE_NAME "TEST_NAME"
#define MY_SERVER_IP "192.168.1.121"
#define PORT 55551
#define BROADCAST_IP "255.255.255.255"
#define BROADCAST_SEND 55550
#define HEAD_SIZE 32

int ret, tcp_sd, brdcfd, i, optval, calculate_CRC, receiveCRC, accept_socket, flag;
unsigned int tcptotalget, tcptotallength;
unsigned char *rom_buf;
unsigned char buf[64] = {0};
char *file_buffer;
FILE *fp;
PJUVCHDR t6_head;
PDEVICEINFO t6_info;
PJUVCHDR pJUVHdr;
struct sockaddr_in server; //br receform
struct sockaddr_in form;   //br sendto
struct timeval tv;

void printmessage(PJUVCHDR t6_head, PDEVICEINFO t6_info) //print head message
{
	if (t6_head != NULL)
	{
		printf("t6_head->Tag = %u\n", t6_head->Tag);
		printf("t6_head->XactType = %d\n", t6_head->XactType);
		printf("t6_head->Flags = %u\n", t6_head->Role);
		printf("t6_head->XactId = %u\n", t6_head->XactId);
		printf("t6_head->TotalLength = %u\n", t6_head->TotalLength);
		printf("t6_head->HdrSize = %d\n", t6_head->HdrSize);
	}
	if (t6_info != NULL)
	{
		printf("t6_info->crc32 = %u\n", t6_info->crc32);
		printf("t6_info->name = %s\n", t6_info->name);
		printf("t6_info->port = %d\n", t6_info->port);
	}
	printf("=========================================================\n");
}
void Broadcast_create()
{
	//=================================Broadcast Create============================================
	int flag;
	char recvbuf[HEAD_SIZE] = {0};

	brdcfd = socket(PF_INET, SOCK_DGRAM, 0);
	i = 0;
	while (brdcfd < 0 && i < 5) //fail mechanism
	{
		DEBUG_PRINT("Socket Fail\n");
		brdcfd = socket(PF_INET, SOCK_DGRAM, 0);
		i++;
		if (i == 4)
		{
			printf("Please Retry!............\n");
			exit(1);
		}
	}

	//ret = setsockopt(brdcfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
	flag = 1;
	optval = 1;
	ret = setsockopt(brdcfd, SOL_SOCKET, SO_REUSEADDR, (const char *)&flag, sizeof(flag));
	ret = setsockopt(brdcfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	ret = setsockopt(brdcfd, SOL_SOCKET, SO_BROADCAST, &optval, sizeof(optval));

	bzero(&server, sizeof(struct sockaddr_in));
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = 0x00; //INADDR_ANY = 0
	server.sin_port = htons(BROADCAST_SEND);

	form.sin_family = AF_INET;
	form.sin_addr.s_addr = htonl(INADDR_BROADCAST); //255.255.255.255
	form.sin_port = htons(55550);
	i = 0;
	while ((ret = bind(brdcfd, (struct sockaddr *)&server, sizeof(struct sockaddr_in))) < 0 && i < 5)
	{
		DEBUG_PRINT("bind Fail = %d\n", ret);
		ret = bind(brdcfd, (struct sockaddr *)&server, sizeof(struct sockaddr_in));
		i++;
		if (i == 4)
		{
			printf("Please Retry!............\n");
			close(brdcfd);
			exit(1);
		}
	}
}
void TCP_create()
{
	//=================================TCP Create======================================
	tcp_sd = socket(AF_INET, SOCK_STREAM, 0);
	i = 0;
	while (tcp_sd < 0 && i < 5)
	{
		DEBUG_PRINT("Socket Create Error!\n");
		tcp_sd = socket(AF_INET, SOCK_STREAM, 0);
		sleep(1);
		if (i == 4)
		{
			printf("Please Retry!............\n");
			exit(1);
		}
	}
	DEBUG_PRINT("test cursor socket tcp_sd = %d ip = %s port = %d \n", tcp_sd, MY_SERVER_IP, PORT);
	flag = 1;
	ret = setsockopt(tcp_sd, SOL_SOCKET, SO_REUSEADDR, (const char *)&flag, sizeof(flag));

	struct sockaddr_in tcp_addr;
	tcp_addr.sin_family = AF_INET;
	tcp_addr.sin_port = htons(PORT);
	tcp_addr.sin_addr.s_addr = inet_addr(MY_SERVER_IP);

	ret = bind(tcp_sd, (struct sockaddr *)&tcp_addr, sizeof(tcp_addr));
	i = 0;
	while (ret < 0 && i < 5)
	{
		DEBUG_PRINT("TCP Bind error!\n");
		ret = bind(tcp_sd, (struct sockaddr *)&tcp_addr, sizeof(tcp_addr));
		sleep(1);
		i++;
		if (i == 4)
		{
			printf("Please Retry !............\n");
			close(tcp_sd);
			close(brdcfd);
			exit(1);
		}
	}
	DEBUG_PRINT("test cursor Bind successfully.\n");
	ret = listen(tcp_sd, 32);
	i = 0;
	while (ret < 0 && i < 5)
	{
		DEBUG_PRINT("Server-listen() Error");
		ret = listen(tcp_sd, 32);
		sleep(1);
		i++;
		if (i == 4)
		{
			printf("Please Retry !............\n");
			close(tcp_sd);
			close(brdcfd);
			exit(1);
		}
	}

	printf("Server-Ready for client connection...\n");
}
int Receive_data()
{
	unsigned int write_length;
	rom_buf = malloc(PACKET_LENGTH);

	//==============================Receive head===================================
	tcptotallength = 0;
	bzero(buf, sizeof(buf));
	ret = recv(accept_socket, buf, 32, 0);
	if (ret <= 0) //  other side disconnected
	{
		DEBUG_PRINT("Receive head Error = %d \n", ret);
		printf("Re-receive Broadcast !...........\n");
		fclose(fp);
		close(accept_socket);
		return -1;
	}
	t6_head = (PJUVCHDR)buf;
	DEBUG_PRINT("rom recvfrom length ret = %d \n", ret);
	printmessage(t6_head, NULL);

	tcptotallength = t6_head->TotalLength - 32;
	if (t6_head->Tag != 1247106627) //JUVC
	{
		DEBUG_PRINT("Tag Error %d\n", t6_head->Tag);
		printf("Re-receive Broadcast and check Tag !...........\n");
		fclose(fp);
		close(accept_socket);
		return -1;
	}
	/*=====Receive packet 、calculate current packet 、rest packet and write each packet=====*/
	tcptotalget = 0;
	while (tcptotallength)
	{
		bzero(rom_buf, PACKET_LENGTH);
		printf("tcptotallength REST =  %d\n", tcptotallength);
		ret = recv(accept_socket, rom_buf, PACKET_LENGTH, 0);
		if (ret <= 0) //disable = 0 ERROR = -1
		{
			DEBUG_PRINT("Recvfrom  Error = %d \n", ret);
			printf("Re-receive Broadcast !..........\n");
			continue;
		}

		write_length = fwrite(rom_buf, 1, ret, fp);
		if (write_length < ret)
		{
			DEBUG_PRINT("File:\t%s Write Failed!\n", FILE_NAME);
			printf("Re-receive Broadcast !..........\n");
			continue;
		}

		DEBUG_PRINT("rom recvfrom ret = %d \n", ret);
		tcptotalget = tcptotalget + ret;
		tcptotallength = tcptotallength - ret;
		printf("tcptotalget = %d\n", tcptotalget);
		printf("tcptotallength REST =  %d\n", tcptotallength);
	}
	if (write_length < ret || ret <= 0)
	{
		free(rom_buf);
		fclose(fp);
		close(accept_socket);
		close(tcp_sd);
		close(brdcfd);
		return -1;
	}

	fclose(fp);
	printf(KGRN "File:\t%s Receive Finished!", FILE_NAME);
	printf("\n" RESET);
	return 0;
}
int CRC_checksum()
{
	image_header_t head;
	//====================Check CRC and send CRC result==========================
	file_buffer = (char *)malloc(tcptotalget);
	fp = fopen(FILE_NAME, "r");
	if (fp == NULL)
		printf("open file error !......\n");
	printf("file_size = %d\n", tcptotalget);
	bzero(file_buffer, tcptotalget);
	fread(file_buffer, 1, tcptotalget, fp);
	t6_info->crc32 = crc32(0, (const unsigned char *)file_buffer + sizeof(image_header_t), tcptotalget - sizeof(image_header_t)); //32 file crc
	calculate_CRC = t6_info->crc32;

	fp = fopen(FILE_NAME, "r");
	if (fp == NULL)
		printf("open file error !......\n");
	fread(&head, 1, sizeof(image_header_t), fp); //read data CRC
	receiveCRC = ntohl(head.ih_dcrc);

	bzero(buf, sizeof(buf));
	pJUVHdr = (PJUVCHDR)buf;
	pJUVHdr->Tag = 1247106627;
	pJUVHdr->XactType = 4; //XactType = 4 (CRC)
	pJUVHdr->HdrSize = 32;
	pJUVHdr->TotalLength = 64;
	fclose(fp);
	char str[20] = {};
	char str2[15] = {};
	if (calculate_CRC != receiveCRC)
	{
		DEBUG_PRINT("Accept File Content is Error !.........\n");
		printf("calculation CRC32 = %u\n", calculate_CRC);
		printf("Recive t6_head->DeviceInfo.crc32 = %u\n", receiveCRC);
		strcat(str, "CRC = ");
		sprintf(str2, "%u", calculate_CRC);
		strncat(str, str2, strlen(str2));
		memcpy(buf + 32, str, strlen(str));
		send(accept_socket, buf, sizeof(buf), 0);
		//send(accept_socket, &head, sizeof(image_header_t), 0);
		printf("%s\n", str);
		free(file_buffer);
		close(accept_socket);
		return -1;
	}
	else
	{
		printf("calculation CRC32 = %u\n", calculate_CRC);
		printf("Recive t6_head->DeviceInfo.crc32 = %u\n", receiveCRC);
		printf("Accept Content is Complete !.........\n");
		memcpy(buf + 32, "CRC Complete", 13);
		send(accept_socket, buf, sizeof(buf), 0);
	}
	return 0;
}
int Write_progress()
{
	/*============Write progress 、read Program process and send=================*/
	int progress, fpeng;
	unsigned int status;
	unsigned char writebuf[4] = {0};
	unsigned char cmd[512] = {0};
	//char *file_buf = malloc(tcptotalget);
	FILE *fp_progress;
	//	bzero(rom_buf, sizeof(rom_buf));
	if (calculate_CRC == receiveCRC) //FW_DEST_RX = 1  rom_head->FW_for_dest == FW_DEST_RX
	{
		//fpeng = open("/home/ryan/samba/updater/RusbFW", O_RDWR | O_CREAT);
		//int fp = fopen(FILE_NAME, "r");
		//write(fpeng, file_buffer, tcptotalget);
		//write(fpeng, rom_buf, tcptotalget); //&rom_buf[32 + 512]
		printf("rom_head->TotalLength:%d\n", tcptotalget);
		snprintf(cmd, sizeof(cmd), "/home/ryan/samba/updater/mtd_write -o %d -l %d write %s Kernel &", 0, tcptotalget, "/home/ryan/samba/updater");
		status = system(cmd);
		usleep(10000);
		progress = 0;
		while (progress < 100)
		{
			fp_progress = fopen("/home/ryan/samba/updater/write_file.txt", "r");
			bzero(writebuf, sizeof(writebuf));
			fread(writebuf, 1, sizeof(writebuf), fp_progress);
			sscanf(writebuf, "%d", &progress);
			printf("Font progress : %d\n", progress);
			bzero(buf, sizeof(buf));
			pJUVHdr = (PJUVCHDR)buf;
			pJUVHdr->Tag = 1247106627;
			pJUVHdr->XactType = 3; //XactType = 3 (status)
			pJUVHdr->HdrSize = 32;
			pJUVHdr->TotalLength = 64;
			memcpy(buf + 32, &progress, sizeof(progress));
			send(accept_socket, buf, 64, 0);
			fclose(fp_progress);
			usleep(200000);
		}
		close(accept_socket);
		system("reboot");
	}
	free(file_buffer);
	return 0;
}
int main(int argc, char *avg[])
{
	int recvbytes, sendBytes, fromlen;
	unsigned int sin_size, write_length, maxfd;
	unsigned char recvbuf[32] = {0};
	struct sockaddr_in their_addr;

	Broadcast_create();
	TCP_create();
	fp = fopen(FILE_NAME, "w");
	printf("Waiting Receive BroadCast !.......  ret = %d\n", ret);

	while (1)
	{
		fd_set read_sd;
		FD_ZERO(&read_sd);
		FD_SET(tcp_sd, &read_sd);
		FD_SET(brdcfd, &read_sd);
		tv.tv_sec = 6;
		tv.tv_usec = 0;

		maxfd = (tcp_sd > brdcfd) ? (tcp_sd + 1) : (brdcfd + 1); //which max+1
		ret = select(maxfd + 1, &read_sd, 0, 0, &tv);

		if (ret == -1) //fail
			DEBUG_PRINT("Select Error Re-receive Broadcast !...... ret = %d\n", ret);
		else if (ret == 0) //timeout
			DEBUG_PRINT("Waiting Receive BroadCast !.......  ret = %d\n", ret);

		else
		{ /*TCP*/
			if (FD_ISSET(tcp_sd, &read_sd))
			{
				sin_size = sizeof(struct sockaddr_in);
				accept_socket = accept(tcp_sd, (struct sockaddr *)&their_addr, &sin_size);
				i = 0;
				if (accept_socket < 0)
				{
					DEBUG_PRINT("Server-accept() Error Re-receive Broadcast !......\n");
					fclose(fp);
					close(accept_socket);
					close(tcp_sd);
					close(brdcfd);
					continue;
				}
				printf("Server-accept() is OK\n");
				printf("Server-new socket, accept_socket is OK...\n");
				printf("Got connection from the client: %s\n", inet_ntoa(their_addr.sin_addr)); /*client IP*/

				if (Receive_data() == -1)
					continue;
				//* CRC Checksum *//
				if (CRC_checksum() == -1)
					continue;

				if (Write_progress() == -1)
					continue;

				free(rom_buf);
				close(accept_socket);
				close(tcp_sd);
				close(brdcfd);
				printf(KGRN "Update Complete!");
				printf("\n" RESET);
				return;
			}
			/*Broadcast*/
			else if (FD_ISSET(brdcfd, &read_sd))
			{
				fromlen = sizeof(struct sockaddr_in);
				recvfrom(brdcfd, recvbuf, 32, 0, (struct sockaddr *)&server, &fromlen);
				t6_head = (PJUVCHDR)recvbuf;
				if (t6_head->XactType == JUVC_TYPE_DISCOVER) //JUVC_TYPE_DISCOVER =5
				{
					printf("ret = %d\n", ret);
					bzero(buf, sizeof(buf));
					t6_head = (PJUVCHDR)buf;
					t6_info = (PDEVICEINFO)(buf + 32);
					t6_head->Tag = 1247106627;
					t6_head->XactType = 1;
					t6_head->Role = 1;
					t6_head->XactId = 204;
					t6_head->HdrSize = 32;
					t6_info->port = 55551;
					strcpy(t6_info->name, DEVICE_NAME);

					t6_head = (PJUVCHDR)buf;
					sendBytes = sendto(brdcfd, buf, sizeof(struct deviceinfo) + sizeof(struct juvc_hdr_packet), 0, (struct sockaddr *)&form, sizeof(struct sockaddr_in));
					printmessage(t6_head, t6_info);
				}
			}
		}
	}
}
