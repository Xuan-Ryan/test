/*
 * composite.c - infrastructure for Composite USB Gadgets
 *
 * Copyright (C) 2006-2008 David Brownell
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* #define VERBOSE_DEBUG */

#include <linux/kallsyms.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/fs.h>

#include <linux/usb/composite.h>
#include "mnspdef.h"

#define VENDOR_REQ_SPI_ROM_WRITE				0x01	//	TBD																			TBD
#define VENDOR_REQ_I2C_WRITE					0x02	//	address				???					???									???
#define VENDOR_REQ_SET_MONITOR_CTRL				0x03	//	View Index			0/off 1/on																				View Index:0:primary;  1:secondary
#define VENDOR_REQ_SET_CURSOR1_POS				0x04	//	xPos				yPos				0									None
#define VENDOR_REQ_SET_CURSOR1_STATE			0x05	//	Cursor index(0~9)	0/off 1/on			0									None
#define VENDOR_REQ_SET_CURSOR2_POS				0x06	//	xPos				yPos				0									None
#define VENDOR_REQ_SET_CURSOR2_STATE			0x07	//	cursor index(0~9)	0/off 1/on			0									None
#define VENDOR_REQ_SET_RESOLUTION				0x08	//	View Index			index				0									None
#define VENDOR_REQ_SET_CURSOR_SHAPE				0x09	//	Cursor index(0~9)	display set			Header+shape data					Header+shape data
#define VENDOR_REQ_SET_CURSOR1_SHAPE			0x10	//	Cursor index(0~9)	data offset			datal length						
#define VENDOR_REQ_SET_CURSOR2_SHAPE			0x11	//	Cursor index(0~9)	data offset			datal length						
#define VENDOR_REQ_SET_RESOLUTION_DETAIL_TIMING	0x12	//	View Index			0					Size of RESOLUTIONTIMING
#define VENDOR_REQ_SET_CANCEL_BULK_OUT			0x14	//	0					0					0									None

#define VENDOR_REQ_RESET_HARDWARE				0x30	//	0x0000				ResetLevel			0x0000								None							Reset hardware.
#define VENDOR_REQ_SOFTWARE_READY				0x31	//	0x0000				Signature			0x0000								None							Software ready.
#define VENDOR_REQ_WRITE_ROM_DATA				0x32	//	Region				DataOffset			Length								Data							Region-0:boot data; 1:current image data; 2: user data

#define VENDOR_REQ_GET_EDID						0x80	//	Offset				ViewIndex			<=256								EDID
#define VENDOR_REQ_I2C_READ						0x81	//	Address				???					???									???
#define VENDOR_REQ_SPI_ROM_READ					0x82	//	TBD																			TBD
#define VENDOR_REQ_GET_MAC_ID					0x83	//	0,					0					6									Data
#define VENDOR_REQ_GET_RESOLUTION_TABLE_NUM		0x84	//	View Index			0					4									Number
#define VENDOR_REQ_GET_RESOLUTION_TABLE_DATA	0x85	//	View Index			0					Num*sizeof(resolution_entry)		USBD_DISPLAY_RESOLUTION_INFO
#define VENDOR_REQ_RESET_JPEG_ENGINE			0x86	//	0					0					0									None
#define VENDOR_REQ_QUERY_MONITOR_CONNECTION_STATUS	0x87//	View Index			0					1									0:disconnect;  1:connect.
#define VENDOR_REQ_QUERY_VIDEO_RAM_SIZE			0x88	//	0					0					1									Unit: MB
#define VENDOR_REQ_GET_RESOLUTION_TIMING_TABLE	0x89	//	View Index			StartIndex			Num*sizeof(timing)					RESOLUTIONTIMING

#define VENDOR_REQ_AUD_GET_NUM_ENGINES			0xA0	//	0x0000				0x0000				sizeof (UINT8)						UINT8 NumDmaEngines				Get number of DMA engines.
#define VENDOR_REQ_AUD_GET_ENGINE_CAPS			0xA1	//	0x0000				EngineIndex			sizeof (T6AUD_ENGINECAPS)			T6AUD_ENGINECAPS EngineCaps		Get DMA engine capabilities.
#define VENDOR_REQ_AUD_GET_NODE_CAPS			0xA2	//	NodeIndex			EngineIndex			sizeof (T6AUD_NODECAPS)				T6AUD_NODECAPS NodeCaps			Get node capabilities.
#define VENDOR_REQ_AUD_GET_NODE_VALUE			0xA3	//	NodeIndex			EngineIndex			sizeof (T6AUD_NODEVALUE)			T6AUD_NODEVALUE Value			Get node value.
#define VENDOR_REQ_AUD_SET_NODE_VALUE			0x23	//	NodeIndex			EngineIndex			sizeof (T6AUD_NODEVALUE)			T6AUD_NODEVALUE Value			Set node value.
#define VENDOR_REQ_AUD_GET_ENGINE_STATE			0xA4	//	0x0000				EngineIndex			sizeof (T6AUD_GETENGINESTATE)		T6AUD_GETENGINESTATE State		Get DMA engine state.
#define VENDOR_REQ_AUD_SET_ENGINE_STATE			0x24	//	0x0000				EngineIndex			sizeof (T6AUD_SETENGINESTATE)		T6AUD_SETENGINESTATE State		Set DMA engine state.
#define VENDOR_REQ_AUD_GET_FORMAT_LIST			0xA5	//	Start Index			EngineIndex			NumFmts * sizeof (T6AUD_FORMAT)		T6AUD_FORMAT array				Get format list of the DMA engine.
#define VENDOR_REQ_AUD_SET_ENGINE_INTERFACE		0x26	//	see doc				EngineIndex			0									None

#define VENDOR_REQ_GET_VERSION					0xB0	//	0x0000				Type				Variant								Variant							Type: see T6VER_TYPE_XXX
#define VENDOR_REQ_GET_FUNC_ID					0xB1	//	0x0000				Signature			sizeof (T6FUNCID)					T6FUNCID						See tT6FuncID
#define VENDOR_REQ_READ_ROM_DATA				0xB2	//	Region				DataOffset			Length								Data							Region-0:boot data; 1:current image data; 2: user data
#define VENDOR_REQ_QUERY_SECTION_DATA			0xB3	//	Signature			DataOffset			Length								Data							Query section data.
#define VENDOR_REQ_QUERY_SIZE					0xB4

//UVC COMMAND
#define UVC_SET_CUR					0x01
#define UVC_GET_CUR					0x81
#define UVC_GET_MIN					0x82
#define UVC_GET_MAX					0x83
#define UVC_GET_RES					0x84
#define UVC_GET_LEN					0x85
#define UVC_GET_INFO					0x86
#define UVC_GET_DEF					0x87
#define UVC_GET_STAT					0x88
#define UVC_CLIENT_LOST					0x89
#define QUERY_SETUP_DATA				0xF0
#define CAMERA_STS_CHANGE				0xF1
#define APPLY_DESC_CHANGE				0xF2
#define INTREP_WRITE					0xF3
#define UVC_SET_MANUFACTURER				0xF4
#define UVC_SET_PRODUCT_NAME				0xF5
#define UVC_SET_PIDVID					0xF6
#define CTRLEP_WRITE					0xF7
#define UVC_EP0_SET_HALT				0xF8
#define UVC_EP0_CLEAR_HALT				0xF9
#define UVC_SET_VC_DESC					0xFA
#define UVC_GET_UVC_VER					0xFB

extern __u16 currentbcdUVC;
extern void uvc_change_desc_compatable(void);
extern void uvc_get_bcdUVC(void);

//End UVC COMMAND

//  Tiger
__u32 client_connected = 0;
__u8 current_b_request;
unsigned current_w_value;

unsigned int g_uvc_mjpeg_res_bitflag = 0x08;
unsigned int g_uvc_h264_res_bitflag= 0x0;
unsigned int g_uvc_yuv_res_bitflag= 0x0;
unsigned int g_uvc_nv12_res_bitflag= 0x0;
unsigned int g_uvc_i420_res_bitflag= 0x0;
unsigned int g_uvc_m420_res_bitflag= 0x0;

unsigned int g_uvc_camera_terminal_controlbit = 0x0;
unsigned int g_uvc_processing_controlbit = 0x0;

module_param(g_uvc_mjpeg_res_bitflag, uint, S_IRWXU|S_IRWXG);
MODULE_PARM_DESC(g_uvc_mjpeg_res_bitflag, "resolution for mjpeg");
module_param(g_uvc_h264_res_bitflag, uint, S_IRWXU|S_IRWXG);
MODULE_PARM_DESC(g_uvc_h264_res_bitflag, "resolution for h264");
module_param(g_uvc_yuv_res_bitflag, uint, S_IRWXU|S_IRWXG);
MODULE_PARM_DESC(g_uvc_yuv_res_bitflag, "resolution for yuv");
module_param(g_uvc_nv12_res_bitflag, uint, S_IRWXU|S_IRWXG);
MODULE_PARM_DESC(g_uvc_nv12_res_bitflag, "resolution for nv12");
module_param(g_uvc_i420_res_bitflag, uint, S_IRWXU|S_IRWXG);
MODULE_PARM_DESC(g_uvc_i420_res_bitflag, "resolution for i420");
module_param(g_uvc_m420_res_bitflag, uint, S_IRWXU|S_IRWXG);
MODULE_PARM_DESC(g_uvc_m420_res_bitflag, "resolution for m420");

module_param(g_uvc_camera_terminal_controlbit, uint, S_IRWXU|S_IRWXG);
MODULE_PARM_DESC(g_uvc_camera_terminal_controlbit, "camera capability");
module_param(g_uvc_processing_controlbit, uint, S_IRWXU|S_IRWXG);
MODULE_PARM_DESC(g_uvc_processing_controlbit, "camera capability");

/*
 * The code in this file is utility code, used to build a gadget driver
 * from one or more "function" drivers, one or more "configuration"
 * objects, and a "usb_composite_driver" by gluing them together along
 * with the relevant device-wide data.
 */

/* big enough to hold our biggest descriptor */
#define USB_BUFSIZ	1024

static struct usb_composite_driver *composite;
static unsigned int bit_rate_control = 10;
static unsigned int compress_rate = 70;
static unsigned int skip_rate_control = 7800;
extern wait_queue_head_t gUVCwq;
wait_queue_head_t gUVCioctl;

/* Some systems will need runtime overrides for the  product identifers
 * published in the device descriptor, either numbers or strings or both.
 * String parameters are in UTF-8 (superset of ASCII's 7 bit characters).
 */

static ushort idVendor;
module_param(idVendor, ushort, 0);
MODULE_PARM_DESC(idVendor, "USB Vendor ID");

static ushort idProduct;
module_param(idProduct, ushort, 0);
MODULE_PARM_DESC(idProduct, "USB Product ID");

static ushort bcdDevice;
module_param(bcdDevice, ushort, 0);
MODULE_PARM_DESC(bcdDevice, "USB Device version (BCD)");

static char *iManufacturer;
module_param(iManufacturer, charp, 0);
MODULE_PARM_DESC(iManufacturer, "USB Manufacturer string");

static char *iProduct;
module_param(iProduct, charp, 0);
MODULE_PARM_DESC(iProduct, "USB Product string");

static char *iSerialNumber;
module_param(iSerialNumber, charp, 0);
MODULE_PARM_DESC(iSerialNumber, "SerialNumber string");

module_param(bit_rate_control, uint, S_IRWXU|S_IRWXG);
module_param(compress_rate, uint, S_IRWXU|S_IRWXG);
module_param(skip_rate_control, uint, S_IRWXU|S_IRWXG);
/*-------------------------------------------------------------------------*/

extern unsigned char g_incomplete;
unsigned char g_ioctlincomplete = 1;

extern ssize_t my_f_hidg_write(const char __user *buffer, size_t count);

/**
 * usb_add_function() - add a function to a configuration
 * @config: the configuration
 * @function: the function being added
 * Context: single threaded during gadget setup
 *
 * After initialization, each configuration must have one or more
 * functions added to it.  Adding a function involves calling its @bind()
 * method to allocate resources such as interface and string identifiers
 * and endpoints.
 *
 * This function returns the value of the function's bind(), which is
 * zero for success else a negative errno value.
 */
int usb_add_function(struct usb_configuration *config,
		struct usb_function *function)
{
	int	value = -EINVAL;

	DBG(config->cdev, "adding '%s'/%p to config '%s'/%p\n",
			function->name, function,
			config->label, config);

	if (!function->set_alt || !function->disable)
		goto done;

	function->config = config;
	list_add_tail(&function->list, &config->functions);

	/* REVISIT *require* function->bind? */
	if (function->bind) {
		value = function->bind(config, function);
		if (value < 0) {
			list_del(&function->list);
			function->config = NULL;
		}
	} else
		value = 0;

	/* We allow configurations that don't work at both speeds.
	 * If we run into a lowspeed Linux system, treat it the same
	 * as full speed ... it's the function drivers that will need
	 * to avoid bulk and ISO transfers.
	 */
	if (!config->fullspeed && function->descriptors)
		config->fullspeed = true;
	if (!config->highspeed && function->hs_descriptors)
		config->highspeed = true;

done:
	if (value)
		DBG(config->cdev, "adding '%s'/%p --> %d\n",
				function->name, function, value);
	return value;
}

/**
 * usb_function_deactivate - prevent function and gadget enumeration
 * @function: the function that isn't yet ready to respond
 *
 * Blocks response of the gadget driver to host enumeration by
 * preventing the data line pullup from being activated.  This is
 * normally called during @bind() processing to change from the
 * initial "ready to respond" state, or when a required resource
 * becomes available.
 *
 * For example, drivers that serve as a passthrough to a userspace
 * daemon can block enumeration unless that daemon (such as an OBEX,
 * MTP, or print server) is ready to handle host requests.
 *
 * Not all systems support software control of their USB peripheral
 * data pullups.
 *
 * Returns zero on success, else negative errno.
 */
int usb_function_deactivate(struct usb_function *function)
{
	struct usb_composite_dev	*cdev = function->config->cdev;
	unsigned long			flags;
	int				status = 0;

	spin_lock_irqsave(&cdev->lock, flags);

	if (cdev->deactivations == 0)
		status = usb_gadget_disconnect(cdev->gadget);
	if (status == 0)
		cdev->deactivations++;

	spin_unlock_irqrestore(&cdev->lock, flags);
	return status;
}

/**
 * usb_function_activate - allow function and gadget enumeration
 * @function: function on which usb_function_activate() was called
 *
 * Reverses effect of usb_function_deactivate().  If no more functions
 * are delaying their activation, the gadget driver will respond to
 * host enumeration procedures.
 *
 * Returns zero on success, else negative errno.
 */
int usb_function_activate(struct usb_function *function)
{
	struct usb_composite_dev	*cdev = function->config->cdev;
	int				status = 0;

	spin_lock(&cdev->lock);

	if (WARN_ON(cdev->deactivations == 0))
		status = -EINVAL;
	else {
		cdev->deactivations--;
		if (cdev->deactivations == 0)
			status = usb_gadget_connect(cdev->gadget);
	}

	spin_unlock(&cdev->lock);
	return status;
}

/**
 * usb_interface_id() - allocate an unused interface ID
 * @config: configuration associated with the interface
 * @function: function handling the interface
 * Context: single threaded during gadget setup
 *
 * usb_interface_id() is called from usb_function.bind() callbacks to
 * allocate new interface IDs.  The function driver will then store that
 * ID in interface, association, CDC union, and other descriptors.  It
 * will also handle any control requests targetted at that interface,
 * particularly changing its altsetting via set_alt().  There may
 * also be class-specific or vendor-specific requests to handle.
 *
 * All interface identifier should be allocated using this routine, to
 * ensure that for example different functions don't wrongly assign
 * different meanings to the same identifier.  Note that since interface
 * identifers are configuration-specific, functions used in more than
 * one configuration (or more than once in a given configuration) need
 * multiple versions of the relevant descriptors.
 *
 * Returns the interface ID which was allocated; or -ENODEV if no
 * more interface IDs can be allocated.
 */
int usb_interface_id(struct usb_configuration *config,
		struct usb_function *function)
{
	unsigned id = config->next_interface_id;

	if (id < MAX_CONFIG_INTERFACES) {
		config->interface[id] = function;
		config->next_interface_id = id + 1;
		return id;
	}
	return -ENODEV;
}

static int config_buf(struct usb_configuration *config,
		enum usb_device_speed speed, void *buf, u8 type)
{
	struct usb_config_descriptor	*c = buf;
	void				*next = buf + USB_DT_CONFIG_SIZE;
	int				len = USB_BUFSIZ - USB_DT_CONFIG_SIZE;
	struct usb_function		*f;
	int				status;

	/* write the config descriptor */
	c = buf;
	c->bLength = USB_DT_CONFIG_SIZE;
	c->bDescriptorType = type;
	/* wTotalLength is written later */
	c->bNumInterfaces = 2;//config->next_interface_id;
	c->bConfigurationValue = config->bConfigurationValue;
	c->iConfiguration = 0x00;//config->iConfiguration;
	c->bmAttributes = 0xC0;//USB_CONFIG_ATT_ONE | config->bmAttributes;
	c->bMaxPower = 0xC8;//config->bMaxPower ? : (CONFIG_USB_GADGET_VBUS_DRAW / 2);

	/* There may be e.g. OTG descriptors */
	if (config->descriptors) {
		status = usb_descriptor_fillbuf(next, len,
				config->descriptors);
		if (status < 0)
			return status;
		len -= status;
		next += status;
	}

	/* add each function's descriptors */
	list_for_each_entry(f, &config->functions, list) {
		struct usb_descriptor_header **descriptors;

		if (speed == USB_SPEED_HIGH)
			descriptors = f->hs_descriptors;
		else
			descriptors = f->descriptors;
		if (!descriptors)
			continue;
		status = usb_descriptor_fillbuf(next, len,
			(const struct usb_descriptor_header **) descriptors);
		if (status < 0)
			return status;
		len -= status;
		next += status;
	}

	len = next - buf;
	c->wTotalLength = cpu_to_le16(len);
	return len;
}

static int config_desc(struct usb_composite_dev *cdev, unsigned w_value)
{
	struct usb_gadget		*gadget = cdev->gadget;
	struct usb_configuration	*c;
	u8				type = w_value >> 8;
	enum usb_device_speed		speed = USB_SPEED_HIGH;

	if (gadget_is_dualspeed(gadget)) {
		int			hs = 0;

		if (gadget->speed == USB_SPEED_HIGH)
			hs = 1;
		if (type == USB_DT_OTHER_SPEED_CONFIG)
			hs = 1;
		if (hs)
			speed = USB_SPEED_HIGH;

	}

	/* This is a lookup by config *INDEX* */
	w_value &= 0xff;
	list_for_each_entry(c, &cdev->configs, list) {
		/* ignore configs that won't work at this speed */
		if (speed == USB_SPEED_HIGH) {
			if (!c->highspeed)
				continue;
		} else {
			if (!c->fullspeed)
				continue;
		}
		if (w_value == 0)
			return config_buf(c, speed, cdev->req->buf, type);
		w_value--;
	}
	return -EINVAL;
}

static int count_configs(struct usb_composite_dev *cdev, unsigned type)
{
	struct usb_gadget		*gadget = cdev->gadget;
	struct usb_configuration	*c;
	unsigned			count = 0;
	int				hs = 0;

	if (gadget_is_dualspeed(gadget)) {
		if (gadget->speed == USB_SPEED_HIGH)
			hs = 1;
		if (type == USB_DT_DEVICE_QUALIFIER)
			hs = 1;
	}
	list_for_each_entry(c, &cdev->configs, list) {
		/* ignore configs that won't work at this speed */
		if (hs) {
			if (!c->highspeed)
				continue;
		} else {
			if (!c->fullspeed)
				continue;
		}
		count++;
	}
	return count;
}

static void device_qual(struct usb_composite_dev *cdev)
{
	struct usb_qualifier_descriptor	*qual = cdev->req->buf;

	qual->bLength = sizeof(*qual);
	qual->bDescriptorType = USB_DT_DEVICE_QUALIFIER;
	/* POLICY: same bcdUSB and device type info at both speeds */
	qual->bcdUSB = cdev->desc.bcdUSB;
	qual->bDeviceClass = cdev->desc.bDeviceClass;
	qual->bDeviceSubClass = cdev->desc.bDeviceSubClass;
	qual->bDeviceProtocol = cdev->desc.bDeviceProtocol;
	/* ASSUME same EP0 fifo size at both speeds */
	qual->bMaxPacketSize0 = cdev->desc.bMaxPacketSize0;
	qual->bNumConfigurations = count_configs(cdev, USB_DT_DEVICE_QUALIFIER);
	qual->bRESERVED = 0;
}

/*-------------------------------------------------------------------------*/

static void reset_config(struct usb_composite_dev *cdev)
{
	struct usb_function		*f;

	DBG(cdev, "reset config\n");

	list_for_each_entry(f, &cdev->config->functions, list) {
		if (f->disable)
			f->disable(f);

		bitmap_zero(f->endpoints, 32);
	}
	cdev->config = NULL;
}

static int set_config(struct usb_composite_dev *cdev,
		const struct usb_ctrlrequest *ctrl, unsigned number)
{
	struct usb_gadget	*gadget = cdev->gadget;
	struct usb_configuration *c = NULL;
	int			result = -EINVAL;
	unsigned		power = gadget_is_otg(gadget) ? 8 : 100;
	int			tmp;

	if (cdev->config)
		reset_config(cdev);

	if (number) {
		list_for_each_entry(c, &cdev->configs, list) {
			if (c->bConfigurationValue == number) {
				result = 0;
				break;
			}
		}
		if (result < 0)
			goto done;
	} else
		result = 0;

	INFO(cdev, "%s speed config #%d: %s\n",
		({ char *speed;
		switch (gadget->speed) {
		case USB_SPEED_LOW:	speed = "low"; break;
		case USB_SPEED_FULL:	speed = "full"; break;
		case USB_SPEED_HIGH:	speed = "high"; break;
		default:		speed = "?"; break;
		} ; speed; }), number, c ? c->label : "unconfigured");

	if (!c)
		goto done;

	cdev->config = c;

	/* Initialize all interfaces by setting them to altsetting zero. */
	for (tmp = 0; tmp < MAX_CONFIG_INTERFACES; tmp++) {
		struct usb_function	*f = c->interface[tmp];
		struct usb_descriptor_header **descriptors;

		if (!f)
			break;

		/*
		 * Record which endpoints are used by the function. This is used
		 * to dispatch control requests targeted at that endpoint to the
		 * function's setup callback instead of the current
		 * configuration's setup callback.
		 */
		if (gadget->speed == USB_SPEED_HIGH)
			descriptors = f->hs_descriptors;
		else
			descriptors = f->descriptors;

		for (; *descriptors; ++descriptors) {
			struct usb_endpoint_descriptor *ep;
			int addr;

			if ((*descriptors)->bDescriptorType != USB_DT_ENDPOINT)
				continue;

			ep = (struct usb_endpoint_descriptor *)*descriptors;
			addr = ((ep->bEndpointAddress & 0x80) >> 3)
			     |  (ep->bEndpointAddress & 0x0f);
			set_bit(addr, f->endpoints);
		}

		result = f->set_alt(f, tmp, 0);
		if (result < 0) {
			DBG(cdev, "interface %d (%s/%p) alt 0 --> %d\n",
					tmp, f->name, f, result);

			reset_config(cdev);
			goto done;
		}
	}

	/* when we return, be sure our power usage is valid */
	power = c->bMaxPower ? (2 * c->bMaxPower) : CONFIG_USB_GADGET_VBUS_DRAW;
done:
	usb_gadget_vbus_draw(gadget, power);
	return result;
}

/**
 * usb_add_config() - add a configuration to a device.
 * @cdev: wraps the USB gadget
 * @config: the configuration, with bConfigurationValue assigned
 * Context: single threaded during gadget setup
 *
 * One of the main tasks of a composite driver's bind() routine is to
 * add each of the configurations it supports, using this routine.
 *
 * This function returns the value of the configuration's bind(), which
 * is zero for success else a negative errno value.  Binding configurations
 * assigns global resources including string IDs, and per-configuration
 * resources such as interface IDs and endpoints.
 */
int usb_add_config(struct usb_composite_dev *cdev,
		struct usb_configuration *config)
{
	int				status = -EINVAL;
	struct usb_configuration	*c;

	DBG(cdev, "adding config #%u '%s'/%p\n",
			config->bConfigurationValue,
			config->label, config);

	if (!config->bConfigurationValue || !config->bind)
		goto done;

	/* Prevent duplicate configuration identifiers */
	list_for_each_entry(c, &cdev->configs, list) {
		if (c->bConfigurationValue == config->bConfigurationValue) {
			status = -EBUSY;
			goto done;
		}
	}

	config->cdev = cdev;
	list_add_tail(&config->list, &cdev->configs);

	INIT_LIST_HEAD(&config->functions);
	config->next_interface_id = 0;

	status = config->bind(config);
	if (status < 0) {
		list_del(&config->list);
		config->cdev = NULL;
	} else {
		unsigned	i;

		DBG(cdev, "cfg %d/%p speeds:%s%s\n",
			config->bConfigurationValue, config,
			config->highspeed ? " high" : "",
			config->fullspeed
				? (gadget_is_dualspeed(cdev->gadget)
					? " full"
					: " full/low")
				: "");

		for (i = 0; i < MAX_CONFIG_INTERFACES; i++) {
			struct usb_function	*f = config->interface[i];

			if (!f)
				continue;
			DBG(cdev, "  interface %d = %s/%p\n",
				i, f->name, f);
		}
	}

	/* set_alt(), or next config->bind(), sets up
	 * ep->driver_data as needed.
	 */
	usb_ep_autoconfig_reset(cdev->gadget);

done:
	if (status)
		DBG(cdev, "added config '%s'/%u --> %d\n", config->label,
				config->bConfigurationValue, status);
	return status;
}

/*-------------------------------------------------------------------------*/

/* We support strings in multiple languages ... string descriptor zero
 * says which languages are supported.  The typical case will be that
 * only one language (probably English) is used, with I18N handled on
 * the host side.
 */

static void collect_langs(struct usb_gadget_strings **sp, __le16 *buf)
{
	const struct usb_gadget_strings	*s;
	u16				language;
	__le16				*tmp;

	while (*sp) {
		s = *sp;
		language = cpu_to_le16(s->language);
		for (tmp = buf; *tmp && tmp < &buf[126]; tmp++) {
			if (*tmp == language)
				goto repeat;
		}
		*tmp++ = language;
repeat:
		sp++;
	}
}

static int lookup_string(
	struct usb_gadget_strings	**sp,
	void				*buf,
	u16				language,
	int				id
)
{
	struct usb_gadget_strings	*s;
	int				value;

	while (*sp) {
		s = *sp++;
		if (s->language != language)
			continue;
		value = usb_gadget_get_string(s, id, buf);
		if (value > 0)
			return value;
	}
	return -EINVAL;
}

static int get_string(struct usb_composite_dev *cdev,
		void *buf, u16 language, int id)
{
	struct usb_configuration	*c;
	struct usb_function		*f;
	int				len;

	/* Yes, not only is USB's I18N support probably more than most
	 * folk will ever care about ... also, it's all supported here.
	 * (Except for UTF8 support for Unicode's "Astral Planes".)
	 */

	/* 0 == report all available language codes */
	if (id == 0) {
		struct usb_string_descriptor	*s = buf;
		struct usb_gadget_strings	**sp;

		memset(s, 0, 256);
		s->bDescriptorType = USB_DT_STRING;

		sp = composite->strings;
		if (sp)
			collect_langs(sp, s->wData);

		list_for_each_entry(c, &cdev->configs, list) {
			sp = c->strings;
			if (sp)
				collect_langs(sp, s->wData);

			list_for_each_entry(f, &c->functions, list) {
				sp = f->strings;
				if (sp)
					collect_langs(sp, s->wData);
			}
		}

		for (len = 0; len <= 126 && s->wData[len]; len++)
			continue;
		if (!len)
			return -EINVAL;

		s->bLength = 2 * (len + 1);
		return s->bLength;
	}

	/* Otherwise, look up and return a specified string.  String IDs
	 * are device-scoped, so we look up each string table we're told
	 * about.  These lookups are infrequent; simpler-is-better here.
	 */
	if (composite->strings) {
		len = lookup_string(composite->strings, buf, language, id);
		if (len > 0)
			return len;
	}
	list_for_each_entry(c, &cdev->configs, list) {
		if (c->strings) {
			len = lookup_string(c->strings, buf, language, id);
			if (len > 0)
				return len;
		}
		list_for_each_entry(f, &c->functions, list) {
			if (!f->strings)
				continue;
			len = lookup_string(f->strings, buf, language, id);
			if (len > 0)
				return len;
		}
	}
	return -EINVAL;
}

/**
 * usb_string_id() - allocate an unused string ID
 * @cdev: the device whose string descriptor IDs are being allocated
 * Context: single threaded during gadget setup
 *
 * @usb_string_id() is called from bind() callbacks to allocate
 * string IDs.  Drivers for functions, configurations, or gadgets will
 * then store that ID in the appropriate descriptors and string table.
 *
 * All string identifier should be allocated using this,
 * @usb_string_ids_tab() or @usb_string_ids_n() routine, to ensure
 * that for example different functions don't wrongly assign different
 * meanings to the same identifier.
 */
int usb_string_id(struct usb_composite_dev *cdev)
{
	if (cdev->next_string_id < 254) {
		/* string id 0 is reserved by USB spec for list of
		 * supported languages */
		/* 255 reserved as well? -- mina86 */
		cdev->next_string_id++;
		return cdev->next_string_id;
	}
	return -ENODEV;
}

/**
 * usb_string_ids() - allocate unused string IDs in batch
 * @cdev: the device whose string descriptor IDs are being allocated
 * @str: an array of usb_string objects to assign numbers to
 * Context: single threaded during gadget setup
 *
 * @usb_string_ids() is called from bind() callbacks to allocate
 * string IDs.  Drivers for functions, configurations, or gadgets will
 * then copy IDs from the string table to the appropriate descriptors
 * and string table for other languages.
 *
 * All string identifier should be allocated using this,
 * @usb_string_id() or @usb_string_ids_n() routine, to ensure that for
 * example different functions don't wrongly assign different meanings
 * to the same identifier.
 */
int usb_string_ids_tab(struct usb_composite_dev *cdev, struct usb_string *str)
{
	int next = cdev->next_string_id;

	for (; str->s; ++str) {
		if (unlikely(next >= 254))
			return -ENODEV;
		str->id = ++next;
	}

	cdev->next_string_id = next;

	return 0;
}

/**
 * usb_string_ids_n() - allocate unused string IDs in batch
 * @c: the device whose string descriptor IDs are being allocated
 * @n: number of string IDs to allocate
 * Context: single threaded during gadget setup
 *
 * Returns the first requested ID.  This ID and next @n-1 IDs are now
 * valid IDs.  At least provided that @n is non-zero because if it
 * is, returns last requested ID which is now very useful information.
 *
 * @usb_string_ids_n() is called from bind() callbacks to allocate
 * string IDs.  Drivers for functions, configurations, or gadgets will
 * then store that ID in the appropriate descriptors and string table.
 *
 * All string identifier should be allocated using this,
 * @usb_string_id() or @usb_string_ids_n() routine, to ensure that for
 * example different functions don't wrongly assign different meanings
 * to the same identifier.
 */
int usb_string_ids_n(struct usb_composite_dev *c, unsigned n)
{
	unsigned next = c->next_string_id;
	if (unlikely(n > 254 || (unsigned)next + n > 254))
		return -ENODEV;
	c->next_string_id += n;
	return next + 1;
}


/*-------------------------------------------------------------------------*/
static void hex_dump(unsigned char *buf, int len)
{
	while (len--)
		printk("%02x,", *buf++);
	printk("\n");
}

#define UDP_CURSORBLOCK_SIZE 8
#define UDP_CURSORSHAPEBLOCK_SIZE 512
#define UDP_CURSORDATABLOCK_SIZE 20*1024
unsigned char ep0_setup_dir;
unsigned char t6_vendor_cmd;
unsigned char g_udp_control_XactId = 0;
unsigned char g_udp_cursor_buf[UDP_CURSORDATABLOCK_SIZE];
unsigned char g_udp_cursor_state_buf[20*1024];
unsigned char g_udp_cursor_shape_buf[10][20*1024];
unsigned char g_tcp_cursor_buf[64];
unsigned int g_cursor_buf_offset;
unsigned int g_cursor_shape_idx;
unsigned int g_cursor_shape_width, g_cursor_shape_height;
extern struct socket *g_UCURconn_socket;
extern struct sockaddr_in g_Usaddr;
extern struct sockaddr_in g_UCURsaddr;
extern int ksocket_send(struct socket *sock, struct sockaddr_in *addr, unsigned char *buf, int len);
extern struct fsg_common cursor_udp_common;
extern struct fsg_common cursor_tcp_common;
extern spinlock_t cursor_lock;
extern unsigned long cursor_flags;
extern struct fsg_common hid_udp_common;
extern void wakeup_thread(struct fsg_common *common);
extern unsigned int udp_send_total_bytes;
extern unsigned char shapeusbin[10];

static struct work_struct ioctl_bottem_work;
static struct work_struct setcfg_bottem_work;

#define MANUFACTORER_RAW                "/etc/manufactorer"
#define PRODUCT_RAW                     "/etc/product"
#define VC_DESC                         "/etc/vc_desc"
#define UVC_SETCONF_DONE                "/etc/uvc_setconf_done"

#define UVC_CONF_FILE                   "/etc/uvc.conf"
#define CY_FX_UVC_MAX_PROBE_SETTING     34

struct uvc_state {
	unsigned char setuptoken_8byte[8];
	__u32 status;
	unsigned char prob_ctrl[CY_FX_UVC_MAX_PROBE_SETTING];
} mct_uvc_data;

void ioctl_bottem_notify(struct work_struct *work)
{
	g_ioctlincomplete = 1;
	wake_up(&gUVCioctl);//for user
}

struct file *file_open(const char *path, int flags, int rights);
void file_close(struct file *file);
int file_read(struct file *file, unsigned long long offset, unsigned char *data, unsigned int size);
int file_write(struct file *file, unsigned long long offset, unsigned char *data, unsigned int size);
int file_sync(struct file *file);

void setcfg_bottem_notify(struct work_struct *work)
{
	struct file * setconf_fp = NULL;

	setconf_fp =filp_open(UVC_SETCONF_DONE, O_RDWR | O_CREAT, 0666);
	if (setconf_fp) {
		file_write(setconf_fp, 0, "1", 1);
		file_sync(setconf_fp);
		file_close(setconf_fp);
	}
}

#define MCT_UVC_NOTIFY_SIZE 256
unsigned char gCurItfnum;
unsigned char gSetDirBuff[MCT_UVC_NOTIFY_SIZE];
unsigned char gGetDirBuff[MCT_UVC_NOTIFY_SIZE];
struct usb_gadget *g_uvc_gadget;

unsigned char glProbeCtrl[CY_FX_UVC_MAX_PROBE_SETTING] = {
    0x00,0x00,                       /* bmHint : No fixed parameters */
    0x01,                            /* Use 1st Video format index */
    0x03,                            /* Use 1st Video frame index */
    0x15,0x16,0x05,0x00,             /* Desired frame interval in 100ns */
    0x00,0x00,                       /* Key frame rate in key frame/video frame units */
    0x00,0x00,                       /* PFrame rate in PFrame / key frame units */
    0x00,0x00,                       /* Compression quality control */
    0x00,0x00,                       /* Window size for average bit rate */
    0x00,0x00,                       /* Internal video streaming i/f latency in ms */
    0x00,0x30,0x2A,0x00,             /* Max video frame size in bytes */
    0x00,0x30,0x2A,0x00,             /* No. of bytes device can transmit in single payload */
    0x00,0x6C,0xDC,0x02,             /* The device clock frequency in Hz for the specified format. 48M*/
    0x03,                            /* bmFramingInfo */
    0x01,                            /* bPreferedVersion */
    0x01,                            /* bMinVersion based on bFormatIndex */
    0x02,                            /* bMaxVersion based on bFormatIndex */
};

unsigned char glProbeCtrlYUV[CY_FX_UVC_MAX_PROBE_SETTING] = {
    0x00,0x00,                       /* bmHint : No fixed parameters */
    0x03,                            /* Use 1st Video format index */
    0x01,                            /* Use 1st Video frame index */
    0x15,0x16,0x05,0x00,             /* Desired frame interval in 100ns */
    0x00,0x00,                       /* Key frame rate in key frame/video frame units */
    0x00,0x00,                       /* PFrame rate in PFrame / key frame units */
    0x00,0x00,                       /* Compression quality control */
    0x00,0x00,                       /* Window size for average bit rate */
    0x00,0x00,                       /* Internal video streaming i/f latency in ms */
    0x00,0x58,0x02,0x00,             /* Max video frame size in bytes */
    0x00,0x02,0x00,0x00,             /* No. of bytes device can transmit in single payload */
    0x00,0x6C,0xDC,0x02,             /* The device clock frequency in Hz for the specified format. 48M*/
    0x03,                            /* bmFramingInfo */
    0x03,                            /* bPreferedVersion */
    0x01,                            /* bMinVersion based on bFormatIndex */
    0x03,                            /* bMaxVersion based on bFormatIndex */
};

#define MCT_UVC_DATA_SIZE                     CY_FX_UVC_MAX_PROBE_SETTING+12

#define MAX_SETUP_QUEUE 5

typedef struct _mct_uvc_setup_queue {
	unsigned char setupcmd[MCT_UVC_NOTIFY_SIZE];
	unsigned char *next;
} MCT_UVC_SETUP_QUEUE;
MCT_UVC_SETUP_QUEUE g_mct_uvc_setup_queue[MAX_SETUP_QUEUE];
MCT_UVC_SETUP_QUEUE *g_mct_uvc_setup_insert;
MCT_UVC_SETUP_QUEUE *g_mct_uvc_setup_remove;

static void composite_setup_complete(struct usb_ep *ep, struct usb_request *req)
{
	//hex_dump(req->buf, req->length);
	//printk("SHAPECOM:%x,%x\n",ep0_setup_dir,t6_vendor_cmd);

	if(ep0_setup_dir & 0x80)//get
	{
	}
	else//set
	{
		if( (gCurItfnum == 0 && (((ep0_setup_dir)&USB_TYPE_CLASS) == USB_TYPE_CLASS)) && client_connected == 1)
		{
			memcpy(&gSetDirBuff[8], req->buf, req->length);
			memcpy(&g_mct_uvc_setup_insert->setupcmd[8], req->buf, req->length);
			g_mct_uvc_setup_insert = (MCT_UVC_SETUP_QUEUE*)g_mct_uvc_setup_insert->next;
			schedule_work(&ioctl_bottem_work);
		} else {
			if (*((char *)(req->buf)+2) != 3) {
				if (req->length > 4) {
					glProbeCtrl[2] = *((char *)(req->buf)+2);
					glProbeCtrl[3] = *((char *)(req->buf)+3);
				}
				memcpy(mct_uvc_data.prob_ctrl, glProbeCtrl, CY_FX_UVC_MAX_PROBE_SETTING);
			} else {
				if (req->length > 4) {
					glProbeCtrlYUV[2] = *((char *)(req->buf)+2);
					glProbeCtrlYUV[3] = *((char *)(req->buf)+3);
				}
				memcpy(mct_uvc_data.prob_ctrl, glProbeCtrlYUV, CY_FX_UVC_MAX_PROBE_SETTING);
			}
			schedule_work(&ioctl_bottem_work);
		}
		if ( ((((ep0_setup_dir)&USB_TYPE_STANDARD) == USB_TYPE_STANDARD) && current_b_request == USB_REQ_CLEAR_FEATURE) ||
		     ((((ep0_setup_dir)&USB_TYPE_CLASS) == USB_TYPE_CLASS) && current_b_request == UVC_SET_CUR) ) {
			if (*((char *)(req->buf)+2) != 3) {
				if (req->length > 4) {
					glProbeCtrl[2] = *((char *)(req->buf)+2);
					glProbeCtrl[3] = *((char *)(req->buf)+3);
				}
				memcpy(mct_uvc_data.prob_ctrl, glProbeCtrl, CY_FX_UVC_MAX_PROBE_SETTING);
			} else {
				if (req->length > 4) {
					glProbeCtrlYUV[2] = *((char *)(req->buf)+2);
					glProbeCtrlYUV[3] = *((char *)(req->buf)+3);
				}
				memcpy(mct_uvc_data.prob_ctrl, glProbeCtrlYUV, CY_FX_UVC_MAX_PROBE_SETTING);
			}
			schedule_work(&ioctl_bottem_work);
		}
	}
	if (req->status || req->actual != req->length)
		DBG((struct usb_composite_dev *) ep->driver_data,
				"setup complete --> %d, %d/%d\n",
				req->status, req->actual, req->length);
}

const unsigned char fsg_bos_desc[] = {
	// ============== REPORT DESCRIPTOR MOUSE ===========
	0x05, 0x0f,
	0x16, 0x00,
	0x02, 0x07,
	0x10, 0x02,
	0x02, 0x00,
	0x00, 0x00,
	0x0a, 0x10,
	0x03, 0x00,
	0x0e, 0x00,
	0x01, 0x0a,
	0x20, 0x00,
};

const unsigned char ReportDescHSMouse1[] = {
	// ============== REPORT DESCRIPTOR MOUSE ===========
	0x05, 0x01,
	0x09, 0x02,
	0xA1, 0x01,
	0x09, 0x01,
	0xA1, 0x00,
	0x05, 0x09,
	0x19, 0x01,
	0x29, 0x03,
	0x15, 0x00,
	0x25, 0x01,
	0x95, 0x08,
	0x75, 0x01,
	0x81, 0x02,
	0x05, 0x01,
	0x09, 0x30,
	0x09, 0x31,
	0x09, 0x38,
	0x15, 0x81,
	0x25, 0x7F,
	0x75, 0x08,
	0x95, 0x03,
	0x81, 0x06,
	0xC0, 0xC0,
};

const unsigned char ReportDescHSMouse[] = {
	// ============== REPORT DESCRIPTOR MOUSE ===========
	0x05, 0x01, 0x09, 0x02, 0xA1, 0x01, 0x85, 0x01, 0x09, 0x01, 0xA1, 0x00, 0x05, 0x09, 0x19, 0x01,
	0x29, 0x03, 0x15, 0x00, 0x25, 0x01, 0x95, 0x03, 0x75, 0x01, 0x81, 0x02, 0x95, 0x05, 0x81, 0x03,
	0x05, 0x01, 0x09, 0x30, 0x09, 0x31, 0x15, 0x81, 0x25, 0x7F, 0x75, 0x08, 0x95, 0x02, 0x81, 0x06,
	0xC0, 0xC0, 0x06, 0x0D, 0xFF, 0x09, 0x01, 0xA1, 0x01, 0x85, 0x11, 0x09, 0x20, 0xA1, 0x00, 0x75,
	0x04, 0x95, 0x01, 0x81, 0x03, 0x09, 0x42, 0x09, 0x44, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95,
	0x02, 0x81, 0x02, 0x75, 0x01, 0x95, 0x01, 0x81, 0x03, 0x09, 0x32, 0x15, 0x00, 0x25, 0x01, 0x75,
	0x01, 0x95, 0x01, 0x81, 0x02, 0x09, 0x30, 0x15, 0x00, 0x26, 0xFF, 0x03, 0x75, 0x0A, 0x95, 0x01,
	0x81, 0x02, 0x75, 0x06, 0x95, 0x01, 0x81, 0x03, 0x0A, 0x30, 0x01, 0x65, 0x11, 0x55, 0x0D, 0x15,
	0x00, 0x26, 0x90, 0x58, 0x75, 0x10, 0x95, 0x01, 0x81, 0x02, 0x0A, 0x31, 0x01, 0x65, 0x11, 0x55,
	0x0D, 0x15, 0x00, 0x26, 0x80, 0x32, 0x75, 0x10, 0x95, 0x01, 0x81, 0x02, 0x09, 0x56, 0x15, 0x00,
	0x27, 0xFF, 0xFF, 0x00, 0x00, 0x75, 0x10, 0x95, 0x01, 0x81, 0x02, 0x0A, 0x20, 0x02, 0x15, 0x00,
	0x27, 0xFF, 0xFF, 0x00, 0x00, 0x75, 0x10, 0x95, 0x01, 0x81, 0x02, 0xC0, 0x85, 0x15, 0x09, 0x39,
	0xA1, 0x00, 0x0A, 0x10, 0x09, 0x0A, 0x11, 0x09, 0x0A, 0x12, 0x09, 0x0A, 0x13, 0x09, 0x15, 0x00,
	0x25, 0x01, 0x75, 0x01, 0x95, 0x04, 0x81, 0x02, 0x75, 0x01, 0x95, 0x04, 0x81, 0x03, 0xC0, 0x85,
	0x12, 0x0A, 0x01, 0x02, 0x15, 0x00, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x95, 0x10, 0x81, 0x02, 0x85,
	0x13, 0x0A, 0x01, 0x02, 0x15, 0x00, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x95, 0x10, 0x81, 0x02, 0x85,
	0x10, 0x0A, 0x00, 0x02, 0x0A, 0x02, 0x02, 0x15, 0x00, 0x25, 0x02, 0x75, 0x08, 0x95, 0x01, 0x81,
	0x02, 0x0A, 0x03, 0x02, 0x15, 0x00, 0x25, 0x02, 0x75, 0x06, 0x95, 0x01, 0x81, 0x02, 0x0A, 0x04,
	0x02, 0x15, 0x00, 0x25, 0x02, 0x75, 0x02, 0x95, 0x01, 0x81, 0x02, 0x0A, 0x05, 0x02, 0x15, 0x00,
	0x25, 0x01, 0x75, 0x01, 0x95, 0x01, 0x81, 0x02, 0x0A, 0x06, 0x02, 0x15, 0x00, 0x25, 0x01, 0x75,
	0x01, 0x95, 0x01, 0x81, 0x02, 0x75, 0x06, 0x95, 0x01, 0x81, 0x03, 0x0A, 0x07, 0x02, 0x15, 0x00,
	0x26, 0xFF, 0x00, 0x75, 0x08, 0x95, 0x01, 0x81, 0x02, 0x0A, 0x08, 0x02, 0x15, 0x00, 0x26, 0xFF,
	0x00, 0x75, 0x08, 0x95, 0x01, 0x81, 0x02, 0x0A, 0x09, 0x02, 0x15, 0x00, 0x26, 0xFF, 0x00, 0x75,
	0x08, 0x95, 0x01, 0x81, 0x02, 0x0A, 0x1A, 0x02, 0x15, 0x00, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x95,
	0x01, 0x81, 0x02, 0x0A, 0x1D, 0x02, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x01, 0x81, 0x02,
	0x0A, 0x1C, 0x02, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x01, 0x81, 0x02, 0x0A, 0x1B, 0x02,
	0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x01, 0x81, 0x02, 0x75, 0x05, 0x95, 0x01, 0x81, 0x03,
	0x75, 0x08, 0x95, 0x08, 0x81, 0x03, 0x09, 0x0E, 0xA1, 0x02, 0x85, 0x02, 0x0A, 0x02, 0x10, 0x15,
	0x00, 0x25, 0x02, 0x75, 0x08, 0x95, 0x01, 0xB1, 0x02, 0x85, 0x03, 0x0A, 0x03, 0x10, 0x15, 0x00,
	0x26, 0xFF, 0x00, 0x75, 0x08, 0x95, 0x01, 0xB1, 0x02, 0x85, 0x04, 0x0A, 0x07, 0x10, 0x15, 0x00,
	0x27, 0xFF, 0xFF, 0x00, 0x00, 0x75, 0x10, 0x95, 0x01, 0xB1, 0x02, 0x0A, 0x08, 0x10, 0x15, 0x00,
	0x26, 0xFF, 0x00, 0x75, 0x08, 0x95, 0x01, 0xB1, 0x02, 0x75, 0x08, 0x95, 0x04, 0xB1, 0x03, 0x85,
	0x06, 0x0A, 0x0A, 0x10, 0x15, 0x01, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x95, 0x01, 0xB1, 0x02, 0x85,
	0xCC, 0x0A, 0xCC, 0x10, 0x15, 0x00, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x95, 0x02, 0xB1, 0x02, 0x85,
	0x0C, 0x0A, 0x14, 0x10, 0x15, 0x00, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x95, 0x0A, 0xB1, 0x02, 0x85,
	0x0E, 0x0A, 0x14, 0x10, 0x15, 0x00, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x95, 0x0D, 0xB1, 0x02, 0x85,
	0x10, 0x0A, 0x02, 0x02, 0x15, 0x00, 0x25, 0x02, 0x75, 0x08, 0x95, 0x01, 0xB1, 0x02, 0x0A, 0x03,
	0x02, 0x15, 0x00, 0x25, 0x02, 0x75, 0x06, 0x95, 0x01, 0xB1, 0x02, 0x0A, 0x04, 0x02, 0x15, 0x00,
	0x25, 0x02, 0x75, 0x02, 0x95, 0x01, 0xB1, 0x02, 0x0A, 0x05, 0x02, 0x15, 0x00, 0x25, 0x01, 0x75,
	0x01, 0x95, 0x01, 0xB1, 0x02, 0x0A, 0x06, 0x02, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x01,
	0xB1, 0x02, 0x75, 0x06, 0x95, 0x01, 0xB1, 0x03, 0x0A, 0x07, 0x02, 0x15, 0x00, 0x26, 0xFF, 0x00,
	0x75, 0x08, 0x95, 0x01, 0xB1, 0x02, 0x0A, 0x08, 0x02, 0x15, 0x00, 0x26, 0xFF, 0x00, 0x75, 0x08,
	0x95, 0x01, 0xB1, 0x02, 0x0A, 0x09, 0x02, 0x15, 0x00, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x95, 0x01,
	0xB1, 0x02, 0x0A, 0x1A, 0x02, 0x15, 0x00, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x95, 0x01, 0xB1, 0x02,
	0x0A, 0x1D, 0x02, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x01, 0xB1, 0x02, 0x0A, 0x1C, 0x02,
	0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x01, 0xB1, 0x02, 0x0A, 0x1B, 0x02, 0x15, 0x00, 0x25,
	0x01, 0x75, 0x01, 0x95, 0x01, 0xB1, 0x02, 0x75, 0x05, 0x95, 0x01, 0xB1, 0x03, 0x75, 0x08, 0x95,
	0x08, 0xB1, 0x03, 0x85, 0x40, 0x09, 0x00, 0x15, 0x00, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x95, 0x43,
	0xB1, 0x02, 0xC0, 0x0A, 0xAC, 0x10, 0xA1, 0x02, 0x15, 0x00, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x85,
	0x05, 0x09, 0x00, 0x95, 0x01, 0xB1, 0x02, 0x85, 0x07, 0x09, 0x00, 0x95, 0x05, 0xB1, 0x02, 0x85,
	0x08, 0x09, 0x00, 0x95, 0x02, 0xB1, 0x02, 0x85, 0x09, 0x09, 0x00, 0x95, 0x03, 0xB1, 0x02, 0x85,
	0x14, 0x09, 0x00, 0x95, 0x3F, 0x81, 0x02, 0x85, 0x0A, 0x09, 0x00, 0x95, 0x01, 0xB1, 0x02, 0x85,
	0x0B, 0x09, 0x00, 0x95, 0x01, 0xB1, 0x02, 0x85, 0x0D, 0x09, 0x00, 0x95, 0x01, 0xB1, 0x02, 0x85,
	0x88, 0x09, 0x00, 0x95, 0x08, 0xB1, 0x02, 0x85, 0xB0, 0x09, 0x00, 0x95, 0x01, 0xB1, 0x02, 0x85,
	0xB5, 0x09, 0x00, 0x95, 0x0A, 0xB1, 0x02, 0x85, 0xB9, 0x09, 0x00, 0x95, 0x0D, 0xB1, 0x02, 0x85,
	0xB1, 0x09, 0x00, 0x95, 0x28, 0x81, 0x02, 0xC0, 0xC0,
};

const unsigned char ReportDescHSKeyboard[] = {
	// ============= REPORT DESCRIPTOR KEYBOARD ==========
	0x05, 0x01,
	0x09, 0x06,
	0xA1, 0x01,
	0x05, 0x07,
	0x19, 0xE0,
	0x29, 0xE7,
	0x15, 0x00,
	0x25, 0x01,
	0x75, 0x01,
	0x95, 0x08,
	0x81, 0x02,
	0x95, 0x01,
	0x75, 0x08,
	0x81, 0x01,
	0x95, 0x03,
	0x75, 0x01,
	0x05, 0x08,
	0x19, 0x01,
	0x29, 0x03,
	0x91, 0x02,
	0x95, 0x05,
	0x75, 0x01,
	0x91, 0x01,
	0x95, 0x06,
	0x75, 0x08,
	0x26, 0xFF, 0x00,
	0x05, 0x07,
	0x19, 0x00,
	0x29, 0x91,
	0x81, 0x00,
	0xC0,
};

const unsigned char ReportDescHSNone[] = {
	// ============= REPORT DESCRIPTOR NONE ==========
	0x05, 0x01,
	0x09, 0x80,
	0xA1, 0x01,
	0x85, 0x01,
	0x19, 0x81,
	0x29, 0x83,
	0x15, 0x00,
	0x25, 0x01,
	0x95, 0x03,
	0x75, 0x01,
	0x81, 0x02,
	0x95, 0x01,
	0x75, 0x05,
	0x81, 0x01,
	0xC0,
	0x05, 0x0C,
	0x09, 0x01,
	0xA1, 0x01,
	0x85, 0x02,
	0x15, 0x00,
	0x25, 0x01,
	0x09, 0xE9,
	0x09, 0xEA,
	0x09, 0xE2,
	0x09, 0xCD,
	0x19, 0xB5,
	0x29, 0xB8,
	0x75, 0x01,
	0x95, 0x08,
	0x81, 0x02,
	0x0A, 0x8A, 0x01,
	0x0A, 0x21, 0x02,
	0x0A, 0x2A, 0x02,
	0x1A, 0x23, 0x02,
	0x2A, 0x27, 0x02,
	0x81, 0x02,
	0x0A, 0x83, 0x01,
	0x0A, 0x96, 0x01,
	0x0A, 0x92, 0x01,
	0x0A, 0x9E, 0x01,
	0x0A, 0x94, 0x01,
	0x0A, 0x06, 0x02,
	0x09, 0xB2,
	0x09, 0xB4,
	0x81, 0xC2,
	0xC0,
};

#define WACOM_1024X600 0

unsigned char t6rom[128] = {
#if WACOM_1024X600
	0x44,0x49,0x53,0x50,0x90,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x11,0x07,0x29,0x56,
	0x44,0x00,0x54,0x00,0x55,0x00,0x2D,0x00,0x31,0x00,0x30,0x00,0x33,0x00,0x31,0x00,
	0x41,0x00,0x58,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x01,0x00,0x00,0x00,0x48,0x00,0x00,0x08,0x08,0x01,0x70,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x5A,0x32,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
#else
	0x44,0x49,0x53,0x50,0x30,0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x11,0x07,0x21,0x56,
	0x54,0x00,0x72,0x00,0x69,0x00,0x67,0x00,0x67,0x00,0x65,0x00,0x72,0x00,0x20,0x00,
	0x36,0x00,0x20,0x00,0x54,0x00,0x78,0x00,0x74,0x00,0x65,0x00,0x72,0x00,0x6E,0x00,
	0x61,0x00,0x6C,0x00,0x20,0x00,0x47,0x00,0x72,0x00,0x61,0x00,0x70,0x00,0x68,0x00,
	0x69,0x00,0x63,0x00,0x73,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x01,0x00,0x00,0x00,0x48,0x00,0x00,0x00,0x08,0x1E,0x70,0x00,0x00,0x00,0x00,0x00,
	0x00,0x30,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
#endif
};

extern struct fsg_common *g_userspaceUVC_common;
extern unsigned char g_start_transfer;

unsigned short myidVendor = cpu_to_le16(HIDG_VENDOR_NUM);
unsigned short myidProduct = cpu_to_le16(HIDG_PRODUCT_NUM);
unsigned char myiManufacturer;
unsigned char myiProduct;
char myManufacturer[256];
unsigned int myManufacturer_len = 0;
char myProduct[256];
unsigned int myProduct_len = 0;

struct file *file_open(const char *path, int flags, int rights) 
{
	struct file *filp = NULL;
	mm_segment_t oldfs;
	int err = 0;

	oldfs = get_fs();
	set_fs(get_ds());
	filp = filp_open(path, flags, rights);  //  O_RDWR|O_RDONLY
	set_fs(oldfs);
	if (IS_ERR(filp)) {
		err = PTR_ERR(filp);
		printk("the error code is %d\n", err);
		return NULL;
	}
	return filp;
}

void file_close(struct file *file) 
{
	filp_close(file, NULL);
}

int file_read(struct file *file, unsigned long long offset, unsigned char *data, unsigned int size) 
{
	mm_segment_t oldfs;
	int ret;
	oldfs = get_fs();
	set_fs(get_ds());
	ret = vfs_read(file, data, size, &offset);
	set_fs(oldfs);
	return ret;
}

int file_write(struct file *file, unsigned long long offset, unsigned char *data, unsigned int size) 
{
	mm_segment_t oldfs;
	int ret;

	oldfs = get_fs();
	set_fs(get_ds());

	ret = vfs_write(file, data, size, &offset);

	set_fs(oldfs);
	return ret;
}

int file_sync(struct file *file) 
{
	vfs_fsync(file, 0);
	return 0;
}

extern char myVcDesc[2048];
extern unsigned int myVcDesc_len;

int read_property(int type)
{
	struct file * raw_fp = NULL;
	char buffer[2048];
	int len = 0;

	memset(buffer, '\0', sizeof(buffer));
	if (type == 1) {
		raw_fp = file_open(MANUFACTORER_RAW, O_RDONLY, 0);
		if (raw_fp) {
			len = file_read(raw_fp, 0, buffer, 256);
			memcpy(myManufacturer, buffer, len);
			myManufacturer_len = len;
			file_close(raw_fp);
		}
	} else if (type == 2) {
		raw_fp = file_open(PRODUCT_RAW, O_RDONLY, 0);
		if (raw_fp) {
			len = file_read(raw_fp, 0, buffer, 256);
			memcpy(myProduct, buffer, len);
			myProduct_len = len;
			file_close(raw_fp);
		}
	} else if (type == 3) {
		raw_fp = file_open(VC_DESC, O_RDONLY, 0);
		if (raw_fp) {
			len = file_read(raw_fp, 0, buffer, 2048);
			memset(myVcDesc, '\0', sizeof(myVcDesc));
			memcpy(myVcDesc, buffer, len);
			myVcDesc_len = len;
			file_close(raw_fp);
		}
	}

	return len;
}

/*
 * The setup() callback implements all the ep0 functionality that's
 * not handled lower down, in hardware or the hardware driver(like
 * device and endpoint feature flags, and their status).  It's all
 * housekeeping for the gadget function we're implementing.  Most of
 * the work is in config and function specific setup.
 */
static int composite_setup(struct usb_gadget *gadget, const struct usb_ctrlrequest *ctrl)
{
	struct usb_composite_dev	*cdev = get_gadget_data(gadget);
	struct usb_request		*req = cdev->req;
	int				value = -EOPNOTSUPP;
	u16				w_index = le16_to_cpu(ctrl->wIndex);
	u8				intf = w_index & 0xFF;
	u16				w_value = le16_to_cpu(ctrl->wValue);
	u16				w_length = le16_to_cpu(ctrl->wLength);
	struct usb_function		*f = NULL;
	u8				endp;
	MNSPXACTHDR udp_hdr;
	//printk("composite_setup_0x%x, 0x%x\n", (ctrl->bRequest), ctrl->bRequestType);
	/* partial re-init of the response message; the function or the
	 * gadget might need to intercept e.g. a control-OUT completion
	 * when we delegate to it.
	 */
	req->zero = 0;
	req->complete = composite_setup_complete;
	req->length = USB_BUFSIZ;
	gadget->ep0->driver_data = cdev;
	ep0_setup_dir = ctrl->bRequestType;
	//t6_vendor_cmd = ctrl->bRequest;

	if(((ctrl->bRequestType) & USB_TYPE_VENDOR) == USB_TYPE_VENDOR)
	{
		//printk("USB_TYPE_VENDOR\n");
		switch(ctrl->bRequest)
		{
			case VENDOR_REQ_GET_VERSION:
				switch(w_index)
				{
					case 0:
						memcpy(req->buf, "\x01\x00\x00\x00", 4);
						value = 4;
						break;
					case 1:
						memcpy(req->buf, "\x99\x28\x06\x16", 4);
						value = 4;
						break;
					case 2:
#if WACOM_1024X600
						memcpy(req->buf, "\x02\x03\x12\x18", 4);
						value = 4;
#else
						memcpy(req->buf, "\x01\x14\x09\x17", 4);
						value = 4;
#endif
						break;
					case 3:
#if WACOM_1024X600
						memcpy(req->buf, "\x41\x4c\x44\x32\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 16);
						value = 16;
#else
						memcpy(req->buf, "\x41\x38\x36\x32\x30\x48\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 16);
						value = 16;
#endif
						break;
					case 4:
						memcpy(req->buf, "\x06\x00\x00\x00", 4);
						value = 4;
						break;
					case 5:
						memcpy(req->buf, "\x00\x00\x00\x00\x00\x00\x00\x00", 8);
						value = 8;
						break;
				}
				break;
			case VENDOR_REQ_QUERY_SIZE:
#if WACOM_1024X600
				memcpy(req->buf, "\x00\x00\x00\x00", 4);
				value = 4;
#else
				memcpy(req->buf, "\x04\x00\x00\x00", 4);
				value = 4;
#endif
				break;
			case VENDOR_REQ_GET_FUNC_ID:
				switch(w_index)
				{
					case 0:
#if WACOM_1024X600
						memcpy(req->buf, "\x11\x07\x29\x56\x44\x00\x54\x00\x55\x00\x2D\x00\x31\x00\x30\x00"
							"\x33\x00\x31\x00\x41\x00\x58\x00\x00\x00\x00\x00\x00\x00\x00\x00"
							"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
							"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
							"\x00\x00\x00\x00\x01\x00\x00\x00\x80\x00\x00\x08\x08\x01\x70\x00"
							"\x00\x00\x00\x00\x40\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
							"\x00\x00\x00\x00\x00\xC8\x00\x00\x3C\x00\x40\x05\x00\x04\x18\x04"
							"\x88\x00\x7B\x02\x58\x02\x5A\x02\x04\x00\xE0\x01\xE8\x03\x14\x02"
							"\x00\x01\x00\x00", 132);
						value = 132;
#else
						memcpy(req->buf, "\x11\x07\x21\x56\x54\x00\x72\x00\x69\x00\x67\x00\x67\x00\x65\x00"
							"\x72\x00\x20\x00\x36\x00\x20\x00\x45\x00\x78\x00\x74\x00\x65\x00"
							"\x72\x00\x6E\x00\x61\x00\x6C\x00\x20\x00\x47\x00\x72\x00\x61\x00"
							"\x70\x00\x68\x00\x69\x00\x63\x00\x73\x00\x00\x00\x00\x00\x00\x00"
							"\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x08\x08\x1E\x70\x00"
							"\x00\x00\x00\x00\x00\x30\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
							"\x00\x00\x00\x00\x57\x62\x00\x00\x3C\x00\x20\x03\x80\x02\x90\x02"
							"\x60\x00\x0D\x02\xE0\x01\xEA\x01\x02\x00\x8C\x00\xE8\x03\x14\x00"
							"\x00\x00\x00\x00", 132);
						value = 132;

#endif
						break;
					case 3:
#if WACOM_1024X600
						memcpy(req->buf, "\xC0\xB1\x00\x00\x03\x00\x84\x00\x42\x00\x20\x00\x41\x00\x75\x00"
							"\x64\x00\x69\x00\x6F\x00\x20\x00\x44\x00\x65\x00\x76\x00\x69\x00"
							"\x63\x00\x65\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
							"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
							"\x00\x00\x00\x00\x03\x01\x70\x00\x00\x00\x00\x00\x11\x32\x00\x00"
							"\x11\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
							"\x00\x00\x00\x00\x80\xBB\x00\x00\x80\x00\x10\x02\x00\x00\x50\x46"
							"\x00\x00\x00\x00\x00\x70\x17\x00\xE9\x44\x01\x00\xEB\x01\xE8\x03"
							"\x00\x00\x00\x00", 8);
						value = 8;
#else
						memcpy(req->buf, "\x11\x07\x40\x56\x55\x00\x53\x00\x42\x00\x20\x00\x41\x00\x75\x00"
							"\x64\x00\x69\x00\x6F\x00\x20\x00\x44\x00\x65\x00\x76\x00\x69\x00"
							"\x63\x00\x65\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
							"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
							"\x00\x00\x00\x00\x03\x01\x70\x00\x00\x00\x00\x00\x11\x32\x00\x00"
							"\x11\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
							"\x00\x00\x00\x00\x80\xBB\x00\x00\x80\x00\x10\x02\x00\x00\x50\x46"
							"\x00\x00\x00\x00\x00\x70\x17\x00\xE9\x44\x01\x00\xEB\x01\xE8\x03"
							"\x00\x00\x00\x00", 132);
						value = 132;
#endif
						break;
				}
				break;
			case VENDOR_REQ_AUD_GET_ENGINE_CAPS:
				switch(w_index)
				{
					case 0:
						memcpy(req->buf, "\x00\x00\x01\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x02\x00", 16);
						value = 16;
						break;
					case 1:
						memcpy(req->buf, "\x00\x00\x01\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x02\x00", 16);
						value = 16;
						break;
				}
				break;
			case VENDOR_REQ_AUD_GET_ENGINE_STATE:
#if WACOM_1024X600
				memcpy(req->buf, "\x00\x05\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x02\x00", 16);
				value = 16;
#else
				memcpy(req->buf, "\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x02\x00", 16);
				value = 16;
#endif
				break;
			case VENDOR_REQ_AUD_GET_FORMAT_LIST:
				memcpy(req->buf, "\x80\xBB\x00\x00\x80\x00\x10\x02\x00\x00\x50\x46\x00\x00\x00\x00"
					"\x00\x70\x17\x00\xE9\x44\x01\x00\xEB\x01\xE8\x03\x00\x00\x00\x00", 32);
				value = 32;
				break;
			case VENDOR_REQ_AUD_GET_NODE_CAPS:
				memcpy(req->buf, "\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x1F\x00\x00\x00", 16);
				value = 16;
				break;
			case VENDOR_REQ_AUD_GET_NODE_VALUE:
				memcpy(req->buf, "\xFF\x00\x00\x00\x00\x00\x00\x00\x0F\x00\x00\x00\x0F\x00\x00\x00"
					"\x0F\x00\x00\x00\x0F\x00\x00\x00\x0F\x00\x00\x00\x0F\x00\x00\x00"
					"\x0F\x00\x00\x00\x0F\x00\x00\x00", 40);
				value = 40;
				break;
			case VENDOR_REQ_AUD_SET_NODE_VALUE:
				value = 40;
				break;
			case VENDOR_REQ_QUERY_VIDEO_RAM_SIZE:
				memcpy(req->buf, "\x3A", 1);
				value = 1;
				break;
			case VENDOR_REQ_QUERY_SECTION_DATA:
				t6rom[0x64] = compress_rate;
				t6rom[0x65] = bit_rate_control;
				memcpy(req->buf, t6rom, 112);
				//memcpy(req->buf, "\x44\x49\x53\x50\x90\x00\x00\x00\x00\x00\x00\x00\x11\x07\x29\x56"
				//	"\x44\x00\x54\x00\x55\x00\x2D\x00\x31\x00\x30\x00\x33\x00\x31\x00"
				//	"\x41\x00\x58\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
				//	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
				//	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
				//	"\x01\x00\x00\x00\x40\x00\x00\x08\x08\x01\x70\x00\x00\x00\x00\x00"
				//	"\x00\x00\x00\x00\x5A\x32\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 112);
				value = 112;
				break;
			case VENDOR_REQ_GET_RESOLUTION_TIMING_TABLE:
				switch(w_value)
				{
					case 0:
						switch(w_index)
						{
							case 0:
								memcpy(req->buf, 
#if WACOM_1024X600
									"\x00\xC8\x00\x00\x3C\x00\x40\x05\x00\x04\x18\x04\x88\x00\x7B\x02"
									"\x58\x02\x5A\x02\x04\x00\xE0\x01\xE8\x03\x14\x02\x00\x01\x00\x00"
									"\x0C\x7B\x00\x00\x4B\x00\x48\x03\x80\x02\x90\x02\x40\x00\xF4\x01"
									"\xE0\x01\xE1\x01\x03\x00\xC7\x00\xE8\x03\x19\x00\x00\x00\x00\x00"
#else
									"\x57\x62\x00\x00\x3C\x00\x20\x03\x80\x02\x90\x02\x60\x00\x0D\x02"
									"\xE0\x01\xEA\x01\x02\x00\x8C\x00\xE8\x03\x14\x00\x00\x00\x00\x00"
									"\x0C\x7B\x00\x00\x4B\x00\x48\x03\x80\x02\x90\x02\x40\x00\xF4\x01"
									"\xE0\x01\xE1\x01\x03\x00\xC7\x00\xE8\x03\x19\x00\x00\x00\x00\x00"
#endif
									"\x40\x9C\x00\x00\x3C\x00\x20\x04\x20\x03\x48\x03\x80\x00\x74\x02"
									"\x58\x02\x59\x02\x04\x00\x00\x00\xE8\x03\x20\x00\x01\x01\x00\x00"
									"\x50\xC3\x00\x00\x48\x00\x10\x04\x20\x03\x58\x03\x78\x00\x9A\x02"
									"\x58\x02\x7D\x02\x06\x00\x00\x00\xE8\x03\x14\x02\x01\x01\x00\x00"
									"\x5C\xC1\x00\x00\x4B\x00\x20\x04\x20\x03\x30\x03\x50\x00\x71\x02"
									"\x58\x02\x59\x02\x03\x00\x58\x02\xE8\x03\x27\x00\x01\x01\x00\x00"
									"\xE8\xFD\x00\x00\x3C\x00\x40\x05\x00\x04\x18\x04\x88\x00\x26\x03"
									"\x00\x03\x03\x03\x06\x00\x00\x00\xE8\x03\x1A\x02\x00\x00\x00\x00"
									"\xF8\x24\x01\x00\x46\x00\x30\x05\x00\x04\x18\x04\x88\x00\x26\x03"
									"\x00\x03\x03\x03\x06\x00\x00\x00\xE8\x03\x1E\x02\x00\x00\x00\x00"
									"\x9E\x33\x01\x00\x4B\x00\x20\x05\x00\x04\x10\x04\x60\x00\x20\x03"
									"\x00\x03\x01\x03\x03\x00\xF4\x01\xE8\x03\x1F\x02\x01\x01\x00\x00"
									"\xE0\xA5\x01\x00\x4B\x00\x40\x06\x80\x04\xC0\x04\x80\x00\x84\x03"
									"\x60\x03\x61\x03\x03\x00\x58\x02\xE8\x03\x15\x01\x01\x01\x00\x00"
									"\x0A\x22\x01\x00\x3C\x00\x72\x06\x00\x05\x6E\x05\x28\x00\xEE\x02"
									"\xD0\x02\xD5\x02\x05\x00\xBB\x02\xE8\x03\x1D\x02\x01\x01\x00\x00"
									"\x8C\x36\x01\x00\x3C\x00\x80\x06\x00\x05\x40\x05\x80\x00\x1E\x03"
									"\x00\x03\x03\x03\x07\x00\x20\x03\xE8\x03\x1F\x02\x00\x01\x00\x00"
									"\x6A\x8F\x01\x00\x4B\x00\xA0\x06\x00\x05\x50\x05\x80\x00\x25\x03"
									"\x00\x03\x03\x03\x07\x00\xC1\x01\xE8\x03\x14\x01\x00\x01\x00\x00"
									"\x2C\x46\x01\x00\x3C\x00\x90\x06\x00\x05\x48\x05\x80\x00\x3F\x03"
									"\x20\x03\x23\x03\x06\x00\x8F\x01\xE8\x03\x21\x02\x00\x01\x00\x00"
									"\x04\xA0\x01\x00\x4B\x00\xA0\x06\x00\x05\x50\x05\x80\x00\x46\x03"
									"\x20\x03\x23\x03\x06\x00\x2C\x01\xE8\x03\x15\x01\x00\x01\x00\x00"
									"\xE0\xA5\x01\x00\x3C\x00\x08\x07\x00\x05\x60\x05\x70\x00\xE8\x03"
									"\xC0\x03\xC1\x03\x03\x00\x58\x02\xE8\x03\x15\x01\x01\x01\x00\x00"
									"\xE0\xA5\x01\x00\x3C\x00\x98\x06\x00\x05\x30\x05\x70\x00\x2A\x04"
									"\x00\x04\x01\x04\x03\x00\x58\x02\xE8\x03\x15\x01\x01\x01\x00\x00", 32);
								value = 32;
								break;
							case 0x200:
#if WACOM_1024X600
								memcpy(req->buf, 
									"\x58\x0F\x02\x00\x4B\x00\x98\x06\x00\x05\x10\x05\x90\x00\x2A\x04"
									"\x00\x04\x01\x04\x03\x00\x00\x00\xE8\x03\x1B\x01\x01\x01\x00\x00"
									"\xFC\x4D\x01\x00\x3C\x00\x00\x07\x50\x05\x90\x05\x70\x00\x1B\x03"
									"\x00\x03\x03\x03\x06\x00\xC8\x00\xE8\x03\x22\x02\x01\x01\x00\x02"
									"\xF6\x4E\x01\x00\x3C\x00\x00\x07\x56\x05\x9C\x05\x8F\x00\x1E\x03"
									"\x00\x03\x03\x03\x03\x00\x2B\x01\xE8\x03\x22\x02\x01\x01\x00\x00"
									"\x96\xDB\x01\x00\x3C\x00\x48\x07\x78\x05\xD0\x05\x90\x00\x41\x04"
									"\x1A\x04\x1D\x04\x04\x00\x5E\x01\xE8\x03\x18\x01\x00\x01\x00\x00"
									"\x60\x61\x02\x00\x4B\x00\x68\x07\x78\x05\xE0\x05\x90\x00\x4B\x04"
									"\x1A\x04\x1D\x04\x04\x00\xC7\x00\xE8\x03\x1F\x01\x00\x01\x00\x00"
									"\x04\xA0\x01\x00\x3C\x00\x70\x07\xA0\x05\xF0\x05\x98\x00\xA6\x03"
									"\x84\x03\x87\x03\x06\x00\x2C\x01\xE8\x03\x15\x01\x00\x01\x00\x00"
									"\x2E\x16\x02\x00\x4B\x00\x90\x07\xA0\x05\x00\x06\x98\x00\xAE\x03"
									"\x84\x03\x87\x03\x06\x00\x5E\x01\xE8\x03\x1B\x01\x00\x01\x00\x00"
									"\xD6\x7D\x01\x00\x3C\x00\xE0\x06\x40\x06\x70\x06\x20\x00\x98\x03"
									"\x84\x03\x87\x03\x05\x00\x64\x00\xE8\x03\x27\x02\x01\x00\x01\x00"
									"\x4A\x3B\x02\x00\x3C\x00\xC0\x08\x90\x06\xF8\x06\xB0\x00\x41\x04"
									"\x1A\x04\x1D\x04\x06\x00\xFA\x00\xE8\x03\x1D\x01\x00\x01\x00\x02"
									"\xD0\x78\x02\x00\x3C\x00\x70\x08\x40\x06\x80\x06\xC0\x00\xE2\x04"
									"\xB0\x04\xB1\x04\x03\x00\x8F\x01\xE8\x03\x20\x01\x01\x01\x00\x00"
									"\x14\x44\x02\x00\x3C\x00\x98\x08\x80\x07\xD8\x07\x2C\x00\x65\x04"
									"\x38\x04\x3C\x04\x05\x00\xBB\x02\xE8\x03\x1D\x01\x01\x01\x00\x00"
									"\xE2\xF2\x02\x00\x3C\x00\x20\x0A\x80\x07\x08\x08\xC8\x00\xDD\x04"
									"\xB0\x04\xB3\x04\x06\x00\x89\x02\xE8\x03\x26\x01\x00\x01\x00\x00"
									"\x88\x01\x03\x00\x3C\x00\xC0\x0A\x00\x08\x88\x08\xD7\x00\xAB\x04"
									"\x80\x04\x85\x04\x05\x00\x8F\x01\xE8\x03\x27\x01\x01\x00\x00\x00"
									"\x70\x05\x03\x00\x3C\x00\xB8\x0B\x00\x0A\xF8\x0A\x2C\x00\x4C\x04"
									"\x38\x04\x3C\x04\x05\x00\x58\x02\xE8\x03\x27\x01\x01\x01\x00\x00"
									"\x24\xB0\x03\x00\x3C\x00\xA0\x0A\x00\x0A\x30\x0A\x20\x00\xC9\x05"
									"\xA0\x05\xA3\x05\x05\x00\xA9\x00\xE8\x03\x18\x03\x01\x00\x01\x00"
									"\xD4\x18\x04\x00\x3C\x00\xA0\x0A\x00\x0A\x30\x0A\x20\x00\x6E\x06"
									"\x40\x06\x43\x06\x06\x00\x52\x03\xE8\x03\x1A\x03\x01\x00\x01\x00", 512);
								value = 512;
#else
								memcpy(req->buf, 
									"\x0A\x22\x01\x00\x3C\x00\x72\x06\x00\x05\x6E\x05\x28\x00\xEE\x02"
									"\xD0\x02\xD5\x02\x05\x00\xBB\x02\xE8\x03\x1D\x02\x01\x01\x00\x00"
									"\x58\x0F\x02\x00\x4B\x00\x98\x06\x00\x05\x10\x05\x90\x00\x2A\x04"
									"\x00\x04\x01\x04\x03\x00\x00\x00\xE8\x03\x1B\x01\x01\x01\x00\x00"
									"\xFC\x4D\x01\x00\x3C\x00\x00\x07\x50\x05\x90\x05\x70\x00\x1B\x03"
									"\x00\x03\x03\x03\x06\x00\xC8\x00\xE8\x03\x22\x02\x01\x01\x00\x02"
									"\xF6\x4E\x01\x00\x3C\x00\x00\x07\x56\x05\x9C\x05\x8F\x00\x1E\x03"
									"\x00\x03\x03\x03\x03\x00\x2B\x01\xE8\x03\x22\x02\x01\x01\x00\x00"
									"\x96\xDB\x01\x00\x3C\x00\x48\x07\x78\x05\xD0\x05\x90\x00\x41\x04"
									"\x1A\x04\x1D\x04\x04\x00\x5E\x01\xE8\x03\x18\x01\x00\x01\x00\x00"
									"\x60\x61\x02\x00\x4B\x00\x68\x07\x78\x05\xE0\x05\x90\x00\x4B\x04"
									"\x1A\x04\x1D\x04\x04\x00\xC7\x00\xE8\x03\x1F\x01\x00\x01\x00\x00"
									"\x04\xA0\x01\x00\x3C\x00\x70\x07\xA0\x05\xF0\x05\x98\x00\xA6\x03"
									"\x84\x03\x87\x03\x06\x00\x2C\x01\xE8\x03\x15\x01\x00\x01\x00\x00"
									"\x2E\x16\x02\x00\x4B\x00\x90\x07\xA0\x05\x00\x06\x98\x00\xAE\x03"
									"\x84\x03\x87\x03\x06\x00\x5E\x01\xE8\x03\x1B\x01\x00\x01\x00\x00"
									"\xD6\x7D\x01\x00\x3C\x00\xE0\x06\x40\x06\x70\x06\x20\x00\x98\x03"
									"\x84\x03\x87\x03\x05\x00\x64\x00\xE8\x03\x27\x02\x01\x00\x01\x00"
									"\x4A\x3B\x02\x00\x3C\x00\xC0\x08\x90\x06\xF8\x06\xB0\x00\x41\x04"
									"\x1A\x04\x1D\x04\x06\x00\xFA\x00\xE8\x03\x1D\x01\x00\x01\x00\x02"
									"\xD0\x78\x02\x00\x3C\x00\x70\x08\x40\x06\x80\x06\xC0\x00\xE2\x04"
									"\xB0\x04\xB1\x04\x03\x00\x8F\x01\xE8\x03\x20\x01\x01\x01\x00\x00"
									"\x14\x44\x02\x00\x3C\x00\x98\x08\x80\x07\xD8\x07\x2C\x00\x65\x04"
									"\x38\x04\x3C\x04\x05\x00\xBB\x02\xE8\x03\x1D\x01\x01\x01\x00\x00"
									"\x14\x44\x02\x00\x32\x00\x50\x0A\x80\x07\x90\x09\x2C\x00\x65\x04"
									"\x38\x04\x3C\x04\x05\x00\xBB\x02\xE8\x03\x1D\x01\x01\x01\x00\x00"
									"\xE2\xF2\x02\x00\x3C\x00\x20\x0A\x80\x07\x08\x08\xC8\x00\xDD\x04"
									"\xB0\x04\xB3\x04\x06\x00\x89\x02\xE8\x03\x26\x01\x00\x01\x00\x00"
									"\x88\x01\x03\x00\x3C\x00\xC0\x0A\x00\x08\x88\x08\xD7\x00\xAB\x04"
									"\x80\x04\x85\x04\x05\x00\x8F\x01\xE8\x03\x27\x01\x01\x00\x00\x00", 480);
								value = 480;
#endif
								break;
							case 0x400:
								memcpy(req->buf, "\x5E\x02\x04\x00\x1E\x00\xA0\x0F\x00\x0F\x30\x0F\x20\x00\x8F\x08"
									"\x70\x08\x73\x08\x05\x00\x12\x01\xE8\x03\x1A\x03\x01\x01\x01\x00"
									"\x14\x44\x02\x00\x32\x00\x50\x0A\x80\x07\x90\x09\x2C\x00\x65\x04"
									"\x38\x04\x3C\x04\x05\x00\xBB\x02\xE8\x03\x1D\x01\x01\x01\x00\x00", 64);
								value = 64;
								break;
						}	
						break;
					case 1:
						switch(w_index)
						{
							case 0:
								memcpy(req->buf, "\x57\x62\x00\x00\x3C\x00\x20\x03\x80\x02\x90\x02\x60\x00\x0D\x02"
									"\xE0\x01\xEA\x01\x02\x00\x8C\x00\xE8\x03\x14\x00\x00\x00\x00\x00"
									"\x0C\x7B\x00\x00\x4B\x00\x48\x03\x80\x02\x90\x02\x40\x00\xF4\x01"
									"\xE0\x01\xE1\x01\x03\x00\xC7\x00\xE8\x03\x19\x00\x00\x00\x00\x00"
									"\x40\x9C\x00\x00\x3C\x00\x20\x04\x20\x03\x48\x03\x80\x00\x74\x02"
									"\x58\x02\x59\x02\x04\x00\x00\x00\xE8\x03\x20\x00\x01\x01\x00\x00"
									"\x50\xC3\x00\x00\x48\x00\x10\x04\x20\x03\x58\x03\x78\x00\x9A\x02"
									"\x58\x02\x7D\x02\x06\x00\x00\x00\xE8\x03\x14\x02\x01\x01\x00\x00"
									"\x5C\xC1\x00\x00\x4B\x00\x20\x04\x20\x03\x30\x03\x50\x00\x71\x02"
									"\x58\x02\x59\x02\x03\x00\x58\x02\xE8\x03\x27\x00\x01\x01\x00\x00"
									"\xE8\xFD\x00\x00\x3C\x00\x40\x05\x00\x04\x18\x04\x88\x00\x26\x03"
									"\x00\x03\x03\x03\x06\x00\x00\x00\xE8\x03\x1A\x02\x00\x00\x00\x00"
									"\xF8\x24\x01\x00\x46\x00\x30\x05\x00\x04\x18\x04\x88\x00\x26\x03"
									"\x00\x03\x03\x03\x06\x00\x00\x00\xE8\x03\x1E\x02\x00\x00\x00\x00"
									"\x9E\x33\x01\x00\x4B\x00\x20\x05\x00\x04\x10\x04\x60\x00\x20\x03"
									"\x00\x03\x01\x03\x03\x00\xF4\x01\xE8\x03\x1F\x02\x01\x01\x00\x00"
									"\xE0\xA5\x01\x00\x4B\x00\x40\x06\x80\x04\xC0\x04\x80\x00\x84\x03"
									"\x60\x03\x61\x03\x03\x00\x58\x02\xE8\x03\x15\x01\x01\x01\x00\x00"
									"\x0A\x22\x01\x00\x3C\x00\x72\x06\x00\x05\x6E\x05\x28\x00\xEE\x02"
									"\xD0\x02\xD5\x02\x05\x00\xBB\x02\xE8\x03\x1D\x02\x01\x01\x00\x00"
									"\x8C\x36\x01\x00\x3C\x00\x80\x06\x00\x05\x40\x05\x80\x00\x1E\x03"
									"\x00\x03\x03\x03\x07\x00\x20\x03\xE8\x03\x1F\x02\x00\x01\x00\x00"
									"\x6A\x8F\x01\x00\x4B\x00\xA0\x06\x00\x05\x50\x05\x80\x00\x25\x03"
									"\x00\x03\x03\x03\x07\x00\xC1\x01\xE8\x03\x14\x01\x00\x01\x00\x00"
									"\x2C\x46\x01\x00\x3C\x00\x90\x06\x00\x05\x48\x05\x80\x00\x3F\x03"
									"\x20\x03\x23\x03\x06\x00\x8F\x01\xE8\x03\x21\x02\x00\x01\x00\x00"
									"\x04\xA0\x01\x00\x4B\x00\xA0\x06\x00\x05\x50\x05\x80\x00\x46\x03"
									"\x20\x03\x23\x03\x06\x00\x2C\x01\xE8\x03\x15\x01\x00\x01\x00\x00"
									"\xE0\xA5\x01\x00\x3C\x00\x08\x07\x00\x05\x60\x05\x70\x00\xE8\x03"
									"\xC0\x03\xC1\x03\x03\x00\x58\x02\xE8\x03\x15\x01\x01\x01\x00\x00"
									"\xE0\xA5\x01\x00\x3C\x00\x98\x06\x00\x05\x30\x05\x70\x00\x2A\x04"
									"\x00\x04\x01\x04\x03\x00\x58\x02\xE8\x03\x15\x01\x01\x01\x00\x00", 512);
								value = 512;
								break;
							case 0x200:
								memcpy(req->buf, "\x58\x0F\x02\x00\x4B\x00\x98\x06\x00\x05\x10\x05\x90\x00\x2A\x04"
									"\x00\x04\x01\x04\x03\x00\x00\x00\xE8\x03\x1B\x01\x01\x01\x00\x00"
									"\xFC\x4D\x01\x00\x3C\x00\x00\x07\x50\x05\x90\x05\x70\x00\x1B\x03"
									"\x00\x03\x03\x03\x06\x00\xC8\x00\xE8\x03\x22\x02\x01\x01\x00\x02"
									"\xF6\x4E\x01\x00\x3C\x00\x00\x07\x56\x05\x9C\x05\x8F\x00\x1E\x03"
									"\x00\x03\x03\x03\x03\x00\x2B\x01\xE8\x03\x22\x02\x01\x01\x00\x00"
									"\x96\xDB\x01\x00\x3C\x00\x48\x07\x78\x05\xD0\x05\x90\x00\x41\x04"
									"\x1A\x04\x1D\x04\x04\x00\x5E\x01\xE8\x03\x18\x01\x00\x01\x00\x00"
									"\x60\x61\x02\x00\x4B\x00\x68\x07\x78\x05\xE0\x05\x90\x00\x4B\x04"
									"\x1A\x04\x1D\x04\x04\x00\xC7\x00\xE8\x03\x1F\x01\x00\x01\x00\x00"
									"\x04\xA0\x01\x00\x3C\x00\x70\x07\xA0\x05\xF0\x05\x98\x00\xA6\x03"
									"\x84\x03\x87\x03\x06\x00\x2C\x01\xE8\x03\x15\x01\x00\x01\x00\x00"
									"\x2E\x16\x02\x00\x4B\x00\x90\x07\xA0\x05\x00\x06\x98\x00\xAE\x03"
									"\x84\x03\x87\x03\x06\x00\x5E\x01\xE8\x03\x1B\x01\x00\x01\x00\x00"
									"\xD6\x7D\x01\x00\x3C\x00\xE0\x06\x40\x06\x70\x06\x20\x00\x98\x03"
									"\x84\x03\x87\x03\x05\x00\x64\x00\xE8\x03\x27\x02\x01\x00\x01\x00"
									"\x4A\x3B\x02\x00\x3C\x00\xC0\x08\x90\x06\xF8\x06\xB0\x00\x41\x04"
									"\x1A\x04\x1D\x04\x06\x00\xFA\x00\xE8\x03\x1D\x01\x00\x01\x00\x02"
									"\xD0\x78\x02\x00\x3C\x00\x70\x08\x40\x06\x80\x06\xC0\x00\xE2\x04"
									"\xB0\x04\xB1\x04\x03\x00\x8F\x01\xE8\x03\x20\x01\x01\x01\x00\x00"
									"\x14\x44\x02\x00\x3C\x00\x98\x08\x80\x07\xD8\x07\x2C\x00\x65\x04"
									"\x38\x04\x3C\x04\x05\x00\xBB\x02\xE8\x03\x1D\x01\x01\x01\x00\x00"
									"\xE2\xF2\x02\x00\x3C\x00\x20\x0A\x80\x07\x08\x08\xC8\x00\xDD\x04"
									"\xB0\x04\xB3\x04\x06\x00\x89\x02\xE8\x03\x26\x01\x00\x01\x00\x00"
									"\x88\x01\x03\x00\x3C\x00\xC0\x0A\x00\x08\x88\x08\xD7\x00\xAB\x04"
									"\x80\x04\x85\x04\x05\x00\x8F\x01\xE8\x03\x27\x01\x01\x00\x00\x00"
									"\x70\x05\x03\x00\x3C\x00\xB8\x0B\x00\x0A\xF8\x0A\x2C\x00\x4C\x04"
									"\x38\x04\x3C\x04\x05\x00\x58\x02\xE8\x03\x27\x01\x01\x01\x00\x00"
									"\x14\x44\x02\x00\x32\x00\x50\x0A\x80\x07\x90\x09\x2C\x00\x65\x04"
									"\x38\x04\x3C\x04\x05\x00\xBB\x02\xE8\x03\x1D\x01\x01\x01\x00\x00", 480);
								value = 480;
								break;
						}
						break;
				}
				break;
			case VENDOR_REQ_QUERY_MONITOR_CONNECTION_STATUS:
				switch(w_value)
				{
					case 0:
						memcpy(req->buf, "\x00", 1);
						value = 1;
						break;
					case 1:
						memcpy(req->buf, "\x01", 1);
						value = 1;
						break;
				}
				break;
			case VENDOR_REQ_SET_MONITOR_CTRL:
				switch(w_index)
				{
					case 0:
						value = 0;
						break;
					case 1:
						value = 0;
						break;
				}
				break;
			case VENDOR_REQ_SET_CURSOR_SHAPE:
				//printk("SHAPE\n");
				t6_vendor_cmd = ctrl->bRequest;
				g_cursor_buf_offset = w_index/512;
				g_cursor_shape_idx = w_value;
				memcpy(&g_udp_cursor_shape_buf[g_cursor_shape_idx][sizeof(udp_hdr)+g_cursor_buf_offset*(UDP_CURSORSHAPEBLOCK_SIZE+UDP_CURSORBLOCK_SIZE)], ctrl, UDP_CURSORBLOCK_SIZE);
				value = w_length;
				break;
			case VENDOR_REQ_SET_CURSOR1_SHAPE:
				//printk("SHAPE1\n");
				t6_vendor_cmd = ctrl->bRequest;
				g_cursor_buf_offset = w_index/512;
				g_cursor_shape_idx = w_value;
				memcpy(&g_udp_cursor_shape_buf[g_cursor_shape_idx][sizeof(udp_hdr)+g_cursor_buf_offset*(UDP_CURSORSHAPEBLOCK_SIZE+UDP_CURSORBLOCK_SIZE)], ctrl, UDP_CURSORBLOCK_SIZE);
				value = w_length;
				break;
			case VENDOR_REQ_SET_CURSOR1_POS:
				//if(g_UCURconn_socket)
				{
					udp_hdr.Tag = MNSP_TAG;
					udp_hdr.XactType = MNSP_XACTTYPE_CURSOR;
					udp_hdr.HdrSize = sizeof(udp_hdr);
					udp_hdr.XactId = g_udp_control_XactId;
					udp_hdr.XactOffset = 0;
					udp_hdr.PayloadLength = UDP_CURSORBLOCK_SIZE;
					udp_hdr.TotalLength = UDP_CURSORBLOCK_SIZE;
					memcpy(g_udp_cursor_buf, &udp_hdr, sizeof(udp_hdr));
					memcpy(&g_udp_cursor_buf[sizeof(udp_hdr)], ctrl, UDP_CURSORBLOCK_SIZE);
					//spin_lock_irqsave(&cursor_lock, cursor_flags);
					smp_wmb();
					//wakeup_thread(&cursor_udp_common);
				}
				value = 0;
				break;
			case VENDOR_REQ_SET_CURSOR1_STATE:
				//if(g_UCURconn_socket)
				{
					t6_vendor_cmd = ctrl->bRequest;
					udp_hdr.Tag = MNSP_TAG;
					udp_hdr.XactType = MNSP_XACTTYPE_CURSOR;
					udp_hdr.HdrSize = sizeof(udp_hdr);
					udp_hdr.XactId = g_udp_control_XactId;
					udp_hdr.XactOffset = 0;
					udp_hdr.PayloadLength = UDP_CURSORBLOCK_SIZE;
					udp_hdr.TotalLength = UDP_CURSORBLOCK_SIZE;
					memcpy(g_udp_cursor_state_buf, &udp_hdr, sizeof(udp_hdr));
					memcpy(&g_udp_cursor_state_buf[sizeof(udp_hdr)], ctrl, UDP_CURSORBLOCK_SIZE);
					//spin_lock_irqsave(&cursor_lock, cursor_flags);
					smp_wmb();
					//wakeup_thread(&cursor_tcp_common);
				}
				value = 0;
				break;
			case VENDOR_REQ_SET_CURSOR2_SHAPE:
				//printk("SHAPE2\n");
				t6_vendor_cmd = ctrl->bRequest;
				g_cursor_buf_offset = w_index/512;
				g_cursor_shape_idx = w_value;
				memcpy(&g_udp_cursor_shape_buf[g_cursor_shape_idx][sizeof(udp_hdr)+g_cursor_buf_offset*(UDP_CURSORSHAPEBLOCK_SIZE+UDP_CURSORBLOCK_SIZE)], ctrl, UDP_CURSORBLOCK_SIZE);
				value = w_length;
				break;
			case VENDOR_REQ_SET_CURSOR2_POS:
				//if(g_UCURconn_socket)
				{
					udp_hdr.Tag = MNSP_TAG;
					udp_hdr.XactType = MNSP_XACTTYPE_CURSOR;
					udp_hdr.HdrSize = sizeof(udp_hdr);
					udp_hdr.XactId = g_udp_control_XactId;
					udp_hdr.XactOffset = 0;
					udp_hdr.PayloadLength = UDP_CURSORBLOCK_SIZE;
					udp_hdr.TotalLength = UDP_CURSORBLOCK_SIZE;
					memcpy(g_udp_cursor_buf, &udp_hdr, sizeof(udp_hdr));
					memcpy(&g_udp_cursor_buf[sizeof(udp_hdr)], ctrl, UDP_CURSORBLOCK_SIZE);
					//spin_lock_irqsave(&cursor_lock, cursor_flags);
					smp_wmb();
					//wakeup_thread(&cursor_udp_common);
				}
				value = 0;
				break;
			case VENDOR_REQ_SET_CURSOR2_STATE:
				//if(g_UCURconn_socket)
				{
					t6_vendor_cmd = ctrl->bRequest;
					udp_hdr.Tag = MNSP_TAG;
					udp_hdr.XactType = MNSP_XACTTYPE_CURSOR;
					udp_hdr.HdrSize = sizeof(udp_hdr);
					udp_hdr.XactId = g_udp_control_XactId;
					udp_hdr.XactOffset = 0;
					udp_hdr.PayloadLength = UDP_CURSORBLOCK_SIZE;
					udp_hdr.TotalLength = UDP_CURSORBLOCK_SIZE;
					memcpy(g_udp_cursor_state_buf, &udp_hdr, sizeof(udp_hdr));
					memcpy(&g_udp_cursor_state_buf[sizeof(udp_hdr)], ctrl, UDP_CURSORBLOCK_SIZE);
					//spin_lock_irqsave(&cursor_lock, cursor_flags);
					smp_wmb();
					//wakeup_thread(&cursor_tcp_common);
				}
				value = 0;
				break;
			case VENDOR_REQ_SET_CANCEL_BULK_OUT:
				value = 0;
				break;
			case VENDOR_REQ_GET_EDID:
				switch(w_value)
				{
					case 0:
						switch(w_index)
						{
							case 0:
#if WACOM_1024X600
								memcpy(req->buf, "\x00\xFF\xFF\xFF\xFF\xFF\xFF\x00\x5C\x23\x45\x10\x01\x00\x00\x00"
									"\x15\x1C\x01\x03\x80\x16\x0D\x78\x2A\x13\x60\x97\x58\x57\x91\x26"
									"\x1E\x50\x54\x00\x00\x00\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01"
									"\x01\x01\x01\x01\x01\x01\xC7\x13\x00\x40\x41\x58\x1C\x20\x18\x88"
									"\x14\x00\xE7\x86\x00\x00\x00\x18\x00\x00\x00\xFC\x00\x44\x54\x55"
									"\x2D\x31\x30\x33\x31\x41\x58\x0A\x20\x20\x00\x00\x00\xFF\x00\x31"
									"\x0A\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x00\x00\x00\x00"
									"\x00\x0B\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xEA", 128);
								value = 128;
#else
								memcpy(req->buf, "\x00\xFF\xFF\xFF\xFF\xFF\xFF\x00\x1C\xAE\x73\x24\x01\x01\x01\x01"
									"\x00\x14\x01\x03\x81\x30\x0B\x78\x2A\xE6\x75\xA4\x56\x4F\x9E\x27"
									"\x0F\x50\x54\xBF\xEF\x80\xB3\x00\xA9\x40\x95\x00\x81\x40\x81\x80"
									"\x95\x0F\x71\x4F\x90\x40\x02\x3A\x80\x18\x71\x38\x2D\x40\x58\x2C"
									"\x45\x00\xDE\x0D\x11\x00\x00\x1E\x66\x21\x50\xB0\x51\x00\x1B\x30"
									"\x40\x70\x36\x00\xDE\x0D\x11\x00\x00\x1E\x00\x00\x00\xFD\x00\x37"
									"\x4C\x1E\x52\x11\x00\x0A\x20\x20\x20\x20\x20\x20\x00\x00\x00\xFC"
									"\x00\x47\x65\x6E\x75\x69\x6E\x65\x32\x31\x2E\x35\x27\x27\x01\x13", 128);
								value = 128;
#endif
								break;
							case 1:
								memcpy(req->buf, "\x00\xFF\xFF\xFF\xFF\xFF\xFF\x00\x10\xAC\xBB\x40\x4C\x30\x31\x30"
									"\x0E\x1B\x01\x03\x80\x3C\x22\x78\xEA\xEE\x95\xA3\x54\x4C\x99\x26"
									"\x0F\x50\x54\xA5\x4B\x00\x71\x4F\x81\x80\xA9\xC0\xD1\xC0\x01\x01"
									"\x01\x01\x01\x01\x01\x01\x02\x3A\x80\x18\x71\x38\x2D\x40\x58\x2C"
									"\x45\x00\x56\x50\x21\x00\x00\x1E\x00\x00\x00\xFF\x00\x39\x33\x4B"
									"\x34\x35\x37\x34\x36\x30\x31\x30\x4C\x0A\x00\x00\x00\xFC\x00\x44"
									"\x45\x4C\x4C\x20\x53\x32\x37\x31\x35\x48\x0A\x20\x00\x00\x00\xFD"
									"\x00\x38\x4C\x1E\x53\x11\x00\x0A\x20\x20\x20\x20\x20\x20\x01\x3C", 128);
								value = 128;
								break;
						}
						break;
					case 0x80:
						switch(w_index)
						{
							case 0:
								memcpy(req->buf, "\x02\x03\x22\xF1\x4F\x9F\x14\x13\x12\x11\x16\x15\x10\x05\x04\x03"
									"\x02\x07\x06\x01\x23\x09\x07\x01\x83\x01\x00\x00\x65\x03\x0C\x00"
									"\x10\x00\x02\x3A\x80\xD0\x72\x38\x2D\x40\x10\x2C\x45\x80\xDE\x0D"
									"\x11\x00\x00\x1F\x01\x1D\x80\xD0\x72\x1C\x16\x20\x10\x2C\x25\x00"
									"\xDE\x0D\x11\x00\x00\x9F\x01\x1D\x00\xBC\x52\xD0\x1E\x20\xB8\x28"
									"\x55\x40\xDE\x0D\x11\x00\x00\x1E\x8C\x0A\xD0\x90\x20\x40\x31\x20"
									"\x0C\x40\x55\x00\xDE\x0D\x11\x00\x00\x18\x02\x3A\x80\x18\x71\x38"
									"\x2D\x40\x58\x2C\x45\x00\xDE\x0D\x11\x00\x00\x1E\x00\x00\x00\x3E", 128);
								value = 128;
								break;
							case 1:
								memcpy(req->buf, "\x02\x03\x26\xF1\x4F\x90\x05\x04\x03\x02\x07\x16\x01\x06\x11\x12"
									"\x15\x13\x14\x1F\x23\x09\x07\x07\x65\x03\x0C\x00\x10\x00\x83\x01"
									"\x00\x00\xE3\x05\x03\x01\x02\x3A\x80\x18\x71\x38\x2D\x40\x58\x2C"
									"\x45\x00\x56\x50\x21\x00\x00\x1E\x01\x1D\x80\x18\x71\x1C\x16\x20"
									"\x58\x2C\x25\x00\x56\x50\x21\x00\x00\x9E\x01\x1D\x00\x72\x51\xD0"
									"\x1E\x20\x6E\x28\x55\x00\x56\x50\x21\x00\x00\x1E\x8C\x0A\xD0\x8A"
									"\x20\xE0\x2D\x10\x10\x3E\x96\x00\x56\x50\x21\x00\x00\x18\x00\x00"
									"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x59", 128);
								value = 128;
								break;
						}
						break;
				}
				break;
			case VENDOR_REQ_SOFTWARE_READY:
				value = 0;
				break;
			case VENDOR_REQ_SET_RESOLUTION_DETAIL_TIMING:
				value = 32;
				break;
			case 0x13:
				value = 0;
			case VENDOR_REQ_AUD_SET_ENGINE_STATE:
				value = 16;
				break;
			default:
				
				break;
		}
	}
	else if(((ctrl->bRequestType) & USB_TYPE_CLASS) == USB_TYPE_CLASS)
	{
		//printk("USB_TYPE_CLASS\n");
		switch (ctrl->bRequest)
		{
			case 0x0A:
				value = 0x00;
				break;
			case UVC_SET_CUR:
				//printk("UVC_SET_CUR, %02x\n", w_value);
				//hex_dump(req->buf, CY_FX_UVC_MAX_PROBE_SETTING);
				gCurItfnum = w_index & 0xff;
				if(gCurItfnum == 0)
				{
					if (client_connected == 1) {
						memcpy(gSetDirBuff, ctrl, 8);
						memcpy(g_mct_uvc_setup_insert->setupcmd, ctrl, 8);
					} else {
						switch(w_value)
						{
							case 0x200://start streaming
								//printk("Start Streaming on interface 0\n");
								//memcpy(mct_uvc_data.setuptoken_8byte, ctrl, 8);
								memset(mct_uvc_data.setuptoken_8byte, '\0', 8);
								current_b_request = ctrl->bRequest;
								current_w_value = w_value;
								mct_uvc_data.status = 0x01;
								break;
							case 0x100:
								break;
							default:
								break;
						}
					}
				}
				else
				{
					switch(w_value)
					{
						case 0x200://start streaming
							//printk("Start Streaming\n");
							//memcpy(mct_uvc_data.setuptoken_8byte, ctrl, 8);
							memset(mct_uvc_data.setuptoken_8byte, '\0', 8);
							current_b_request = ctrl->bRequest;
							current_w_value = w_value;
							mct_uvc_data.status = 0x01;
							break;
						case 0x100:
							break;
						default:
							break;
					}
				}
				value = w_length;
				break;
			case UVC_GET_CUR:
				//printk("UVC_GET_CUR\n");
				//hex_dump(req->buf, CY_FX_UVC_MAX_PROBE_SETTING);
				gCurItfnum = w_index & 0xff;
				if(gCurItfnum == 0 && client_connected == 1)
				{
					g_uvc_gadget = gadget;
					memcpy(&gGetDirBuff[0], ctrl, 8);
					memcpy(g_mct_uvc_setup_insert->setupcmd, ctrl, 8);
					g_mct_uvc_setup_insert = (MCT_UVC_SETUP_QUEUE*)g_mct_uvc_setup_insert->next;
					value = w_length;
					schedule_work(&ioctl_bottem_work);
					goto done;
				}
				else
				{
					if (*((char *)(req->buf)+2) != 3)
						memcpy(req->buf, glProbeCtrl, CY_FX_UVC_MAX_PROBE_SETTING);
					else
						memcpy(req->buf, glProbeCtrlYUV, CY_FX_UVC_MAX_PROBE_SETTING);
					value = w_length;
				}
				break;
			case UVC_GET_MIN:
				//printk("UVC_GET_MIN\n");
				//hex_dump(req->buf, CY_FX_UVC_MAX_PROBE_SETTING);
				gCurItfnum = w_index & 0xff;
				if(gCurItfnum == 0 && client_connected == 1)
				{
					g_uvc_gadget = gadget;
					memcpy(&gGetDirBuff[0], ctrl, 8);
					memcpy(g_mct_uvc_setup_insert->setupcmd, ctrl, 8);
					g_mct_uvc_setup_insert = (MCT_UVC_SETUP_QUEUE*)g_mct_uvc_setup_insert->next;
					value = w_length;
					schedule_work(&ioctl_bottem_work);
					goto done;
				}
				else
				{
					//memcpy(mct_uvc_data.prob_ctrl, glProbeCtrl, CY_FX_UVC_MAX_PROBE_SETTING);
					if (*((char *)(req->buf)+2) != 3)
						memcpy(req->buf, glProbeCtrl, CY_FX_UVC_MAX_PROBE_SETTING);
					else
						memcpy(req->buf, glProbeCtrlYUV, CY_FX_UVC_MAX_PROBE_SETTING);
					*((char *)(req->buf)+3) = 1;
					value = w_length;
				}
				break;
			case UVC_GET_MAX:
				//printk("UVC_GET_MAX\n");
				//hex_dump(req->buf, CY_FX_UVC_MAX_PROBE_SETTING);
				gCurItfnum = w_index & 0xff;
				if(gCurItfnum == 0 && client_connected == 1)
				{
					g_uvc_gadget = gadget;
					memcpy(&gGetDirBuff[0], ctrl, 8);
					memcpy(g_mct_uvc_setup_insert->setupcmd, ctrl, 8);
					g_mct_uvc_setup_insert = (MCT_UVC_SETUP_QUEUE*)g_mct_uvc_setup_insert->next;
					value = w_length;
					schedule_work(&ioctl_bottem_work);
					goto done;
				}
				else
				{
					//memcpy(mct_uvc_data.prob_ctrl, glProbeCtrl, CY_FX_UVC_MAX_PROBE_SETTING);
					if (*((char *)(req->buf)+2) != 3)
						memcpy(req->buf, glProbeCtrl, CY_FX_UVC_MAX_PROBE_SETTING);
					else
						memcpy(req->buf, glProbeCtrlYUV, CY_FX_UVC_MAX_PROBE_SETTING);

					if (*((char *)(req->buf)+2) == 2)
						*((char *)(req->buf)+3) = 5;
					else
						*((char *)(req->buf)+3) = 4;
					value = w_length;
				}
				break;
			case UVC_GET_RES:
				//printk("UVC_GET_RES\n");
				gCurItfnum = w_index & 0xff;
				if(gCurItfnum == 0 && client_connected == 1)
				{
					g_uvc_gadget = gadget;
					memcpy(&gGetDirBuff[0], ctrl, 8);
					memcpy(g_mct_uvc_setup_insert->setupcmd, ctrl, 8);
					g_mct_uvc_setup_insert = (MCT_UVC_SETUP_QUEUE*)g_mct_uvc_setup_insert->next;
					value = w_length;
					schedule_work(&ioctl_bottem_work);
					goto done;
				}
				else
				{

				}
				break;
			case UVC_GET_LEN:
				//printk("UVC_GET_LEN\n");
				gCurItfnum = w_index & 0xff;
				if(gCurItfnum == 0 && client_connected == 1)
				{
					g_uvc_gadget = gadget;
					memcpy(&gGetDirBuff[0], ctrl, 8);
					memcpy(g_mct_uvc_setup_insert->setupcmd, ctrl, 8);
					g_mct_uvc_setup_insert = (MCT_UVC_SETUP_QUEUE*)g_mct_uvc_setup_insert->next;
					value = w_length;
					schedule_work(&ioctl_bottem_work);
					goto done;
				}
				else
				{

				}
				break;
			case UVC_GET_INFO:
				//printk("UVC_GET_INFO\n");
				gCurItfnum = w_index & 0xff;
				if(gCurItfnum == 0 && client_connected == 1)
				{
					g_uvc_gadget = gadget;
					memcpy(&gGetDirBuff[0], ctrl, 8);
					memcpy(g_mct_uvc_setup_insert->setupcmd, ctrl, 8);
					g_mct_uvc_setup_insert = (MCT_UVC_SETUP_QUEUE*)g_mct_uvc_setup_insert->next;
					value = w_length;
					schedule_work(&ioctl_bottem_work);
					goto done;
				}
				else
				{

				}
				break;
			case UVC_GET_DEF:
				//printk("UVC_GET_DEF\n");
				gCurItfnum = w_index & 0xff;
				if(gCurItfnum == 0 && client_connected == 1)
				{
					g_uvc_gadget = gadget;
					memcpy(&gGetDirBuff[0], ctrl, 8);
					memcpy(g_mct_uvc_setup_insert->setupcmd, ctrl, 8);
					g_mct_uvc_setup_insert = (MCT_UVC_SETUP_QUEUE*)g_mct_uvc_setup_insert->next;
					value = w_length;
					schedule_work(&ioctl_bottem_work);
					goto done;
				}
				else
				{
					if (*((char *)(req->buf)+2) != 3)
						memcpy(req->buf, glProbeCtrl, CY_FX_UVC_MAX_PROBE_SETTING);
					else
						memcpy(req->buf, glProbeCtrlYUV, CY_FX_UVC_MAX_PROBE_SETTING);
					value = w_length;
				}
				break;
			default:
				VDBG(cdev,
					"non-core control req%02x.%02x v%04x i%04x l%d\n",
					ctrl->bRequestType, ctrl->bRequest,
					w_value, w_index, w_length);

				/* functions always handle their interfaces and endpoints...
				 * punt other recipients (other, WUSB, ...) to the current
				 * configuration code.
				 *
				 * REVISIT it could make sense to let the composite device
				 * take such requests too, if that's ever needed:  to work
				 * in config 0, etc.
				 */
				switch (ctrl->bRequestType & USB_RECIP_MASK) {
				case USB_RECIP_INTERFACE:
					f = cdev->config->interface[0];
					break;

				case USB_RECIP_ENDPOINT:
					endp = ((w_index & 0x80) >> 3) | (w_index & 0x0f);
					list_for_each_entry(f, &cdev->config->functions, list) {
						if (test_bit(endp, f->endpoints))
							break;
					}
					if (&f->list == &cdev->config->functions)
						f = NULL;
					break;
				}

				if (f && f->setup)
					value = f->setup(f, ctrl);
				else {
					struct usb_configuration	*c;

					c = cdev->config;
					if (c && c->setup)
						value = c->setup(c, ctrl);
				}

				goto done;
		}
	}
	else if(((ctrl->bRequestType) & USB_TYPE_STANDARD) == USB_TYPE_STANDARD)
	{
		//printk("USB_TYPE_STANDARD: w_value=%04X, w_index=%04X\n", w_value, w_index);
		switch (ctrl->bRequest) {
		/* we handle all standard USB descriptors */
		case USB_REQ_GET_DESCRIPTOR:
			if ((ctrl->bRequestType & USB_DIR_IN) != USB_DIR_IN)
				goto unknown;
			switch (w_value >> 8) {
			case USB_DT_DEVICE:
				cdev->desc.bNumConfigurations =
					count_configs(cdev, USB_DT_DEVICE);
				value = min(w_length, (u16) sizeof cdev->desc);
				cdev->desc.idVendor = myidVendor;
				cdev->desc.idProduct = myidProduct;
				memcpy(req->buf, &cdev->desc, value);
				break;
			case USB_DT_DEVICE_QUALIFIER:
				if (!gadget_is_dualspeed(gadget))
					break;
				device_qual(cdev);
				value = min_t(int, w_length,
					sizeof(struct usb_qualifier_descriptor));
				break;
			case USB_DT_OTHER_SPEED_CONFIG:
				if (!gadget_is_dualspeed(gadget))
					break;
				/* FALLTHROUGH */
			case USB_DT_CONFIG:
				/*
				 * It's a workaround for the Logitech device which has two configuration 
				 * descriptors, once the user attaches this device we just report one 
				 * configuration.
				 */
				value = config_desc(cdev, w_value&0xFF00);
				if (value >= 0)
					value = min(w_length, (u16) value);
				break;
			case USB_DT_STRING:
				value = get_string(cdev, req->buf, w_index, w_value & 0xff);
				if (value >= 0) {
					value = min(w_length, (u16) value);
					if ((w_value&0xff) == 1) {
						//  manufactorer
						if (myManufacturer_len > 0) {
							memcpy(req->buf, myManufacturer, myManufacturer_len);
							value = min(w_length, (u16)myManufacturer_len);
						}
					} else if ((w_value&0xff) == 2) {
						//  product
						if (myProduct_len > 0) {
							memcpy(req->buf, myProduct, myProduct_len);
							value = min(w_length, (u16)myProduct_len);
						}
					}
				} else {
					if (myProduct_len > 0) {
						memcpy(req->buf, myProduct, myProduct_len);
						value = min(w_length, (u16)myProduct_len);
					} else {
						memcpy(req->buf, "UVC Camera", strlen("UVC Camera"));
						value = strlen("UVC Camera");
					}
				}
				break;
			case USB_DT_BOS:
				memcpy(req->buf, fsg_bos_desc, sizeof(fsg_bos_desc));
				value = min(w_length, (u16) sizeof(fsg_bos_desc));
			case (USB_TYPE_CLASS | 0x02):
				DBG(cdev, "USB_REQ_GET_DESCRIPTOR: REPORT\n");
				switch(w_index)
				{
					case 1:
						value = 0x379;
						memcpy(req->buf, ReportDescHSMouse, value);
						break;
					case 2:
						value = 0x3e;
						memcpy(req->buf, ReportDescHSKeyboard, value);
						break;
					case 3:
						value = 0x65;
						memcpy(req->buf, ReportDescHSNone, value);
						break;
				}
				break;
			}
			break;

		/* any number of configs can work */
		case USB_REQ_SET_CONFIGURATION:
			if (ctrl->bRequestType != 0)
				goto unknown;
			if (gadget_is_otg(gadget)) {
				if (gadget->a_hnp_support)
					DBG(cdev, "HNP available\n");
				else if (gadget->a_alt_hnp_support)
					DBG(cdev, "HNP on another port\n");
				else
					VDBG(cdev, "HNP inactive\n");
			}
			spin_lock(&cdev->lock);
			value = set_config(cdev, ctrl, w_value);
			schedule_work(&setcfg_bottem_work);
			spin_unlock(&cdev->lock);
			break;
		case USB_REQ_GET_CONFIGURATION:
			if (ctrl->bRequestType != USB_DIR_IN)
				goto unknown;
			if (cdev->config)
				*(u8 *)req->buf = cdev->config->bConfigurationValue;
			else
				*(u8 *)req->buf = 0;
			value = min(w_length, (u16) 1);
			break;
		case USB_REQ_GET_STATUS:
			value = 0x2;
			memcpy(req->buf, "\x00\x00", value);
			break;
		/* function drivers must handle get/set altsetting; if there's
		 * no get() method, we know only altsetting zero works.
		 */
		case USB_REQ_SET_INTERFACE:
			/*if (ctrl->bRequestType != USB_RECIP_INTERFACE)
				goto unknown;
			if (!cdev->config || w_index >= MAX_CONFIG_INTERFACES)
				break;
			f = cdev->config->interface[intf];
			if (!f)
				break;
			if (w_value && !f->set_alt)
				break;
			value = f->set_alt(f, w_index, w_value);*/
			value = 0;
			break;
		case USB_REQ_GET_INTERFACE:
			if (ctrl->bRequestType != (USB_DIR_IN|USB_RECIP_INTERFACE))
				goto unknown;
			if (!cdev->config || w_index >= MAX_CONFIG_INTERFACES)
				break;
			f = cdev->config->interface[intf];
			if (!f)
				break;
			/* lots of interfaces only need altsetting zero... */
			value = f->get_alt ? f->get_alt(f, w_index) : 0;
			if (value < 0)
				break;
			*((u8 *)req->buf) = value;
			value = min(w_length, (u16) 1);
			break;
		case USB_REQ_CLEAR_FEATURE:
			if(w_value == 0)//halt_endpoint
			{
				if(w_index == 0x82)//stop UVC bulk streaming
				{
					//printk("Stop Streaming\n");
					memset(mct_uvc_data.setuptoken_8byte, '\0', 8);
					//current_b_request = ctrl->bRequest;
					//current_w_value = w_value;
					//mct_uvc_data.status = 0x02;
				}
			}
			value = 0;
			break;
		default:
unknown:
			VDBG(cdev,
				"non-core control req%02x.%02x v%04x i%04x l%d\n",
				ctrl->bRequestType, ctrl->bRequest,
				w_value, w_index, w_length);

			/* functions always handle their interfaces and endpoints...
			 * punt other recipients (other, WUSB, ...) to the current
			 * configuration code.
			 *
			 * REVISIT it could make sense to let the composite device
			 * take such requests too, if that's ever needed:  to work
			 * in config 0, etc.
			 */
			switch (ctrl->bRequestType & USB_RECIP_MASK) {
			case USB_RECIP_INTERFACE:
				f = cdev->config->interface[intf];
				break;

			case USB_RECIP_ENDPOINT:
				endp = ((w_index & 0x80) >> 3) | (w_index & 0x0f);
				list_for_each_entry(f, &cdev->config->functions, list) {
					if (test_bit(endp, f->endpoints))
						break;
				}
				if (&f->list == &cdev->config->functions)
					f = NULL;
				break;
			}

			if (f && f->setup)
				value = f->setup(f, ctrl);
			else {
				struct usb_configuration	*c;

				c = cdev->config;
				if (c && c->setup)
					value = c->setup(c, ctrl);
			}

			goto done;
		}
	}

	/* respond with data transfer before status phase? */
	if (value >= 0) {
		req->length = value;
		req->zero = value < w_length;
		if(value%64 == 0)
			req->zero = 1;
		value = usb_ep_queue(gadget->ep0, req, GFP_ATOMIC);
		if (value < 0) {
			DBG(cdev, "ep_queue --> %d\n", value);
			req->status = 0;
			composite_setup_complete(gadget->ep0, req);
		}
	}

done:
	/* device either stalls (value < 0) or reports success */
	return value;
}

static void composite_disconnect(struct usb_gadget *gadget)
{
	struct usb_composite_dev	*cdev = get_gadget_data(gadget);
	unsigned long			flags;

	/* REVISIT:  should we have config and device level
	 * disconnect callbacks?
	 */
	spin_lock_irqsave(&cdev->lock, flags);
	if (cdev->config)
		reset_config(cdev);
	if (composite->disconnect)
		composite->disconnect(cdev);
	spin_unlock_irqrestore(&cdev->lock, flags);
}

/*-------------------------------------------------------------------------*/

static ssize_t composite_show_suspended(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct usb_gadget *gadget = dev_to_usb_gadget(dev);
	struct usb_composite_dev *cdev = get_gadget_data(gadget);

	return sprintf(buf, "%d\n", cdev->suspended);
}

static DEVICE_ATTR(suspended, 0444, composite_show_suspended, NULL);

static void
composite_unbind(struct usb_gadget *gadget)
{
	struct usb_composite_dev	*cdev = get_gadget_data(gadget);

	/* composite_disconnect() must already have been called
	 * by the underlying peripheral controller driver!
	 * so there's no i/o concurrency that could affect the
	 * state protected by cdev->lock.
	 */
	WARN_ON(cdev->config);

	while (!list_empty(&cdev->configs)) {
		struct usb_configuration	*c;

		c = list_first_entry(&cdev->configs,
				struct usb_configuration, list);
		while (!list_empty(&c->functions)) {
			struct usb_function		*f;

			f = list_first_entry(&c->functions,
					struct usb_function, list);
			list_del(&f->list);
			if (f->unbind) {
				DBG(cdev, "unbind function '%s'/%p\n",
						f->name, f);
				f->unbind(c, f);
				/* may free memory for "f" */
			}
		}
		list_del(&c->list);
		if (c->unbind) {
			DBG(cdev, "unbind config '%s'/%p\n", c->label, c);
			c->unbind(c);
			/* may free memory for "c" */
		}
	}
	if (composite->unbind)
		composite->unbind(cdev);

	if (cdev->req) {
		kfree(cdev->req->buf);
		usb_ep_free_request(gadget->ep0, cdev->req);
	}
	kfree(cdev);
	set_gadget_data(gadget, NULL);
	device_remove_file(&gadget->dev, &dev_attr_suspended);
	composite = NULL;
}

static void
string_override_one(struct usb_gadget_strings *tab, u8 id, const char *s)
{
	struct usb_string		*str = tab->strings;

	for (str = tab->strings; str->s; str++) {
		if (str->id == id) {
			str->s = s;
			return;
		}
	}
}

static void
string_override(struct usb_gadget_strings **tab, u8 id, const char *s)
{
	while (*tab) {
		string_override_one(*tab, id, s);
		tab++;
	}
}

static int composite_bind(struct usb_gadget *gadget)
{
	struct usb_composite_dev	*cdev;
	int				status = -ENOMEM;

	cdev = kzalloc(sizeof *cdev, GFP_KERNEL);
	if (!cdev)
		return status;

	spin_lock_init(&cdev->lock);
	spin_lock_init(&cursor_lock);
	cdev->gadget = gadget;
	set_gadget_data(gadget, cdev);
	INIT_LIST_HEAD(&cdev->configs);

	/* preallocate control response and buffer */
	cdev->req = usb_ep_alloc_request(gadget->ep0, GFP_KERNEL);
	if (!cdev->req)
		goto fail;
	cdev->req->buf = kmalloc(USB_BUFSIZ, GFP_KERNEL);
	if (!cdev->req->buf)
		goto fail;
	cdev->req->complete = composite_setup_complete;
	gadget->ep0->driver_data = cdev;

	cdev->bufsiz = USB_BUFSIZ;
	cdev->driver = composite;

	usb_gadget_set_selfpowered(gadget);

	/* interface and string IDs start at zero via kzalloc.
	 * we force endpoints to start unassigned; few controller
	 * drivers will zero ep->driver_data.
	 */
	usb_ep_autoconfig_reset(cdev->gadget);

	/* standardized runtime overrides for device ID data */
	if (idVendor)
		cdev->desc.idVendor = cpu_to_le16(idVendor);
	if (idProduct)
		cdev->desc.idProduct = cpu_to_le16(idProduct);
	if (bcdDevice)
		cdev->desc.bcdDevice = cpu_to_le16(bcdDevice);

	/* composite gadget needs to assign strings for whole device (like
	 * serial number), register function drivers, potentially update
	 * power state and consumption, etc
	 */
	status = composite->bind(cdev);
	if (status < 0)
		goto fail;

	cdev->desc = *composite->dev;
	cdev->desc.bMaxPacketSize0 = gadget->ep0->maxpacket;

	/* strings can't be assigned before bind() allocates the
	 * releavnt identifiers
	 */
	myiManufacturer = cdev->desc.iManufacturer;
	if (cdev->desc.iManufacturer && iManufacturer)
		string_override(composite->strings,
			cdev->desc.iManufacturer, iManufacturer);
	myiProduct = cdev->desc.iProduct;
	if (cdev->desc.iProduct && iProduct)
		string_override(composite->strings,
			cdev->desc.iProduct, iProduct);
	if (cdev->desc.iSerialNumber && iSerialNumber)
		string_override(composite->strings,
			cdev->desc.iSerialNumber, iSerialNumber);

	status = device_create_file(&gadget->dev, &dev_attr_suspended);
	if (status)
		goto fail;

	INFO(cdev, "%s ready\n", composite->name);
	return 0;

fail:
	composite_unbind(gadget);
	return status;
}

/*-------------------------------------------------------------------------*/

static void
composite_suspend(struct usb_gadget *gadget)
{
	struct usb_composite_dev	*cdev = get_gadget_data(gadget);
	struct usb_function		*f;

	/* REVISIT:  should we have config level
	 * suspend/resume callbacks?
	 */
	DBG(cdev, "suspend\n");
	if (cdev->config) {
		list_for_each_entry(f, &cdev->config->functions, list) {
			if (f->suspend)
				f->suspend(f);
		}
	}
	if (composite->suspend)
		composite->suspend(cdev);

	cdev->suspended = 1;
}

static void
composite_resume(struct usb_gadget *gadget)
{
	struct usb_composite_dev	*cdev = get_gadget_data(gadget);
	struct usb_function		*f;

	/* REVISIT:  should we have config level
	 * suspend/resume callbacks?
	 */
	DBG(cdev, "resume\n");
	if (composite->resume)
		composite->resume(cdev);
	if (cdev->config) {
		list_for_each_entry(f, &cdev->config->functions, list) {
			if (f->resume)
				f->resume(f);
		}
	}

	cdev->suspended = 0;
}

/*-------------------------------------------------------------------------*/

static struct usb_gadget_driver composite_driver = {
	.speed		= USB_SPEED_HIGH,

	.bind		= composite_bind,
	.unbind		= composite_unbind,

	.setup		= composite_setup,
	.disconnect	= composite_disconnect,

	.suspend	= composite_suspend,
	.resume		= composite_resume,

	.driver	= {
		.owner		= THIS_MODULE,
	},
};

/**
 * usb_composite_register() - register a composite driver
 * @driver: the driver to register
 * Context: single threaded during gadget setup
 *
 * This function is used to register drivers using the composite driver
 * framework.  The return value is zero, or a negative errno value.
 * Those values normally come from the driver's @bind method, which does
 * all the work of setting up the driver to match the hardware.
 *
 * On successful return, the gadget is ready to respond to requests from
 * the host, unless one of its components invokes usb_gadget_disconnect()
 * while it was binding.  That would usually be done in order to wait for
 * some userspace participation.
 */
void my_ctrlep_write(const char __user *buffer, size_t count)
{
	struct usb_composite_dev	*cdev = get_gadget_data(g_uvc_gadget);
	struct usb_request		*req = cdev->req;
	memcpy(req->buf, buffer, count);
	req->length = count;
	if(count%64 == 0)
	{
		req->zero = 1;
	}
	else
	{
		req->zero = 0;
	}
	usb_ep_queue(g_uvc_gadget->ep0, req, GFP_ATOMIC);
}


int WebUVCMajor;

extern u32 reg_read(u32 addr);
extern void reg_write(u32 addr, u32 value);
int device_open(struct inode *inode, struct file *filp)
{
	//printk("WebUVCO\n");
	return 0;
}

int device_release(struct inode *inode, struct file *filp)
{
	return 0;
}

extern int Enable_UVC_Bulk(struct fsg_common *common);
extern int Disable_UVC_Bulk(struct fsg_common *common);

int device_read(struct file *filp, char __user *buf, size_t count, loff_t *offset)
{
	//unsigned int temp;
	printk("WebUVCR\n");
	//reg_write(0xB0000034, 0x02000000);
	//temp = reg_read(0xB0000030);
	//reg_write(0xB0000030, temp & 0xFDFFFFFF);
	//reg_write(0xB0000030, temp | 0x02000000);
	//reg_write(0xB0000034, 0x00000001);
	Enable_UVC_Bulk(g_userspaceUVC_common);
	//Disable_UVC_Bulk(g_userspaceUVC_common);
	return 0;
}

extern int send_UVB_bulk_usr(struct fsg_common *common, unsigned int length, unsigned char *UVCbuf);

int device_write(struct file *filp, const char __user *buf, size_t count, loff_t *offset)
{
	unsigned char UVC_Buff[512];
	if (mct_uvc_data.status == 0x1) {
		g_incomplete = 0;
		copy_from_user(UVC_Buff, buf, count);
		send_UVB_bulk_usr(g_userspaceUVC_common, count, UVC_Buff);
		wait_event_interruptible(gUVCwq, g_incomplete);
	}

	return 0;
}

typedef struct _uvc_capabilities_data{
	unsigned int mjpeg;
	unsigned int h264;
	unsigned int yuv;
	unsigned int nv12;
	unsigned int i420;
	unsigned int m420;
	unsigned int camera;
	unsigned int process;
}UVC_CAPABILITIES_DATA;
extern void uvc_change_desc(void);
extern void uvc_copy_desc(void);

long device_ioctl(struct file *file, unsigned int cmd, unsigned long data)
{
	UVC_CAPABILITIES_DATA capabilities;
	unsigned char UVC_INTR_Buff[16];
	unsigned char UVC_CTRL_Buff[MCT_UVC_NOTIFY_SIZE];
	char string_buf[256];

	//printk("device_ioctl event\n");

	switch (cmd) {
	case UVC_GET_STAT:
		//printk("UVC_GET_STAT:    ");
		wait_event_interruptible(gUVCioctl, g_ioctlincomplete);
		g_ioctlincomplete = 0;
		if((gCurItfnum == 0 && (((ep0_setup_dir)&USB_TYPE_CLASS) == USB_TYPE_CLASS)) && client_connected == 1)
		{
			if((g_mct_uvc_setup_remove->setupcmd[0]) & 0x80)//get
			{
				copy_to_user((char*)data, &gGetDirBuff[0], MCT_UVC_NOTIFY_SIZE);
			}
			else
			{
				copy_to_user((char*)data, &g_mct_uvc_setup_remove->setupcmd[0], MCT_UVC_NOTIFY_SIZE);
			}
			g_mct_uvc_setup_remove = (MCT_UVC_SETUP_QUEUE*)g_mct_uvc_setup_remove->next;
			if(g_mct_uvc_setup_remove != g_mct_uvc_setup_insert)
			{
				g_ioctlincomplete = 1;
				wake_up(&gUVCioctl);//for user
			}
		}
		else
		{
			//printk("mct_uvc_data.status = %x\n", mct_uvc_data.status);
			copy_to_user((char*)data, &mct_uvc_data, MCT_UVC_DATA_SIZE);
		}
		break;
	case UVC_CLIENT_LOST:
		client_connected = 0;
		break;
	case CAMERA_STS_CHANGE:
		copy_from_user(&capabilities, (char*)data, sizeof(capabilities));
		g_uvc_camera_terminal_controlbit = capabilities.camera;
		g_uvc_processing_controlbit = capabilities.process;
		g_uvc_mjpeg_res_bitflag = capabilities.mjpeg;
		g_uvc_h264_res_bitflag = capabilities.h264;
		g_uvc_yuv_res_bitflag = capabilities.yuv;
		g_uvc_nv12_res_bitflag = capabilities.nv12;
		g_uvc_i420_res_bitflag = capabilities.i420;
		g_uvc_m420_res_bitflag = capabilities.m420;
		if(currentbcdUVC == 0x0150)  //  commented out this line for 1.5 only if you want.
		{
			uvc_change_desc_compatable();
		}
		else
		{
			uvc_change_desc();
		}
		uvc_copy_desc();
		client_connected = 1;
		break;
	case APPLY_DESC_CHANGE:
		if(currentbcdUVC == 0x0150)
		{
			uvc_change_desc_compatable();
		}
		else
		{
			uvc_change_desc();
		}
		uvc_copy_desc();
		break;
	case QUERY_SETUP_DATA:
		break;
	case CTRLEP_WRITE:
		copy_from_user(UVC_CTRL_Buff, (char*)data, 256);
		my_ctrlep_write(&UVC_CTRL_Buff[1], UVC_CTRL_Buff[0]);
		break;
	case INTREP_WRITE:
		copy_from_user(UVC_INTR_Buff, (char*)data, 16);
		my_f_hidg_write(UVC_INTR_Buff, 16);
		break;
	case UVC_SET_MANUFACTURER:
		read_property(1);
		break;
	case UVC_SET_PRODUCT_NAME:
		read_property(2);
		break;
	case UVC_SET_VC_DESC:
		read_property(3);
		break;
	case UVC_GET_UVC_VER:
		uvc_get_bcdUVC();
		break;
	case UVC_SET_PIDVID:
		myidVendor = (data >> 16) & 0xffff;
		myidProduct = (data & 0xffff);
		break;
	case UVC_EP0_SET_HALT:
		usb_ep_set_halt(g_uvc_gadget->ep0);
		break;
	case UVC_EP0_CLEAR_HALT:
		usb_ep_clear_halt(g_uvc_gadget->ep0);
		break;
	default :
		printk("device_ioctl: command format error\n");
	}
	return 0;
}

struct file_operations webUVCfops = {
  owner : THIS_MODULE,
  read : device_read,
  write : device_write,
  open : device_open,
  release : device_release,
  unlocked_ioctl : device_ioctl,
  //ioctl : device_ioctl,
};

#define WEBUVC_DEVICE_NAME "Web_UVC0"

int usb_composite_register(struct usb_composite_driver *driver)
{
	//struct file * conf_fp = NULL;
	int i;
	if (!driver || !driver->dev || !driver->bind || composite)
		return -EINVAL;

	if (!driver->name)
		driver->name = "composite";
	composite_driver.function =  (char *) driver->name;
	composite_driver.driver.name = driver->name;
	composite = driver;

	WebUVCMajor = register_chrdev(255, WEBUVC_DEVICE_NAME, &webUVCfops);

	//conf_fp = file_open(UVC_CONF_FILE, O_RDONLY, 0);
	//if (conf_fp) {
	//     file_read(conf_fp, 0, glProbeCtrl, CY_FX_UVC_MAX_PROBE_SETTING);
	//     file_close(conf_fp);
	//}

	init_waitqueue_head(&gUVCwq);
	init_waitqueue_head(&gUVCioctl);
	INIT_WORK(&ioctl_bottem_work, ioctl_bottem_notify);
	INIT_WORK(&setcfg_bottem_work, setcfg_bottem_notify);

	for(i=0;i<MAX_SETUP_QUEUE;i++)
	{
		if(i == (MAX_SETUP_QUEUE-1))
		{
			g_mct_uvc_setup_queue[i].next = &g_mct_uvc_setup_queue[0];
		}
		else
		{
			g_mct_uvc_setup_queue[i].next = &g_mct_uvc_setup_queue[i+1];
		}
	}
	g_mct_uvc_setup_insert = &g_mct_uvc_setup_queue[0];
	g_mct_uvc_setup_remove = &g_mct_uvc_setup_queue[0];

	//printk("reg Web_UVC0\n");
	if (WebUVCMajor < 0) {
		//printk("Register char device Web_UVC0 fail\n");
		return WebUVCMajor;
	}

	return usb_gadget_register_driver(&composite_driver);
}

/**
 * usb_composite_unregister() - unregister a composite driver
 * @driver: the driver to unregister
 *
 * This function is used to unregister drivers using the composite
 * driver framework.
 */
void usb_composite_unregister(struct usb_composite_driver *driver)
{
	if (composite != driver)
		return;
	
	unregister_chrdev(255, WEBUVC_DEVICE_NAME);
	usb_gadget_unregister_driver(&composite_driver);
}
