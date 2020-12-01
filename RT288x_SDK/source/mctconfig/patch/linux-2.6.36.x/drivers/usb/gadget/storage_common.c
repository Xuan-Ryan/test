/*
 * storage_common.c -- Common definitions for mass storage functionality
 *
 * Copyright (C) 2003-2008 Alan Stern
 * Copyeight (C) 2009 Samsung Electronics
 * Author: Michal Nazarewicz (m.nazarewicz@samsung.com)
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


/*
 * This file requires the following identifiers used in USB strings to
 * be defined (each of type pointer to char):
 *  - fsg_string_manufacturer -- name of the manufacturer
 *  - fsg_string_product      -- name of the product
 *  - fsg_string_serial       -- product's serial
 *  - fsg_string_config       -- name of the configuration
 *  - fsg_string_interface    -- name of the interface
 * The first four are only needed when FSG_DESCRIPTORS_DEVICE_STRINGS
 * macro is defined prior to including this file.
 */

/*
 * When FSG_NO_INTR_EP is defined fsg_fs_intr_in_desc and
 * fsg_hs_intr_in_desc objects as well as
 * FSG_FS_FUNCTION_PRE_EP_ENTRIES and FSG_HS_FUNCTION_PRE_EP_ENTRIES
 * macros are not defined.
 *
 * When FSG_NO_DEVICE_STRINGS is defined FSG_STRING_MANUFACTURER,
 * FSG_STRING_PRODUCT, FSG_STRING_SERIAL and FSG_STRING_CONFIG are not
 * defined (as well as corresponding entries in string tables are
 * missing) and FSG_STRING_INTERFACE has value of zero.
 *
 * When FSG_NO_OTG is defined fsg_otg_desc won't be defined.
 */

/*
 * When FSG_BUFFHD_STATIC_BUFFER is defined when this file is included
 * the fsg_buffhd structure's buf field will be an array of FSG_BUFLEN
 * characters rather then a pointer to void.
 */


#include <asm/unaligned.h>
#include <linux/usb/video.h>


/*
 * Thanks to NetChip Technologies for donating this product ID.
 *
 * DO NOT REUSE THESE IDs with any other driver!!  Ever!!
 * Instead:  allocate your own, using normal USB-IF procedures.
 */
#define FSG_VENDOR_ID	0x0711	/* NetChip */
#define FSG_PRODUCT_ID	0xC600	/* Linux-USB File-backed Storage Gadget */


/*-------------------------------------------------------------------------*/


#ifndef DEBUG
#undef VERBOSE_DEBUG
#undef DUMP_MSGS
#endif /* !DEBUG */

#ifdef VERBOSE_DEBUG
#define VLDBG	LDBG
#else
#define VLDBG(lun, fmt, args...) do { } while (0)
#endif /* VERBOSE_DEBUG */

#define LDBG(lun, fmt, args...)   dev_dbg (&(lun)->dev, fmt, ## args)
#define LERROR(lun, fmt, args...) dev_err (&(lun)->dev, fmt, ## args)
#define LWARN(lun, fmt, args...)  dev_warn(&(lun)->dev, fmt, ## args)
#define LINFO(lun, fmt, args...)  dev_info(&(lun)->dev, fmt, ## args)

/*
 * Keep those macros in sync with those in
 * include/linux/usb/composite.h or else GCC will complain.  If they
 * are identical (the same names of arguments, white spaces in the
 * same places) GCC will allow redefinition otherwise (even if some
 * white space is removed or added) warning will be issued.
 *
 * Those macros are needed here because File Storage Gadget does not
 * include the composite.h header.  For composite gadgets those macros
 * are redundant since composite.h is included any way.
 *
 * One could check whether those macros are already defined (which
 * would indicate composite.h had been included) or not (which would
 * indicate we were in FSG) but this is not done because a warning is
 * desired if definitions here differ from the ones in composite.h.
 *
 * We want the definitions to match and be the same in File Storage
 * Gadget as well as Mass Storage Function (and so composite gadgets
 * using MSF).  If someone changes them in composite.h it will produce
 * a warning in this file when building MSF.
 */
#define DBG(d, fmt, args...)     dev_dbg(&(d)->gadget->dev , fmt , ## args)
#define VDBG(d, fmt, args...)    dev_vdbg(&(d)->gadget->dev , fmt , ## args)
#define ERROR(d, fmt, args...)   dev_err(&(d)->gadget->dev , fmt , ## args)
#define WARNING(d, fmt, args...) dev_warn(&(d)->gadget->dev , fmt , ## args)
#define INFO(d, fmt, args...)    dev_info(&(d)->gadget->dev , fmt , ## args)



#ifdef DUMP_MSGS

#  define dump_msg(fsg, /* const char * */ label,			\
		   /* const u8 * */ buf, /* unsigned */ length) do {	\
	if (length < 512) {						\
		DBG(fsg, "%s, length %u:\n", label, length);		\
		print_hex_dump(KERN_DEBUG, "", DUMP_PREFIX_OFFSET,	\
			       16, 1, buf, length, 0);			\
	}								\
} while (0)

#  define dump_cdb(fsg) do { } while (0)

#else

#  define dump_msg(fsg, /* const char * */ label, \
		   /* const u8 * */ buf, /* unsigned */ length) do { } while (0)

#  ifdef VERBOSE_DEBUG

#    define dump_cdb(fsg)						\
	print_hex_dump(KERN_DEBUG, "SCSI CDB: ", DUMP_PREFIX_NONE,	\
		       16, 1, (fsg)->cmnd, (fsg)->cmnd_size, 0)		\

#  else

#    define dump_cdb(fsg) do { } while (0)

#  endif /* VERBOSE_DEBUG */

#endif /* DUMP_MSGS */





/*-------------------------------------------------------------------------*/

/* SCSI device types */
#define TYPE_DISK	0x00
#define TYPE_CDROM	0x05

/* USB protocol value = the transport method */
#define USB_PR_CBI	0x00		/* Control/Bulk/Interrupt */
#define USB_PR_CB	0x01		/* Control/Bulk w/o interrupt */
#define USB_PR_BULK	0x50		/* Bulk-only */

/* USB subclass value = the protocol encapsulation */
#define USB_SC_RBC	0x01		/* Reduced Block Commands (flash) */
#define USB_SC_8020	0x02		/* SFF-8020i, MMC-2, ATAPI (CD-ROM) */
#define USB_SC_QIC	0x03		/* QIC-157 (tape) */
#define USB_SC_UFI	0x04		/* UFI (floppy) */
#define USB_SC_8070	0x05		/* SFF-8070i (removable) */
#define USB_SC_SCSI	0x06		/* Transparent SCSI */

/* Bulk-only data structures */

/* Command Block Wrapper */
struct fsg_bulk_cb_wrap {
	__le32	Signature;		/* Contains 'USBC' */
	u32	Tag;			/* Unique per command id */
	__le32	DataTransferLength;	/* Size of the data */
	u32	Length;			/* Direction in bit 7 */
	u8	CDB[16];		/* Command Data Block */
};

#define USB_BULK_CB_WRAP_LEN	32
#define USB_BULK_CB_SIG		0x00000000	/* Spells out USBC */
#define USB_BULK_IN_FLAG	0x80

/* Command Status Wrapper */
struct bulk_cs_wrap {
	__le32	Signature;		/* Should = 'USBS' */
	u32	Tag;			/* Same as original command */
	__le32	Residue;		/* Amount not transferred */
	u8	Status;			/* See below */
};

#define USB_BULK_CS_WRAP_LEN	13
#define USB_BULK_CS_SIG		0x53425355	/* Spells out 'USBS' */
#define USB_STATUS_PASS		0
#define USB_STATUS_FAIL		1
#define USB_STATUS_PHASE_ERROR	2

/* Bulk-only class specific requests */
#define USB_BULK_RESET_REQUEST		0xff
#define USB_BULK_GET_MAX_LUN_REQUEST	0xfe


/* CBI Interrupt data structure */
struct interrupt_data {
	u8	bType;
	u8	bValue;
};

#define CBI_INTERRUPT_DATA_LEN		2

/* CBI Accept Device-Specific Command request */
#define USB_CBI_ADSC_REQUEST		0x00


/* Length of a SCSI Command Data Block */
#define MAX_COMMAND_SIZE	16

/* SCSI commands that we recognize */
#define SC_FORMAT_UNIT			0x04
#define SC_INQUIRY			0x12
#define SC_MODE_SELECT_6		0x15
#define SC_MODE_SELECT_10		0x55
#define SC_MODE_SENSE_6			0x1a
#define SC_MODE_SENSE_10		0x5a
#define SC_PREVENT_ALLOW_MEDIUM_REMOVAL	0x1e
#define SC_READ_6			0x08
#define SC_READ_10			0x28
#define SC_READ_12			0xa8
#define SC_READ_CAPACITY		0x25
#define SC_READ_FORMAT_CAPACITIES	0x23
#define SC_READ_HEADER			0x44
#define SC_READ_TOC			0x43
#define SC_RELEASE			0x17
#define SC_REQUEST_SENSE		0x03
#define SC_RESERVE			0x16
#define SC_SEND_DIAGNOSTIC		0x1d
#define SC_START_STOP_UNIT		0x1b
#define SC_SYNCHRONIZE_CACHE		0x35
#define SC_TEST_UNIT_READY		0x00
#define SC_VERIFY			0x2f
#define SC_WRITE_6			0x0a
#define SC_WRITE_10			0x2a
#define SC_WRITE_12			0xaa

/* SCSI Sense Key/Additional Sense Code/ASC Qualifier values */
#define SS_NO_SENSE				0
#define SS_COMMUNICATION_FAILURE		0x040800
#define SS_INVALID_COMMAND			0x052000
#define SS_INVALID_FIELD_IN_CDB			0x052400
#define SS_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE	0x052100
#define SS_LOGICAL_UNIT_NOT_SUPPORTED		0x052500
#define SS_MEDIUM_NOT_PRESENT			0x023a00
#define SS_MEDIUM_REMOVAL_PREVENTED		0x055302
#define SS_NOT_READY_TO_READY_TRANSITION	0x062800
#define SS_RESET_OCCURRED			0x062900
#define SS_SAVING_PARAMETERS_NOT_SUPPORTED	0x053900
#define SS_UNRECOVERED_READ_ERROR		0x031100
#define SS_WRITE_ERROR				0x030c02
#define SS_WRITE_PROTECTED			0x072700

#define SK(x)		((u8) ((x) >> 16))	/* Sense Key byte, etc. */
#define ASC(x)		((u8) ((x) >> 8))
#define ASCQ(x)		((u8) (x))


/*-------------------------------------------------------------------------*/
#define INPUT_HEADER_IDX 11
#define INPUT_FORMAT_IDX 12

struct fsg_lun {
	struct file	*filp;
	loff_t		file_length;
	loff_t		num_sectors;

	unsigned int	initially_ro:1;
	unsigned int	ro:1;
	unsigned int	removable:1;
	unsigned int	cdrom:1;
	unsigned int	prevent_medium_removal:1;
	unsigned int	registered:1;
	unsigned int	info_valid:1;
	unsigned int	nofua:1;

	u32		sense_data;
	u32		sense_data_info;
	u32		unit_attention_data;

	struct device	dev;
};

#define fsg_lun_is_open(curlun)	((curlun)->filp != NULL)

static struct fsg_lun *fsg_lun_from_dev(struct device *dev)
{
	return container_of(dev, struct fsg_lun, dev);
}


/* Big enough to hold our biggest descriptor */
#define EP0_BUFSIZE	256
#define DELAYED_STATUS	(EP0_BUFSIZE + 999)	/* An impossibly large value */

/* Number of buffers we will use.  2 is enough for double-buffering */
#define FSG_NUM_BUFFERS	2

/* Default size of buffer length. */
#define FSG_BUFLEN	((u32)512)
#define HID_BUFLEN  ((u32)64)

/* Maximal number of LUNs supported in mass storage function */
#define FSG_MAX_LUNS	8

enum fsg_buffer_state {
	BUF_STATE_EMPTY = 0,
	BUF_STATE_FULL,
	BUF_STATE_BUSY
};

struct fsg_buffhd {
#ifdef FSG_BUFFHD_STATIC_BUFFER
	char				buf[FSG_BUFLEN];
#else
	void				*buf;
#endif
	enum fsg_buffer_state		state;
	struct fsg_buffhd		*next;

	/*
	 * The NetChip 2280 is faster, and handles some protocol faults
	 * better, if we don't submit any short bulk-out read requests.
	 * So we will record the intended request length here.
	 */
	unsigned int			bulk_out_intended_length;

	struct usb_request		*inreq;
	int				inreq_busy;
	struct usb_request		*outreq;
	int				outreq_busy;
};

enum fsg_state {
	/* This one isn't used anywhere */
	FSG_STATE_COMMAND_PHASE = -10,
	FSG_STATE_DATA_PHASE,
	FSG_STATE_STATUS_PHASE,

	FSG_STATE_IDLE = 0,
	FSG_STATE_ABORT_BULK_OUT,
	FSG_STATE_RESET,
	FSG_STATE_INTERFACE_CHANGE,
	FSG_STATE_CONFIG_CHANGE,
	FSG_STATE_DISCONNECT,
	FSG_STATE_EXIT,
	FSG_STATE_TERMINATED
};

enum data_direction {
	DATA_DIR_UNKNOWN = 0,
	DATA_DIR_FROM_HOST,
	DATA_DIR_TO_HOST,
	DATA_DIR_NONE
};


/*-------------------------------------------------------------------------*/


static inline u32 get_unaligned_be24(u8 *buf)
{
	return 0xffffff & (u32) get_unaligned_be32(buf - 1);
}


/*-------------------------------------------------------------------------*/


enum {
#ifndef FSG_NO_DEVICE_STRINGS
	FSG_STRING_MANUFACTURER	= 1,
	FSG_STRING_PRODUCT,
	FSG_STRING_SERIAL,
	FSG_STRING_CONFIG,
#endif
	FSG_STRING_INTERFACE
};


#ifndef FSG_NO_OTG
static struct usb_otg_descriptor
fsg_otg_desc = {
	.bLength =		sizeof fsg_otg_desc,
	.bDescriptorType =	USB_DT_OTG,

	.bmAttributes =		USB_OTG_SRP,
};
#endif

/* There is only one interface. */

static struct usb_interface_descriptor
fsg_intf_desc = {
	.bLength =		sizeof fsg_intf_desc,
	.bDescriptorType =	USB_DT_INTERFACE,

	.bNumEndpoints =	3,		/* Adjusted during fsg_bind() */
	.bInterfaceClass =	0xff,//USB_CLASS_MASS_STORAGE,
	.bInterfaceSubClass =	0x00,//USB_SC_SCSI,	/* Adjusted during fsg_bind() */
	.bInterfaceProtocol =	0x00,//USB_PR_BULK,	/* Adjusted during fsg_bind() */
	.iInterface =		0x00,//FSG_STRING_INTERFACE,
};

static struct usb_interface_descriptor
hid0_intf_desc = {
	.bLength =		sizeof hid0_intf_desc,
	.bDescriptorType =	USB_DT_INTERFACE,
	.bInterfaceNumber = 1,
	.bAlternateSetting	= 0,
	.bNumEndpoints =	1,		/* Adjusted during fsg_bind() */
	.bInterfaceClass =	3,
	.bInterfaceSubClass =	1,	/* Adjusted during fsg_bind() */
	.bInterfaceProtocol =	2,	/* Adjusted during fsg_bind() */
	.iInterface =		0,
};

static struct hid_descriptor hidg0_desc = {
	.bLength			= sizeof hidg_desc,
	.bDescriptorType		= HID_DT_HID,
	.bcdHID				= 0x0110,
	.bCountryCode			= 0x00,
	.bNumDescriptors		= 0x1,
	.desc[0].bDescriptorType	= 0x22,
	.desc[0].wDescriptorLength	= 0x379,
};

/* High-Speed Support */

static struct usb_endpoint_descriptor hidg0_hs_in_ep_desc = {
	.bLength		= USB_DT_ENDPOINT_SIZE,
	.bDescriptorType	= USB_DT_ENDPOINT,
	.bEndpointAddress	= USB_DIR_IN,// | 0x01,
	.bmAttributes		= USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize	= 0x10,
	.bInterval		= 0x08, /* FIXME: Add this field in the
				      * HID gadget configuration?
				      * (struct hidg_func_descriptor)
				      */
};

static struct usb_interface_descriptor
hid1_intf_desc = {
	.bLength =		sizeof hid1_intf_desc,
	.bDescriptorType =	USB_DT_INTERFACE,
	.bInterfaceNumber = 2,
	.bAlternateSetting	= 0,
	.bNumEndpoints =	1,		/* Adjusted during fsg_bind() */
	.bInterfaceClass =	3,
	.bInterfaceSubClass =	1,	/* Adjusted during fsg_bind() */
	.bInterfaceProtocol =	1,	/* Adjusted during fsg_bind() */
	.iInterface =		0,
};

static struct hid_descriptor hidg1_desc = {
	.bLength			= sizeof hidg1_desc,
	.bDescriptorType		= HID_DT_HID,
	.bcdHID				= 0x0110,
	.bCountryCode			= 0x00,
	.bNumDescriptors		= 0x1,
	.desc[0].bDescriptorType	= 0x22,
	.desc[0].wDescriptorLength	= 0x3e,
};

/* High-Speed Support */

static struct usb_endpoint_descriptor hidg1_hs_in_ep_desc = {
	.bLength		= USB_DT_ENDPOINT_SIZE,
	.bDescriptorType	= USB_DT_ENDPOINT,
	.bEndpointAddress	= USB_DIR_IN,// | 0x01,
	.bmAttributes		= USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize	= 8,
	.bInterval		= 1, /* FIXME: Add this field in the
				      * HID gadget configuration?
				      * (struct hidg_func_descriptor)
				      */
};

static struct usb_interface_descriptor
hid2_intf_desc = {
	.bLength =		sizeof hid2_intf_desc,
	.bDescriptorType =	USB_DT_INTERFACE,
	.bInterfaceNumber = 3,
	.bAlternateSetting	= 0,
	.bNumEndpoints =	1,		/* Adjusted during fsg_bind() */
	.bInterfaceClass =	3,
	.bInterfaceSubClass =	0,	/* Adjusted during fsg_bind() */
	.bInterfaceProtocol =	0,	/* Adjusted during fsg_bind() */
	.iInterface =		0,
};

static struct hid_descriptor hidg2_desc = {
	.bLength			= sizeof hidg2_desc,
	.bDescriptorType		= HID_DT_HID,
	.bcdHID				= 0x0110,
	.bCountryCode			= 0x00,
	.bNumDescriptors		= 0x1,
	.desc[0].bDescriptorType	= 0x22,
	.desc[0].wDescriptorLength	= 0x65,
};

/* High-Speed Support */

static struct usb_endpoint_descriptor hidg2_hs_in_ep_desc = {
	.bLength		= USB_DT_ENDPOINT_SIZE,
	.bDescriptorType	= USB_DT_ENDPOINT,
	.bEndpointAddress	= USB_DIR_IN,// | 0x01,
	.bmAttributes		= USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize	= 8,
	.bInterval		= 1, /* FIXME: Add this field in the
				      * HID gadget configuration?
				      * (struct hidg_func_descriptor)
				      */
};

/*
 * Three full-speed endpoint descriptors: bulk-in, bulk-out, and
 * interrupt-in.
 */

static struct usb_endpoint_descriptor
fsg_fs_bulk_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	USB_DIR_IN | 0x01,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	/* wMaxPacketSize set by autoconfiguration */
};

static struct usb_endpoint_descriptor
fsg_fs_bulk_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	USB_DIR_OUT | 0x02,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	/* wMaxPacketSize set by autoconfiguration */
};

#ifndef FSG_NO_INTR_EP

static struct usb_endpoint_descriptor
fsg_fs_intr_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize =	cpu_to_le16(2),
	.bInterval =		32,	/* frames -> 32 ms */
};

#ifndef FSG_NO_OTG
#  define FSG_FS_FUNCTION_PRE_EP_ENTRIES	2
#else
#  define FSG_FS_FUNCTION_PRE_EP_ENTRIES	1
#endif

#endif

static struct usb_descriptor_header *fsg_fs_function[] = {
#ifndef FSG_NO_OTG
	(struct usb_descriptor_header *) &fsg_otg_desc,
#endif
	(struct usb_descriptor_header *) &fsg_intf_desc,
	(struct usb_descriptor_header *) &fsg_fs_bulk_in_desc,
	(struct usb_descriptor_header *) &fsg_fs_bulk_out_desc,
#ifndef FSG_NO_INTR_EP
	(struct usb_descriptor_header *) &fsg_fs_intr_in_desc,
#endif
	NULL,
};


/*
 * USB 2.0 devices need to expose both high speed and full speed
 * descriptors, unless they only run at full speed.
 *
 * That means alternate endpoint descriptors (bigger packets)
 * and a "device qualifier" ... plus more construction options
 * for the configuration descriptor.
 */
static struct usb_endpoint_descriptor
fsg_hs_bulk_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	/* bEndpointAddress copied from fs_bulk_in_desc during fsg_bind() */
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(512),
	//.bInterval =		1,	/* NAK every 1 uframe */
};

static struct usb_endpoint_descriptor
fsg_hs_bulk_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	/* bEndpointAddress copied from fs_bulk_out_desc during fsg_bind() */
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(512),
	.bInterval =		1,	/* NAK every 1 uframe */
};

#ifndef FSG_NO_INTR_EP

static struct usb_endpoint_descriptor
fsg_hs_intr_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	/* bEndpointAddress copied from fs_intr_in_desc during fsg_bind() */
	.bmAttributes =		USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize =	cpu_to_le16(2),
	.bInterval =		9,	/* 2**(9-1) = 256 uframes -> 32 ms */
};

#ifndef FSG_NO_OTG
#  define FSG_HS_FUNCTION_PRE_EP_ENTRIES	2
#else
#  define FSG_HS_FUNCTION_PRE_EP_ENTRIES	1
#endif

#endif

#define UVC_INTF_VIDEO_CONTROL			0
#define UVC_INTF_VIDEO_STREAMING		1

static struct usb_interface_assoc_descriptor M_uvc_iad __initdata = {
	.bLength		= sizeof(M_uvc_iad),
	.bDescriptorType	= USB_DT_INTERFACE_ASSOCIATION,
	.bFirstInterface	= 0,
	.bInterfaceCount	= 2,
	.bFunctionClass		= USB_CLASS_VIDEO,
	.bFunctionSubClass	= UVC_SC_VIDEO_INTERFACE_COLLECTION,
	.bFunctionProtocol	= 0x00,
	.iFunction		= 0,
};

static struct usb_interface_descriptor M_uvc_control_intf __initdata = {
	.bLength		= USB_DT_INTERFACE_SIZE,
	.bDescriptorType	= USB_DT_INTERFACE,
	.bInterfaceNumber	= UVC_INTF_VIDEO_CONTROL,
	.bAlternateSetting	= 0,
	.bNumEndpoints		= 1,
	.bInterfaceClass	= USB_CLASS_VIDEO,
	.bInterfaceSubClass	= UVC_SC_VIDEOCONTROL,
	.bInterfaceProtocol	= 0x00,
	.iInterface		= 0,
};

DECLARE_UVC_HEADER_DESCRIPTOR(1);
DECLARE_UVC_INPUT_HEADER_DESCRIPTOR(1, 1);
DECLARE_UVC_INPUT_HEADER_DESCRIPTOR(1, 2);
DECLARE_UVC_INPUT_HEADER_DESCRIPTOR(1, 3);
DECLARE_UVC_INPUT_HEADER_DESCRIPTOR(1, 4);
DECLARE_UVC_INPUT_HEADER_DESCRIPTOR(1, 5);
DECLARE_UVC_INPUT_HEADER_DESCRIPTOR(1, 6);
DECLARE_UVC_FRAME_MJPEG(1);
DECLARE_UVC_FRAME_FRAME_BASED(1);
DECLARE_UVC_EXTENSION_UNIT_DESCRIPTOR(1, 1);
DECLARE_UVC_FRAME_UNCOMPRESSED(1);

static struct UVC_HEADER_DESCRIPTOR(1) M_uvc_control_header = {
	.bLength		= UVC_DT_HEADER_SIZE(1),
	.bDescriptorType	= USB_DT_CS_INTERFACE,
	.bDescriptorSubType	= UVC_VC_HEADER,
	.bcdUVC			= cpu_to_le16(0x0110),
	.wTotalLength		= 0, /* dynamic */
	.dwClockFrequency	= cpu_to_le32(48000000),
	.bInCollection		= 1, /* dynamic */
	.baInterfaceNr[0]	= 1, /* dynamic */
};

static struct uvc_camera_terminal_descriptor M_uvc_camera_terminal = {
	.bLength		= UVC_DT_CAMERA_TERMINAL_SIZE(3),
	.bDescriptorType	= USB_DT_CS_INTERFACE,
	.bDescriptorSubType	= UVC_VC_INPUT_TERMINAL,
	.bTerminalID		= 1,
	.wTerminalType		= cpu_to_le16(0x0201),
	.bAssocTerminal		= 0,
	.iTerminal		= 0,
	.wObjectiveFocalLengthMin	= cpu_to_le16(0),
	.wObjectiveFocalLengthMax	= cpu_to_le16(0),
	.wOcularFocalLength		= cpu_to_le16(0),
	.bControlSize		= 3,
	.bmControls[0]		= 0,
	.bmControls[1]		= 0,
	.bmControls[2]		= 0,
};

static struct uvc_processing_unit_descriptor M_uvc_processing = {
	.bLength		= UVC_DT_PROCESSING_UNIT_SIZE(3),
	.bDescriptorType	= USB_DT_CS_INTERFACE,
	.bDescriptorSubType	= UVC_VC_PROCESSING_UNIT,
	.bUnitID		= 2,
	.bSourceID		= 1,
	.wMaxMultiplier		= 0,
	.bControlSize		= 3,
	.bmControls[0]		= 0,
	.bmControls[1]		= 0,
	.bmControls[2]		= 0,
	.iProcessing		= 0,
	.bmVideoStandards   = 0x09,
};

static struct UVC_EXTENSION_UNIT_DESCRIPTOR(1, 1) M_uvc_extension = {
	.bLength = UVC_DT_EXTENSION_UNIT_SIZE(1, 1),
	.bDescriptorType	= USB_DT_CS_INTERFACE,
	.bDescriptorSubType	= UVC_VC_EXTENSION_UNIT,
	.bUnitID = 3,
	.guidExtensionCode[0] = 0xFF,
	.guidExtensionCode[1] = 0xFF,
	.guidExtensionCode[2] = 0xFF,
	.guidExtensionCode[3] = 0xFF,
	.guidExtensionCode[4] = 0xFF,
	.guidExtensionCode[5] = 0xFF,
	.guidExtensionCode[6] = 0xFF,
	.guidExtensionCode[7] = 0xFF,
	.guidExtensionCode[8] = 0xFF,
	.guidExtensionCode[9] = 0xFF,
	.guidExtensionCode[10] = 0xFF,
	.guidExtensionCode[11] = 0xFF,
	.guidExtensionCode[12] = 0xFF,
	.guidExtensionCode[13] = 0xFF,
	.guidExtensionCode[14] = 0xFF,
	.guidExtensionCode[15] = 0xFF,
	.bNumControls = 0x00,
	.bNrInPins = 0x01,
	.baSourceID[0] = 0x02,
	.bControlSize = 0x01,
	.bmControls[0] = 0x00,
	.iExtension = 0x00,
};

static struct uvc_output_terminal_descriptor M_uvc_output_terminal = {
	.bLength		= UVC_DT_OUTPUT_TERMINAL_SIZE,
	.bDescriptorType	= USB_DT_CS_INTERFACE,
	.bDescriptorSubType	= UVC_VC_OUTPUT_TERMINAL,
	.bTerminalID		= 4,
	.wTerminalType		= cpu_to_le16(0x0101),
	.bAssocTerminal		= 0,
	.bSourceID		= 3,
	.iTerminal		= 0,
};

static struct uvc_control_endpoint_descriptor M_uvc_control_cs_ep __initdata = {
	.bLength		= UVC_DT_CONTROL_ENDPOINT_SIZE,
	.bDescriptorType	= USB_DT_CS_ENDPOINT,
	.bDescriptorSubType	= UVC_EP_INTERRUPT,
	.wMaxTransferSize	= cpu_to_le16(16),
};

static struct usb_interface_descriptor M_uvc_streaming_intf_alt0 __initdata = {
	.bLength		= USB_DT_INTERFACE_SIZE,
	.bDescriptorType	= USB_DT_INTERFACE,
	.bInterfaceNumber	= UVC_INTF_VIDEO_STREAMING,
	.bAlternateSetting	= 0,
	.bNumEndpoints		= 1,
	.bInterfaceClass	= USB_CLASS_VIDEO,
	.bInterfaceSubClass	= UVC_SC_VIDEOSTREAMING,
	.bInterfaceProtocol	= 0x00,
	.iInterface		= 0,
};

static struct uvc_input_header_descriptor *M_uvc_input_header;

static struct UVC_INPUT_HEADER_DESCRIPTOR(1, 1) M_uvc_input_header1 = {
	.bLength		= UVC_DT_INPUT_HEADER_SIZE(1, 1),
	.bDescriptorType	= USB_DT_CS_INTERFACE,
	.bDescriptorSubType	= UVC_VS_INPUT_HEADER,
	.bNumFormats		= 1,
	.wTotalLength		= 0, /* dynamic */
	.bEndpointAddress	= 0x82, /* dynamic */
	.bmInfo			= 0,
	.bTerminalLink		= 4,
	.bStillCaptureMethod	= 1,
	.bTriggerSupport	= 0,
	.bTriggerUsage		= 0,
	.bControlSize		= 1,
	.bmaControls[0][0]	= 0,
};

static struct UVC_INPUT_HEADER_DESCRIPTOR(1, 2) M_uvc_input_header2 = {
	.bLength		= UVC_DT_INPUT_HEADER_SIZE(1, 2),
	.bDescriptorType	= USB_DT_CS_INTERFACE,
	.bDescriptorSubType	= UVC_VS_INPUT_HEADER,
	.bNumFormats		= 2,
	.wTotalLength		= 0, /* dynamic */
	.bEndpointAddress	= 0x82, /* dynamic */
	.bmInfo			= 0,
	.bTerminalLink		= 4,
	.bStillCaptureMethod	= 1,
	.bTriggerSupport	= 0,
	.bTriggerUsage		= 0,
	.bControlSize		= 1,
	.bmaControls[0][0]	= 0,
	.bmaControls[1][0]	= 0,
};

static struct UVC_INPUT_HEADER_DESCRIPTOR(1, 3) M_uvc_input_header3 = {
	.bLength		= UVC_DT_INPUT_HEADER_SIZE(1, 3),
	.bDescriptorType	= USB_DT_CS_INTERFACE,
	.bDescriptorSubType	= UVC_VS_INPUT_HEADER,
	.bNumFormats		= 3,
	.wTotalLength		= 0, /* dynamic */
	.bEndpointAddress	= 0x82, /* dynamic */
	.bmInfo			= 0,
	.bTerminalLink		= 4,
	.bStillCaptureMethod	= 1,
	.bTriggerSupport	= 0,
	.bTriggerUsage		= 0,
	.bControlSize		= 1,
	.bmaControls[0][0]	= 0,
	.bmaControls[1][0]	= 0,
	.bmaControls[2][0]	= 0,
};

static struct UVC_INPUT_HEADER_DESCRIPTOR(1, 4) M_uvc_input_header4 = {
	.bLength		= UVC_DT_INPUT_HEADER_SIZE(1, 4),
	.bDescriptorType	= USB_DT_CS_INTERFACE,
	.bDescriptorSubType	= UVC_VS_INPUT_HEADER,
	.bNumFormats		= 4,
	.wTotalLength		= 0, /* dynamic */
	.bEndpointAddress	= 0x82, /* dynamic */
	.bmInfo			= 0,
	.bTerminalLink		= 4,
	.bStillCaptureMethod	= 1,
	.bTriggerSupport	= 0,
	.bTriggerUsage		= 0,
	.bControlSize		= 1,
	.bmaControls[0][0]	= 0,
	.bmaControls[1][0]	= 0,
	.bmaControls[2][0]	= 0,
	.bmaControls[3][0]	= 0,
};


static struct UVC_INPUT_HEADER_DESCRIPTOR(1, 5) M_uvc_input_header5 = {
	.bLength		= UVC_DT_INPUT_HEADER_SIZE(1, 5),
	.bDescriptorType	= USB_DT_CS_INTERFACE,
	.bDescriptorSubType	= UVC_VS_INPUT_HEADER,
	.bNumFormats		= 5,
	.wTotalLength		= 0, /* dynamic */
	.bEndpointAddress	= 0x82, /* dynamic */
	.bmInfo			= 0,
	.bTerminalLink		= 4,
	.bStillCaptureMethod	= 1,
	.bTriggerSupport	= 0,
	.bTriggerUsage		= 0,
	.bControlSize		= 1,
	.bmaControls[0][0]	= 0,
	.bmaControls[1][0]	= 0,
	.bmaControls[2][0]	= 0,
	.bmaControls[3][0]	= 0,
	.bmaControls[4][0]	= 0,
};

static struct UVC_INPUT_HEADER_DESCRIPTOR(1, 6) M_uvc_input_header6 = {
	.bLength		= UVC_DT_INPUT_HEADER_SIZE(1, 6),
	.bDescriptorType	= USB_DT_CS_INTERFACE,
	.bDescriptorSubType	= UVC_VS_INPUT_HEADER,
	.bNumFormats		= 6,
	.wTotalLength		= 0, /* dynamic */
	.bEndpointAddress	= 0x82, /* dynamic */
	.bmInfo			= 0,
	.bTerminalLink		= 4,
	.bStillCaptureMethod	= 1,
	.bTriggerSupport	= 0,
	.bTriggerUsage		= 0,
	.bControlSize		= 1,
	.bmaControls[0][0]	= 0,
	.bmaControls[1][0]	= 0,
	.bmaControls[2][0]	= 0,
	.bmaControls[3][0]	= 0,
	.bmaControls[4][0]	= 0,
	.bmaControls[5][0]	= 0,
};

static struct uvc_format_mjpeg M_uvc_format_mjpg = {
	.bLength		= UVC_DT_FORMAT_MJPEG_SIZE,
	.bDescriptorType	= USB_DT_CS_INTERFACE,
	.bDescriptorSubType	= UVC_VS_FORMAT_MJPEG,
	.bFormatIndex		= 1,
	.bNumFrameDescriptors	= 4,
	.bmFlags		= 1,
	.bDefaultFrameIndex	= 1,
	.bAspectRatioX		= 0,
	.bAspectRatioY		= 0,
	.bmInterfaceFlags	= 0,
	.bCopyProtect		= 0,
};

static struct UVC_FRAME_MJPEG(1) M_uvc_frame_mjpg_320p = {
	.bLength		= UVC_DT_FRAME_MJPEG_SIZE(1),
	.bDescriptorType	= USB_DT_CS_INTERFACE,
	.bDescriptorSubType	= UVC_VS_FRAME_MJPEG,
	.bFrameIndex		= 1,
	.bmCapabilities		= 0,
	.wWidth			= cpu_to_le16(320),
	.wHeight		= cpu_to_le16(240),
	.dwMinBitRate		= cpu_to_le32(663552000),
	.dwMaxBitRate		= cpu_to_le32(663552000),
	.dwMaxVideoFrameBufferSize	= cpu_to_le32(2764800),
	.dwDefaultFrameInterval	= cpu_to_le32(333333),
	.bFrameIntervalType	= 1,
	.dwFrameInterval[0]	= cpu_to_le32(333333),
};

static struct UVC_FRAME_MJPEG(1) M_uvc_frame_mjpg_480p = {
	.bLength		= UVC_DT_FRAME_MJPEG_SIZE(1),
	.bDescriptorType	= USB_DT_CS_INTERFACE,
	.bDescriptorSubType	= UVC_VS_FRAME_MJPEG,
	.bFrameIndex		= 2,
	.bmCapabilities		= 0,
	.wWidth			= cpu_to_le16(640),
	.wHeight		= cpu_to_le16(480),
	.dwMinBitRate		= cpu_to_le32(663552000),
	.dwMaxBitRate		= cpu_to_le32(663552000),
	.dwMaxVideoFrameBufferSize	= cpu_to_le32(2764800),
	.dwDefaultFrameInterval	= cpu_to_le32(333333),
	.bFrameIntervalType	= 1,
	.dwFrameInterval[0]	= cpu_to_le32(333333),
};

static struct UVC_FRAME_MJPEG(1) M_uvc_frame_mjpg_720p = {
	.bLength		= UVC_DT_FRAME_MJPEG_SIZE(1),
	.bDescriptorType	= USB_DT_CS_INTERFACE,
	.bDescriptorSubType	= UVC_VS_FRAME_MJPEG,
	.bFrameIndex		= 3,
	.bmCapabilities		= 0,
	.wWidth			= cpu_to_le16(1280),
	.wHeight		= cpu_to_le16(720),
	.dwMinBitRate		= cpu_to_le32(663552000),
	.dwMaxBitRate		= cpu_to_le32(663552000),
	.dwMaxVideoFrameBufferSize	= cpu_to_le32(2764800),
	.dwDefaultFrameInterval	= cpu_to_le32(333333),
	.bFrameIntervalType	= 1,
	.dwFrameInterval[0]	= cpu_to_le32(333333),
};

static struct UVC_FRAME_MJPEG(1) M_uvc_frame_mjpg_1080p = {
	.bLength		= UVC_DT_FRAME_MJPEG_SIZE(1),
	.bDescriptorType	= USB_DT_CS_INTERFACE,
	.bDescriptorSubType	= UVC_VS_FRAME_MJPEG,
	.bFrameIndex		= 4,
	.bmCapabilities		= 0,
	.wWidth			= cpu_to_le16(1920),
	.wHeight		= cpu_to_le16(1080),
	.dwMinBitRate		= cpu_to_le32(663552000),
	.dwMaxBitRate		= cpu_to_le32(663552000),
	.dwMaxVideoFrameBufferSize	= cpu_to_le32(2764800),
	.dwDefaultFrameInterval	= cpu_to_le32(333333),
	.bFrameIntervalType	= 1,
	.dwFrameInterval[0]	= cpu_to_le32(333333),
};

static struct uvc_color_matching_descriptor M_uvc_color_matching_mjpeg = {
	.bLength		= UVC_DT_COLOR_MATCHING_SIZE,
	.bDescriptorType	= USB_DT_CS_INTERFACE,
	.bDescriptorSubType	= UVC_VS_COLORFORMAT,
	.bColorPrimaries	= 1,
	.bTransferCharacteristics	= 1,
	.bMatrixCoefficients	= 4,
};

static struct uvc_format_frame_based M_uvc_format_h264 = {
	.bLength = UVC_DT_FORMAT_FRAME_BASED_SIZE,
	.bDescriptorType = USB_DT_CS_INTERFACE,
	.bDescriptorSubType = UVC_VS_FORMAT_FRAME_BASED,
	.bFormatIndex = 2,
	.bNumFrameDescriptors = 4,
	.guidFormat		=
		{ 'H',  '2',  '6',  '4', 0x00, 0x00, 0x10, 0x00,
		 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71},
	.bBitsPerPixel = 0x10,
	.bDefaultFrameIndex = 1,
	.bAspectRatioX = 0x00,
	.bAspectRatioY = 0x00,
	.bmInterfaceFlags = 0x00,
	.bCopyProtect = 0x00,
	.bVariableSize = 0x01,
};

static struct UVC_FRAME_FRAME_BASED(1) M_uvc_frame_h264_320p = {
	.bLength = UVC_DT_FRAME_FRAME_BASED_SIZE(1),
	.bDescriptorType = USB_DT_CS_INTERFACE,
	.bDescriptorSubType = UVC_VS_FRAME_FRAME_BASED,
	.bFrameIndex = 1,
	.bmCapabilities = 0,
	.wWidth = cpu_to_le16(320),
	.wHeight = cpu_to_le16(240),
	.dwMinBitRate = cpu_to_le32(663552000),
	.dwMaxBitRate = cpu_to_le32(663552000),
	.dwDefaultFrameInterval = cpu_to_le32(333333),
	.bFrameIntervalType = 1,
	.dwBytesPerLine = cpu_to_le32(0),
	.dwFrameInterval[0] = cpu_to_le32(333333),
};

static struct UVC_FRAME_FRAME_BASED(1) M_uvc_frame_h264_480p = {
	.bLength = UVC_DT_FRAME_FRAME_BASED_SIZE(1),
	.bDescriptorType = USB_DT_CS_INTERFACE,
	.bDescriptorSubType = UVC_VS_FRAME_FRAME_BASED,
	.bFrameIndex = 2,
	.bmCapabilities = 0,
	.wWidth = cpu_to_le16(640),
	.wHeight = cpu_to_le16(480),
	.dwMinBitRate = cpu_to_le32(663552000),
	.dwMaxBitRate = cpu_to_le32(663552000),
	.dwDefaultFrameInterval = cpu_to_le32(333333),
	.bFrameIntervalType = 1,
	.dwBytesPerLine = cpu_to_le32(0),
	.dwFrameInterval[0] = cpu_to_le32(333333),
};

static struct UVC_FRAME_FRAME_BASED(1) M_uvc_frame_h264_720p = {
	.bLength = UVC_DT_FRAME_FRAME_BASED_SIZE(1),
	.bDescriptorType = USB_DT_CS_INTERFACE,
	.bDescriptorSubType = UVC_VS_FRAME_FRAME_BASED,
	.bFrameIndex = 3,
	.bmCapabilities = 0,
	.wWidth = cpu_to_le16(1280),
	.wHeight = cpu_to_le16(720),
	.dwMinBitRate = cpu_to_le32(663552000),
	.dwMaxBitRate = cpu_to_le32(663552000),
	.dwDefaultFrameInterval = cpu_to_le32(333333),
	.bFrameIntervalType = 1,
	.dwBytesPerLine = cpu_to_le32(0),
	.dwFrameInterval[0] = cpu_to_le32(333333),
};

static struct UVC_FRAME_FRAME_BASED(1) M_uvc_frame_h264_1080p = {
	.bLength = UVC_DT_FRAME_FRAME_BASED_SIZE(1),
	.bDescriptorType = USB_DT_CS_INTERFACE,
	.bDescriptorSubType = UVC_VS_FRAME_FRAME_BASED,
	.bFrameIndex = 4,
	.bmCapabilities = 0,
	.wWidth = cpu_to_le16(1920),
	.wHeight = cpu_to_le16(1080),
	.dwMinBitRate = cpu_to_le32(663552000),
	.dwMaxBitRate = cpu_to_le32(663552000),
	.dwDefaultFrameInterval = cpu_to_le32(333333),
	.bFrameIntervalType = 1,
	.dwBytesPerLine = cpu_to_le32(0),
	.dwFrameInterval[0] = cpu_to_le32(333333),
};

static struct uvc_color_matching_descriptor M_uvc_color_matching_h264 = {
	.bLength		= UVC_DT_COLOR_MATCHING_SIZE,
	.bDescriptorType	= USB_DT_CS_INTERFACE,
	.bDescriptorSubType	= UVC_VS_COLORFORMAT,
	.bColorPrimaries	= 1,
	.bTransferCharacteristics	= 1,
	.bMatrixCoefficients	= 4,
};

static struct uvc_format_uncompressed M_uvc_format_yuv = {
	.bLength		= UVC_DT_FORMAT_UNCOMPRESSED_SIZE,
	.bDescriptorType	= USB_DT_CS_INTERFACE,
	.bDescriptorSubType	= UVC_VS_FORMAT_UNCOMPRESSED,
	.bFormatIndex		= 3,
	.bNumFrameDescriptors	= 2,
	.guidFormat		=
		{ 'Y',  'U',  'Y',  '2', 0x00, 0x00, 0x10, 0x00,
		 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71},
	.bBitsPerPixel		= 16,
	.bDefaultFrameIndex	= 1,
	.bAspectRatioX		= 0,
	.bAspectRatioY		= 0,
	.bmInterfaceFlags	= 0,
	.bCopyProtect		= 0,
};

static struct UVC_FRAME_UNCOMPRESSED(1) M_uvc_frame_yuv_320p = {
	.bLength		= UVC_DT_FRAME_UNCOMPRESSED_SIZE(1),
	.bDescriptorType	= USB_DT_CS_INTERFACE,
	.bDescriptorSubType	= UVC_VS_FRAME_UNCOMPRESSED,
	.bFrameIndex		= 1,
	.bmCapabilities		= 0,
	.wWidth			= cpu_to_le16(320),
	.wHeight		= cpu_to_le16(240),
	.dwMinBitRate		= cpu_to_le32(55296000),
	.dwMaxBitRate		= cpu_to_le32(55296000),
	.dwMaxVideoFrameBufferSize	= cpu_to_le32(1843200),
	.dwDefaultFrameInterval	= cpu_to_le32(333333),
	.bFrameIntervalType	= 1,
	.dwFrameInterval[0]	= cpu_to_le32(333333),
};

static struct UVC_FRAME_UNCOMPRESSED(1) M_uvc_frame_yuv_480p = {
	.bLength		= UVC_DT_FRAME_UNCOMPRESSED_SIZE(1),
	.bDescriptorType	= USB_DT_CS_INTERFACE,
	.bDescriptorSubType	= UVC_VS_FRAME_UNCOMPRESSED,
	.bFrameIndex		= 2,
	.bmCapabilities		= 0,
	.wWidth			= cpu_to_le16(640),
	.wHeight		= cpu_to_le16(480),
	.dwMinBitRate		= cpu_to_le32(55296000),
	.dwMaxBitRate		= cpu_to_le32(55296000),
	.dwMaxVideoFrameBufferSize	= cpu_to_le32(1843200),
	.dwDefaultFrameInterval	= cpu_to_le32(333333),
	.bFrameIntervalType	= 1,
	.dwFrameInterval[0]	= cpu_to_le32(333333),
};

static struct uvc_color_matching_descriptor M_uvc_color_matching_yuv = {
	.bLength		= UVC_DT_COLOR_MATCHING_SIZE,
	.bDescriptorType	= USB_DT_CS_INTERFACE,
	.bDescriptorSubType	= UVC_VS_COLORFORMAT,
	.bColorPrimaries	= 1,
	.bTransferCharacteristics	= 1,
	.bMatrixCoefficients	= 4,
};

static struct uvc_format_uncompressed M_uvc_format_nv12 = {
	.bLength		= UVC_DT_FORMAT_UNCOMPRESSED_SIZE,
	.bDescriptorType	= USB_DT_CS_INTERFACE,
	.bDescriptorSubType	= UVC_VS_FORMAT_UNCOMPRESSED,
	.bFormatIndex		= 4,
	.bNumFrameDescriptors	= 2,
	.guidFormat		=
		{ 'N',  'V',  '1',  '2', 0x00, 0x00, 0x10, 0x00,
		 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71},
	.bBitsPerPixel		= 16,
	.bDefaultFrameIndex	= 1,
	.bAspectRatioX		= 0,
	.bAspectRatioY		= 0,
	.bmInterfaceFlags	= 0,
	.bCopyProtect		= 0,
};

static struct UVC_FRAME_UNCOMPRESSED(1) M_uvc_frame_nv12_320p = {
	.bLength		= UVC_DT_FRAME_UNCOMPRESSED_SIZE(1),
	.bDescriptorType	= USB_DT_CS_INTERFACE,
	.bDescriptorSubType	= UVC_VS_FRAME_UNCOMPRESSED,
	.bFrameIndex		= 1,
	.bmCapabilities		= 0,
	.wWidth			= cpu_to_le16(320),
	.wHeight		= cpu_to_le16(240),
	.dwMinBitRate		= cpu_to_le32(55296000),
	.dwMaxBitRate		= cpu_to_le32(55296000),
	.dwMaxVideoFrameBufferSize	= cpu_to_le32(1843200),
	.dwDefaultFrameInterval	= cpu_to_le32(333333),
	.bFrameIntervalType	= 1,
	.dwFrameInterval[0]	= cpu_to_le32(333333),
};

static struct UVC_FRAME_UNCOMPRESSED(1) M_uvc_frame_nv12_480p = {
	.bLength		= UVC_DT_FRAME_UNCOMPRESSED_SIZE(1),
	.bDescriptorType	= USB_DT_CS_INTERFACE,
	.bDescriptorSubType	= UVC_VS_FRAME_UNCOMPRESSED,
	.bFrameIndex		= 2,
	.bmCapabilities		= 0,
	.wWidth			= cpu_to_le16(640),
	.wHeight		= cpu_to_le16(480),
	.dwMinBitRate		= cpu_to_le32(55296000),
	.dwMaxBitRate		= cpu_to_le32(55296000),
	.dwMaxVideoFrameBufferSize	= cpu_to_le32(1843200),
	.dwDefaultFrameInterval	= cpu_to_le32(333333),
	.bFrameIntervalType	= 1,
	.dwFrameInterval[0]	= cpu_to_le32(333333),
};

static struct uvc_color_matching_descriptor M_uvc_color_matching_nv12 = {
	.bLength		= UVC_DT_COLOR_MATCHING_SIZE,
	.bDescriptorType	= USB_DT_CS_INTERFACE,
	.bDescriptorSubType	= UVC_VS_COLORFORMAT,
	.bColorPrimaries	= 1,
	.bTransferCharacteristics	= 1,
	.bMatrixCoefficients	= 4,
};

static struct uvc_format_uncompressed M_uvc_format_i420 = {
	.bLength		= UVC_DT_FORMAT_UNCOMPRESSED_SIZE,
	.bDescriptorType	= USB_DT_CS_INTERFACE,
	.bDescriptorSubType	= UVC_VS_FORMAT_UNCOMPRESSED,
	.bFormatIndex		= 5,
	.bNumFrameDescriptors	= 2,
	.guidFormat		=
		{ 'I',  '4',  '2',  '0', 0x00, 0x00, 0x10, 0x00,
		 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71},
	.bBitsPerPixel		= 16,
	.bDefaultFrameIndex	= 1,
	.bAspectRatioX		= 0,
	.bAspectRatioY		= 0,
	.bmInterfaceFlags	= 0,
	.bCopyProtect		= 0,
};

static struct UVC_FRAME_UNCOMPRESSED(1) M_uvc_frame_i420_320p = {
	.bLength		= UVC_DT_FRAME_UNCOMPRESSED_SIZE(1),
	.bDescriptorType	= USB_DT_CS_INTERFACE,
	.bDescriptorSubType	= UVC_VS_FRAME_UNCOMPRESSED,
	.bFrameIndex		= 1,
	.bmCapabilities		= 0,
	.wWidth			= cpu_to_le16(320),
	.wHeight		= cpu_to_le16(240),
	.dwMinBitRate		= cpu_to_le32(55296000),
	.dwMaxBitRate		= cpu_to_le32(55296000),
	.dwMaxVideoFrameBufferSize	= cpu_to_le32(1843200),
	.dwDefaultFrameInterval	= cpu_to_le32(333333),
	.bFrameIntervalType	= 1,
	.dwFrameInterval[0]	= cpu_to_le32(333333),
};

static struct UVC_FRAME_UNCOMPRESSED(1) M_uvc_frame_i420_480p = {
	.bLength		= UVC_DT_FRAME_UNCOMPRESSED_SIZE(1),
	.bDescriptorType	= USB_DT_CS_INTERFACE,
	.bDescriptorSubType	= UVC_VS_FRAME_UNCOMPRESSED,
	.bFrameIndex		= 2,
	.bmCapabilities		= 0,
	.wWidth			= cpu_to_le16(640),
	.wHeight		= cpu_to_le16(480),
	.dwMinBitRate		= cpu_to_le32(55296000),
	.dwMaxBitRate		= cpu_to_le32(55296000),
	.dwMaxVideoFrameBufferSize	= cpu_to_le32(1843200),
	.dwDefaultFrameInterval	= cpu_to_le32(333333),
	.bFrameIntervalType	= 1,
	.dwFrameInterval[0]	= cpu_to_le32(333333),
};

static struct uvc_color_matching_descriptor M_uvc_color_matching_i420 = {
	.bLength		= UVC_DT_COLOR_MATCHING_SIZE,
	.bDescriptorType	= USB_DT_CS_INTERFACE,
	.bDescriptorSubType	= UVC_VS_COLORFORMAT,
	.bColorPrimaries	= 1,
	.bTransferCharacteristics	= 1,
	.bMatrixCoefficients	= 4,
};

static struct uvc_format_uncompressed M_uvc_format_m420 = {
	.bLength		= UVC_DT_FORMAT_UNCOMPRESSED_SIZE,
	.bDescriptorType	= USB_DT_CS_INTERFACE,
	.bDescriptorSubType	= UVC_VS_FORMAT_UNCOMPRESSED,
	.bFormatIndex		= 6,
	.bNumFrameDescriptors	= 2,
	.guidFormat		=
		{ 'M',  '4',  '2',  '0', 0x00, 0x00, 0x10, 0x00,
		 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71},
	.bBitsPerPixel		= 16,
	.bDefaultFrameIndex	= 1,
	.bAspectRatioX		= 0,
	.bAspectRatioY		= 0,
	.bmInterfaceFlags	= 0,
	.bCopyProtect		= 0,
};

static struct UVC_FRAME_UNCOMPRESSED(1) M_uvc_frame_m420_320p = {
	.bLength		= UVC_DT_FRAME_UNCOMPRESSED_SIZE(1),
	.bDescriptorType	= USB_DT_CS_INTERFACE,
	.bDescriptorSubType	= UVC_VS_FRAME_UNCOMPRESSED,
	.bFrameIndex		= 1,
	.bmCapabilities		= 0,
	.wWidth			= cpu_to_le16(320),
	.wHeight		= cpu_to_le16(240),
	.dwMinBitRate		= cpu_to_le32(55296000),
	.dwMaxBitRate		= cpu_to_le32(55296000),
	.dwMaxVideoFrameBufferSize	= cpu_to_le32(1843200),
	.dwDefaultFrameInterval	= cpu_to_le32(333333),
	.bFrameIntervalType	= 1,
	.dwFrameInterval[0]	= cpu_to_le32(333333),
};

static struct UVC_FRAME_UNCOMPRESSED(1) M_uvc_frame_m420_480p = {
	.bLength		= UVC_DT_FRAME_UNCOMPRESSED_SIZE(1),
	.bDescriptorType	= USB_DT_CS_INTERFACE,
	.bDescriptorSubType	= UVC_VS_FRAME_UNCOMPRESSED,
	.bFrameIndex		= 2,
	.bmCapabilities		= 0,
	.wWidth			= cpu_to_le16(640),
	.wHeight		= cpu_to_le16(480),
	.dwMinBitRate		= cpu_to_le32(55296000),
	.dwMaxBitRate		= cpu_to_le32(55296000),
	.dwMaxVideoFrameBufferSize	= cpu_to_le32(1843200),
	.dwDefaultFrameInterval	= cpu_to_le32(333333),
	.bFrameIntervalType	= 1,
	.dwFrameInterval[0]	= cpu_to_le32(333333),
};

static struct uvc_color_matching_descriptor M_uvc_color_matching_m420 = {
	.bLength		= UVC_DT_COLOR_MATCHING_SIZE,
	.bDescriptorType	= USB_DT_CS_INTERFACE,
	.bDescriptorSubType	= UVC_VS_COLORFORMAT,
	.bColorPrimaries	= 1,
	.bTransferCharacteristics	= 1,
	.bMatrixCoefficients	= 4,
};

#define VC_START_IDX 2
#define RESOLUTION_BIT_MASK 0xf
#define RESOLUTION_320p 0x1
#define RESOLUTION_480p 0x2
#define RESOLUTION_720p 0x4
#define RESOLUTION_1080p 0x8
#define FORMAT_COUNT1 0x1
#define FORMAT_COUNT2 0x2
#define FORMAT_COUNT3 0x3
#define FORMAT_COUNT4 0x4
#define FORMAT_COUNT5 0x5
#define FORMAT_COUNT6 0x6
#define MAX_DESC_NUM 100

static struct usb_descriptor_header *fsg_hs_function[MAX_DESC_NUM] = {
#ifndef FSG_NO_OTG
	(struct usb_descriptor_header *) &fsg_otg_desc,
#endif
	(struct usb_descriptor_header *) &M_uvc_iad,
	(struct usb_descriptor_header *) &M_uvc_control_intf,
	(struct usb_descriptor_header *) &M_uvc_control_header,
	(struct usb_descriptor_header *) &M_uvc_camera_terminal,
	(struct usb_descriptor_header *) &M_uvc_processing,
	(struct usb_descriptor_header *) &M_uvc_extension,
	(struct usb_descriptor_header *) &M_uvc_output_terminal,
	(struct usb_descriptor_header *) &hidg0_hs_in_ep_desc,
	(struct usb_descriptor_header *) &M_uvc_control_cs_ep,
	(struct usb_descriptor_header *) &M_uvc_streaming_intf_alt0,
	(struct usb_descriptor_header *) &fsg_hs_bulk_in_desc,
	(struct usb_descriptor_header *) &M_uvc_input_header,                        //fsg_hs_function[INPUT_HEADER_IDX]
	//(struct usb_descriptor_header *) &M_uvc_format_mjpg,                         //fsg_hs_function[INPUT_FORMAT_IDX]
	//(struct usb_descriptor_header *) &M_uvc_frame_mjpg_720p,
	//(struct usb_descriptor_header *) &M_uvc_frame_mjpg_1080p,
	//(struct usb_descriptor_header *) &M_uvc_color_matching_mjpeg,
	//(struct usb_descriptor_header *) &M_uvc_format_h264,
	//(struct usb_descriptor_header *) &M_uvc_frame_h264_720p,
	//(struct usb_descriptor_header *) &M_uvc_frame_h264_1080p,
	//(struct usb_descriptor_header *) &M_uvc_color_matching_h264,
	//(struct usb_descriptor_header *) &fsg_intf_desc,
	//(struct usb_descriptor_header *) &fsg_hs_bulk_in_desc,
	//(struct usb_descriptor_header *) &fsg_hs_bulk_out_desc,
	//(struct usb_descriptor_header *) &hid0_intf_desc,
	//(struct usb_descriptor_header *) &hidg0_desc,
	//(struct usb_descriptor_header *) &hidg0_hs_in_ep_desc,
	//(struct usb_descriptor_header *) &hid1_intf_desc,
	//(struct usb_descriptor_header *) &hidg1_desc,
	//(struct usb_descriptor_header *) &hidg1_hs_in_ep_desc,
	//(struct usb_descriptor_header *) &hid2_intf_desc,
	//(struct usb_descriptor_header *) &hidg2_desc,
	//(struct usb_descriptor_header *) &hidg2_hs_in_ep_desc,
#ifndef FSG_NO_INTR_EP
	(struct usb_descriptor_header *) &fsg_hs_intr_in_desc,
#endif
	NULL,
};

/* Maxpacket and other transfer characteristics vary by speed. */
static struct usb_endpoint_descriptor *
fsg_ep_desc(struct usb_gadget *g, struct usb_endpoint_descriptor *fs,
		struct usb_endpoint_descriptor *hs)
{
	if (gadget_is_dualspeed(g) && g->speed == USB_SPEED_HIGH)
		return hs;
	return fs;
}


/* Static strings, in UTF-8 (for simplicity we use only ASCII characters) */
static struct usb_string		fsg_strings[] = {
#ifndef FSG_NO_DEVICE_STRINGS
	{FSG_STRING_MANUFACTURER,	fsg_string_manufacturer},
	{FSG_STRING_PRODUCT,		fsg_string_product},
	{FSG_STRING_SERIAL,		fsg_string_serial},
	{FSG_STRING_CONFIG,		fsg_string_config},
#endif
	{FSG_STRING_INTERFACE,		fsg_string_interface},
	{}
};

static struct usb_gadget_strings	fsg_stringtab = {
	.language	= 0x0409,		/* en-us */
	.strings	= fsg_strings,
};


 /*-------------------------------------------------------------------------*/

/*
 * If the next two routines are called while the gadget is registered,
 * the caller must own fsg->filesem for writing.
 */

static int fsg_lun_open(struct fsg_lun *curlun, const char *filename)
{
	int				ro;
	struct file			*filp = NULL;
	int				rc = -EINVAL;
	struct inode			*inode = NULL;
	loff_t				size;
	loff_t				num_sectors;
	loff_t				min_sectors;

	/* R/W if we can, R/O if we must */
	ro = curlun->initially_ro;
	if (!ro) {
		filp = filp_open(filename, O_RDWR | O_LARGEFILE, 0);
		if (-EROFS == PTR_ERR(filp))
			ro = 1;
	}
	if (ro)
		filp = filp_open(filename, O_RDONLY | O_LARGEFILE, 0);
	if (IS_ERR(filp)) {
		LINFO(curlun, "unable to open backing file: %s\n", filename);
		return PTR_ERR(filp);
	}

	if (!(filp->f_mode & FMODE_WRITE))
		ro = 1;

	if (filp->f_path.dentry)
		inode = filp->f_path.dentry->d_inode;
	if (inode && S_ISBLK(inode->i_mode)) {
		if (bdev_read_only(inode->i_bdev))
			ro = 1;
	} else if (!inode || !S_ISREG(inode->i_mode)) {
		LINFO(curlun, "invalid file type: %s\n", filename);
		goto out;
	}

	/*
	 * If we can't read the file, it's no good.
	 * If we can't write the file, use it read-only.
	 */
	if (!filp->f_op || !(filp->f_op->read || filp->f_op->aio_read)) {
		LINFO(curlun, "file not readable: %s\n", filename);
		goto out;
	}
	if (!(filp->f_op->write || filp->f_op->aio_write))
		ro = 1;

	size = i_size_read(inode->i_mapping->host);
	if (size < 0) {
		LINFO(curlun, "unable to find file size: %s\n", filename);
		rc = (int) size;
		goto out;
	}
	num_sectors = size >> 9;	/* File size in 512-byte blocks */
	min_sectors = 1;
	if (curlun->cdrom) {
		num_sectors &= ~3;	/* Reduce to a multiple of 2048 */
		min_sectors = 300*4;	/* Smallest track is 300 frames */
		if (num_sectors >= 256*60*75*4) {
			num_sectors = (256*60*75 - 1) * 4;
			LINFO(curlun, "file too big: %s\n", filename);
			LINFO(curlun, "using only first %d blocks\n",
					(int) num_sectors);
		}
	}
	if (num_sectors < min_sectors) {
		LINFO(curlun, "file too small: %s\n", filename);
		rc = -ETOOSMALL;
		goto out;
	}

	get_file(filp);
	curlun->ro = ro;
	curlun->filp = filp;
	curlun->file_length = size;
	curlun->num_sectors = num_sectors;
	LDBG(curlun, "open backing file: %s\n", filename);
	rc = 0;

out:
	filp_close(filp, current->files);
	return rc;
}


static void fsg_lun_close(struct fsg_lun *curlun)
{
	if (curlun->filp) {
		LDBG(curlun, "close backing file\n");
		fput(curlun->filp);
		curlun->filp = NULL;
	}
}


/*-------------------------------------------------------------------------*/

/*
 * Sync the file data, don't bother with the metadata.
 * This code was copied from fs/buffer.c:sys_fdatasync().
 */
static int fsg_lun_fsync_sub(struct fsg_lun *curlun)
{
	struct file	*filp = curlun->filp;

	if (curlun->ro || !filp)
		return 0;
	return vfs_fsync(filp, 1);
}

static void store_cdrom_address(u8 *dest, int msf, u32 addr)
{
	if (msf) {
		/* Convert to Minutes-Seconds-Frames */
		addr >>= 2;		/* Convert to 2048-byte frames */
		addr += 2*75;		/* Lead-in occupies 2 seconds */
		dest[3] = addr % 75;	/* Frames */
		addr /= 75;
		dest[2] = addr % 60;	/* Seconds */
		addr /= 60;
		dest[1] = addr;		/* Minutes */
		dest[0] = 0;		/* Reserved */
	} else {
		/* Absolute sector */
		put_unaligned_be32(addr, dest);
	}
}


/*-------------------------------------------------------------------------*/


static ssize_t fsg_show_ro(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	struct fsg_lun	*curlun = fsg_lun_from_dev(dev);

	return sprintf(buf, "%d\n", fsg_lun_is_open(curlun)
				  ? curlun->ro
				  : curlun->initially_ro);
}

static ssize_t fsg_show_nofua(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	struct fsg_lun	*curlun = fsg_lun_from_dev(dev);

	return sprintf(buf, "%u\n", curlun->nofua);
}

static ssize_t fsg_show_file(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	struct fsg_lun	*curlun = fsg_lun_from_dev(dev);
	struct rw_semaphore	*filesem = dev_get_drvdata(dev);
	char		*p;
	ssize_t		rc;

	down_read(filesem);
	if (fsg_lun_is_open(curlun)) {	/* Get the complete pathname */
		p = d_path(&curlun->filp->f_path, buf, PAGE_SIZE - 1);
		if (IS_ERR(p))
			rc = PTR_ERR(p);
		else {
			rc = strlen(p);
			memmove(buf, p, rc);
			buf[rc] = '\n';		/* Add a newline */
			buf[++rc] = 0;
		}
	} else {				/* No file, return 0 bytes */
		*buf = 0;
		rc = 0;
	}
	up_read(filesem);
	return rc;
}


static ssize_t fsg_store_ro(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t count)
{
	ssize_t		rc = count;
	struct fsg_lun	*curlun = fsg_lun_from_dev(dev);
	struct rw_semaphore	*filesem = dev_get_drvdata(dev);
	unsigned long	ro;

	if (strict_strtoul(buf, 2, &ro))
		return -EINVAL;

	/*
	 * Allow the write-enable status to change only while the
	 * backing file is closed.
	 */
	down_read(filesem);
	if (fsg_lun_is_open(curlun)) {
		LDBG(curlun, "read-only status change prevented\n");
		rc = -EBUSY;
	} else {
		curlun->ro = ro;
		curlun->initially_ro = ro;
		LDBG(curlun, "read-only status set to %d\n", curlun->ro);
	}
	up_read(filesem);
	return rc;
}

static ssize_t fsg_store_nofua(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	struct fsg_lun	*curlun = fsg_lun_from_dev(dev);
	unsigned long	nofua;

	if (strict_strtoul(buf, 2, &nofua))
		return -EINVAL;

	/* Sync data when switching from async mode to sync */
	if (!nofua && curlun->nofua)
		fsg_lun_fsync_sub(curlun);

	curlun->nofua = nofua;

	return count;
}

static ssize_t fsg_store_file(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct fsg_lun	*curlun = fsg_lun_from_dev(dev);
	struct rw_semaphore	*filesem = dev_get_drvdata(dev);
	int		rc = 0;

	if (curlun->prevent_medium_removal && fsg_lun_is_open(curlun)) {
		LDBG(curlun, "eject attempt prevented\n");
		return -EBUSY;				/* "Door is locked" */
	}

	/* Remove a trailing newline */
	if (count > 0 && buf[count-1] == '\n')
		((char *) buf)[count-1] = 0;		/* Ugh! */

	/* Eject current medium */
	down_write(filesem);
	if (fsg_lun_is_open(curlun)) {
		fsg_lun_close(curlun);
		curlun->unit_attention_data = SS_MEDIUM_NOT_PRESENT;
	}

	/* Load new medium */
	if (count > 0 && buf[0]) {
		rc = fsg_lun_open(curlun, buf);
		if (rc == 0)
			curlun->unit_attention_data =
					SS_NOT_READY_TO_READY_TRANSITION;
	}
	up_write(filesem);
	return (rc < 0 ? rc : count);
}
