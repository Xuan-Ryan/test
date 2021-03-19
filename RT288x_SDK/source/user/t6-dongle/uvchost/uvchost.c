/*
 *  V4L2 video capture example
 *
 *  This program can be used and distributed without restrictions.
 *
 *      This program is provided with the V4L2 API
 * see http://linuxtv.org/docs.php for more information
 */

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

#include <linux/videodev2.h>


#include "alsauvc.h"
#include"network.h"

#include "uvchost.h"

#include "uvcdescription.h"


int g_exit_program = 0;
struct uvcdev g_udev;
struct  timeval start;
struct  timeval end;
        


void mysignal(int signo)  
{  
  printf("signeal = %d \n",signo);
  if(signo == SIGUSR1){
    printf("video start/stop \n");
	return;
  }
  g_exit_program = 1; 
  g_udev.audio_webcam_cap_active= 0;
  g_udev.audio_webcam_cap_run= 0;
  g_udev.audio_dev_cap_active= 0;
  g_udev.audio_dev_cap_run= 0;
  g_udev.audio_thread_run = 0;
  g_udev.audio_active = 0;
  g_udev.video_thread_run = 0;
  g_udev.video_active = 0;
  g_udev.cmd_thread_run = 0;
  g_udev.cmd_active= 0;
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

static void errno_exit(const char *s)
{
        fprintf(stderr, "%s error %d, %s\n", s, errno, strerror(errno));
        exit(EXIT_FAILURE);
}

static int xioctl(int fh, int request, void *arg)
{
        int r;

        do {
                r = ioctl(fh, request, arg);
        } while (-1 == r && EINTR == errno);

        return r;
}

static void print_qctrl(int fd, struct v4l2_queryctrl *queryctrl,
		struct v4l2_ext_control *ctrl, int show_menus)
{
	struct v4l2_querymenu qmenu;
	//std::string s = name2var(queryctrl->name);
	int i;

	memset(&qmenu, 0, sizeof(qmenu));
	qmenu.id = queryctrl->id;
	//printf("id = %x \n",queryctrl->id);
	switch (queryctrl->type) {
	case V4L2_CTRL_TYPE_INTEGER:
		printf("%s (int)    : min=%d max=%d step=%d default=%d value=%d",
				queryctrl->name,
				queryctrl->minimum, queryctrl->maximum,
				queryctrl->step, queryctrl->default_value,
				ctrl->value);
		break;
	case V4L2_CTRL_TYPE_INTEGER64:
		printf("%s (int64)  : value=%lld", queryctrl->name, ctrl->value64);
		break;
	case V4L2_CTRL_TYPE_STRING:
		printf("%s (str)    : min=%d max=%d step=%d value='%s'",
				queryctrl->name,
				queryctrl->minimum, queryctrl->maximum,
				queryctrl->step, ctrl->string);
		break;
	case V4L2_CTRL_TYPE_BOOLEAN:
		printf("%s (bool)   : default=%d value=%d",
				queryctrl->name,
				queryctrl->default_value, ctrl->value);
		break;
	case V4L2_CTRL_TYPE_MENU:
		printf("%s (menu)   : min=%d max=%d default=%d value=%d",
				queryctrl->name,
				queryctrl->minimum, queryctrl->maximum,
				queryctrl->default_value, ctrl->value);
		break;
	
		break;
	case V4L2_CTRL_TYPE_BUTTON:
		printf("%s (button) :", queryctrl->name);
		break;
	
	default: break;
	}
	if (queryctrl->flags)
		printf(" flags=%x", queryctrl->flags);
	printf("\n");
	if (queryctrl->type == V4L2_CTRL_TYPE_MENU  && show_menus) {
		for (i = queryctrl->minimum; i <= queryctrl->maximum; i++) {
			qmenu.index = i;
			if (xioctl(fd, VIDIOC_QUERYMENU, &qmenu))
				continue;
			if (queryctrl->type == V4L2_CTRL_TYPE_MENU)
				printf("\t\t\t\t%d: %s\n", i, qmenu.name);
			//else
			//	printf("\t\t\t\t%d: %lld (0x%llx)\n", i, qmenu.value, qmenu.value);
		}
	}
}


static int print_control(int fd, struct v4l2_queryctrl qctrl, int show_menus)
{
	struct v4l2_control ctrl;
	struct v4l2_ext_control ext_ctrl;
	struct v4l2_ext_controls ctrls;

	memset(&ctrl, 0, sizeof(ctrl));
	memset(&ext_ctrl, 0, sizeof(ext_ctrl));
	memset(&ctrls, 0, sizeof(ctrls));
	if (qctrl.flags & V4L2_CTRL_FLAG_DISABLED)
		return 1;
	if (qctrl.type == V4L2_CTRL_TYPE_CTRL_CLASS) {
		printf("1 \n%s\n\n", qctrl.name);
		return 1;
	}
	ext_ctrl.id = qctrl.id;
	if ((qctrl.flags & V4L2_CTRL_FLAG_WRITE_ONLY) ||
	    qctrl.type == V4L2_CTRL_TYPE_BUTTON) {
		print_qctrl(fd, &qctrl, &ext_ctrl, show_menus);
		return 1;
	}
	ctrls.ctrl_class = V4L2_CTRL_ID2CLASS(qctrl.id);
	ctrls.count = 1;
	ctrls.controls = &ext_ctrl;
	if (qctrl.type == V4L2_CTRL_TYPE_INTEGER64 ||
	    qctrl.type == V4L2_CTRL_TYPE_STRING ||
	    (V4L2_CTRL_ID2CLASS(qctrl.id) != V4L2_CTRL_CLASS_USER &&
	     qctrl.id < V4L2_CID_PRIVATE_BASE)) {
		if (qctrl.type == V4L2_CTRL_TYPE_STRING) {
		    ext_ctrl.size = qctrl.maximum + 1;
		    ext_ctrl.string = (char *)malloc(ext_ctrl.size);
		    ext_ctrl.string[0] = 0;
		}
		if (xioctl(fd, VIDIOC_G_EXT_CTRLS, &ctrls)) {
			printf("error %d getting ext_ctrl %s\n",
					errno, qctrl.name);
			return 0;
		}
	}
	else {
		ctrl.id = qctrl.id;
		if (xioctl(fd, VIDIOC_G_CTRL, &ctrl)) {
			printf("error %d getting ctrl %s\n",
					errno, qctrl.name);
			return 0;
		}
		ext_ctrl.value = ctrl.value;
	}
	print_qctrl(fd, &qctrl, &ext_ctrl, show_menus);
	if (qctrl.type == V4L2_CTRL_TYPE_STRING)
		free(ext_ctrl.string);
	return 1;
}


void get_control_list(int fd,union Vc_Processing* vcp,union Vc_Input* vci)
{
	struct v4l2_queryctrl qctrl;
	int id;
	
	
	memset(&qctrl, 0, sizeof(qctrl));
	qctrl.id = V4L2_CTRL_FLAG_NEXT_CTRL;
	while (xioctl(fd, VIDIOC_QUERYCTRL, &qctrl) == 0) {
		if (qctrl.flags & V4L2_CTRL_FLAG_DISABLED)
			continue;	
		 printf("qctrl.id = %x \n",qctrl.id);

        if(qctrl.id == V4L2_CID_BRIGHTNESS) 
			vcp->Processing.Brightness = 1;
        if(qctrl.id == V4L2_CID_CONTRAST) 
			vcp->Processing.Contrast= 1;
		if(qctrl.id == V4L2_CID_HUE) 
			vcp->Processing.Hue= 1;
		if(qctrl.id == V4L2_CID_SATURATION) 
			vcp->Processing.Saturation = 1;
		if(qctrl.id == V4L2_CID_SHARPNESS) 	
			vcp->Processing.Sharpness= 1;
        if(qctrl.id == V4L2_CID_GAMMA) 
			vcp->Processing.Gamma= 1;
		if(qctrl.id == V4L2_CID_BACKLIGHT_COMPENSATION) 
			vcp->Processing.Backlight_Compensation= 1;
		if(qctrl.id == V4L2_CID_GAIN) 
			vcp->Processing.Gain = 1;
		if(qctrl.id == V4L2_CID_POWER_LINE_FREQUENCY) 
			vcp->Processing.Power_line_Frequency = 1;
		if(qctrl.id == V4L2_CID_HUE_AUTO) 
			vcp->Processing.Hue_Auto= 1;
		if(qctrl.id == V4L2_CID_EXPOSURE_AUTO) 
			vci->Input.Auto_Exposure_mode = 1;
		if(qctrl.id == V4L2_CID_EXPOSURE_AUTO_PRIORITY) 
			vci->Input.Auto_Exposure_Priority = 1;
		if(qctrl.id == V4L2_CID_EXPOSURE_ABSOLUTE) 
		    vci->Input.Exposure_Time_Absolute = 1;
		if(qctrl.id == V4L2_CID_WHITE_BALANCE_TEMPERATURE) 
			vcp->Processing.White_Balance_Temperature = 1;
		if(qctrl.id == V4L2_CID_AUTO_WHITE_BALANCE)
			vcp->Processing.White_Balance_Temperature_Auto = 1;
		if(qctrl.id == V4L2_CID_BLUE_BALANCE)
			vcp->Processing.White_Balance_Component = 1;  // offset = 0;
		if(qctrl.id == V4L2_CID_RED_BALANCE)
			vcp->Processing.White_Balance_Component = 1;  //offset = 16; 
		if(qctrl.id == V4L2_CID_FOCUS_ABSOLUTE) 
			vci->Input.Focus_Absolute = 1;
		if(qctrl.id == V4L2_CID_FOCUS_AUTO)
			vci->Input.Focus_Auto = 1;
		if(qctrl.id == V4L2_CID_IRIS_ABSOLUTE) 	
			vci->Input.Iris_Absolute = 1;
		if(qctrl.id == V4L2_CID_IRIS_RELATIVE)
			vci->Input.Iris_Relative= 1;
		if(qctrl.id == V4L2_CID_ZOOM_ABSOLUTE) 
			vci->Input.Zoom_Absolute = 1;
		if(qctrl.id == V4L2_CID_ZOOM_CONTINUOUS) 
			vci->Input.Zoom_Relative= 1;
		if(qctrl.id == V4L2_CID_PAN_ABSOLUTE) 
			vci->Input.PanTilt_Absolute= 1 ;          //  offset = 0
		if(qctrl.id == V4L2_CID_TILT_ABSOLUTE)  
			vci->Input.PanTilt_Absolute = 1 ;          //offset = 32
		if(qctrl.id == V4L2_CID_PRIVACY) 
	        vci->Input.Reserved = 0;
		
		qctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
		//printf("2 qctrl.id = %x \n",qctrl.id);	
		
	}


}

static void list_controls(int fd, int show_menus)
{
	struct v4l2_queryctrl qctrl;
	int id;
    printf("method 1 \n") ;
	memset(&qctrl, 0, sizeof(qctrl));
	qctrl.id = V4L2_CTRL_FLAG_NEXT_CTRL;
	while (xioctl(fd, VIDIOC_QUERYCTRL, &qctrl) == 0) {
			print_control(fd, qctrl, show_menus);
		//printf("1 qctrl.id = %x \n",qctrl.id);	
		qctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
		//printf("2 qctrl.id = %x \n",qctrl.id);	
		
	}
	
	//printf("%s \n",qctrl.name);
	if (qctrl.id != V4L2_CTRL_FLAG_NEXT_CTRL)
		return;

    printf("method 2 \n") ;
	for (id = V4L2_CID_USER_BASE; id < V4L2_CID_LASTP1; id++) {
		qctrl.id = id;
		if (xioctl(fd, VIDIOC_QUERYCTRL, &qctrl) == 0)
			print_control(fd, qctrl, show_menus);
	}
	 printf("method 3 \n") ;
	for (qctrl.id = V4L2_CID_PRIVATE_BASE;
			xioctl(fd, VIDIOC_QUERYCTRL, &qctrl) == 0; qctrl.id++) {
		print_control(fd, qctrl, show_menus);
	}

	printf("list_controls \n");		

}

int v4l2_get_control_info(int fd ,char* data)
{
    int ret = 0;
	struct v4l2_direct_control dcmd;
	memset(&dcmd,0,sizeof(dcmd));
	dcmd.mode = 1;
	dcmd.data = data;
	ret =xioctl(fd, VIDIOC_DR_GET, &dcmd);
	return ret;
}

int v4l2_control_transfer(int fd ,unsigned char bmRequestType,unsigned char bRequest,unsigned short wvalue,
	unsigned short windex,unsigned char* data, unsigned short wlength)
{
    struct v4l2_direct_control dcmd;
	int ret = 0;
    if(wlength > 4096)
		return -1;
    dcmd.mode = 0;
	dcmd.bmRequestType = bmRequestType;
	dcmd.bRequest = bRequest;
    dcmd.wValue = wvalue;
	dcmd.wIndex = windex;
	dcmd.wLength = wlength;
	dcmd.data = data;
	if(bmRequestType & 0x80)
		ret =xioctl(fd, VIDIOC_DR_GET, &dcmd);
	else
		ret =xioctl(fd, VIDIOC_DR_SET, &dcmd);
	return ret;

}




int v4l2_get_control(int fd, int cid)
{
	struct v4l2_queryctrl qctrl = { .id = cid };
	struct v4l2_control ctrl = { .id = cid };
	int retval = 0;
	
	if(xioctl(fd, VIDIOC_QUERYCTRL, &qctrl) == -1){
		printf("error %d getting ctrl %s\n",
				errno, qctrl.name);
		return -1;

	}

	if (qctrl.flags & V4L2_CTRL_FLAG_DISABLED) {
		errno = EINVAL;
		return -1;
	}

	if (xioctl(fd, VIDIOC_G_CTRL, &ctrl) == 1) {
		printf("error %d getting ctrl %s\n",
				errno, qctrl.name);
		return -1;
	}

	return ctrl.value;
}

int v4l2_set_control(int fd, int cid, int value)
{

	
	struct v4l2_queryctrl qctrl = { .id = cid };
	struct v4l2_control ctrl = { .id = cid , .value = value};

	int retval = 0;
	
	if(xioctl(fd, VIDIOC_QUERYCTRL, &qctrl) == -1){
		printf("error %d getting ctrl %s\n",
				errno, qctrl.name);
		return -1;

	}

	if (qctrl.flags & V4L2_CTRL_FLAG_DISABLED) {
		errno = EINVAL;
		return -1;
	}

	if (xioctl(fd, VIDIOC_S_CTRL, &ctrl) == 1) {
		printf("error %d getting ctrl %s\n",
				errno, qctrl.name);
		return -1;
	}
	return 0;	


}
int GetDevVidPid(int fd ,uint16_t* vid ,uint16_t* pid )
{
	int ret = 0 ;
	char cmd[18];
	if(fd <= 0){
		return -1;  
    }
	ret =v4l2_control_transfer(fd ,0x80,0x06,0x0100,0,cmd,18);
	if(ret <= 0){
		return -1;  
	}
	memcpy(vid,&cmd[8],2);
	memcpy(pid,&cmd[10],2);
	return ret;
}

int GetDevConfigDesc(int fd ,char* desc)
{
    int ret = 0;
	unsigned short wTotalLength = 0;
	char cofig[9];
    if(fd <= 0){
		return -1;  
    }
	ret =v4l2_control_transfer(fd ,0x80,0x06,0x0200,0,cofig,9);
	if(ret <= 0){
		return -1;  
	}	
    memcpy(&wTotalLength,&cofig[2],2);
	printf("wTotalLength = %d \n",wTotalLength);
	if(wTotalLength > 4096)
		wTotalLength = 4096;
	ret =v4l2_control_transfer(fd ,0x80,0x06,0x0200,0,desc,wTotalLength);
	if(ret <= 0){
		return -1;  
	}
	return ret;

}


int GetDevString(int fd ,char* sMf ,char* sPt ,char* des)
{
	char cmd[256];
	int ret = 0;
	unsigned char  iManufacturer;
    unsigned char  iProduct;
	unsigned short windex =0;
    if(fd <= 0){
		return -1;  
    }
	
	ret =v4l2_control_transfer(fd ,0x80,0x06,0x0100,0,cmd,18);
	if(ret <= 0){
		return -1;  
	}	
	printf("ret = %d \n",ret);
	hex_dump(cmd,ret,"dev descrip");
	memcpy(des,&cmd[8],4);
	iManufacturer = cmd[14];
	iProduct = cmd[15];	
	ret =v4l2_control_transfer(fd ,0x80,0x06,0x0300,0,cmd,255);
    if(ret <= 0){
		return -1;  
	}	
	printf("ret = %d \n",ret);
	hex_dump(cmd,ret,"string id"); 
	windex = 0x0300 | iManufacturer;
    ret =v4l2_control_transfer(fd ,0x80,0x06,windex,0x0409,cmd,255);
    if(ret <= 0){
		return -1;  
	}	
	printf("ret = %d \n",ret);
	hex_dump(cmd,ret,"Manufacturer string descrip");
	memcpy(sMf,cmd,ret);
	windex = 0x0300 | iProduct;
    ret =v4l2_control_transfer(fd ,0x80,0x06,windex,0x0409,cmd,255);
    if(ret <= 0){
		return -1;  
	}	
	printf("ret = %d \n",ret);
	hex_dump(cmd,ret,"Product string descrip"); 
	memcpy(sPt,cmd,ret);
	return ret;

}

static void process_image(char* id ,const void *p, int size)
{
     printf("id = %d image size = %d \n",id,size);
     
	 
	 
	 /*
	 char  pcmname[256]="/home/pic.yuyv";
	 FILE *fp=fopen(pcmname,"wb");

	 fwrite(p, size, 1, fp);
	 fflush(fp);
	 fclose(fp);
	 */
	 
	 
}

static int read_frame(struct uvcdev *udev )
{
	struct v4l2_buffer buf;
	unsigned int i;
    int ret;
	char cmd[256];

	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;

    if (-1 == xioctl(udev->fd, VIDIOC_DQBUF, &buf)) {
          
        printf("Error VIDIOC_DQBUF \n");
		return -1;
         
    }

    if(buf.index > udev->n_buffers){
		printf("overflow index = %d \n",buf.index);
		return -1;
    }
/*
    if(udev->format == V4L2_PIX_FMT_MJPEG){
	    if(udpImageWrite(udev->udpsocket,"10.10.10.254",GADGET_CAMERA_PORT,udev->video_id++,udev->buffers[buf.index].start, buf.bytesused)<0){
			printf("udpImageWrite error \n");
			return -1;
	    }
    }else if(udev->format == V4L2_PIX_FMT_H264){
		//int len = buf.bytesused;
		if(TcpWrite(udev->hsocket,(char *)&buf.bytesused,4) <= 0){
			printf("TcpWrite len error \n");
			return -1;
		}
		if(TcpWrite(udev->hsocket,(char *)udev->buffers[buf.index].start,buf.bytesused) <= 0){
			printf("TcpWrite data error \n");
			return -1;
		}
		

    }
 */   

  
	if(udpImageWrite(udev->udpsocket,"10.10.10.254",GADGET_CAMERA_PORT,udev->video_id++,udev->buffers[buf.index].start, buf.bytesused)<0){
		printf("udpImageWrite error \n");
		return -1;
	}

	//process_image(&udev->video_id,udev->buffers[buf.index].start, buf.bytesused);

	if (-1 == xioctl(udev->fd, VIDIOC_QBUF, &buf)){
	    printf("Error VIDIOC_QBUF \n");
		return -1;
	}
	return 1;
              
}

static int readframe(struct uvcdev *udev)
{
        
    int ret = 0;          
	fd_set fds;
	struct timeval tv;
	int r;

	FD_ZERO(&fds);
	FD_SET(udev->fd, &fds);

	/* Timeout. */
	tv.tv_sec = 2;
	tv.tv_usec = 0;

	r = select(udev->fd + 1, &fds, NULL, NULL, &tv);

	if ( r < 0) {
	   printf("read error  \n")  ;
	   return -1;
	}else if ( r ==0) {
	   printf("read time out \n")  ;
	   return -1;
	}
   
    ret = read_frame(udev) ;
    return ret;   
      
}

int stop_capturing(struct uvcdev *udev)
{
    enum v4l2_buf_type type;
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (-1 == xioctl(udev->fd, VIDIOC_STREAMOFF, &type)){
            printf("Error VIDIOC_STREAMOFF \n");
			return -1;
    }
	return 1;
               
}

int start_capturing(struct uvcdev *udev)
{
    unsigned int i;
    enum v4l2_buf_type type;

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (-1 == xioctl(udev->fd, VIDIOC_STREAMON, &type)){
            printf("Error VIDIOC_STREAMON \n");
			return -1;

    }

	return 0;
             
}

static void uninit_mmap(struct uvcdev *udev)
{
    unsigned int i;
    for (i = 0; i < udev->n_buffers; ++i)
            if (-1 == munmap(udev->buffers[i].start, udev->buffers[i].length))
                    printf("error munmap");
          
    free(udev->buffers);
}



int init_mmap(struct uvcdev *udev)
{
	struct v4l2_requestbuffers req;
	int i = 0;
	int n = 2;	
	CLEAR(req);
	//if(udev->w == 3842 && udev->h ==2160)
	//	n = 1;

	req.count = n;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;

	if (-1 == xioctl(udev->fd, VIDIOC_REQBUFS, &req)) {
	   
	   printf("Error VIDIOC_REQBUFS \n");
	   return -1;

	}

	if (req.count < 2) {
	   fprintf(stderr, "Insufficient buffer memory on \n");
	   return -1;
	}

	udev->buffers = calloc(req.count, sizeof(struct buffer));

	if (! udev->buffers) {
		   fprintf(stderr, "Out of memory\n");
		   return -1;
	}

	for (udev->n_buffers = 0; udev->n_buffers < req.count; ++udev->n_buffers) {
		   struct v4l2_buffer buf;

		   CLEAR(buf);

		   buf.type 	   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		   buf.memory	   = V4L2_MEMORY_MMAP;
		   buf.index	   = udev->n_buffers;

		   if (-1 == xioctl(udev->fd, VIDIOC_QUERYBUF, &buf)){
				  printf("error VIDIOC_QUERYBUF \n");
				  return -1;

		   }	   
           ///printf("buf.length = %d \n",buf.length);
		   udev->buffers[udev->n_buffers].length = buf.length;
		   udev->buffers[udev->n_buffers].start =
				   mmap(NULL /* start anywhere */,
						 buf.length,
						 PROT_READ  /* required */,
						 MAP_SHARED /* recommended */,
						 udev->fd, buf.m.offset);

		   if (MAP_FAILED == udev->buffers[udev->n_buffers].start){
				   printf("error mmap \n");
				   return -1;

		   }
	}

	for (i = 0; i < udev->n_buffers; ++i) {
		
		struct v4l2_buffer buf;
		CLEAR(buf);
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;

		if (-1 == xioctl(udev->fd, VIDIOC_QBUF, &buf)){
		       printf("Error VIDIOC_QBUF \n");
			   return -1;

		}		
    }
	return 1;

}





int enum_frame_intervals(int fd,int pix_fmt, const int width, const int height)          //! 枚舉幀間隔
{
     int ret;
     struct v4l2_frmivalenum fival;

     memset(&fival, 0, sizeof(struct v4l2_frmivalenum));

     fival.index = 0;                                                                     //! 設置對應序號
     fival.pixel_format = pix_fmt;                                                        //! 設置對應要查詢的格式
     fival.width = width;                                                                 //! 設置寬度
     fival.height = height;                                                               //! 設置高度
     
     printf("\tTime  interval between frame");

     while((ret = xioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &fival)) == 0)                 //! 根據設置內容輪詢幀間隔
     {
	 	 if(fival.type == V4L2_FRMIVAL_TYPE_DISCRETE)
		 {
	             printf("type:DISCRETE %u/%u, ",fival.discrete.numerator,  fival.discrete.denominator);
		 }
		 else if (fival.type == V4L2_FRMIVAL_TYPE_CONTINUOUS)
	         {
		     printf("type:CONTINUOUS {min { %u/%u } .. max { %u/%u } }, ",
	             fival.stepwise.min.numerator, fival.stepwise.min.numerator,
	             fival.stepwise.max.denominator, fival.stepwise.max.denominator);
		     break;
	    	 }        
		 else if (fival.type == V4L2_FRMIVAL_TYPE_STEPWISE)
		 {			             
	            printf("type:STEPWISE {min { %u/%u } .. max { %u/%u } / ""stepsize { %u/%u } }, ",
	            fival.stepwise.min.numerator, fival.stepwise.min.denominator,
	            fival.stepwise.max.numerator, fival.stepwise.max.denominator,
		    fival.stepwise.step.numerator, fival.stepwise.step.denominator);
		    break;
		 }

		 fival.index++;
     }
     printf("\n\n");
     /*
     if(ret != 0 && errno != EINVAL)
     {
		 printf("ERROR enumerating frame intervals:  %d\n",errno);
		 return errno;
     }
     */
     return  0;

}

int enum_frame_sizes(int fd ,const int pix_fmt)                                           //! 枚舉幀大小
{
    int ret;
    struct v4l2_frmsizeenum fsize;

    memset(&fsize, 0, sizeof(struct v4l2_frmsizeenum));
    fsize.index = 0;                                                                      //! 要查詢所支持的幀大小的對應序號  
    fsize.pixel_format = pix_fmt;                                                         //! 根據他支持的格式去查詢支持幀的大小
    
    while((ret = xioctl(fd, VIDIOC_ENUM_FRAMESIZES, &fsize))  == 0)
    {
		if(fsize.type == V4L2_FRMSIZE_TYPE_DISCRETE)                                      //! 網上查詢說UVC驅動固定的這種類型  
		{
		    printf("{ discrete:  width  = %u, height = %u}\n",
				    fsize.discrete.width, fsize.discrete.height);
	            
	            ret = enum_frame_intervals(fd,pix_fmt, fsize.discrete.width,                     //! 枚舉幀間隔 
	                                       fsize.discrete.height);
	            if(ret != 0)
	            { 
	                printf("Unable to enumerate  frame sizes.\n");
	            } 
		} 
        else if (fsize.type == V4L2_FRMSIZE_TYPE_CONTINUOUS)
        {
            printf("{ continuous: min { width = %u, height = %u } .. "
                   "max { width = %u, height = %u } }\n",
                   fsize.stepwise.min_width, fsize.stepwise.min_height,
                   fsize.stepwise.max_width, fsize.stepwise.max_height);
            printf("  Refusing to enumerate frame intervals.\n");
            break;
        }
        else if (fsize.type == V4L2_FRMSIZE_TYPE_STEPWISE)
        {
            printf("{ stepwise: min { width = %u, height = %u } .. "
                   "max { width = %u, height = %u } / "
                   "stepsize { width = %u, height = %u } }\n",
                   fsize.stepwise.min_width, fsize.stepwise.min_height,
                   fsize.stepwise.max_width, fsize.stepwise.max_height,
                   fsize.stepwise.step_width, fsize.stepwise.step_height);
            printf("  Refusing to enumerate frame intervals.\n");
            break;
        }
        fsize.index++;
    }
  
    if(ret != 0 && errno != EINVAL)
    {
		printf("ERROR enumerating frame size: %d\n", errno);
		return errno;
    }

    return 0;

}



static void enum_device(int fd)
{
    int ret;
	struct v4l2_fmtdesc fmt;
	memset(&fmt, 0, sizeof(fmt));
	fmt.index = 0;
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	while ((ret = xioctl(fd, VIDIOC_ENUM_FMT, &fmt)) == 0)
	{
	        fmt.index++;
			printf("pixelformat = %x \n",fmt.pixelformat);
	        printf("{ pixelformat = ''%c%c%c%c'', description = ''%s'' }\n",
	                  fmt.pixelformat & 0xFF, (fmt.pixelformat >> 8) & 0xFF, (fmt.pixelformat >> 16) & 0xFF,
	                  (fmt.pixelformat >> 24) & 0xFF, fmt.description);
			ret =  enum_frame_sizes(fd,fmt.pixelformat);
			if(ret != 0)
			{
			    printf("Unable to enumerate frame sizes.\n");
			}
	}

}


void enum_uvc_frame_intervals(int fd,int pix_fmt, const int width, const int height ,struct Resolution *res)          //! 枚舉幀間隔
{
     int ret;
     struct v4l2_frmivalenum fival;

     memset(&fival, 0, sizeof(struct v4l2_frmivalenum));

     fival.index = 0;                                                                     //! 設置對應序號
     fival.pixel_format = pix_fmt;                                                        //! 設置對應要查詢的格式
     fival.width = width;                                                                 //! 設置寬度
     fival.height = height;                                                               //! 設置高度
     
   
     while((ret = xioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &fival)) == 0)                 //! 根據設置內容輪詢幀間隔
     {
	 	 if(fival.type == V4L2_FRMIVAL_TYPE_DISCRETE)
		 {
             //printf("type:DISCRETE %u/%u, \n",fival.discrete.numerator,  fival.discrete.denominator);
			 res->framerate= fival.discrete.denominator;
			 break;
		 }
   	}
}
void enum_uvc_frame_sizes(int fd ,const int pix_fmt ,struct Resolution_list *rl)    
{
	int ret;
    struct v4l2_frmsizeenum fsize;

    memset(&fsize, 0, sizeof(struct v4l2_frmsizeenum));
    fsize.index = 0;                                                                      //! 要查詢所支持的幀大小的對應序號  
    fsize.pixel_format = pix_fmt;  
	rl->res_number = 0;
	while((ret = xioctl(fd, VIDIOC_ENUM_FRAMESIZES, &fsize))  == 0)
    {
        
    	if(fsize.type == V4L2_FRMSIZE_TYPE_DISCRETE)                                      //! 網上查詢說UVC驅動固定的這種類型  
		{
		    //printf("{ discrete:  width  = %u, height = %u}\n",fsize.discrete.width, fsize.discrete.height);
			
			rl->res_number++;
			rl->res[fsize.index].Width = fsize.discrete.width;
			rl->res[fsize.index].Hight=  fsize.discrete.height;
			enum_uvc_frame_intervals(fd,pix_fmt,fsize.discrete.width,fsize.discrete.height,&rl->res[fsize.index]);  
    	}
		fsize.index++;

	}

}
void enum_uvc_device(int fd,struct format_list *fl)
{
	int ret;
	int i , j;
	struct v4l2_fmtdesc fmt;
	memset(&fmt, 0, sizeof(fmt));
	fmt.index = 0;
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	while ((ret = xioctl(fd, VIDIOC_ENUM_FMT, &fmt)) == 0)
	{
	    //printf("pixelformat = %x \n",fmt.pixelformat);
		if(fmt.pixelformat == FMT_MJPEG){
			
			fl->res_list[fl->index].format = 0x01;//FMT_MJPEG;
            enum_uvc_frame_sizes(fd ,fmt.pixelformat,&fl->res_list[fl->index]);  
			fl->index++;

		}else if(fmt.pixelformat == FMT_H264 ){
			
			fl->res_list[fl->index].format = 0x02;//FMT_H264;
            enum_uvc_frame_sizes(fd ,fmt.pixelformat,&fl->res_list[fl->index]); 
			fl->index++;

		}else if(fmt.pixelformat == FMT_YUY2){
			
			fl->res_list[fl->index].format = 0x03;//FMT_YUY2;
            enum_uvc_frame_sizes(fd ,fmt.pixelformat,&fl->res_list[fl->index]); 
			fl->index++;

		}else if(fmt.pixelformat == FMT_NV12){
			fl->res_list[fl->index].format = 0x04;//FMT_NV12;
            enum_uvc_frame_sizes(fd ,fmt.pixelformat,&fl->res_list[fl->index]); 
			fl->index++;
		}
		
		fmt.index++;
	}
/*
    printf("===========dump format list================= \n\n");
	printf("fl->index = %d\n",fl->index);
	for(i = 0 ; i < fl->index ; i++){
	   if(fl->res_list[i].format == 1)	
       	 printf("FMT_MJPEG \n");
	   else if(fl->res_list[i].format == 2)	
       	 printf("FMT_H264 \n");
	   else if(fl->res_list[i].format == 3)	
       	 printf("FMT_YUY2 \n");
       else if(fl->res_list[i].format == 4)	
       	 printf("FMT_NV12 \n");
	   
	   printf("fl->res_list[i].res_number = %d \n",fl->res_list[i].res_number);
       for(j = 0 ; j < fl->res_list[i].res_number ;j++){
	   	   printf("width = %d \n",fl->res_list[i].res[j].Width);
		   printf("Hight = %d \n",fl->res_list[i].res[j].Hight);
		   printf("frame rate = %d \n",fl->res_list[i].res[j].framerate);
       }
	}
*/  
	
}



int init_device(struct uvcdev *udev)
{
    struct v4l2_capability cap;
    struct v4l2_cropcap cropcap;
    struct v4l2_crop crop;
    struct v4l2_format fmt;
	struct v4l2_streamparm setfps;
	
    unsigned int min;

    if (-1 == xioctl(udev->fd, VIDIOC_QUERYCAP, &cap)) {
		printf("Error VIDIOC_QUERYCAP \n");
		return -1;
		
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
		printf("Error V4L2_CAP_VIDEO_CAPTURE \n");
		return -1;
		
    }
   
	
    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        printf("Error V4L2_CAP_STREAMING \n");
		return -1;
    }
      
 


    /* Select video input, video standard and tune here. */
    CLEAR(cropcap);
    cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (0 == xioctl(udev->fd, VIDIOC_CROPCAP, &cropcap)) {
            crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            crop.c = cropcap.defrect; /* reset to default */

            if (-1 == xioctl(udev->fd, VIDIOC_S_CROP, &crop)) {
                    switch (errno) {
                    case EINVAL:
                            /* Cropping not supported. */
                            break;
                    default:
                            /* Errors ignored. */
                            break;
                    }
            }
    } else {
            /* Errors ignored. */
    }


    CLEAR(fmt);

    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (udev->force_format) {
	
		//printf("udev->format = %x w =%d h =%d \n" ,udev->format,udev->w,udev->h);
        fmt.fmt.pix.width       = udev->w; //replace
        fmt.fmt.pix.height      = udev->h; //replace
        fmt.fmt.pix.pixelformat = udev->format  ;//V4L2_PIX_FMT_H264;//
        fmt.fmt.pix.field       = V4L2_FIELD_ANY;
        
        if (-1 == xioctl(udev->fd, VIDIOC_S_FMT, &fmt)){
                printf("Error VIDIOC_S_FMT \n");
				return -1;

        }

            /* Note VIDIOC_S_FMT may change width and height. */
    } else {
        /* Preserve original settings as set by v4l2-ctl for example */
        if (-1 == xioctl(udev->fd, VIDIOC_G_FMT, &fmt)){
                printf("Error VIDIOC_G_FMT \n");
				return -1;

        }
    }

	memset(&setfps, 0, sizeof(struct v4l2_streamparm));
	setfps.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	setfps.parm.capture.timeperframe.numerator = 1;
	setfps.parm.capture.timeperframe.denominator =30;
	if (-1 == xioctl(udev->fd, VIDIOC_S_PARM, &setfps)){
        printf("Error VIDIOC_S_PARM \n");
		return -1;

    }
		
	return 1;
           
}

void close_device(struct uvcdev *udev)
{
    if(udev->fd > 0){
        close(udev->fd);
        udev->fd = -1;
    }
}

int dect_device(void)
{
	struct stat st;
    char dev_name[256]="/dev/video0";
    if (-1 == stat(dev_name, &st)) {
            fprintf(stderr, "Cannot identify '%s': %d, %s\n",
                     dev_name, errno, strerror(errno));
            return -1;
    }
	return 0;


}

int open_device(void)
{
    int fd = 0; 
	struct stat st;
    char dev_name[256]="/dev/video0";

    if (-1 == stat(dev_name, &st)) {
            fprintf(stderr, "Cannot identify '%s': %d, %s\n",
                     dev_name, errno, strerror(errno));
            return -1;
    }
    /*
    if (!S_ISCHR(st.st_mode)) {
            fprintf(stderr, "%s is no device\n", dev_name);
            return -1;
    }
    */
    fd = open(dev_name, O_RDWR|O_NONBLOCK);

    if (-1 == fd) {
            fprintf(stderr, "Cannot open '%s': %d, %s\n",
                     dev_name, errno, strerror(errno));
            return -1;
    }

	return fd;
}





void uvcformatset( int formatindex ,int *format)
{

	switch(formatindex)
	{
		case M_FMT_MJPEG :
			*format = V4L2_PIX_FMT_MJPEG;
			break;
		case M_FMT_H264:	
            *format = V4L2_PIX_FMT_H264;
			break;
		case M_FMT_YUY2:
			*format = V4L2_PIX_FMT_YUYV;
			break;
	    case M_FMT_NV12:	
			*format = V4L2_PIX_FMT_NV12;
			break;
        defalut:
            *format = V4L2_PIX_FMT_MJPEG;
			break;

	}

}


void uvcframeset( int frameindex ,int *w ,int *h)
{

	switch(frameindex)
	{
		case M_RES_4K:
			*w = 3840;
			*h = 2160;
			break;
		case M_RES_1920X1080 :
			*w = 1920;
			*h = 1080;
			break;
		case M_RES_1280X720:	
            *w  = 1280;
			*h = 720;
			break;
		case M_RES_640X480:
			*w = 640;
			*h = 480;
			break;
	    case M_RES_320X240:	
			*w = 320;
			*h = 240;
			break;
        defalut:
            *w = 1280;
			*h = 720;
			break;

	}

}

int SendUvcInfo(struct uvcdev* pudev , struct format_list* pfl,struct uvc_ctrl_info* puci)
{
	int ret = 0;
	int fd = 0; 
	int i = 0;
    int j = 0;
	int vclen = 0;
	char cmd[32];
	char Mf[256];
	char Pt[256];
	char des[4];
    char buf[4096];
	char vc[1024];
	unsigned char   mjpeg_res_bitfield = 0;
    unsigned char   h264_res_bitfield  = 0;
    unsigned char   yuv_res_bitfield   = 0;
    unsigned char   nv12_res_bitfield  = 0;
    CLEAR(Mf);
	CLEAR(Pt);
	PJUVCHDR juvchdr =(PJUVCHDR) cmd;

  
	ret = GetDevConfigDesc(pudev->fd ,buf);
  
	if(ret <= 0){
		printf("Get dev config failed \n");
		return -1;
	}
	vclen=uvc_prase_vc(buf ,ret,vc,&pudev->uvcver);
	if(vclen > 0){
		juvchdr->Tag = JUVC_TAG;
		juvchdr->XactType = JUVC_TYPE_CONTROL;
		juvchdr->HdrSize = sizeof(JUVCHDR);
		juvchdr->XactId = 0;
		juvchdr->XactOffset = 0;
		juvchdr->Flags = JUVC_CONTROL_VC_DESC;
		juvchdr->PayloadLength = vclen;
		juvchdr->TotalLength = vclen;
		ret = TcpWrite(pudev->cmd_socket,cmd,32);
	    if(ret <= 0){
	        printf("vc cmd write sokcet failed  \n ");
			return -1;
	    }

		ret = TcpWrite(pudev->cmd_socket,vc,vclen);
	    if(ret <= 0){
	        printf("vc write sokcet failed  \n ");
			return -1;
	    }
	
	}
	
  
	if(GetDevString(pudev->fd,Mf,Pt,juvchdr->c.Rsvd) < 0){
		printf("Get dev string failed \n");
		return -1;
	}
    memcpy(&pudev->vid,&juvchdr->c.Rsvd[0],2);
	memcpy(&pudev->pid,&juvchdr->c.Rsvd[2],2);
	//printf("vid = %x \n",pudev->vid);
	//printf("pid = %x \n",pudev->pid);
	
	juvchdr->Tag = JUVC_TAG;
	juvchdr->XactType = JUVC_TYPE_CONTROL;
	juvchdr->HdrSize = sizeof(JUVCHDR);
	juvchdr->XactId = 0;
	juvchdr->XactOffset = 0;
	juvchdr->Flags = JUVC_CONTROL_DESCRIPTOR;
	juvchdr->PayloadLength = 0;
	juvchdr->TotalLength = 0;
	ret = TcpWrite(pudev->cmd_socket,cmd,32);
    if(ret <= 0){
        printf("cmd write sokcet failed  \n ");
		return -1;
    }
	
    if(Mf[0] > 0){
		int len = Mf[0];
		juvchdr->Flags = JUVC_CONTROL_MANUFACTURER;
		juvchdr->PayloadLength = len;
		juvchdr->TotalLength = len;
		ret = TcpWrite(pudev->cmd_socket,cmd,32);
	    if(ret <= 0){
	        printf("cmd write sokcet failed  \n ");
			return -1;
	    }
		ret = TcpWrite(pudev->cmd_socket,Mf,len);
	    if(ret <= 0){
	        printf("Mf write sokcet failed  \n ");
			return -1;
	    }
		
	}

    if(Pt[0] > 0){
		int len = Pt[0];
		juvchdr->Flags = JUVC_CONTROL_PRODUCT;
		juvchdr->PayloadLength = len;
		juvchdr->TotalLength = len;
		ret = TcpWrite(pudev->cmd_socket,cmd,32);
	    if(ret <= 0){
	        printf("cmd write sokcet failed  \n ");
			return -1;
	    }
		ret = TcpWrite(pudev->cmd_socket,Pt,len);
	    if(ret <= 0){
	        printf("Pt write sokcet failed  \n ");
			return -1;
	    }
		
	}

	enum_uvc_device(pudev->fd,pfl);
	
	if(pfl->index == 0){
		return -1;
	}
	
    //printf("fl->index = %d\n",pfl->index);
	for(i = 0 ; i < pfl->index ; i++){
	  
	   //printf("fl->res_list[i].res_number = %d \n",pfl->res_list[i].res_number);
       for(j = 0 ; j < pfl->res_list[i].res_number ;j++){
	   	
	   	   //printf("width = %d \n",pfl->res_list[i].res[j].Width);
		   //printf("Hight = %d \n",pfl->res_list[i].res[j].Hight);
		   //printf("frame rate = %d \n",pfl->res_list[i].res[j].framerate);
		   if(pfl->res_list[i].format == 1){	
				//printf("FMT_MJPEG \n");
				if(pfl->res_list[i].res[j].Width == 1920 && pfl->res_list[i].res[j].Hight == 1080)
				 	mjpeg_res_bitfield |= 0x08 ; 
				if(pfl->res_list[i].res[j].Width == 1280 && pfl->res_list[i].res[j].Hight == 720)
				 	mjpeg_res_bitfield |= 0x04 ;
				if(pfl->res_list[i].res[j].Width == 640 && pfl->res_list[i].res[j].Hight == 480)
				 	mjpeg_res_bitfield |= 0x02 ;
				if(pfl->res_list[i].res[j].Width == 320 && pfl->res_list[i].res[j].Hight == 240)
				 	mjpeg_res_bitfield |= 0x01 ;
		   }else if(pfl->res_list[i].format == 2){	
       	 		//printf("FMT_H264 \n");
				if(pfl->res_list[i].res[j].Width == 3840 && pfl->res_list[i].res[j].Hight == 2160)
					h264_res_bitfield  |= 0x10 ;
				if(pfl->res_list[i].res[j].Width == 1920 && pfl->res_list[i].res[j].Hight == 1080)
				 	 h264_res_bitfield |= 0x08 ;
				if(pfl->res_list[i].res[j].Width == 1280 && pfl->res_list[i].res[j].Hight == 720)
				 	 h264_res_bitfield |= 0x04 ;
				if(pfl->res_list[i].res[j].Width == 640 && pfl->res_list[i].res[j].Hight == 480)
				 	 h264_res_bitfield |= 0x02 ;
				if(pfl->res_list[i].res[j].Width == 320 && pfl->res_list[i].res[j].Hight == 240)
				 	 h264_res_bitfield |= 0x01 ;
		   }else if(pfl->res_list[i].format == 3){	
       	 		//printf("FMT_YUY2 \n");
				if(pfl->res_list[i].res[j].Width == 320 && pfl->res_list[i].res[j].Hight == 240)
				 	 yuv_res_bitfield |= 0x01 ;
		   }else if(pfl->res_list[i].format == 4){	
       	 		//printf("FMT_NV12 \n");
				if(pfl->res_list[i].res[j].Width == 320 && pfl->res_list[i].res[j].Hight == 240)
				 	 nv12_res_bitfield |= 0x01 ;
		   }
		   
       }
	}
	
	
	
    printf("mjpeg_res_bitfield = %x\n",mjpeg_res_bitfield);
	printf("h264_res_bitfield = %x\n",h264_res_bitfield);
	printf("yuv_res_bitfield = %x\n",yuv_res_bitfield);  
	printf("nv12_res_bitfield = %x\n",nv12_res_bitfield);  

	ret = v4l2_get_control_info(pudev->fd,(char*)puci);
	if(ret <= 0){
		return -1;
	}
	printf("Vc_Interface = %x\n",puci->Vc_Interface);
	printf("Input_ID = %x\n",puci->Input_ID);
	printf("Processing_ID = %x\n",puci->Processing_ID);
	//hex_dump(puci->vci.bmControl,3,"input data");
	//hex_dump(puci->vcp.bmControl,3,"process data");

	
	
	juvchdr->Tag = JUVC_TAG;
	juvchdr->XactType = JUVC_TYPE_CONTROL;
	juvchdr->HdrSize = sizeof(JUVCHDR);
	juvchdr->XactId = 0;
	juvchdr->Flags = JUVC_CONTROL_CAMERAINFO;
	juvchdr->PayloadLength = 0;
	juvchdr->TotalLength = 0;
	juvchdr->XactOffset = 0;
	juvchdr->c.CamaraInfo.mjpeg_res_bitfield=mjpeg_res_bitfield;
	juvchdr->c.CamaraInfo.h264_res_bitfield= h264_res_bitfield;
	juvchdr->c.CamaraInfo.yuv_res_bitfield = yuv_res_bitfield;
	juvchdr->c.CamaraInfo.nv12_res_bitfield = nv12_res_bitfield;
	juvchdr->c.CamaraInfo.i420_res_bitfield =0;
	juvchdr->c.CamaraInfo.m420_res_bitfield = 0;	
	memcpy(juvchdr->c.CamaraInfo.camera,puci->vci.bmControl, 3);
	memcpy(juvchdr->c.CamaraInfo.process,puci->vcp.bmControl, 3);
	hex_dump(cmd,32,"cmd data");
	ret = TcpWrite(pudev->cmd_socket,cmd,32);
    if(ret <= 0){
        printf("cmd write sokcet failed  \n ");
		return -1;
    }

    return ret; 




	




}




void* uvc_cmd_system(void *lp)
{
	printf("Into uvc_cmd_system \n");
	char cmd[32];
	int ret = 0;
	struct uvcdev* pudev = (struct uvcdev*) lp;
	struct format_list fl;
	struct uvc_ctrl_info uci ;
	
	int sformat = 0 ;
	int sw = 0;
	int sh = 0;
	pudev->cmd_thread_run = 1;
    while(pudev->cmd_thread_run)
    {
        if(pudev->fd <= 0){
			sleep(1);
			continue;
        }
		
		CLEAR(fl);
		CLEAR(uci);
        pudev->uvcver = 0;
		
		pudev->cmd_socket= TcpConnect("10.10.10.254",GADGET_CONTROL_PORT,0);//UdpInit();
		if(pudev->cmd_socket < 0){
			printf("Tcp cmd link failed\n");
			sleep(3);
			continue;
		}
		printf("Tcp cmd link successful\n");
        
		 
		pudev->cmd_active = 1;
		while(pudev->cmd_active){
	        ret = TcpRead(pudev->cmd_socket,cmd,32);
	        if(ret <= 0){
                printf("cmd read sokcet failed \n");
				system("gpio l 14 4000 0 1 0 4000");
				system("date > /tmp/uvcclient_disconnect");
				break;
	        }
            PJUVCHDR pjuvchdr = (PJUVCHDR)cmd;
            if(pjuvchdr->Tag != JUVC_TAG){
				printf("uvc Tag error \n");
				continue;
            }
           
			if(pjuvchdr->XactType == JUVC_TYPE_CONTROL){

				//printf("pjuvchdr->Flags = %d \n",pjuvchdr->Flags);
				switch(pjuvchdr->Flags){
					case JUVC_CONTROL_VC_STOP:
                        printf("video start/stop \n");
						break;
					case JUVC_CONTROL_CAMERACTRL:
						printf("formatidx = %d \n",pjuvchdr->c.CamaraCtrl.formatidx);
						printf("frameidx = %d \n",pjuvchdr->c.CamaraCtrl.frameidx);
						uvcformatset(pjuvchdr->c.CamaraCtrl.formatidx,&sformat);
						uvcframeset(pjuvchdr->c.CamaraCtrl.frameidx,&sw,&sh);
	                    if(pudev->format == sformat && pudev->w ==sw && pudev->h == sh)
							break;
						pudev->format = sformat ;
						pudev->w = sw ;
						pudev->h = sh ;
						pudev->video_active= 0;

						break;
					case JUVC_CONTROL_CAMERAINFO:

						if(SendUvcInfo(pudev,&fl,&uci) <=0){
						 	printf("set SendUvcInfo failed \n");
							break;
			        	}
                        
						system("gpio l 52 4000 0 1 0 4000");
						system("gpio l 14 0 4000 0 1 4000");
						system("rm /tmp/uvcclient_disconnect");
						break;


					case JUVC_CONTROL_UVCCTRL_SET:
                    case JUVC_CONTROL_UVCCTRL_GET:

						if(pudev->uvcver == 0){
							printf("unkonw uvc version !!\n");
							break;

						}
						
						if(pjuvchdr->c.Rsvd[4] != 0){
							printf("No uvc ctrl cmd !!\n");
							break;
						}

						
						
						if(pjuvchdr->c.UvcControl.bmRequestType != 0x21  ){
							if(pjuvchdr->c.UvcControl.bmRequestType != 0xA1 ){
								printf("No uvc RequestType = %x !!\n",pjuvchdr->c.UvcControl.bmRequestType);
								break;
							}
						}

						pjuvchdr->c.Rsvd[4] = uci.Vc_Interface & 0xff;
					    
                        if(pjuvchdr->c.Rsvd[5] == uci.Input_ID || pjuvchdr->c.Rsvd[5] == 1){
							if(pjuvchdr->c.UvcControl.wValue == 0x300){
								char data[8];
								CLEAR(data);
								if(pjuvchdr->c.UvcControl.bRequest == 0x82 ||
								   pjuvchdr->c.UvcControl.bRequest == 0x83 ||
								   pjuvchdr->c.UvcControl.bRequest == 0x84 ||
								   pjuvchdr->c.UvcControl.bRequest == 0x87 ){
								    printf("no uvc cmd auto return \n");
									pjuvchdr->PayloadLength = pjuvchdr->c.UvcControl.wLength;
									pjuvchdr->TotalLength   = pjuvchdr->c.UvcControl.wLength;
			                        ret = TcpWrite(pudev->cmd_socket,cmd,32);
								    if(ret <= 0){
								        printf("cmd write sokcet failed  \n ");
										break;
								    }
									if(pjuvchdr->PayloadLength > 0){
										ret = TcpWrite(pudev->cmd_socket,data,pjuvchdr->PayloadLength);
									    if(ret <= 0){
									        printf("cmd write sokcet failed  \n ");
											break;
									    }
									}
									break;
								}
						        

							}
                        }
						
						if(pudev->uvcver == 0x0150){
					        pjuvchdr->c.Rsvd[4] = 0;
							if(pjuvchdr->c.Rsvd[5] != 1 && pjuvchdr->c.Rsvd[5] != 2 ){
								printf("No uvc input or process id !!\n");
								break;
							}
	                    
							if(pjuvchdr->c.Rsvd[5] == 1)
								pjuvchdr->c.Rsvd[5] = uci.Input_ID;
							if(pjuvchdr->c.Rsvd[5] == 2)
								pjuvchdr->c.Rsvd[5] = uci.Processing_ID;
						}
                       
						if(pjuvchdr->c.Rsvd[0] & 0x80 ){
                    
	                    	char data[128];
						    if(pjuvchdr->c.UvcControl.wLength > 128){
								printf("uvc r cmd data length overflow 128\n");
								break;
						    }

							
							ret = v4l2_control_transfer(pudev->fd,pjuvchdr->c.UvcControl.bmRequestType,pjuvchdr->c.UvcControl.bRequest,pjuvchdr->c.UvcControl.wValue,
																				pjuvchdr->c.UvcControl.wIndex,data,pjuvchdr->c.UvcControl.wLength);
							if(ret <= 0){
								
								printf("get 4l2_control_transfer failed ret = %d \n",ret); 
								printf("bmRequestType = %x \n",pjuvchdr->c.UvcControl.bmRequestType) ;
								printf("bRequest = %x \n",pjuvchdr->c.UvcControl.bRequest) ;
								printf("wValue = %x \n",pjuvchdr->c.UvcControl.wValue) ;
								printf("wIndex = %x \n",pjuvchdr->c.UvcControl.wIndex) ;
							    printf("wLength = %x \n",pjuvchdr->c.UvcControl.wLength) ;
								ret = 0;
							}
							/*
							else if(pjuvchdr->c.Rsvd[5] == 3){
		                    	printf("bmRequestType = %x \n",pjuvchdr->c.UvcControl.bmRequestType) ;
								printf("bRequest = %x \n",pjuvchdr->c.UvcControl.bRequest) ;
								printf("wValue = %x \n",pjuvchdr->c.UvcControl.wValue) ;
								printf("wIndex = %x \n",pjuvchdr->c.UvcControl.wIndex) ;
								printf("wLength = %x \n",pjuvchdr->c.UvcControl.wLength) ;
								hex_dump(data,ret,"control Get ");
                        	}
							*/
							pjuvchdr->PayloadLength = ret;
							pjuvchdr->TotalLength   = ret;
	                        ret = TcpWrite(pudev->cmd_socket,cmd,32);
						    if(ret <= 0){
						        printf("cmd write sokcet failed  \n ");
								break;
						    }
							if(pjuvchdr->PayloadLength > 0){
								ret = TcpWrite(pudev->cmd_socket,data,pjuvchdr->PayloadLength);
							    if(ret <= 0){
							        printf("cmd write sokcet failed  \n ");
									break;
							    }
							}
	

						}else{
	            
	                     	char data[128];
						    //printf("cmd PayloadLength = %x \n",pjuvchdr->PayloadLength) ;
						    if(pjuvchdr->PayloadLength > 128){
								printf("uvc w cmd data length overflow 16\n");
								break;
						    }
							//printf("pjuvchdr->PayloadLength = %d \n",pjuvchdr->PayloadLength);
							if(pjuvchdr->PayloadLength > 0){
							 	ret = TcpRead(pudev->cmd_socket,data,pjuvchdr->PayloadLength);
							    if(ret != pjuvchdr->PayloadLength){
									printf("tcp read uvc data failed \n");
									break;
							    }
							
							}
							
							
							ret = v4l2_control_transfer(pudev->fd,pjuvchdr->c.UvcControl.bmRequestType,pjuvchdr->c.UvcControl.bRequest,pjuvchdr->c.UvcControl.wValue,
														pjuvchdr->c.UvcControl.wIndex,data,pjuvchdr->c.UvcControl.wLength);

							if(ret != pjuvchdr->c.UvcControl.wLength){
								printf("Set v4l2_control_transfer failed ret = %d \n",ret);
								printf("bmRequestType = %x \n",pjuvchdr->c.UvcControl.bmRequestType) ;
								printf("bRequest = %x \n",pjuvchdr->c.UvcControl.bRequest) ;
								printf("wValue = %x \n",pjuvchdr->c.UvcControl.wValue) ;
								printf("wIndex = %x \n",pjuvchdr->c.UvcControl.wIndex) ;
							    printf("wLength = %x \n",pjuvchdr->c.UvcControl.wLength) ;
								hex_dump(data,pjuvchdr->c.UvcControl.wLength,"set data");
							}
							/*
							else if(pjuvchdr->c.Rsvd[5] == 3){
		                    	printf("bmRequestType = %x \n",pjuvchdr->c.UvcControl.bmRequestType) ;
								printf("bRequest = %x \n",pjuvchdr->c.UvcControl.bRequest) ;
								printf("wValue = %x \n",pjuvchdr->c.UvcControl.wValue) ;
								printf("wIndex = %x \n",pjuvchdr->c.UvcControl.wIndex) ;
								printf("wLength = %x \n",pjuvchdr->c.UvcControl.wLength) ;
								hex_dump(data,ret,"control Set ");
                        	}
                            */  

						}
						break;
					default:
						printf("unknown cmd \n");
						break;
				}
		
		
			}

				
		}
		closeSocket(pudev->cmd_socket);
		sleep(2);
    }
    printf("leave uvc_cmd_system \n");


	

	
	

}

void* uvc_video_system(void *lp)
{

	
	printf("Into uvc_video_system \n");
	int ret = 0 ;
	unsigned  long diff = 0;
	struct uvcdev* pudev = (struct uvcdev*) lp;
	
	pudev->video_thread_run = 1;
	while(pudev->video_thread_run){

		pudev->udpsocket = 0;
		pudev->fd = open_device();
	    if(pudev->fd <= 0 ){
			sleep(1);
			continue;
	    }
     
		if(init_device(pudev) < 0 ){
			printf("init_device faied \n");
			close_device(pudev);
			closeSocket(pudev->cmd_socket);
			continue;
		}
	/*
		if(pudev->format ==V4L2_PIX_FMT_H264 )
		{
		    printf("h264 tcp link \n");
			pudev->hsocket = TcpConnect("10.10.10.254",GADGET_H264_PORT);
			if(pudev->hsocket <= 0){
				printf("TcpInit faied \n");
				close_device(pudev);
				sleep(1);
				continue;

			}
			printf("h264 tcp link suncessful\n");

		}else if(pudev->format == V4L2_PIX_FMT_MJPEG ){ 
			pudev->udpsocket = UdpInit();
			if(pudev->udpsocket <= 0){
				printf("UdpInit faied \n");
				close_device(pudev);
				sleep(1);
				continue;

			}
		}
		*/

	    pudev->udpsocket = UdpInit();
		if(pudev->udpsocket <= 0){
			printf("UdpInit faied \n");
			close_device(pudev);
			closeSocket(pudev->cmd_socket);
			continue;

		}
		
		if(init_mmap(pudev) < 0 ){
			printf("init_mmap faied \n");
			close_device(pudev);
			closeSocket(pudev->udpsocket);
			closeSocket(pudev->cmd_socket);
			continue;
		}
#if 1	

        if(start_capturing(pudev) < 0){
			printf("start_capturing  failed \n");
			uninit_mmap(pudev);
			close_device(pudev);
			closeSocket(pudev->udpsocket);
			closeSocket(pudev->cmd_socket);
			continue;

        }
    
	    pudev->video_active= 1;
		pudev->video_id = 0;
		
		
		
		
				
				
				
		while(pudev->video_active){
			
			if(pudev->video_id == 0){
				gettimeofday(&start,NULL);
				diff = 0;
			}	
		
			if(readframe(pudev) < 0){
		  		//printf("readframe  failed \n");
				closeSocket(pudev->cmd_socket);
		  	 	break;
		  	}
		
			gettimeofday(&end,NULL);
			diff = 1000000 * (end.tv_sec-start.tv_sec)+ end.tv_usec-start.tv_usec;
		    if(diff >= 1000000){
				printf("sec = %d frame = %d \n",diff,pudev->video_id );
				pudev->video_id =0;
		    }
		   
		}
        stop_capturing(pudev);
#endif			
        uninit_mmap(pudev);
	
        close_device(pudev);
		closeSocket(pudev->udpsocket);
		
		//closeSocket(pudev->hsocket);
        sleep(1);

	}

	printf("Leave uvc_video_system \n");

}

void* uvc_audio_system(void *lp)
{
	printf("Into uvc_audio_system \n");
	struct uvcdev* pudev = (struct uvcdev*) lp;
	struct audio_para ap;
	memset(&ap,0,sizeof(ap));

	
	pudev->audio_thread_run = 1;
	while(pudev->audio_thread_run){
		if(0 != access("/proc/asound/card1/stream0", 0)){
			sleep(3);
			continue;
		}
		
#ifndef WRITE_FILE	
		pudev->socket= UdpInit(); //TcpConnect("10.10.10.254",GADGET_MIC_PORT,3); //UdpInit(); 
		if(pudev->socket < 0){
			printf("Tcp audio link failed\n");
			sleep(3);
			continue;
		}

		
		ap.socket = pudev->socket;
		printf("tcp audio link suncessful \n");
#endif

	    pudev->audio_active = 1; 
		GetAudioStream(AUDIO_CARD1_STREAM,&ap,CAP);
		DumpAudioParameter(&ap);
		JudgeAudioResample(&ap);
		ap.run = &pudev->audio_active;
		RunAudioCapture(&ap);
		closeSocket(ap.socket);
        sleep(1); // wait  access  stream0
	
	}
    printf("Leave uvc_audio_system \n");
}

void* uvc_audio_webcam_capture(void *lp) 
{
	printf("Into uvc_audio_webcam_capture \n");
	char capname[128];
	char streamname[128];
	int ret = 0 ;
	uint16_t vid = 0 , pid = 0;
	struct uvcdev* pudev = (struct uvcdev*) lp;
	struct audio_para ap;
	memset(&ap,0,sizeof(ap));
	pudev->audio_webcam_cap_run = 1;
	while(pudev->audio_webcam_cap_run){
		
      
		if(pudev->fd <= 0 || pudev->vid == 0 || pudev->pid==0 || pudev->audio_dev_cap_active == 1 ){
			sleep(1);
			continue;
		}
		vid = 0;
		pid = 0;
		GetAudioUsbid(AUDIO_CARD1_USBID,&vid,&pid);
		//printf("webcan vid = %x pid = %x Audio vid = %x pid = %x \n",pudev->vid,pudev->pid,vid,pid);
		if(vid != pudev->vid && pid != pudev->pid){
			GetAudioUsbid(AUDIO_CARD2_USBID,&vid,&pid);
			if(vid != pudev->vid && pid != pudev->pid){
				sleep(1);
				continue;
			}else{
				strcpy(capname,AUDIO_CARD2_CAPTURE);
				strcpy(streamname,AUDIO_CARD2_STREAM);
				strcpy(ap.dev,AUDIO_CARD2_DEV);
		    }	
		}else{
			strcpy(capname,AUDIO_CARD1_CAPTURE);
			strcpy(streamname,AUDIO_CARD1_STREAM);
			strcpy(ap.dev,AUDIO_CARD1_DEV);
		}

		if(0 != access(capname,0)){
			sleep(1);
			continue;
		}
		pudev->audio_webcam_cap_active = 1;
		printf("streamname = %s \n",streamname);
        if(GetAudioStream(streamname,&ap,CAP) < 0){
			sleep(1);
			continue;
        }
		DumpAudioParameter(&ap);
		JudgeAudioResample(&ap);
		ap.run = &pudev->audio_webcam_cap_active;
		ap.run2 = &pudev->audio_dev_cap_active;
		SendAudioCapture(&ap,1);
		pudev->audio_webcam_cap_active =0;
		sleep(1);
	}
	
    printf("Leave uvc_audio_webcam_capture \n");
}

void* uvc_audio_dev_capture(void *lp) 
{
	printf("Into uvc_audio_dev_capture \n");
	char capname[128];
	char streamname[128];
	int ret = 0 ;
    int run2 = 0;
	
	uint16_t vid = 0 , pid = 0;
	struct uvcdev* pudev = (struct uvcdev*) lp;
	struct audio_para ap;
	memset(&ap,0,sizeof(ap));
	pudev->audio_dev_cap_run = 1;
	while(pudev->audio_dev_cap_run){
  
		if(pudev->fd <= 0 || pudev->vid == 0 || pudev->pid==0 ){
			sleep(1);
			continue;
		}
		vid = 0;
		pid = 0;
		GetAudioUsbid(AUDIO_CARD1_USBID,&vid,&pid);
		if(vid == 0 || pid == 0){
            goto CARD2;
		}else if(vid == pudev->vid && pid == pudev->pid){
			goto CARD2;
		}else if(vid != pudev->vid && pid != pudev->pid){
			strcpy(capname,AUDIO_CARD1_CAPTURE);
			strcpy(streamname,AUDIO_CARD1_STREAM);
			strcpy(ap.dev,AUDIO_CARD1_DEV);
			goto CARDRUN;
		}
CARD2:		
		GetAudioUsbid(AUDIO_CARD2_USBID,&vid,&pid);
		if(vid == 0 || pid == 0){
            sleep(1);
			continue;
		}else if(vid == pudev->vid && pid == pudev->pid){
			sleep(1);
			continue;
		}else if(vid != pudev->vid && pid != pudev->pid){
			strcpy(capname,AUDIO_CARD2_CAPTURE);
			strcpy(streamname,AUDIO_CARD2_STREAM);
			strcpy(ap.dev,AUDIO_CARD2_DEV);
		}
		
CARDRUN:


		
		//printf("webcan vid = %x pid = %x Audio vid = %x pid = %x \n",pudev->vid,pudev->pid,vid,pid);
		
		if(0 != access(capname,0)){
			sleep(1);
			continue;
		}
		pudev->audio_dev_cap_active = 1;
		
		printf("streamname = %s \n",streamname);
        if(GetAudioStream(streamname,&ap,CAP) < 0){
			sleep(1);
			continue;
        }
		DumpAudioParameter(&ap);
		JudgeAudioResample(&ap);
		ap.run = &pudev->audio_dev_cap_active;
		ap.run2 = &run2;
		SendAudioCapture(&ap,2);
		pudev->audio_dev_cap_active=0;
        sleep(1);

	}
	
    printf("Leave uvc_audio_webcam_capture \n");
}

void* uvc_audio_webcam_playback(void *lp) 
{
	printf("Into uvc_audio_webcam_playback \n");
	char capname[128];
	char streamname[128];
	int ret = 0 ;
    int run2 = 0;
	
	uint16_t vid = 0 , pid = 0;
	struct uvcdev* pudev = (struct uvcdev*) lp;
	struct audio_para ap;
	memset(&ap,0,sizeof(ap));
	pudev->audio_webcam_play_run = 1;
	while(pudev->audio_webcam_play_run){
         
		if(pudev->fd <= 0 || pudev->vid == 0 || pudev->pid==0 || pudev->audio_dev_play_active== 1 ){
			sleep(1);
			continue;
		}
		vid = 0;
		pid = 0;
		GetAudioUsbid(AUDIO_CARD1_USBID,&vid,&pid);
		//printf("webcan vid = %x pid = %x Audio vid = %x pid = %x \n",pudev->vid,pudev->pid,vid,pid);
		if(vid != pudev->vid && pid != pudev->pid){
			GetAudioUsbid(AUDIO_CARD2_USBID,&vid,&pid);
			if(vid != pudev->vid && pid != pudev->pid){
				sleep(1);
				continue;
			}else{
				strcpy(capname,AUDIO_CARD2_PLAYBACK);
				strcpy(streamname,AUDIO_CARD2_STREAM);
				strcpy(ap.dev,AUDIO_CARD2_DEV);
		    }	
		}else{
			strcpy(capname,AUDIO_CARD1_PLAYBACK);
			strcpy(streamname,AUDIO_CARD1_STREAM);
			strcpy(ap.dev,AUDIO_CARD1_DEV);
		}

		if(0 != access(capname,0)){
			sleep(1);
			continue;
		}
		
		if(GetAudioStream(streamname,&ap,PLK) < 0){
			sleep(1);
			continue;
        }
		ap.socket= UdpInit();
		pudev->audio_webcam_play_active = 1;
		printf("streamname = %s \n",streamname);
		DumpAudioParameter(&ap);
		JudgeAudioResample2(&ap);
		ap.run = &pudev->audio_webcam_play_active ;
		ap.run2 = &pudev->audio_dev_play_active ;
		PlayAudio(&ap,1);
		pudev->audio_webcam_play_active = 0;

	}

}

void* uvc_audio_dev_playback(void *lp) 
{
	printf("Into uvc_audio_dev_playback \n");
	char capname[128];
	char streamname[128];
	int ret = 0 ;
    int run2 = 0;
	
	uint16_t vid = 0 , pid = 0;
	struct uvcdev* pudev = (struct uvcdev*) lp;
	struct audio_para ap;
	memset(&ap,0,sizeof(ap));
	pudev->audio_dev_play_run= 1;
	while(pudev->audio_dev_play_run){
  
		if(pudev->fd <= 0 || pudev->vid == 0 || pudev->pid==0 ){
			sleep(1);
			continue;
		}
		vid = 0;
		pid = 0;
		GetAudioUsbid(AUDIO_CARD1_USBID,&vid,&pid);
		if(vid == 0 || pid == 0){
            goto CARD2;
		}else if(vid == pudev->vid && pid == pudev->pid){
			goto CARD2;
		}else if(vid != pudev->vid && pid != pudev->pid){
			strcpy(capname,AUDIO_CARD1_PLAYBACK);
			strcpy(streamname,AUDIO_CARD1_STREAM);
			strcpy(ap.dev,AUDIO_CARD1_DEV);
			goto CARDRUN;
		}
CARD2:		
		GetAudioUsbid(AUDIO_CARD2_USBID,&vid,&pid);
		if(vid == 0 || pid == 0){
            sleep(1);
			continue;
		}else if(vid == pudev->vid && pid == pudev->pid){
			sleep(1);
			continue;
		}else if(vid != pudev->vid && pid != pudev->pid){
			strcpy(capname,AUDIO_CARD2_PLAYBACK);
			strcpy(streamname,AUDIO_CARD2_STREAM);
			strcpy(ap.dev,AUDIO_CARD2_DEV);
		}
		
CARDRUN:


		
		//printf("webcan vid = %x pid = %x Audio vid = %x pid = %x \n",pudev->vid,pudev->pid,vid,pid);
		
		if(0 != access(capname,0)){
			sleep(1);
			continue;
		}
		pudev->audio_dev_play_active= 1;
		
		printf("streamname = %s \n",streamname);
        if(GetAudioStream(streamname,&ap,PLK) < 0){
			sleep(1);
			continue;
        }
		DumpAudioParameter(&ap);
		JudgeAudioResample2(&ap);
		ap.socket= UdpInit();
		ap.run = &pudev->audio_dev_play_active;
		ap.run2 = &run2;
		PlayAudio(&ap,2);
		pudev->audio_dev_play_active = 0;
        sleep(1);

	}
	
    printf("Leave uvc_audio_webcam_capture \n");

}
int PidfileCreate(const char *pidfile)
{
    int pidfd = 0;
     char val[16];

    int len = snprintf(val, sizeof(val), "%d\n",getpid());
    if (len <= 0) {
         printf("Pid error (%s)", strerror(errno));
         return-1;
    }
 
     pidfd = open(pidfile, O_CREAT | O_WRONLY, 0644);
     if (pidfd < 0) {
         printf("unable to set pidfile ‘%s‘: %s",
                    pidfile,
                    strerror(errno));
         return-1;
     }
 
     ssize_t r = write(pidfd, val, (unsigned int)len);
     if (r == -1) {
         printf("unable to write pidfile: %s", strerror(errno));
         close(pidfd);
         return-1;
      } else if ((size_t)r != len) {
          printf("unable to write pidfile: wrote"
                 " %d of %d bytes.", r, len);
         close(pidfd);
         return -1;
     }
 
     close(pidfd);
     return 0;
 }


int main(int argc, char **argv)
{

	pthread_t uvc_audio_cap1;
	pthread_t uvc_audio_cap2;
	pthread_t uvc_audio;
    pthread_t uvc_video;
	pthread_t uvc_cmd;
    pthread_t usb_paser;
    uint16_t vid , pid;  
	
	int ret = 0;	
    initsignal();
    CLEAR(g_udev);
    

	
	g_udev.fd = 0;
	g_udev.force_format = 1;
	g_udev.format = V4L2_PIX_FMT_MJPEG;
	g_udev.w = 1280;
	g_udev.h = 720;
/*
	g_udev.fd = open_device();
	GetDevVidPid(g_udev.fd,&g_udev.vid,&g_udev.pid);
	char buf[4096];
	char vc[1024];
	ret = GetDevConfigDesc(g_udev.fd ,buf);
	if(ret > 0){
		ret =uvc_prase_vc(buf ,ret,vc);
		hex_dump(vc,ret,"vc desc");
	}
*/		
#if 1	
/*

	if(pthread_create(&uvc_audio_cap1, NULL,uvc_audio_webcam_capture,&g_udev) != 0) {
			printf("Error creating uvc_audio_system\n");
			return -1;
	}

	if(pthread_create(&uvc_audio_cap2, NULL,uvc_audio_dev_capture,&g_udev) != 0) {
			printf("Error creating uvc_audio_system\n");
			return -1;
	}
*/	
    PidfileCreate("/var/run/uvcclient.pid");
    system("date > /tmp/uvcclient_disconnect");

	if (pthread_create(&uvc_audio, NULL,uvc_audio_system,&g_udev) != 0) {
			printf("Error creating uvc_audio_system\n");
			return -1;
	}



	if (pthread_create(&uvc_video, NULL,uvc_video_system,&g_udev) != 0) {
			printf("Error creating uvc_video_system\n");
			return -1;
	}

	if (pthread_create(&uvc_cmd, NULL,uvc_cmd_system,&g_udev) != 0) {
			printf("Error creating uvc_cmd_system\n");
			return -1;
	}

	pthread_join(uvc_cmd,   NULL); // 等待子執行緒執行完成
	pthread_join(uvc_video, NULL); // 等待子執行緒執行完成
	pthread_join(uvc_audio, NULL); // 等待子執行緒執行完成
	//pthread_join(uvc_audio_cap1,   NULL); // 等待子執行緒執行完成
	//pthread_join(uvc_audio_cap2,   NULL); // 等待子執行緒執行完成  
#endif	
    system("rm /var/run/uvcclient.pid");
    printf("Leave main \n");
	kill(getpid(), SIGKILL);




}


