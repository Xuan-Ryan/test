#include <stdio.h> 
#include <stdlib.h> 
#include <errno.h> 
#include <string.h> 
#include <netdb.h> 
#include <sys/types.h> 
#include <netinet/in.h> 
#include <sys/socket.h> 
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/tcp.h> 
#include <pthread.h>

pthread_mutex_t tcp_mutex = PTHREAD_MUTEX_INITIALIZER;



#include"network.h"

void hex_dump(char *data, int size, char *caption)
{
        int i; // index in data...
        int j; // index in line...
        char temp[8];
        char buffer[128];
        char *ascii;

        memset(buffer, 0, 128);

        printf("---------> %s <--------- (%d bytes from %p)\n", caption, size, data);

        // Printing the ruler...
        printf("        +0          +4          +8          +c            0   4   8   c   \n");

        // Hex portion of the line is 8 (the padding) + 3 * 16 = 52 chars long
        // We add another four bytes padding and place the ASCII version...
        ascii = buffer + 58;
        memset(buffer, ' ', 58 + 16);
        buffer[58 + 16] = '\n';
        buffer[58 + 17] = '\0';
        buffer[0] = '+';
        buffer[1] = '0';
        buffer[2] = '0';
        buffer[3] = '0';
        buffer[4] = '0';
        for (i = 0, j = 0; i < size; i++, j++) {
                if (j == 16) {
                        printf("%s", buffer);
                        memset(buffer, ' ', 58 + 16);

                        sprintf(temp, "+%04x", i);
                        memcpy(buffer, temp, 5);

                        j = 0;
                }

                sprintf(temp, "%02x", 0xff & data[i]);
                memcpy(buffer + 8 + (j * 3), temp, 2);
                if ((data[i] > 31) && (data[i] < 127))
                        ascii[j] = data[i];
                else
                        ascii[j] = '.';
        }

        if (j != 0)
                printf("%s", buffer);
}


int UdpInit()
{
	int sockfd;
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if(sockfd < 0)
	{
		printf("create socket fail!\n");
		return -1;
	}
	int nSendBuf=MNSP_MAX_MCAST_PACKET_SIZE;//設置為32K
	setsockopt(sockfd,SOL_SOCKET,SO_SNDBUF,(const char*)&nSendBuf,sizeof(int));
    return sockfd ;

}

int UdpRead(int sockfd ,char *ipaddr ,int port, char* buf,int size)
{
    int ret;
	struct sockaddr_in ser_addr;
	int addr_len = sizeof(struct sockaddr_in);
	memset(&ser_addr, 0, sizeof(struct sockaddr_in));
	ser_addr.sin_family = AF_INET;
	ser_addr.sin_addr.s_addr = inet_addr(ipaddr);
	ser_addr.sin_port = htons(port);	//注意网?序??

	ret = recvfrom(sockfd,buf,size,0,(struct sockaddr*)&ser_addr, &addr_len);
	if(ret < 0){
		return -1;
	}
	return ret;

}


int UdpWrite(int sockfd ,char *ipaddr ,int port, char* buf,int size)
{
    int ret;
	struct sockaddr_in ser_addr;
	memset(&ser_addr, 0, sizeof(struct sockaddr_in));
	ser_addr.sin_family = AF_INET;
	ser_addr.sin_addr.s_addr = inet_addr(ipaddr);
	ser_addr.sin_port = htons(port);	//注意网?序??

	ret = sendto(sockfd,buf,size,0,(struct sockaddr*)&ser_addr, sizeof(struct sockaddr_in));
	if(ret < 0){
		return -1;
	}
	return ret;

}

int TcpConnect(char* ipaddr ,int port ,int t)
{
	
	int sockfd = 0;
    int on = 1; 
	struct timeval timeout;  
	printf("TcpConnect add= %s port= %d\n",ipaddr,port);
    sockfd = socket(AF_INET , SOCK_STREAM , 0);

    if (sockfd <= 0){
        printf("Fail to create a socket.");
		return -1;
    }

    //socket的連線
    struct sockaddr_in info;
    bzero(&info,sizeof(info));
    info.sin_family = PF_INET;
    info.sin_addr.s_addr = inet_addr(ipaddr);
    info.sin_port = htons(port);
     
    int err = connect(sockfd,(struct sockaddr *)&info,sizeof(info));
    if(err==-1){
        printf("Connection error \n");
		close(sockfd);
		return -1;
    }
    setsockopt(sockfd,IPPROTO_TCP,TCP_NODELAY, (char *)&on, sizeof(on)); 
	if(t > 0){
	    timeout.tv_sec = t;
	    timeout.tv_usec = 0;
		setsockopt (sockfd, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout,sizeof(timeout));
	}else{
		
		int keepalive = 1;  // 開啟keepalive
		int keepidle = 3;  // 空閒3s開始傳送檢測包（系統預設2小時）
		int keepinterval = 1;  // 傳送檢測包間隔 1s（系統預設75s）
		int keepcount = 5;  // 傳送次數如果5次都沒有迴應，就認定peer端斷開了。（系統預設9次）
		setsockopt(sockfd, SOL_SOCKET,  SO_KEEPALIVE,&keepalive, sizeof(keepalive));
		setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPIDLE,&keepidle, sizeof(keepidle));
		setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPINTVL,&keepinterval, sizeof(keepinterval));
		setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPCNT,&keepcount, sizeof(keepcount));
	}
    return sockfd;
    

}

int TcpWrite(int clientSocket,char *buffer, int size)
{
	int length = sizeof(int);
	int write_bytes = 0;
	int offset = 0;
    pthread_mutex_lock(&tcp_mutex);
	do {
		int write_ret = 0;
		offset = write_bytes;
		write_ret = write(clientSocket, buffer+offset, size-offset);
		if(write_ret <= 0){
			 pthread_mutex_unlock(&tcp_mutex);
			return write_ret;
		}
		
		write_bytes += write_ret;
	}while(write_bytes < size);
    pthread_mutex_unlock(&tcp_mutex);
	return write_bytes;
}

int TcpRead(int clientSocket,char *buffer, int size)
{
  
    int rc ;
    int ret ;
	int read_bytes = 0;
	int offset = 0;
	int datasize = size;
    int retry = 0 ;
	int read_retry = 0 ;
    do{

		ret = read(clientSocket, buffer+offset, datasize);
		if(ret <= 0)
			return ret ;
	
		
		read_bytes += ret;
		offset += ret ;
		datasize = datasize - ret ;
		setsockopt(clientSocket, IPPROTO_TCP, TCP_QUICKACK, (int[]){1}, sizeof(int));
	}while(read_bytes < size);	
	return read_bytes;
	
	
}


int udpImageWrite(int sockfd ,char *ipaddr ,int port,char id ,char* image_data , int total_size)
{

		
	char mnspbuf[MNSP_MAX_MCAST_PACKET_SIZE ];
	PMNSPXACTHDR  mnsp = (PMNSPXACTHDR) mnspbuf;

	int ret = 0 ;
	int page = total_size / IMAGE_DIVIDED_UNIT;
	int page_num = total_size % IMAGE_DIVIDED_UNIT;
	int offset = 0 ;
    char*  ptr_image ;
	
	ptr_image = (char *)image_data;
	//printf("page = %d \n",page);
	//printf("page_num = %d \n",page_num);
   
	while(page > 0)
	{
		mnsp->Tag = JUVC_TAG;
		mnsp->HdrSize = 32;
		mnsp->PayloadLength = IMAGE_DIVIDED_UNIT;
		mnsp->TotalLength = total_size;
		mnsp->XactOffset = offset;
		mnsp->XactId = id;
		memcpy(mnspbuf+32,ptr_image,IMAGE_DIVIDED_UNIT);
		offset+= IMAGE_DIVIDED_UNIT;
		ptr_image+=IMAGE_DIVIDED_UNIT;
		page--;
		ret = UdpWrite(sockfd,ipaddr,port,mnspbuf,MNSP_MAX_MCAST_PACKET_SIZE);
		if(ret < 0){
			return -1;
		}
	}

	if(page_num > 0){
		mnsp->Tag = JUVC_TAG;
		mnsp->HdrSize = 32;
		mnsp->PayloadLength = page_num;
		mnsp->TotalLength = total_size;
		mnsp->XactOffset = offset;
		mnsp->XactId = id;
		memcpy(mnspbuf+32,ptr_image,page_num);
		ret = UdpWrite(sockfd,ipaddr,port,mnspbuf,MNSP_MAX_MCAST_PACKET_SIZE);
		if(ret < 0){
			return -1;
		}
	}
	
	return 0;





}


void closeSocket(int clientSocket)
{
	pthread_mutex_lock(&tcp_mutex);
	if(clientSocket > 0){
		printf("close socket = %d \n",clientSocket);
		shutdown(clientSocket, SHUT_RDWR);
		close(clientSocket);
	}
	pthread_mutex_unlock(&tcp_mutex);
	clientSocket = -1;
}



