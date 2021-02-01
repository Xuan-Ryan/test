#ifndef __UVCDESC_H
#define __UVCDESC_H


#define UVC_CONTROL_GET_INFO (1 << 0) 
#define UVC_CONTROL_GET_CUR	 (1 << 1) 
#define UVC_CONTROL_GET_MIN	 (1 << 2)
#define UVC_CONTROL_GET_MAX	 (1 << 3)
#define UVC_CONTROL_GET_RES	 (1 << 4) 
#define UVC_CONTROL_GET_DEF	 (1 << 5) 

int Get_uvc_ctrol_info(void *ptr);
void usb_parser();
int uvc_prase_vc(char *src ,int src_len ,char* desc,unsigned short* bcdUVC);









#endif

