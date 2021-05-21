
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <getopt.h>             /* getopt_long() */
#include <stdint.h>

#include <fcntl.h>              /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <signal.h>
#include <netinet/in.h> 



#include <semaphore.h>
#include <pthread.h>


#include"network.h"

#include "t6usbdongle.h"
//#include "hdmi_t6.h"
#include "queue.h"

#define M_FMT_MJPEG     1
#define M_FMT_H264      2
#define M_FMT_YUY2      3
#define M_FMT_NV12      4

#define M_RES_4K        5
#define M_RES_1920X1080 4
#define M_RES_1280X720  3
#define M_RES_640X480   2
#define M_RES_320X240   1

#define T6_EN           1


int g_set_w = 0;
int g_set_h = 0;
int g_disp_w = 1920;
int g_disp_h = 1080;
int g_exit_program = 0;
int mysocket =0;
int mysocket_audio =0;

int g_hdmi_enable = 0;

queue_t * g_videoqueue = NULL;




void mysignal(int signo)  
{  
  printf("signeal = %d \n",signo);
 
  g_exit_program = 1; 
  closeSocket(mysocket);
  closeSocket(mysocket_audio);
}  

void initsignal(void)  
{  
  struct sigaction act, act2;  
  signal(SIGPIPE, SIG_IGN);

  act.sa_handler = mysignal;  
  act.sa_flags   = 0;  
  sigemptyset(&act.sa_mask);  
  sigaction(SIGHUP,&act,NULL);  
  sigaction(SIGINT,&act,NULL);  
  sigaction(SIGQUIT,&act,NULL);  
  sigaction(SIGILL,&act,NULL);  
  sigaction(SIGTERM,&act,NULL); 
  sigaction(SIGUSR1,&act,NULL); 
  sigaction(SIGUSR2,&act,NULL); 

}  

void* HdmiCmdHandle(void *lp)
{
    int client_socket ;
	int ret = 0;
	char cmd[32];
	
	mysocket = TcpServer_Init("10.10.10.254",GADGET_CONTROL_PORT);
	if(mysocket < 0){
		g_exit_program = 1;
		goto END_THREAD;
	}
		
	while(!g_exit_program){
		
		client_socket = TcpWaitingClient(mysocket,0);
	    if(client_socket < 0){
	        continue;
	    }
		PJUVCHDR pjuvchdr = (PJUVCHDR)cmd;
        pjuvchdr->Tag = JUVC_TAG;
		pjuvchdr->XactType = JUVC_TYPE_CONTROL;
		pjuvchdr->Flags    = JUVC_CONTROL_CAMERACTRL;
        pjuvchdr->c.CamaraCtrl.formatidx = M_FMT_MJPEG;
		pjuvchdr->c.CamaraCtrl.frameidx  = M_RES_1920X1080;
		ret = TcpWrite(client_socket,cmd,32);
		if(ret < 0){
			printf("sokect write failed = %d \n",ret);
			closeSocket(client_socket);
			continue;
		}
		
		while(1){
			ret = TcpRead(client_socket,cmd,32);
			if(ret <= 0){
				printf("sokect read failed = %d \n",ret);
				closeSocket(client_socket);
				break;
			}

		}
			
		
	}

	
END_THREAD:
	printf("exit HdmiCmdHandle thread \n");


}

void *HdmiT6Disp(void *lp)
{
    char edid[256];
	int  hdmi_en = 0;
	int  hdmi_en_prv = 0;
	int  ret = 0; 
	while(!g_exit_program){

		if(g_hdmi_enable == 1){
			sleep(1);
			continue;
		}
		
		
	    ret = openT6dev();
		if(ret <= 0){
			printf("T6 open faild \n");
			sleep(1);
			continue;
		}

		hdmi_en = t6_libusb_get_monitorstatus(0);
		printf("hdmi state = %d \n",hdmi_en);
		
		if(hdmi_en == 0){
			sleep(1);
			closeT6evd();
			continue;
		}
		

		ret = t6_libusb_set_resolution(g_disp_w,g_disp_h);
		if(ret <0){
			printf("t6_libusb_set_resolution faild \n");
			closeT6evd();
			sleep(1);
			continue;
		}
		g_hdmi_enable = 1;
	
	}
	t6_libusb_set_monitor_power(0);
	closeT6evd();
    printf("Leave thread HdmiT6Disp\n");

}
void* AudioPacket2T6Tcp(void *lp)
{

	int client_socket ;
	int ret = 0;
	char request[1920];
	
	mysocket_audio= TcpServer_Init("10.10.10.254",GADGET_MIC_PORT);
	if(mysocket_audio < 0){
		g_exit_program = 1;
		goto END_THREAD;
	}
		
	while(!g_exit_program){
		/*
		client_socket = TcpWaitingClient(mysocket_audio,0);
	    if(client_socket < 0){
	        continue;
	    }
	
		
		while(1){
			
			ret = TcpRead(client_socket,request,1920);
			if(ret <= 0){
				printf("audio sokect read failed = %d \n",ret);
				closeSocket(client_socket);
				break;
			}
		
            if(g_hdmi_enable == 1){
				
				ret =t6_libusb_SendAudio(request , 1920 );
				//printf("audio  size =%d \n",ret);
				if(ret < 0){
					printf("T6 audio w failed =%d \n",ret);
					audio_release();
					g_hdmi_enable = 0;
					closeT6evd();
					break;
				}
            }
		
			


		}
		*/	
		
	}
	
END_THREAD:
		printf("exit AudioPacket2T6_audio thread \n");


}

void* AudioPacket2T6UDP(void *lp)
{
	int ret;
	struct sockaddr_in from;
	struct timeval tv;
	socklen_t len;
	char request[MNSP_AUDIO_PACKET_SIZE];
   
	
	int fd=socket(AF_INET,SOCK_DGRAM,0);
	if(fd< 0)
	{
	 	printf("udp socket create error!\n");
	 	goto END_THREAD;
	}

	
	struct sockaddr_in addr;
	addr.sin_family=AF_INET;
	addr.sin_port=htons(GADGET_MIC_PORT);
	addr.sin_addr.s_addr=inet_addr("10.10.10.254");

	int n = UDP_MAX_BUFFER ;
	if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &n, sizeof(n)) == -1) {
  		printf("setsockopt SO_RCVBUF  error");
		close(fd);
		goto END_THREAD;
	}

	tv.tv_sec =  3;  
	tv.tv_usec = 0;  
	if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO,(char *)&tv,sizeof(struct timeval))== -1) {
		printf("setsockopt SO_RCVTIMEO  error");
		close(fd);
		goto END_THREAD;
		
	}
	
	ret=bind(fd,(struct sockaddr*)&addr,sizeof(addr));
	if(ret < 0)
	{
		printf("audio Bind error!\n");
		close(fd);
		goto END_THREAD;	
	}
	
	
	printf("audio Bind successfully.\n");
	while(!g_exit_program)
	{
		
	 	ret = recvfrom(fd,(void*)request,MNSP_AUDIO_PACKET_SIZE,0,(struct sockaddr*)&from,&len);
		if(ret <= 0 || ret != MNSP_AUDIO_PACKET_SIZE){
			continue;
		}
			
#if T6_EN
	
        ret =t6_libusb_SendAudio(request , MNSP_AUDIO_PACKET_SIZE );
		//printf("audio  size =%d \n",ret);
   
		if(ret < 0)
			break;
#else
		//printf("audio  size =%d \n",ret);
#endif

	
	}
	
END_THREAD:
	printf("exit AudioPacket2T6 thread \n");

}

void* VideoPacket2T6(void *lp)
{
    int ret = 0; 

	int cmdAddrr = 0;
	int fbAddr =0;
	int fbAddr1=  (58 - 8) * 1024 * 1024;
	int fbAddr2=  (58 - 4) * 1024 * 1024;
	int cmdoffset = 0;
	int resetflag =0;
	int data_len = 0;

	unsigned  long diff = 0;
    struct  timeval start;
    struct  timeval end;

	
	int video_id = 0;
	
	while(!g_exit_program){

	
		if(queue_length(g_videoqueue) <= 0){
			usleep(10000);
			continue;
		}
			
        PVIDEO_FRAME image_data =(PVIDEO_FRAME*)queue_remove(g_videoqueue);
	    if(image_data == NULL){
			usleep(10000);
			continue;
	    }
		
	    if(g_hdmi_enable == 1){

			if(video_id == 0){
	 			gettimeofday(&start,NULL);
				diff = 0;
			}
			
			if(fbAddr == fbAddr1)
			 	fbAddr = fbAddr2;
			else
				fbAddr = fbAddr1;

			data_len= image_data->length+ 1024 +48;

			if(data_len < 0x100000 )
				cmdoffset = 0x100000;
			else if(data_len < 0x200000)
			 	cmdoffset = 0x200000;
			else
			 	cmdoffset = 0x300000;
			 
			if(cmdAddrr + cmdoffset > fbAddr1){
			 	cmdAddrr = 0;
				resetflag = 0x80;
			}else{
			 	resetflag = 0;  
			}
		
			
			ret = t6_libusb2_FilpJpegFrame422(image_data->buf,image_data->length,resetflag,cmdAddrr,fbAddr,g_disp_w,g_disp_h);
		
	
			if(ret< 0){
				g_hdmi_enable = 0;
				free_video_frame(image_data);
				closeT6evd();
				continue;
			}
			video_id++;
			cmdAddrr = cmdAddrr +cmdoffset;
			gettimeofday(&end,NULL);
			diff = 1000000 * (end.tv_sec-start.tv_sec)+ end.tv_usec-start.tv_usec;
		    if(diff >= 1000000){
				printf("sec = %d frame = %d  \n",diff,video_id );
				video_id =0;
			
		    }
	    }

		free_video_frame(image_data);
    
	}
	g_exit_program =1;
    printf("exit VideoPacket2T6 thread \n");

}


void* VideoPacket2Queue(void *lp)
{
	int ret;
	struct sockaddr_in from;
	struct timeval tv;
	socklen_t len;
	char request[MNSP_MAX_MCAST_PACKET_SIZE];
    PJUVCHDR puvchdr = NULL;
	int total_len = 0;
	int image_size = 0;
	int packet_id = 0;
	PVIDEO_FRAME image_data =NULL;
	char* ptr_image=NULL;

   

	int fd=socket(AF_INET,SOCK_DGRAM,0);
	if(fd< 0)
	{
	 	printf("udp socket create error!\n");
	 	goto END_THREAD;
	}

	
	struct sockaddr_in addr;
	addr.sin_family=AF_INET;
	addr.sin_port=htons(GADGET_CAMERA_PORT);
	addr.sin_addr.s_addr=inet_addr("10.10.10.254");

	int n = UDP_MAX_BUFFER ;
	if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &n, sizeof(n)) == -1) {
  		printf("setsockopt SO_RCVBUF  error");
		close(fd);
		goto END_THREAD;
	}

	tv.tv_sec =  3;  
	tv.tv_usec = 0;  
	if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO,(char *)&tv,sizeof(struct timeval))== -1) {
		printf("setsockopt SO_RCVTIMEO  error");
		close(fd);
		goto END_THREAD;
		
	}
	
	ret=bind(fd,(struct sockaddr*)&addr,sizeof(addr));
	if(ret < 0)
	{
		printf("video Bind error!\n");
		close(fd);
		goto END_THREAD;	
	}
	
	
	printf("video Bind successfully.\n");
	while(!g_exit_program)
	{
	
	 	ret = recvfrom(fd,(void*)request,MNSP_MAX_MCAST_PACKET_SIZE,0,(struct sockaddr*)&from,&len);
		if(ret <= 0 ){
			continue;
		}
		puvchdr = (PJUVCHDR)request;
		if(puvchdr->Tag != JUVC_TAG)
			continue;
		if(puvchdr->XactOffset == 0){
			if(total_len  > 0){
               // printf("image loss rlen =%d total len = %d \n",total_len ,image_size);
				total_len = 0;
			    if(image_data != NULL){
					free_video_frame(image_data);
				}
			}
			
			packet_id = puvchdr->XactId;
			image_size = puvchdr->TotalLength;
			if(image_size <= 0){
				continue;
			}
			
            image_data = allocate_video_frame(image_size);
			if (image_data  == NULL) {
				continue;
			}
			
			
			ptr_image = image_data->buf + puvchdr->XactOffset;
			memcpy(ptr_image,&request[32],puvchdr->PayloadLength);
			total_len +=  puvchdr->PayloadLength;
				
		}else if(packet_id == puvchdr->XactId){

			if( puvchdr->PayloadLength >0 ){
				int relen = puvchdr->XactOffset + puvchdr->PayloadLength;
				if( relen <= image_size){
					ptr_image = image_data->buf + puvchdr->XactOffset;
					memcpy(ptr_image,&request[32],puvchdr->PayloadLength);
					total_len +=  puvchdr->PayloadLength;
				}
			}
		}
		
		if(total_len == image_size ){
			
			if(queue_length(g_videoqueue) < 10){
				queue_add(g_videoqueue,image_data);
			}else{
				free_video_frame(image_data);
			}
		
         
			total_len = 0;
       
		}
		
		
	}
	
	
END_THREAD:
	printf("exit VideoPacket2Queue thread \n");

}


int main(int argc ,char* avg[])
{
    int ret =0;
	pthread_t hdmi_audio;
    pthread_t hdmi_video;
	pthread_t hdmi_video2;
	pthread_t hdmi_cmd;
	pthread_t hdmi_t6;
    pthread_attr_t attr1;
	pthread_attr_t attr2;
	struct sched_param param;
	struct sched_param param2;
	
	if(argc == 3){
		g_set_w=atoi(avg[1]);
        g_set_h=atoi(avg[2]);
		printf("set_w = %d , set_h= %d \n",g_set_w,g_set_h);
	}
	initsignal();
	
	g_videoqueue = queue_create();
    
	if (pthread_create(&hdmi_t6,NULL,HdmiT6Disp,NULL) != 0) {
			printf("Error creating HdmiCmdHandle\n");
			return -1;
	}
	
	if (pthread_create(&hdmi_cmd,NULL,HdmiCmdHandle,NULL) != 0) {
		printf("Error creating HdmiCmdHandle\n");
		return -1;
	}

	if (pthread_create(&hdmi_audio,NULL,AudioPacket2T6Tcp,NULL) != 0) {
		printf("Error creating AudioPacket2T6\n");
		return -1;
	}

	
	if (pthread_create(&hdmi_video,NULL,VideoPacket2Queue,NULL) != 0) {
		printf("Error creating VideoPacket2T6\n");
		return -1;
	}

	if (pthread_create(&hdmi_video2,NULL,VideoPacket2T6,NULL) != 0) {
		printf("Error creating VideoPacket2T6\n");
		return -1;
	}

	pthread_join(hdmi_t6,   NULL); // 等待子執行緒執行完成
	pthread_join(hdmi_cmd,   NULL); // 等待子執行緒執行完成
	pthread_join(hdmi_audio,   NULL); // 等待子執行緒執行完成
	pthread_join(hdmi_video,   NULL); // 等待子執行緒執行完成
	pthread_join(hdmi_video2,   NULL); // 等待子執行緒執行完成
	
	releses_queue(g_videoqueue);
	queue_destroy(g_videoqueue);
	
END_MAIN:	
	
	printf("Leave main \n");
	kill(getpid(), SIGKILL);
	
  
}


