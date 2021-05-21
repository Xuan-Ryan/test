#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>		/* open */
#include <sys/ioctl.h>		/* ioctl */
#include<stdbool.h>
#include <pthread.h>

#include "t6usbdongle.h"
int disp_interface = 0;
int g_audio_fill_flga = 0;
libusb_context *ctx = NULL;
libusb_device_handle* t6usbdev = NULL;

pthread_mutex_t usb_mutex = PTHREAD_MUTEX_INITIALIZER;


int is_T6dev( libusb_device *dev)
{
	struct libusb_device_descriptor desc;
	int rc = libusb_get_device_descriptor( dev, &desc );

	if( LIBUSB_SUCCESS == rc && desc.idVendor == 0x0711 && (desc.idProduct == 0x5600 || desc.idProduct == 0x5601 || desc.idProduct == 0x5621))
    	return 1;
	else if(LIBUSB_SUCCESS == rc && desc.idVendor == 0x03f0 && desc.idProduct == 0x0182)	
		return 1;
	else 
		return 0;
}

void closeT6evd()
{
  if(t6usbdev != NULL)
	libusb_close(t6usbdev);
  if(ctx != NULL)
	libusb_exit(ctx);
  t6usbdev = NULL;
  ctx = NULL;      
}

int openT6dev()
{

	int t6found = -1 ;
	int i; 
	int evdi_id = -1 ;
	int ret ;
	int disp_interface_number = 0;
	
	ssize_t cnt=0;
	libusb_device **list;
    libusb_device *T6dev = NULL;
   
	
    libusb_init(&ctx);
	//DEBUG_PRINT("ENTER %s\n",__FUNCTION__);
	cnt = libusb_get_device_list(ctx, &list);
	
	// find T6 device
	for(i = 0; i < cnt; i++){
		libusb_device *device = list[i];
		if(is_T6dev(device)){	
			ret = libusb_open (device, &t6usbdev);
	
			if(ret != 0) {
				printf("T6:  libusb_open failed\n");
				return -1 ;
			}
			
			if((ret = libusb_set_configuration(t6usbdev, 1)) != 0) {
				 printf("T6: %s: libusb_set_configuration failed(%s)\n", __func__);
				 libusb_close(t6usbdev);
				 return -1 ;
			}

			if((ret = libusb_claim_interface(t6usbdev, 0)) != 0) {
				printf("T6: %s: libusb_claim_interface 0 failed(%s)\n", __func__);
				libusb_close(t6usbdev);
				return -1 ;
			}
	        t6found = 1;
			break; 
			
		}
  	}
	
	libusb_free_device_list(list, 1);
	
	
	return t6found ;

	
	

}

int t6_libusb_get_displaysectionheader()
{
	int  number = 0 ;
	int  ret ,usize;
	usize = sizeof(T6ROMDISPLAYSECTIONHDR);
	PT6ROMDISPLAYSECTIONHDR  Dispsecthdr;
	
	
	Dispsecthdr = (PT6ROMDISPLAYSECTIONHDR)malloc(usize);
	printf("T6ROMDISPLAYSECTIONHDR =%d \n",usize);
#ifdef __i386__
	printf("T6ROMDISPLAYCAPS =%u \n",sizeof(T6ROMDISPLAYCAPS));
	printf("T6ROMDISPLAYINTERFACE =%u \n",sizeof(T6ROMDISPLAYINTERFACE));
#else
	printf("T6ROMDISPLAYCAPS =%lu \n",sizeof(T6ROMDISPLAYCAPS));
	printf("T6ROMDISPLAYINTERFACE =%lu \n",sizeof(T6ROMDISPLAYINTERFACE));
#endif
	ret = libusb_control_transfer(t6usbdev, 0xc0, VENDOR_REQ_QUERY_SECTION_DATA, 0, 0, (UINT8 *)Dispsecthdr, usize, 3000);
	if(ret < 0) {
		printf("%s: libusb_control_transfer fail\n", __FUNCTION__);
		return 0;
	} 
   
	printf(" display section header ver =%d \n", Dispsecthdr->Version);
	printf(" display section header vid =%x \n", Dispsecthdr->vid);
	printf(" display section header pid =%x \n", Dispsecthdr->pid);
	printf(" Display1Caps.LinkInterfaces =%d \n", Dispsecthdr->Display1Caps.LinkInterfaces);
	printf(" Display2Caps.LinkInterfaces =%d \n", Dispsecthdr->Display2Caps.LinkInterfaces);
	if(Dispsecthdr->Display1Caps.LinkInterfaces != 0)
	    number++;
	if(Dispsecthdr->Display2Caps.LinkInterfaces != 0)
	    number++ ;
	//hex_dump((char *)Dispsecthdr,ret,(char*)"section");
	free(Dispsecthdr);
	return number;

}

int  t6_libusb_dongle_reset()
{
	 t6_libusb_donglereset(t6usbdev);
	 //libusb_close(t6dev->t6usbdev);
}

int t6_libusb_get_monitorstatus(char view) // veiw = 0 hdmi , view =1 vga; 
{
	int ret = 0 ;
	char status;
    ret = libusb_control_transfer(t6usbdev,0xc0,VENDOR_REQ_QUERY_MONITOR_CONNECTION_STATUS,view,0,&status,1,1000);
    if(ret <= 0)
		return -1;
	//printf("monitor status =%d  ret =%d\n",status,ret);
	return status;

}



static int 
t6_libusb_get_resolution_num()
{
	int ret ;
	int resnum ;
	ret = libusb_control_transfer(t6usbdev,0xc0,VENDOR_REQ_GET_RESOLUTION_TABLE_NUM,disp_interface ,0,(char *)&resnum,4,3000);
	if(ret < 0)
		return -1;

	//printf("resolution number=%d \n",resnum);
	return resnum;
	
}


int t6_libusb_get_resolution_timing(int w , int h ,PRESOLUTIONTIMING myres)
{
	int r_number , ret ,usize;
	int i = 0 , index = -1;
	unsigned char buffer[4096];
	PRESOLUTIONTIMING r_table = (PRESOLUTIONTIMING)buffer ;
	r_number = t6_libusb_get_resolution_num(t6usbdev);
	if(r_number < 0)
		return -1;
	
	usize = r_number * sizeof(RESOLUTIONTIMING);
	printf("total size res table =%d \n",usize );
	//printf("RESOLUTIONTIMING =%d \n",sizeof(RESOLUTIONTIMING) );
	//r_table = malloc(usize);
	ret = libusb_control_transfer(t6usbdev,0xc0,VENDOR_REQ_GET_RESOLUTION_TIMING_TABLE,disp_interface ,0,(char *)r_table,usize,3000);
	if(ret < 0){
		//free(r_table);
		return -1; 
	}
	for(i = 0 ; i < r_number ; i++){
		
		
		printf("Width = %d , Height = %d , Frequency = %d \n",
			r_table->HorAddrTime,r_table->VerAddrTime,r_table->Frequency);
		
		if(w == r_table->HorAddrTime && h == r_table->VerAddrTime){
			memcpy((char*)myres,(char*)r_table,sizeof(RESOLUTIONTIMING));
			break;
			}
		r_table++;
	}
	//free(r_table);
	return 0;

}

int t6_libusb_set_monitor_power(char on)
{
	int ret;
	ret = libusb_control_transfer(t6usbdev,0x40,VENDOR_REQ_SET_MONITOR_CTRL,disp_interface ,on,NULL,0,3000);
	if(ret < 0)
		return -1; 
	return 0;

}
void t6_libusb_donglereset()
{
	libusb_control_transfer(t6usbdev,0x40,VENDOR_REQ_SET_CANCEL_BULK_OUT,0,0,NULL,0,1000);
}


static int
EDID_Header_Check(UINT8 *pbuf)
{
	if (pbuf[0] != 0x00 || pbuf[1] != 0xFF || pbuf[2] != 0xFF ||
	    pbuf[3] != 0xFF || pbuf[4] != 0xFF || pbuf[5] != 0xFF ||
	    pbuf[6] != 0xFF || pbuf[7] != 0x00) {
		printf("EDID block0 header error\n");
		return -1;
	}
	return 0;
}

static int
EDID_Version_Check(UINT8 *pbuf)
{
	//printf("EDID version: %d.%d\n", pbuf[0x12], pbuf[0x13]);
	if (pbuf[0x12] != 0x01) {
		//printf("Unsupport EDID format,EDID parsing exit\n");
		return -1;
	}
	if (pbuf[0x13] < 3 && !(pbuf[0x18] & 0x02)) {
		//printf("EDID revision < 3 and preferred timing feature bit "
		//	"not set, ignoring EDID info\n");
		return -1;
	}
	return 0;
}

static int
EDID_CheckSum( unsigned char *buf)
{
	int i = 0, CheckSum = 0;
	unsigned char *pbuf = buf ;

	for (i = 0; i < 128; i++) {
		CheckSum += pbuf[i];
		CheckSum &= 0xFF;
	}

	return CheckSum;
}


int t6_libusb_get_edid( char *edid )
{
	
	int ret , i ;
	int ucExtendEDID = 0 ;
	int edid_size =128;
    if(t6usbdev == NULL)
        return -1;

	ret = libusb_control_transfer(t6usbdev,0xc0,VENDOR_REQ_GET_EDID, 0, disp_interface,edid,128,3000);
    printf("EDID_ret= %d \n",ret);
	if(ret < 0)
		return -1;
	
	if(ret < 128)
		return 0;
	
	if(EDID_CheckSum(edid) != 0){
		printf("EDID_CheckSum error \n");
		return 0;
	}
	
	if (EDID_Header_Check(edid) != 0){
		printf("EDID_Header_Check error \n");
		return 0;
	}
	
	if (EDID_Version_Check(edid) != 0){
		printf("EDID_Version_Check error \n");
		return 0;
	}
	//hex_dump(t6dev->edid,edid_size,"EDID1");	

	ucExtendEDID = edid[126];
	if (ucExtendEDID > 0)	{//extended EDID
		for(i=1;(i<= ucExtendEDID)&&(i<=3);i++){
			ret = libusb_control_transfer( t6usbdev, 0xc0, VENDOR_REQ_GET_EDID, 128*i, disp_interface, (UINT8 *)edid+128*i, 128, 3000);
			if(ret == 128) 
				edid_size += 128; 
			else
				break;
		}
	}
	

	
	//hex_dump(t6dev->edid,edid_size,"EDID2");		
	return edid_size;
}





int  t6_libusb_get_AudioEngineStatus(PT6AUD_SETENGINESTATE setEngine )
{
	int ret  = 0;
	unsigned char num ;
	T6AUD_GETENGINESTATE aud_engine;
	ret = libusb_control_transfer(t6usbdev,0xc0,VENDOR_REQ_AUD_GET_ENGINE_STATE,0,0,&aud_engine,sizeof(aud_engine),3000);
	if(ret < 0)
		return -1;
	
	setEngine->Activity = 0x01;
	setEngine->CyclicBufferSize = aud_engine.CyclicBufferSize;
	setEngine->FormatIndex = aud_engine.FormatIndex;
	setEngine->ReturnSize  = 9600 ; // 10ms
	
	//printf("aud_engine.Activity =%d \n",aud_engine.Activity);
	//printf("aud_engine.CyclicBufferSize =%d \n",aud_engine.CyclicBufferSize);
	//printf("aud_engine.FormatIndex =%d \n",aud_engine.FormatIndex);
	//printf("aud_engine.JackState =%d \n",aud_engine.JackState);
	return ret;
	
}	

int  t6_libusb_set_AudioEngineStatus()
{
    int ret ; 
	T6AUD_SETENGINESTATE setEngine ;
	memset(&setEngine,0,sizeof(setEngine));
	if(t6_libusb_get_AudioEngineStatus(&setEngine) < 0){
			return -1;
		}
	//hex_dump((char *) &setEngine,sizeof(setEngine),"set endine status");
	ret = libusb_control_transfer(t6usbdev,0x40,VENDOR_REQ_AUD_SET_ENGINE_STATE,0,0,&setEngine,sizeof(setEngine),3000);
	if(ret < 0){
		
		printf("set audio engine failed \n");
		return -1;
	}

	
	return 0;

}

int  t6_libusb_set_softready()
{
	int ret = 0 ;
	ret =  libusb_control_transfer(t6usbdev,0x40,VENDOR_REQ_SET_SOFTWARE_READY,disp_interface,0,NULL,0,3000);
	if(ret < 0)
		return -1;
	 
	return 0;

}


int  t6_libusb_set_resolution( int w,int h)
{
	int ret;
	RESOLUTIONTIMING myres;
	
	if(t6_libusb_get_resolution_timing(w,h,&myres) <0 ){
		printf(" t6_libusb_get_resolution_timing failed 1\n");
		return -1;
	}
	ret = libusb_control_transfer(t6usbdev,0x40,VENDOR_REQ_SET_RESOLUTION_DETAIL_TIMING,disp_interface ,0,(char*)&myres,sizeof(RESOLUTIONTIMING),3000);
	if(ret < 0){
		printf(" t6_libusb_get_resolution_timing failed 2\n");
		return -1;
	}
#if 1
	ret = t6_libusb_set_AudioEngineStatus();
	if(ret <0 ){
		printf(" t6_libusb_get_resolution_timing failed 3\n");
		return -1;
	}

	
	ret =t6_libusb_set_monitor_power(1);
    if(ret <0 ){
		printf(" t6_libusb_get_resolution_timing failed 4\n");
		return -1;
    }	
#endif		
	return t6_libusb_set_softready( );
}


int t6_libusb_get_ram_size()
{
	int ret ;
	UINT8 ramsize;
	ret = libusb_control_transfer(t6usbdev, 0xc0, VENDOR_REQ_QUERY_VIDEO_RAM_SIZE, 0, 0, (UINT8 *)&ramsize, 1, 3000);
	if(ret < 0)
		return -1;

	printf("ramsize =%d \n",ramsize);
	return ramsize;
	
}
int t6_libusb_FilpJpegFrame444(char *jpgimage ,int jpgsize,int flag ,int cmdAddr ,int fbAddr,int Width ,int Height)
{
    int ret = 0 ;
	int transferred = 0;
	int len = jpgsize + 48 + 1024;
	int tmp = len % 1024;
	int VideoDataSize = len -48 ;
	int Y_BlockSize = (((Width+31)/32)*32) * (((Height+31)/32)*32) +1024 ; 
    

	BULK_CMD_HEADER bch ;
	memset(&bch,0,sizeof(BULK_CMD_HEADER));
	bch.Signature = 0 ;
	bch.PayloadLength =  len ;
	bch.PayloadAddress = cmdAddr;
	bch.PacketLength  = bch.PayloadLength;

	ret = libusb_bulk_transfer(t6usbdev, EP_BLK_OUT_ADDR,(UINT8 *)&bch, 32, &transferred, 5000);
    if(ret < 0){
		printf("bulk out failed 32 =%d \n",ret);
		return -1;
    }
	char *videobuf = (char*) malloc(len);
	PVIDEO_FLIP_HEADER vfh = (PVIDEO_FLIP_HEADER)videobuf;
    memset(videobuf,0,len) ;
	if(disp_interface  == 0)
		vfh->Command = VIDEO_CMD_FLIP_PRIMARY;
	if(disp_interface  == 1)
		vfh->Command = VIDEO_CMD_FLIP_SECONDARY;
	vfh->FenceID = 0 ;
	vfh->TargetFormat = VIDEO_COLOR_YUV444;
	vfh->Y_RGB_Pitch = (((Width+31)/32)*32) ;
	vfh->UV_Pitch =    (((Width+31)/32)*32);
	vfh->Y_RGB_Data_FB_Offset = fbAddr ;
	vfh->U_UV_Data_Offset = vfh->Y_RGB_Data_FB_Offset + Y_BlockSize;
	vfh->V_Data_Offset = vfh->U_UV_Data_Offset + Y_BlockSize;
	vfh->Flag = 0;
	vfh->SourceFormat = VIDEO_COLOR_JPEG;
	vfh->PayloadSize = VideoDataSize ;
	
	vfh++;
	memcpy((char*)vfh,jpgimage,jpgsize);
	ret = libusb_bulk_transfer(t6usbdev, EP_BLK_OUT_ADDR, videobuf, len, &transferred, 5000);
    if(ret < 0){
		free(videobuf);
		printf("bulk out failed 3 =%d \n",ret);
		t6_libusb_donglereset();
		return -1;
    }

	free(videobuf);
	return 0;

}

void audio_ready(){
	g_audio_fill_flga = 1;
}
void audio_release(){
	g_audio_fill_flga = 0;
}

int t6_libusb2_FilpJpegFrame422(char *jpgimage ,int jpgsize,int flag ,int cmdAddr ,int fbAddr,int Width ,int Height)
{
	int ret = 0 ;
	int transferred = 0;
	int total_len = jpgsize + 48 + 1024;
	int VideoDataSize = jpgsize+ 1024 ;

	int page = total_len / T6PAGE;
	int pagenumber = total_len % T6PAGE ;
	int poffset = 0;
    char* pdata =NULL; 

	char *videobuf = (char*)calloc(1,total_len);

	PVIDEO_FLIP_HEADER vfh = (PVIDEO_FLIP_HEADER)videobuf;
	if(disp_interface  == 0)
		vfh->Command = VIDEO_CMD_FLIP_PRIMARY;
	if(disp_interface  == 1)
		vfh->Command = VIDEO_CMD_FLIP_SECONDARY;
	vfh->FenceID = 0 ;
	vfh->TargetFormat = VIDEO_COLOR_YUYV;
	vfh->Y_RGB_Pitch = (((Width * 2+31)/32)*32) ;
	vfh->UV_Pitch =    0;
	vfh->Y_RGB_Data_FB_Offset = fbAddr ;
	vfh->U_UV_Data_Offset = 0;
	vfh->V_Data_Offset = 0;
	vfh->Flag = 0;
	vfh->SourceFormat = VIDEO_COLOR_JPEG;
	vfh->PayloadSize = VideoDataSize ;
	vfh++;
	memcpy(videobuf+48,jpgimage,jpgsize);

	pdata = videobuf;
	
	while(page > 0){

		
		T6BULKDMAHDR t6hdr;
		t6hdr.Signature = 0;
		t6hdr.PayloadLength =  total_len;
		t6hdr.PayloadAddress = cmdAddr;
        t6hdr.PacketSize = T6PAGE;
		t6hdr.PacketOffset = poffset;
        t6hdr.PacketFlags  = T6_PACKET_FLAG_CONTINUETOSEND;
		
		pthread_mutex_lock(&usb_mutex); 
		ret = libusb_bulk_transfer(t6usbdev, EP_BLK_OUT_ADDR,(UINT8 *)&t6hdr, 32, &transferred, 5000);
	    if(ret < 0){
			pthread_mutex_unlock(&usb_mutex); 
			free(videobuf);
			printf("bulk out failed 32 =%d \n",ret);
			return -1;
	    }
		ret = libusb_bulk_transfer(t6usbdev, EP_BLK_OUT_ADDR, videobuf+poffset, T6PAGE, &transferred, 5000);
	    if(ret < 0){
			pthread_mutex_unlock(&usb_mutex); 
			free(videobuf);
			printf("bulk out failed  =%d \n",ret);
			t6_libusb_donglereset();
			return -1;
	    }
		pthread_mutex_unlock(&usb_mutex);
		poffset += T6PAGE;
		page--;
	}

	if(pagenumber > 0){
		
		T6BULKDMAHDR t6hdr;
		t6hdr.Signature = 0;
		t6hdr.PayloadLength =  total_len;
		t6hdr.PayloadAddress = cmdAddr;
        t6hdr.PacketSize = pagenumber;
		t6hdr.PacketOffset = poffset;
        t6hdr.PacketFlags  = T6_PACKET_FLAG_NONE;
		pthread_mutex_lock(&usb_mutex); 
		ret = libusb_bulk_transfer(t6usbdev, EP_BLK_OUT_ADDR,(UINT8 *)&t6hdr, 32, &transferred, 5000);
	    if(ret < 0){
			pthread_mutex_unlock(&usb_mutex); 
			free(videobuf);
			printf("bulk out failed 32 =%d \n",ret);
			return -1;
	    }
		ret = libusb_bulk_transfer(t6usbdev, EP_BLK_OUT_ADDR, videobuf+poffset, pagenumber, &transferred, 5000);
	    if(ret < 0){
			pthread_mutex_unlock(&usb_mutex); 
			free(videobuf);
			printf("bulk out failed  =%d \n",ret);
			t6_libusb_donglereset();
			return -1;
	    }
		pthread_mutex_unlock(&usb_mutex);
        
	}
	free(videobuf);
	return 0; 


	


}

int t6_libusb_FilpJpegFrame422(char *jpgimage ,int jpgsize,int flag ,int cmdAddr ,int fbAddr,int Width ,int Height)
{
    int ret = 0 ;
	int transferred = 0;
	int len = jpgsize + 48 + 1024;
	//int tmp = len % 1024;
	int VideoDataSize = len -48 ;
	int Y_BlockSize = (((Width+31)/32)*32) * (((Height+31)/32)*32) +1024 ; 
    

	BULK_CMD_HEADER bch ;
	memset(&bch,0,sizeof(BULK_CMD_HEADER));
	bch.Signature = 0 ;
	bch.PayloadLength =  len ;
	bch.PayloadAddress = cmdAddr;
	bch.PacketLength  = bch.PayloadLength;
    pthread_mutex_lock(&usb_mutex); 
	ret = libusb_bulk_transfer(t6usbdev, EP_BLK_OUT_ADDR,(UINT8 *)&bch, 32, &transferred, 5000);
    if(ret < 0){
		pthread_mutex_unlock(&usb_mutex); 
		printf("bulk out failed 32 =%d \n",ret);
		return -1;
    }
	char *videobuf = (char*) malloc(len);
	PVIDEO_FLIP_HEADER vfh = (PVIDEO_FLIP_HEADER)videobuf;
    memset(videobuf,0,len) ;
	if(disp_interface  == 0)
		vfh->Command = VIDEO_CMD_FLIP_PRIMARY;
	if(disp_interface  == 1)
		vfh->Command = VIDEO_CMD_FLIP_SECONDARY;
	vfh->FenceID = 0 ;
	vfh->TargetFormat = VIDEO_COLOR_YUYV;
	vfh->Y_RGB_Pitch = (((Width * 2+31)/32)*32) ;
	vfh->UV_Pitch =    0;
	vfh->Y_RGB_Data_FB_Offset = fbAddr ;
	vfh->U_UV_Data_Offset = 0;
	vfh->V_Data_Offset = 0;
	vfh->Flag = 0;
	vfh->SourceFormat = VIDEO_COLOR_JPEG;
	vfh->PayloadSize = VideoDataSize ;
	
	vfh++;
	memcpy((char*)vfh,jpgimage,jpgsize);
	ret = libusb_bulk_transfer(t6usbdev, EP_BLK_OUT_ADDR, videobuf, len, &transferred, 5000);
    if(ret < 0){
		pthread_mutex_unlock(&usb_mutex); 
		free(videobuf);
		printf("bulk out failed 3 =%d \n",ret);
		t6_libusb_donglereset();
		return -1;
    }
    pthread_mutex_unlock(&usb_mutex); 
	free(videobuf);
	return 0;

}


int t6_libusb_FilpJpegFrame(char *jpgimage ,int jpgsize,int flag ,int cmdAddr ,int fbAddr,int Width ,int Height)
{
    int ret = 0 ;
	int transferred = 0;
	int len = jpgsize + 48 + 1024;
	int tmp = len % 1024;
	int VideoDataSize = len -48 ;
	int Y_BlockSize = (((Width+31)/32)*32) * (((Height+31)/32)*32) +1024 ; 
    

	BULK_CMD_HEADER bch ;
	memset(&bch,0,sizeof(BULK_CMD_HEADER));
	bch.Signature = 0 ;
	bch.PayloadLength =  len ;
	bch.PayloadAddress = cmdAddr;
	bch.PacketLength  = bch.PayloadLength;
    pthread_mutex_lock(&usb_mutex); 
	ret = libusb_bulk_transfer(t6usbdev, EP_BLK_OUT_ADDR,(UINT8 *)&bch, 32, &transferred, 5000);
    if(ret < 0){
		pthread_mutex_unlock(&usb_mutex); 
		printf("bulk out failed 32 =%d \n",ret);
		return -1;
    }
	char *videobuf = (char*) malloc(len);
	PVIDEO_FLIP_HEADER vfh = (PVIDEO_FLIP_HEADER)videobuf;
    memset(videobuf,0,len) ;
	if(disp_interface  == 0)
		vfh->Command = VIDEO_CMD_FLIP_PRIMARY;
	if(disp_interface  == 1)
		vfh->Command = VIDEO_CMD_FLIP_SECONDARY;
	vfh->FenceID = 0 ;
	vfh->TargetFormat = VIDEO_COLOR_NV12;
	vfh->Y_RGB_Pitch = (((Width+31)/32)*32) ;
	vfh->UV_Pitch =    (((Width+31)/32)*32);
	vfh->Y_RGB_Data_FB_Offset = fbAddr ;
	vfh->U_UV_Data_Offset = vfh->Y_RGB_Data_FB_Offset + Y_BlockSize;
	vfh->V_Data_Offset = 0;
	vfh->Flag = 0;
	vfh->SourceFormat = VIDEO_COLOR_JPEG;
	vfh->PayloadSize = VideoDataSize ;
	
	vfh++;
	memcpy((char*)vfh,jpgimage,jpgsize);
	ret = libusb_bulk_transfer(t6usbdev, EP_BLK_OUT_ADDR, videobuf, len, &transferred, 5000);
    if(ret < 0){
		pthread_mutex_unlock(&usb_mutex); 
		free(videobuf);
		printf("bulk out failed 3 =%d \n",ret);
		t6_libusb_donglereset();
		return -1;
    }
    pthread_mutex_unlock(&usb_mutex); 
	free(videobuf);
	return 0;

}

int t6_libusb_SendAudio(char * data , int len  )
{	int ret ;	
    int transferred = 0;
	BULK_CMD_HEADER bulkhead;	
	memset(&bulkhead,0,sizeof(bulkhead));	
	bulkhead.Signature = SIGNATURE_AUDIO;	
	bulkhead.PayloadLength = len ;	
	bulkhead.PacketLength  = len ;	
	bulkhead.PayloadAddress = 0;
	pthread_mutex_lock(&usb_mutex); 
	ret = libusb_bulk_transfer(t6usbdev, EP_BLK_OUT_ADDR,(char *)&bulkhead,32, &transferred,5000);    
	if(ret < 0){	
		pthread_mutex_unlock(&usb_mutex); 
		printf("bulk out failed 1 =%d \n",ret);	
		t6_libusb_donglereset();
		return -1;    
		}	
	ret = libusb_bulk_transfer(t6usbdev, EP_BLK_OUT_ADDR,data,len,&transferred,5000);    
	if(ret < 0){	
		pthread_mutex_unlock(&usb_mutex); 
		printf("bulk out failed 2 =%d \n",ret);	
		t6_libusb_donglereset();
		return -1;    
		}
	pthread_mutex_unlock(&usb_mutex); 
	return 0 ;		
}

















