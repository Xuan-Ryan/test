#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "uvcdev.h"
#include "mct_gadget.h"

int uvc_fd = -1;

//static pthread_mutex_t uvc_mutex = PTHREAD_MUTEX_INITIALIZER;

struct uvc_state mct_uvc_data;
unsigned char gSetDirBuff[MCT_UVC_NOTIFY_SIZE];
unsigned char gGetDirBuff[MCT_UVC_NOTIFY_SIZE];

int init_uvc_dev(void)
{
	DBG_MSG("mct_gadget: Initial UVC\n");
	uvc_fd = open(UVC_DEV_NAME, O_RDWR);
	if (uvc_fd < 0) {
		DBG_MSG("mct_gadget: Can't open uvc device\n");
		return -1;
	}

	memset(&mct_uvc_data, '\0', sizeof(struct uvc_state));
	mct_uvc_data.status = 2;
	memset(gSetDirBuff, '\0', sizeof(gSetDirBuff));
	memset(gGetDirBuff, '\0', sizeof(gGetDirBuff));

	//pthread_mutex_init(&uvc_mutex, 0);
	return 0;
}

static int cnt = 0;
int write_uvc(char * buffer, int size)
{
	int offset = 0;
	int page = JUVC_PAGE_SIZE;
	char first_buf[JUVC_PAGE_SIZE];
	char counter[2] = {0x82, 0x83};

	if (size <= 0)
		return 0;

	//if (mct_uvc_data.status != 1)
	//	return 0;

	if (uvc_fd > 0) {
		/*  incrase 2 for header  */
		size += 2;
		first_buf[0] = 0x02;
		first_buf[1] = counter[cnt];
		cnt = ++cnt%2;

		if (JUVC_PAGE_SIZE >= size+2) {
			/*  packet too small  */
			page = size + 2;
			memcpy(&first_buf[2], buffer, size);
		} else {
			memcpy(&first_buf[2], buffer, JUVC_PAGE_SIZE-2);
		}

		offset = JUVC_PAGE_SIZE-2;
		write(uvc_fd, first_buf, page);
		while (offset < size) {
			if (offset+JUVC_PAGE_SIZE > size) {
				page = size-offset;
			}
			write(uvc_fd, buffer+offset, page);
			offset += JUVC_PAGE_SIZE;
		}
	}
	return offset;
}

int write_uvc4yuv(char * buffer, int size)
{
	int ret = 0;
	int offset = 0;
	int page = JUVC_PAGE_SIZE;
	char inner_buf[JUVC_PAGE_SIZE];
	char counter[2] = {0x82, 0x83};

	if (size <= 0)
		return 0;

	if (mct_uvc_data.status != 1)
		return 0;

	if (uvc_fd > 0) {
		inner_buf[0] = 0x02;
		inner_buf[1] = 0x80;

		while (offset < size) {
			if (JUVC_PAGE_SIZE >= size+2) {
				/*  packet too small  */
				page = size + 2;
				memcpy(&inner_buf[2], buffer, size);
				offset = size;
			} else {
				memcpy(&inner_buf[2], buffer+offset, JUVC_PAGE_SIZE-2);
				if (offset+(JUVC_PAGE_SIZE-2) > size) {
					page = size-offset;
					offset += page;
					page += 2;
					inner_buf[1] = counter[cnt];
					cnt = ++cnt%2;
				} else {
					page = JUVC_PAGE_SIZE;
					offset += JUVC_PAGE_SIZE-2;
					if (offset == size) {
						inner_buf[1] = counter[cnt];
						cnt = ++cnt%2;
					}
				}
			}

			write(uvc_fd, inner_buf, page);
		}
	}
	return ret;
}

int ioctl_uvc(int ioctrl_cmd, void * data)
{
	UVC_CAPABILITIES_DATA * cap_data = (UVC_CAPABILITIES_DATA *)data;
	int i = 0;
	if (uvc_fd > 0) {
		switch(ioctrl_cmd) {
		case UVC_SET_MANUFACTURER:
			ioctl(uvc_fd, UVC_SET_MANUFACTURER, (char *)data);
			return 0;
		case UVC_SET_PRODUCT_NAME:
			ioctl(uvc_fd, UVC_SET_PRODUCT_NAME, (char *)data);
			return 0;
		case UVC_SET_PIDVID:
			ioctl(uvc_fd, UVC_SET_PIDVID, *((int *)data));
			return 0;
		case CAMERA_STS_CHANGE:
			//pthread_mutex_lock(&uvc_mutex);
			DBG_MSG("enter ioctl : ");
			ioctl(uvc_fd, CAMERA_STS_CHANGE, cap_data);
			ioctl(uvc_fd, APPLY_DESC_CHANGE, NULL);
			DBG_MSG("ioctl done\n");
			//pthread_mutex_unlock(&uvc_mutex);
			return 0;
		case CTRLEP_WRITE:
			ioctl(uvc_fd, CTRLEP_WRITE, (char *)data);
			return 0;
		default:
			ioctl(uvc_fd, ioctrl_cmd, data);
			return 0;
		}
	}
	return -1;
}

int get_status(void)
{
#if MCT_PROTORCOL_VER_2
	char data[256];
	struct uvc_state * ptr;
#endif

	int i = 0;
	if (uvc_fd > 0) {
		//pthread_mutex_lock(&uvc_mutex);
		DBG_MSG("enter get_status : \n");
		DBG_MSG("------------- mct_gadget -------------\n");
#if MCT_PROTORCOL_VER_2
		ioctl(uvc_fd, UVC_GET_STAT, data);
		ptr = (struct uvc_state *)data;

		DBG_MSG("mct_gadget: %02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X\n", ptr->setuptoken_8byte[0], ptr->setuptoken_8byte[1], ptr->setuptoken_8byte[2], ptr->setuptoken_8byte[3], ptr->setuptoken_8byte[4], ptr->setuptoken_8byte[5], ptr->setuptoken_8byte[6], ptr->setuptoken_8byte[7]);
		DBG_MSG("mct_gadget: bmRequestType = %02x\n", ptr->direct_control.bmRequestType);
		DBG_MSG("mct_gadget: bRequest = %02x\n", ptr->direct_control.bRequest);
		DBG_MSG("mct_gadget: wValue = %04x\n", ptr->direct_control.wValue);
		DBG_MSG("mct_gadget: wIndex = %04x\n", ptr->direct_control.wIndex);
		DBG_MSG("mct_gadget: wLength = %d\n", ptr->direct_control.wLength);
		if (ptr->direct_control.wIndex == 0 &&
		    ptr->direct_control.bmRequestType == 0 &&
		    ptr->direct_control.bRequest == 0 &&
		    ptr->direct_control.wValue == 0 &&
		    ptr->direct_control.wLength == 0) {  /*  for video stream  */
			memcpy(&mct_uvc_data, data, sizeof(mct_uvc_data));
		} else {
			if (ptr->direct_control.bmRequestType&0x20 == 0x20) {  /*  CLASS  */
				if (ptr->direct_control.bmRequestType & 0x80) {  /*  get  */
					memcpy(gGetDirBuff, data, sizeof(gGetDirBuff));
				} else {
					memcpy(gSetDirBuff, data, sizeof(gSetDirBuff));
				}
				memcpy(&mct_uvc_data.direct_control, data, 8);
			}
		}

		if (ptr->direct_control.wIndex == 0 &&
		    ptr->direct_control.bmRequestType == 0 &&
		    ptr->direct_control.bRequest == 0 &&
		    ptr->direct_control.wValue == 0 &&
		    ptr->direct_control.wLength == 0) {  /*  for video stream  */
#else
			ioctl(uvc_fd, UVC_GET_STAT, &mct_uvc_data);
#endif
#ifdef GADGET_DEBUG
			DBG_MSG("mct_gadget: status = %d\n", mct_uvc_data.status);
			for (i=0; i<8; i++) {
				DBG_MSG("'%02X'", mct_uvc_data.prob_ctrl[i]&0xFF);
				if (i!=0 && ((i+1)%8)== 0) {
					DBG_MSG("\n");
				} else {
					DBG_MSG("-");
				}
			}
			DBG_MSG("\n");
#endif
#if MCT_PROTORCOL_VER_2
		}
#endif
		//pthread_mutex_unlock(&uvc_mutex);
		return 0;
	}
	return -1;
}

int close_uvc(void)
{
	DBG_MSG("mct_gadget: Close UVC\n");

	if (uvc_fd > 0) {
		close(uvc_fd);
		uvc_fd = -1;
	}

	//pthread_mutex_destroy(&uvc_mutex);
	return 0;
}
