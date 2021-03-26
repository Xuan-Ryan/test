#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "libusb/libusb.h"


#include "uvcdescription.h"

#include  "uvchost.h"
#include  "USBBldr.h"

//=============  Camera Terminal  =====================
const unsigned int   Scanning_Mode_Control_flag = 0x03;
const unsigned short Scanning_Mode_Control_len =1;

const unsigned int   Auto_Exposure_Mode_Control_flag = 0x33;
const unsigned short Auto_Exposure_Mode_Control_len =1;

const unsigned int   Auto_Exposure_Priority_Control_flag = 0x03;
const unsigned short Auto_Exposure_Priority_Control_len =1;

const unsigned int   Exposure_Time_Absolute_Control_flag = 0x3f;
const unsigned short Exposure_Time_Absolute_Control_len =4;

const unsigned int   Exposure_Time_Relative_Control_flag = 0x03;
const unsigned short Exposure_Time_Relative_Control_len =1;

const unsigned int   Focus_Absolute_Control_flag = 0x3f;
const unsigned short Focus_Absolute_Control_len =2;

const unsigned int   Focus_Relative_Control_flag = 0x3f;
const unsigned short Focus_Relative_Control_len =2;

const unsigned int   Focus_Auto_Control_flag = 0x23;
const unsigned short Focus_Auto_Control_len =1;

const unsigned int   Iris_Absolute_Control_flag = 0x3f;
const unsigned short Iris_Absolute_Control_len =2;

const unsigned int   Iris_Relative_Control_flag = 0x03;
const unsigned short Iris_Relative_Control_len =1;

const unsigned int   Zoom_Absolute_Control_flag = 0x3f;
const unsigned short Zoom_Absolute_Control_len =2;

const unsigned int   Zoom_Relative_Control_flag = 0x3f;
const unsigned short Zoom_Relative_Control_len =3;

const unsigned int   PanTilt_Absolute_Control_flag = 0x3f;
const unsigned short PanTilt_Absolute_Control_len =8;

const unsigned int   PanTilt_Relative_Control_flag = 0x3f;
const unsigned short PanTilt_Relative_Control_len =4;

const unsigned int   Roll_Absolute_Control_flag = 0x3f;
const unsigned short Roll_Absolute_Control_len =2;

const unsigned int   Roll_Relative_Control_flag = 0x3f;
const unsigned short Roll_Relative_Control_len =2;

const unsigned int   Privacy_Shutter_Control_flag = 0x03;
const unsigned short Privacy_Shutter_Control_len =1;


//===========  processing =========================

const unsigned int   Backlight_Compensation_Control_flag = 0x3f;
const unsigned short Backlight_Compensation_Control_len =2;

const unsigned int   Brightness_Control_flag = 0x3f;
const unsigned short Brightness_Control_len =2;

const unsigned int   Contrast_Control_flag = 0x3f;
const unsigned short Contrast_Control_len =2;

const unsigned int   Gain_Control_flag = 0x3f;
const unsigned short Gain_Control_len =2;

const unsigned int   Power_Line_Frequency_flag = 0x23;
const unsigned short Power_Line_Frequency_len =1;

const unsigned int   Hue_Control_flag = 0x3f;
const unsigned short Hue_Control_len =2;

const unsigned int   Hue_Auto_Control_flag = 0x23;
const unsigned short Hue_Auto_Control_len =1;

const unsigned int   Saturation_Control_flag = 0x3f;
const unsigned short Saturation_Control_len =2;

const unsigned int   Sharpness_Control_flag = 0x3f;
const unsigned short Sharpness_Control_len =2;

const unsigned int   Gamma_Control_flag = 0x3f;
const unsigned short Gamma_Control_len =2;

const unsigned int   White_Balance_Temperature_Control_flag = 0x3f;
const unsigned short White_Balance_Temperature_Control_len =2;

const unsigned int   White_Balance_Temperature_Auto_Control_flag = 0x23;
const unsigned short White_Balance_Temperature_Auto_Control_len =1;

const unsigned int   White_Balance_Commponent_Control_flag = 0x3f;
const unsigned short White_Balance_Commponent_Control_len =4;

const unsigned int   White_Balance_Commponent_Auto_Control_flag = 0x23;
const unsigned short White_Balance_Commponent_Auto_Control_len =1;

const unsigned int   Digital_Multiplier_Control_flag = 0x3f;
const unsigned short Digital_Multiplier_Control_len =2;

const unsigned int   Digital_Multiplier_limit_Control_flag = 0x3f;
const unsigned short Digital_Multiplier_limit_Control_len =2;

const unsigned int   Analog_Video_Standard_Control_flag = 0x03;
const unsigned short Analog_Video_Standard_Control_len =1;

const unsigned int   Analog_Video_lock_status_Control_flag = 0x03;
const unsigned short Analog_Video_lock_status_Control_len =1;



int verbose = 0;
char g_dev_desc[128];
char g_config_desc[4096];
char g_string_desc[1024]={0x04,0x03,0x09,0x04};

static void print_endpoint(const struct libusb_endpoint_descriptor *endpoint)
{
	int i, ret;
	
	printf("      Endpoint:\n");
	printf("        bEndpointAddress:    %02xh\n", endpoint->bEndpointAddress);
	printf("        bmAttributes:        %02xh\n", endpoint->bmAttributes);
	printf("        wMaxPacketSize:      %u\n", endpoint->wMaxPacketSize);
	printf("        bInterval:           %u\n", endpoint->bInterval);
	printf("        bRefresh:            %u\n", endpoint->bRefresh);
	printf("        bSynchAddress:       %u\n", endpoint->bSynchAddress);
 
	
}

static void print_altsetting(const struct libusb_interface_descriptor *interface)
{
	uint8_t i;

	printf("    Interface:\n");
	printf("      bInterfaceNumber:      %u\n", interface->bInterfaceNumber);
	printf("      bAlternateSetting:     %u\n", interface->bAlternateSetting);
	printf("      bNumEndpoints:         %u\n", interface->bNumEndpoints);
	printf("      bInterfaceClass:       %u\n", interface->bInterfaceClass);
	printf("      bInterfaceSubClass:    %u\n", interface->bInterfaceSubClass);
	printf("      bInterfaceProtocol:    %u\n", interface->bInterfaceProtocol);
	printf("      iInterface:            %u\n", interface->iInterface);

    //hex_dump((char*)interface ,interface->bLength," Interface");
	for (i = 0; i < interface->bNumEndpoints; i++)
		print_endpoint(&interface->endpoint[i]);
}




static void print_interface(const struct libusb_interface *interface)
{
	int i;
	for (i = 0; i < interface->num_altsetting; i++)
		print_altsetting(&interface->altsetting[i]);
}

static void print_configuration(struct libusb_config_descriptor *config)
{
	uint8_t i;

	printf("  Configuration:\n");
	printf("    wTotalLength:            %u\n", config->wTotalLength);
	printf("    bNumInterfaces:          %u\n", config->bNumInterfaces);
	printf("    bConfigurationValue:     %u\n", config->bConfigurationValue);
	printf("    iConfiguration:          %u\n", config->iConfiguration);
	printf("    bmAttributes:            %02xh\n", config->bmAttributes);
	printf("    MaxPower:                %u\n", config->MaxPower);

 
	for (i = 0; i < config->bNumInterfaces; i++){
	   print_interface(&config->interface[i]);
	}
}

static void print_device(libusb_device *dev, libusb_device_handle *handle)
{
	struct libusb_device_descriptor desc;
	unsigned char string[256];
	const char *speed;
	int ret;
	uint8_t i;
/*
	switch (libusb_get_device_speed(dev)) {
	case LIBUSB_SPEED_LOW:		speed = "1.5M"; break;
	case LIBUSB_SPEED_FULL:		speed = "12M"; break;
	case LIBUSB_SPEED_HIGH:		speed = "480M"; break;
	case LIBUSB_SPEED_SUPER:	speed = "5G"; break;
	case LIBUSB_SPEED_SUPER_PLUS:	speed = "10G"; break;
	default:			speed = "Unknown";
	}
*/
	ret = libusb_get_device_descriptor(dev, &desc);
	if (ret < 0) {
		fprintf(stderr, "failed to get device descriptor");
		return;
	}

	printf("Dev (bus %u, device %u): %04X - %04X speed: %s\n",
	       libusb_get_bus_number(dev), libusb_get_device_address(dev),
	       desc.idVendor, desc.idProduct, speed);

	if (!handle)
		libusb_open(dev, &handle);

	if (handle) {
		if (desc.iManufacturer) {
			ret = libusb_get_string_descriptor_ascii(handle, desc.iManufacturer, string, sizeof(string));
			if (ret > 0)
				printf("  Manufacturer:              %s\n", (char *)string);
		}

		if (desc.iProduct) {
			ret = libusb_get_string_descriptor_ascii(handle, desc.iProduct, string, sizeof(string));
			if (ret > 0)
				printf("  Product:                   %s\n", (char *)string);
		}

		if (desc.iSerialNumber && verbose) {
			ret = libusb_get_string_descriptor_ascii(handle, desc.iSerialNumber, string, sizeof(string));
			if (ret > 0)
				printf("  Serial Number:             %s\n", (char *)string);
		}
	}

	if (verbose) {
		for (i = 0; i < desc.bNumConfigurations; i++) {
			struct libusb_config_descriptor *config;

			ret = libusb_get_config_descriptor(dev, i, &config);
			if (LIBUSB_SUCCESS != ret) {
				printf("  Couldn't retrieve descriptors\n");
				continue;
			}

			print_configuration(config);

			libusb_free_config_descriptor(config);
		}

		
	}

	if (handle)
		libusb_close(handle);
}


int uvc_query_ctrl(libusb_device_handle *usbdev,uint8_t bmRequestType, uint8_t bRequest,uint8_t unit,
	uint8_t intfnum, uint8_t cs, void *data, uint16_t size,int timeout)

{
    int ret ;
	uint16_t wValue = cs << 8;
    uint16_t wIndex = unit << 8 |intfnum ;
	ret = libusb_control_transfer(usbdev,bmRequestType,bRequest,wValue, wIndex, data,size,timeout); // Get all config desc
	if(ret < 0){
		printf("libusb_control_transfer error  \n");
		return -1;
	}
	return ret;
}

int _uvc_get_ctrl_data(libusb_device_handle *usbdev, int flags ,uint8_t unit,uint8_t intfnum, uint8_t cs ,int len,char* data)
{

    char* pdata = data;

	if (flags & UVC_CONTROL_GET_INFO) {
		uvc_query_ctrl(usbdev,0xa1,GET_INFO,unit,intfnum,cs,data,1,3000);
		pdata += 1;
	}	

	if (flags & UVC_CONTROL_GET_CUR) {
		uvc_query_ctrl(usbdev,0xa1,GET_CUR,unit,intfnum,cs,data,len,3000);
		pdata += len;
	}	

	if (flags & UVC_CONTROL_GET_MIN) {
		uvc_query_ctrl(usbdev,0xa1,GET_MIN,unit,intfnum,cs,data,len,3000);
		pdata += len;
	}	

	if (flags & UVC_CONTROL_GET_MAX) {
		uvc_query_ctrl(usbdev,0xa1,GET_MAX,unit,intfnum,cs,data,len,3000);
		pdata += len;
	}	

	if (flags & UVC_CONTROL_GET_RES) {
		uvc_query_ctrl(usbdev,0xa1,GET_RES,unit,intfnum,cs,data,len,3000);
		pdata += len;
	}	

	if (flags & UVC_CONTROL_GET_DEF) {
		uvc_query_ctrl(usbdev,0xa1,GET_DEF,unit,intfnum,cs,data,len,3000);
		pdata += len;
	}	
	return 0;


}


int uvc_get_ctrl_data(libusb_device_handle *usbdev,struct uvc_ctrl_info* uci ,char* data)
{

	if(uci->vci.Input.Scanning_mode == 1){
    	 _uvc_get_ctrl_data(usbdev, Scanning_Mode_Control_flag,uci->Input_ID,uci->Vc_Interface,
		 	CT_SCANNING_MODE_CONTROL,Scanning_Mode_Control_len,data) ;   	
	}

	if(uci->vci.Input.Auto_Exposure_mode== 1){
    	 _uvc_get_ctrl_data(usbdev, Auto_Exposure_Mode_Control_flag,uci->Input_ID,uci->Vc_Interface,
		 	CT_AE_MODE_CONTROL,Auto_Exposure_Mode_Control_len,data) ;   	
	}

	if(uci->vci.Input.Auto_Exposure_Priority== 1){
    	_uvc_get_ctrl_data(usbdev, Auto_Exposure_Priority_Control_flag,uci->Input_ID,uci->Vc_Interface,
			CT_AE_PRIORITY_CONTROL,Auto_Exposure_Priority_Control_len,data) ;   	
	}

	
	if(uci->vci.Input.Exposure_Time_Absolute== 1){
		_uvc_get_ctrl_data(usbdev, Exposure_Time_Absolute_Control_flag,uci->Input_ID,uci->Vc_Interface,
			CT_EXPOSURE_TIME_ABSOLUTE_CONTROL,Exposure_Time_Absolute_Control_len,data) ;	
	}

	if(uci->vci.Input.Exposure_Time_Relative== 1){
		_uvc_get_ctrl_data(usbdev, Exposure_Time_Relative_Control_flag,uci->Input_ID,uci->Vc_Interface,
			CT_EXPOSURE_TIME_RELATIVE_CONTROL,Exposure_Time_Relative_Control_len,data) ;	
	}

	if(uci->vci.Input.Focus_Absolute== 1){
		_uvc_get_ctrl_data(usbdev, Focus_Absolute_Control_flag,uci->Input_ID,uci->Vc_Interface,
			CT_FOCUS_ABSOLUTE_CONTROL,Focus_Absolute_Control_len,data) ;	
	}

	if(uci->vci.Input.Focus_Relative== 1){
		_uvc_get_ctrl_data(usbdev, Focus_Relative_Control_flag,uci->Input_ID,uci->Vc_Interface,
			CT_FOCUS_RELATIVE_CONTROL,Focus_Relative_Control_len,data) ;	
	}

	if(uci->vci.Input.Iris_Absolute== 1){
		_uvc_get_ctrl_data(usbdev, Iris_Absolute_Control_flag,uci->Input_ID,uci->Vc_Interface,
			CT_IRIS_ABSOLUTE_CONTROL,Iris_Absolute_Control_len,data) ;	
	}	

	if(uci->vci.Input.Iris_Relative== 1){
		_uvc_get_ctrl_data(usbdev, Iris_Relative_Control_flag,uci->Input_ID,uci->Vc_Interface,
			CT_IRIS_RELATIVE_CONTROL,Iris_Relative_Control_len,data) ;	
	}	
	

	return 0;


}






/*
int main(int argc, char *argv[])
{
	const char *device_name = NULL;
	libusb_device **devs;
	ssize_t cnt;
	int r, i;

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-v")) {
			verbose = 1;
		} else if (!strcmp(argv[i], "-d") && (i + 1) < argc) {
			i++;
			device_name = argv[i];
		} else {
			printf("Usage %s [-v] [-d </dev/bus/usb/...>]\n", argv[0]);
			printf("Note use -d to test libusb_wrap_sys_device()\n");
			return 1;
		}
	}

	r = libusb_init(NULL);
	if (r < 0)
		return r;

	if (device_name) {
		r = test_wrapped_device(device_name);
	} else {
		cnt = libusb_get_device_list(NULL, &devs);
		if (cnt < 0) {
			libusb_exit(NULL);
			return 1;
		}

		for (i = 0; devs[i]; i++)
			print_device(devs[i], NULL);

		libusb_free_device_list(devs, 1);
	}

	libusb_exit(NULL);
	return r;
}

*/
int uvc_prase_vc(char *src ,int src_len ,char* desc ,unsigned short* bcdUVC)
{
	
	int len  = src_len;
	int blen  = 0;
	int find_video_class = 0;
	int find_contrl_inf = 0;
	int vc_len = 0;
	char* vc_desc = desc;
	USB_DESCRIPTOR_HEADER *phead ;
  

	while(len > 0){

		phead = (USB_DESCRIPTOR_HEADER *)src;
		blen = phead->bLength ;
		if(phead->bDescriptorType == USB_DESCRIPTOR_TYPE_INTERFACE_ASSOCIATION){
		   USB_INTERFACE_ASSOCIATION_DESCRIPTOR* pIad =(USB_INTERFACE_ASSOCIATION_DESCRIPTOR *)phead;
		   if(pIad->bFunctionClass == USB_INTERFACE_CC_VIDEO && pIad->bFunctionSubClass == USB_INTERFACE_VC_SC_VIDEO_INTERFACE_COLLECTION){ //Video Interface Collection
				find_video_class = 1;
		   }
		}


		if(phead->bDescriptorType == USB_DESCRIPTOR_TYPE_INTERFACE){
			USB_INTERFACE_DESCRIPTOR *intface =(USB_INTERFACE_DESCRIPTOR *)src;
			if(intface->bInterfaceClass == USB_INTERFACE_CC_VIDEO && intface->bInterfaceSubClass == USB_INTERFACE_VC_SC_VIDEOCONTROL){
				//uci->Vc_Interface = intface->bInterfaceNumber ;
				find_contrl_inf = 1;
			}

			if(intface->bInterfaceClass == USB_INTERFACE_CC_VIDEO && intface->bInterfaceSubClass == USB_INTERFACE_VC_SC_VIDEOSTREAMING){
				find_contrl_inf = 0;
				break;
			}
		}
	
		if(find_contrl_inf == 1){
			if(phead->bDescriptorType == USB_DESCRIPTOR_TYPE_VC_CS_INTERFACE){
				USB_CS_DESCRIPTOR_HEADER *cs = (USB_CS_DESCRIPTOR_HEADER *)src;
				if(cs->bDescriptorSubtype == USB_INTERFACE_SUBTYPE_VC_HEADER){
					USB_UVC_VC_HEADER_DESCRIPTOR *vch = (USB_UVC_VC_HEADER_DESCRIPTOR *)src;
					 printf("VC_HEADER = %x\n",vch->bcdUVC);
					 vch->bcdUVC = 0x0150;
					 *bcdUVC = vch->bcdUVC;
					 memcpy(vc_desc,src,cs->bLength);
					 vc_desc+= cs->bLength;
					 vc_len += cs->bLength;
					 
				}
				
				if(cs->bDescriptorSubtype == USB_INTERFACE_SUBTYPE_VC_INPUT_TERMINAL){
					 printf("VC_INPUT_TERMINAL \n");
					 memcpy(vc_desc,src,cs->bLength);
					 vc_desc+= cs->bLength;
					 vc_len += cs->bLength;
				}

				if(cs->bDescriptorSubtype == USB_INTERFACE_SUBTYPE_VC_OUTPUT_TERMINAL){
					printf("VC_OUTPUT_TERMINAL \n");
					memcpy(vc_desc,src,cs->bLength);
					vc_desc+= cs->bLength;
					vc_len += cs->bLength;
				}

				if(cs->bDescriptorSubtype == USB_INTERFACE_SUBTYPE_VC_SELECTOR_UNIT){
					printf("VC_SELECTOR_UNIT \n");
					memcpy(vc_desc,src,cs->bLength);
					vc_desc+= cs->bLength;
					vc_len += cs->bLength;
				}

				if(cs->bDescriptorSubtype == USB_INTERFACE_SUBTYPE_VC_PROCESSING_UNIT){
					printf("VC_PROCESSING_UNIT \n");
					memcpy(vc_desc,src,cs->bLength);
					vc_desc+= cs->bLength;
					vc_len += cs->bLength;
				   
				}

				if(cs->bDescriptorSubtype == USB_INTERFACE_SUBTYPE_VC_EXTENSION_UNIT){
					printf("VC_EXTENSION_UNIT \n");
					memcpy(vc_desc,src,cs->bLength);
					vc_desc+= cs->bLength;
					vc_len += cs->bLength;
				}

				if(cs->bDescriptorSubtype == USB_INTERFACE_SUBTYPE_VC_ENCODING_UNIT){
					printf("VC_ENCODING_UNIT \n");
					memcpy(vc_desc,src,cs->bLength);
					vc_desc+= cs->bLength;
					vc_len += cs->bLength;
					
				}
				
			}
		}
     
	    src += blen;
        len -= blen;
		
	}
	return vc_len;

}


void uvc_prase_info(char *src ,int src_len ,struct uvc_ctrl_info *uci)
{
	
	int len  = src_len;
	int blen  = 0;
	int find_video_class = 0;
	int find_contrl_inf = 0;
	USB_DESCRIPTOR_HEADER *phead ;
  

	while(len > 0){

		phead = (USB_DESCRIPTOR_HEADER *)src;
		blen = phead->bLength ;
		if(phead->bDescriptorType == USB_DESCRIPTOR_TYPE_INTERFACE_ASSOCIATION){
		   USB_INTERFACE_ASSOCIATION_DESCRIPTOR* pIad =(USB_INTERFACE_ASSOCIATION_DESCRIPTOR *)phead;
		   if(pIad->bFunctionClass == USB_INTERFACE_CC_VIDEO && pIad->bFunctionSubClass == USB_INTERFACE_VC_SC_VIDEO_INTERFACE_COLLECTION){ //Video Interface Collection
				find_video_class = 1;
		   }
		}


		if(phead->bDescriptorType == USB_DESCRIPTOR_TYPE_INTERFACE){
			USB_INTERFACE_DESCRIPTOR *intface =(USB_INTERFACE_DESCRIPTOR *)src;
			if(intface->bInterfaceClass == USB_INTERFACE_CC_VIDEO && intface->bInterfaceSubClass == USB_INTERFACE_VC_SC_VIDEOCONTROL){
				uci->Vc_Interface = intface->bInterfaceNumber ;
				find_contrl_inf = 1;
			}

			if(intface->bInterfaceClass == USB_INTERFACE_CC_VIDEO && intface->bInterfaceSubClass == USB_INTERFACE_VC_SC_VIDEOSTREAMING){
				find_contrl_inf = 0;
			}
		}
	
		if(find_contrl_inf == 1){
			if(phead->bDescriptorType == USB_DESCRIPTOR_TYPE_VC_CS_INTERFACE){
				USB_CS_DESCRIPTOR_HEADER *cs = (USB_CS_DESCRIPTOR_HEADER *)src;
				if(cs->bDescriptorSubtype == USB_INTERFACE_SUBTYPE_VC_INPUT_TERMINAL){
					USB_UVC_CAMERA_TERMINAL *ct = (USB_UVC_CAMERA_TERMINAL *)src;
					if(ct->wTerminalType == USB_UVC_ITT_CAMERA){
						printf("ct->bTerminalID = %d \n",ct->bTerminalID);
						uci->Input_ID = ct->bTerminalID;
						memcpy(uci->vci.bmControl,ct->bmControls,ct->bControlBitfieldSize);
					}
				}

				if(cs->bDescriptorSubtype == USB_INTERFACE_SUBTYPE_VC_PROCESSING_UNIT){
					USB_UVC_VC_PROCESSING_UNIT *cp = (USB_UVC_VC_PROCESSING_UNIT *)src;
					printf(" cp->bUnitID = %d \n", cp->bUnitID);
					uci->Processing_ID = cp->bUnitID;
					memcpy(uci->vcp.bmControl,cp->bmControls,cp->bControlSize);
				}
			}
		}
     
	    src += blen;
        len -= blen;
		
	}

}

int uvc_generate_config_desc(char *src ,int src_len ,char *dst)
{
	int config_size = 0;
	int vs_size     = 0;
	int len  = src_len;
	int blen  = 0;
	int vc_endpoint = 0;
    int vs_endpoint = 0;
	int video_desc  = 0;
	USB_DESCRIPTOR_HEADER *phead ;
  
	
	while(len > 0){
	   phead = (USB_DESCRIPTOR_HEADER *)src;
       blen = phead->bLength ;
       if(phead->bDescriptorType == USB_DESCRIPTOR_TYPE_CONFIGURATION){
           memcpy(dst,(char*)phead,blen);
		   dst += blen;
		   config_size += blen;
       }
	   if(video_desc == 1){
		   if(phead->bDescriptorType == 0x24){ // video contrl interface  video streaming 
		       USB_UVC_VS_INPUT_HEADER_DESCRIPTOR *pch = (USB_UVC_VS_INPUT_HEADER_DESCRIPTOR *)src;
			   if(pch->bDescriptorSubType == 0x01){
               		if(vs_endpoint == 1){
                       pch->bEndpointAddress = 0x82;
               		}
			   }
			   memcpy(dst,(char*)phead,blen);
			   dst += blen;
			   config_size += blen;
		   }
		   
		   if(phead->bDescriptorType == 0x25){ // video contrl endpontdescrip
			   memcpy(dst,(char*)phead,blen);
			   dst += blen;
			   config_size += blen;
		   }
	   }
	   if(phead->bDescriptorType == USB_DESCRIPTOR_TYPE_INTERFACE_ASSOCIATION){
           USB_INTERFACE_ASSOCIATION_DESCRIPTOR* pIad =(USB_INTERFACE_ASSOCIATION_DESCRIPTOR *)phead;
		   if(pIad->bFunctionClass == USB_INTERFACE_CC_VIDEO && pIad->bFunctionSubClass == 0x03){ //Video Interface Collection
		        memcpy(dst,(char*)phead,blen);
				 dst += blen;
		   		config_size += blen;
				video_desc   = 1;
		   	}else{
                video_desc   = 0; 
		   	}
       }

	   if(phead->bDescriptorType == USB_DESCRIPTOR_TYPE_INTERFACE){
		   USB_INTERFACE_DESCRIPTOR *inf = (USB_INTERFACE_DESCRIPTOR *)phead;
		   if(inf->bInterfaceClass == USB_INTERFACE_CC_VIDEO && inf->bInterfaceSubClass == 0x01){ // vide0 contrl
		   		if(inf->bNumEndpoints != 0x01 )
			  		inf->bNumEndpoints == 0x01;
	        	vc_endpoint = 1;
				memcpy(dst,(char*)phead,blen);
				dst += blen;
	   			config_size += blen;
		   }
		   if(inf->bInterfaceClass == USB_INTERFACE_CC_VIDEO && inf->bInterfaceSubClass == 0x02){ // vide0  streaming
		        if(vs_endpoint == 0){
					inf->bNumEndpoints == 0x01;
	                inf->bAlternateSetting = 0;
	                memcpy(dst,(char*)phead,blen);
					dst += blen;
		   			config_size += blen;
					USB_ENDPOINT_DESCRIPTOR bulkep;
					bulkep.bLength = 0x07;
					bulkep.bDescriptorType = USB_DESCRIPTOR_TYPE_ENDPOINT;
					bulkep.bEndpointAddress = 0x82;
					bulkep.bmAttributes = 0x02 ;//bulk
					bulkep.wMaxPacketSize = 0x0200;
					bulkep.bInterval = 0;
					memcpy(dst,(char*)&bulkep,0x07);
					dst += 0x07;
		   			config_size += 0x07;
	                vs_endpoint = 1; 
		        }
		   }

	   }
	   if(phead->bDescriptorType == USB_DESCRIPTOR_TYPE_ENDPOINT){
	   		USB_ENDPOINT_DESCRIPTOR* ep = (USB_ENDPOINT_DESCRIPTOR*)phead;
			if(vc_endpoint == 1){
                ep->bEndpointAddress = 0x81;
				ep->bmAttributes     = 0x03; //interrupt
                ep->wMaxPacketSize   = 0x0010;
				memcpy(dst,(char*)phead,blen);
				dst += blen;
	   			config_size += blen;
				vc_endpoint = 0 ;
			}

	   }



	   
       //printf("blen = %d \n",blen); 
	   src += blen;
       len -= blen;
	
       

	}

    return config_size;
}

int Get_uvc_ctrol_info(void *ptr)
{
	int i ;
	int c ;
	int ret = -1;
	ssize_t cnt=0;
	int busid = 0 , devid = 0 ;
	libusb_context *ctx = NULL;
	libusb_device **list;
	libusb_device_handle *handle = NULL;
	struct libusb_device_descriptor desc;
    libusb_init(&ctx);
	struct uvc_ctrl_info *uci =(struct uvc_ctrl_info *)ptr;
	cnt = libusb_get_device_list(ctx, &list);
	for(i = 0; i < cnt; i++){
		
		libusb_device *device = list[i];
		busid = libusb_get_bus_number(device);
		devid = libusb_get_device_address(device);
	    ret = libusb_get_device_descriptor( device, &desc );
		if(ret != 0){
			ret = -1;
			break;
		}
	
		if(desc.idVendor == 0x1d6b)
			continue;

	
	    struct libusb_config_descriptor *config = NULL;
		ret = libusb_get_config_descriptor(device, 0, &config);
		if(ret != 0){
			ret = -1;
			break;
		}
			
		if(config->extra_length > 0){
			
            USB_INTERFACE_ASSOCIATION_DESCRIPTOR *IAD = (USB_INTERFACE_ASSOCIATION_DESCRIPTOR *)config->extra ;
            if(IAD->header.bDescriptorType != 0x0B) //IDA
                continue;
            if(IAD->bFunctionClass != 0x0E) //(video)
                continue;
			if(IAD->bFunctionSubClass!= 0x03) //(video)
                continue;
			if (!handle)
				libusb_open(device, &handle);
			char *data = malloc(config->wTotalLength);
			ret = libusb_control_transfer(handle,0x80,0x06,0x0200, 0, data, config->wTotalLength, 3000); // Get all config desc
			if(ret < 0){
				printf("libusb_control_transfer error  n");
				free(data);
				break;
			}
			if (handle){
				libusb_close(handle);
				handle = NULL;
			}
			
			uvc_prase_info(data,config->wTotalLength,uci);
			free(data);
			ret = 0;
			break;
			
		}
		libusb_free_config_descriptor(config);
  	}
	libusb_free_device_list(list, 1);
	return ret;
    
}



void usb_parser()
{

    int i ;
	int c ;
	int ret;
	ssize_t cnt=0;
	char *ptrdesc = g_config_desc ;
	int busid = 0 , devid = 0 ;
	unsigned char string[1024];
	libusb_context *ctx = NULL;
	libusb_device **list;
	libusb_device_handle *handle = NULL;
	struct libusb_device_descriptor desc;
    libusb_init(&ctx);
	cnt = libusb_get_device_list(ctx, &list);
	
	
	printf("cnt = %d \n",cnt);
	for(i = 0; i < cnt; i++){
		
	
		
		libusb_device *device = list[i];
		
		busid = libusb_get_bus_number(device);
		devid = libusb_get_device_address(device);
		//printf("busid = %d devid = %d \n",busid,devid);
	
	    ret = libusb_get_device_descriptor( device, &desc );
		
		if(ret != 0)
			return ;
		if(desc.idVendor == 0x1d6b)
			continue;

	
	    struct libusb_config_descriptor *config = NULL;
		ret = libusb_get_config_descriptor(device, 0, &config);
		if(ret != 0)
			return ;
		
		
		if(config->extra_length > 0){
            USB_INTERFACE_ASSOCIATION_DESCRIPTOR *IAD = (USB_INTERFACE_ASSOCIATION_DESCRIPTOR *)config->extra ;
            if(IAD->header.bDescriptorType != 0x0B) //IDA
                continue;
            if(IAD->bFunctionClass != 0x0E) //(video)
                continue;
			if(IAD->bFunctionSubClass!= 0x03) //(video)
                continue;
			
			if (!handle)
				libusb_open(device, &handle);
			char *data = malloc(config->wTotalLength);
			ret = libusb_control_transfer(handle,0x80,0x06,0x0200, 0, data, config->wTotalLength, 3000); // Get all config desc
		
			if(ret < 0)
				printf("libusb_control_transfer error  n");
			memcpy(g_dev_desc,(char*)&desc ,desc.bLength);
			hex_dump(g_dev_desc ,desc.bLength,"Dev desc");
			hex_dump((char*)data,config->wTotalLength,"Dev config");
            ret = uvc_generate_config_desc(data,config->wTotalLength,g_config_desc);
			if(ret > 0){
			  USB_CONFIG_DESCRIPTOR *pconfig = (USB_CONFIG_DESCRIPTOR *)g_config_desc;
			  pconfig->bNumInterfaces = 0x02;
			  pconfig->wTotalLength = ret;
			  hex_dump((char*)g_config_desc,ret,"generate config");
			}  
			free(data);
			if (handle) {
				int len =0;
				int offset = 4;
				if (desc.iManufacturer) {
					ret = libusb_get_string_descriptor_ascii(handle, desc.iManufacturer, string, sizeof(string));
					if (ret > 0)
						printf("  Manufacturer:              %s\n", (char *)string);
					len = strlen(string);
					g_string_desc[offset] = len + 2;
					g_string_desc[offset + 1] = 0x03;
					offset +=2;
					memcpy(g_string_desc+offset,string,len);
					offset += len; 
				}
				

				if (desc.iProduct) {
					ret = libusb_get_string_descriptor_ascii(handle, desc.iProduct, string, sizeof(string));
					if (ret > 0)
						printf("  Product:                   %s\n", (char *)string);
					len = strlen(string);
					g_string_desc[offset] = len + 2;
					g_string_desc[offset + 1] = 0x03;
					offset +=2;
					memcpy(g_string_desc+offset,string,len);
					offset += len; 
				}
				if(offset > 0)
                	hex_dump((char*)g_string_desc,offset,"string desc");
			}
			
			if (handle){
				libusb_close(handle);
				handle = NULL;
			}
			
		}
		libusb_free_config_descriptor(config);
  	}
	libusb_free_device_list(list, 1);
	


}


