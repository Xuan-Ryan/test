/*
 * mass_storage.c -- Mass Storage USB Gadget
 *
 * Copyright (C) 2003-2008 Alan Stern
 * Copyright (C) 2009 Samsung Electronics
 *                    Author: Michal Nazarewicz <m.nazarewicz@samsung.com>
 * All rights reserved.
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
 * The Mass Storage Gadget acts as a USB Mass Storage device,
 * appearing to the host as a disk drive or as a CD-ROM drive.  In
 * addition to providing an example of a genuinely useful gadget
 * driver for a USB device, it also illustrates a technique of
 * double-buffering for increased throughput.  Last but not least, it
 * gives an easy way to probe the behavior of the Mass Storage drivers
 * in a USB host.
 *
 * Since this file serves only administrative purposes and all the
 * business logic is implemented in f_mass_storage.* file.  Read
 * comments in this file for more detailed description.
 */


#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/list.h>
#include <linux/utsname.h>
#include <linux/usb/ch9.h>


/*-------------------------------------------------------------------------*/

#define DRIVER_DESC		"MCT camera"
#define DRIVER_VERSION		"2009/09/11"

/*-------------------------------------------------------------------------*/

#define HIDG_VENDOR_NUM		0x0711	/* XXX NetChip */
#define HIDG_PRODUCT_NUM	0xC600	/* Linux-USB HID gadget */

/*
 * kbuild is not very cooperative with respect to linking separately
 * compiled library objects into one module.  So for now we won't use
 * separate compilation ... ensuring init/exit sections work to shrink
 * the runtime footprint, and giving us at least some parts of what
 * a "gcc --combine ... part1.c part2.c part3.c ... " build would.
 */

#include "composite.c"
#include "usbstring.c"
#include "config.c"
#include "epautoconf.c"
#include "f_hid.c"
#include "f_mass_storage.c"
#include "mnspdef.h"

static struct hidg_func_descriptor my_hid_data = {
	.subclass		= 0, /* No subclass */
	.protocol		= 1, /* Keyboard */
	.report_length		= 8,
	.report_desc_length	= 63,
	.report_desc		= {
		0x05, 0x01,	/* USAGE_PAGE (Generic Desktop)	          */
		0x09, 0x06,	/* USAGE (Keyboard)                       */
		0xa1, 0x01,	/* COLLECTION (Application)               */
		0x05, 0x07,	/*   USAGE_PAGE (Keyboard)                */
		0x19, 0xe0,	/*   USAGE_MINIMUM (Keyboard LeftControl) */
		0x29, 0xe7,	/*   USAGE_MAXIMUM (Keyboard Right GUI)   */
		0x15, 0x00,	/*   LOGICAL_MINIMUM (0)                  */
		0x25, 0x01,	/*   LOGICAL_MAXIMUM (1)                  */
		0x75, 0x01,	/*   REPORT_SIZE (1)                      */
		0x95, 0x08,	/*   REPORT_COUNT (8)                     */
		0x81, 0x02,	/*   INPUT (Data,Var,Abs)                 */
		0x95, 0x01,	/*   REPORT_COUNT (1)                     */
		0x75, 0x08,	/*   REPORT_SIZE (8)                      */
		0x81, 0x03,	/*   INPUT (Cnst,Var,Abs)                 */
		0x95, 0x05,	/*   REPORT_COUNT (5)                     */
		0x75, 0x01,	/*   REPORT_SIZE (1)                      */
		0x05, 0x08,	/*   USAGE_PAGE (LEDs)                    */
		0x19, 0x01,	/*   USAGE_MINIMUM (Num Lock)             */
		0x29, 0x05,	/*   USAGE_MAXIMUM (Kana)                 */
		0x91, 0x02,	/*   OUTPUT (Data,Var,Abs)                */
		0x95, 0x01,	/*   REPORT_COUNT (1)                     */
		0x75, 0x03,	/*   REPORT_SIZE (3)                      */
		0x91, 0x03,	/*   OUTPUT (Cnst,Var,Abs)                */
		0x95, 0x06,	/*   REPORT_COUNT (6)                     */
		0x75, 0x08,	/*   REPORT_SIZE (8)                      */
		0x15, 0x00,	/*   LOGICAL_MINIMUM (0)                  */
		0x25, 0x65,	/*   LOGICAL_MAXIMUM (101)                */
		0x05, 0x07,	/*   USAGE_PAGE (Keyboard)                */
		0x19, 0x00,	/*   USAGE_MINIMUM (Reserved)             */
		0x29, 0x65,	/*   USAGE_MAXIMUM (Keyboard Application) */
		0x81, 0x00,	/*   INPUT (Data,Ary,Abs)                 */
		0xc0		/* END_COLLECTION                         */
	}
};

static struct platform_device my_hid = {
	.name			= "hidg",
	.id			= 0,
	.num_resources		= 0,
	.resource		= 0,
	.dev.platform_data	= &my_hid_data,
};

struct hidg_func_node {
	struct list_head node;
	struct hidg_func_descriptor *func;
};

static LIST_HEAD(hidg_func_list);

static struct usb_device_descriptor device_desc = {
	.bLength =		sizeof device_desc,
	.bDescriptorType =	USB_DT_DEVICE,

	.bcdUSB =		cpu_to_le16(0x0200),

	/* .bDeviceClass =		USB_CLASS_COMM, */
	/* .bDeviceSubClass =	0, */
	/* .bDeviceProtocol =	0, */
	.bDeviceClass =		0x00,
	.bDeviceSubClass =	0,
	.bDeviceProtocol =	0,
	.bMaxPacketSize0 = 0x40,

	/* Vendor and product id can be overridden by module parameters.  */
	.idVendor =		cpu_to_le16(HIDG_VENDOR_NUM),
	.idProduct =		cpu_to_le16(HIDG_PRODUCT_NUM),
	.bcdDevice = cpu_to_le16(0x1010),
	.iManufacturer = 0x01,
	.iProduct = 0x02,
	/* NO SERIAL NUMBER */
	.bNumConfigurations =	1,
};

/*-------------------------------------------------------------------------*/

static struct usb_device_descriptor msg_device_desc = {
	.bLength =		sizeof msg_device_desc,
	.bDescriptorType =	USB_DT_DEVICE,

	.bcdUSB =		cpu_to_le16(0x0200),
	.bDeviceClass =		0xEF,//USB_CLASS_PER_INTERFACE,
	.bDeviceSubClass =	2,
	.bDeviceProtocol =	1,
	.bMaxPacketSize0 = 0x40,
	/* Vendor and product id can be overridden by module parameters.  */
	.idVendor =		cpu_to_le16(FSG_VENDOR_ID),
	.idProduct =		cpu_to_le16(FSG_PRODUCT_ID),
	.bcdDevice = 0x1010,
	/* .iManufacturer = DYNAMIC */
	/* .iProduct = DYNAMIC */
	/* NO SERIAL NUMBER */
	.bNumConfigurations =	1,
};

static struct usb_otg_descriptor otg_descriptor = {
	.bLength =		sizeof otg_descriptor,
	.bDescriptorType =	USB_DT_OTG,

	/* REVISIT SRP-only hardware is possible, although
	 * it would not be called "OTG" ...
	 */
	.bmAttributes =		USB_OTG_SRP | USB_OTG_HNP,
};

static const struct usb_descriptor_header *otg_desc[] = {
	(struct usb_descriptor_header *) &otg_descriptor,
	NULL,
};


/* string IDs are assigned dynamically */

#define STRING_MANUFACTURER_IDX		0
#define STRING_PRODUCT_IDX		1
#define STRING_CONFIGURATION_IDX	2
#define STRING_MICROSOFT_OS0_IDX	238
#define STRING_MICROSOFT_OS1_IDX	239

static char manufacturer[50] = "MCT Corp.";
static char product[50] = DRIVER_DESC;

static struct usb_string strings_dev[] = {
	[STRING_MANUFACTURER_IDX].s = manufacturer,
	[STRING_PRODUCT_IDX].s = product,
	[STRING_CONFIGURATION_IDX].s = "Self Powered",
	[3].s = "MCT UVC Gadget",
	[4].s = "MCT UVC Camera",
	{  } /* end of list */
};

static struct usb_gadget_strings stringtab_dev = {
	.language	= 0x0409,	/* en-us */
	.strings	= strings_dev,
};

static struct usb_gadget_strings *dev_strings[] = {
	&stringtab_dev,
	NULL,
};



/****************************** Configurations ******************************/

static struct fsg_module_parameters mod_data = {
	.stall = 1
};
FSG_MODULE_PARAMETERS(/* no prefix */, mod_data);

static unsigned long msg_registered = 0;
static void msg_cleanup(void);

static int msg_thread_exits(struct fsg_common *common)
{
	msg_cleanup();
	return 0;
}

static int __ref msg_do_config(struct usb_configuration *c)
{
	static const struct fsg_operations ops = {
		.thread_exits = msg_thread_exits,
	};
	static struct fsg_common common;
printk("%s, %d\n", __FUNCTION__, __LINE__);
	struct fsg_common *retp;
	struct fsg_config config;
	int ret;

	if (gadget_is_otg(c->cdev->gadget)) {
		c->descriptors = otg_desc;
		c->bmAttributes |= USB_CONFIG_ATT_WAKEUP;
	}

	fsg_config_from_params(&config, &mod_data);
	config.ops = &ops;

	retp = fsg_common_init(&common, c->cdev, &config);
	if (IS_ERR(retp))
		return PTR_ERR(retp);

	ret = fsg_bind_config(c->cdev, c, &common);
	fsg_common_put(&common);
	return ret;
}

static struct usb_configuration msg_config_driver = {
	.label			= "Linux File-Backed Storage",
	.bind			= msg_do_config,
	.bConfigurationValue	= 1,
	/* .iConfiguration = DYNAMIC */
	.bmAttributes		= USB_CONFIG_ATT_SELFPOWER,
};

static int __ref do_config(struct usb_configuration *c)
{
	struct hidg_func_node *e;
	int func = 0, status = 0;

	if (gadget_is_otg(c->cdev->gadget)) {
		c->descriptors = otg_desc;
		c->bmAttributes |= USB_CONFIG_ATT_WAKEUP;
	}

	list_for_each_entry(e, &hidg_func_list, node) {
		status = hidg_bind_config(c, e->func, func++);
		if (status)
			break;
	}

	return status;
}

static struct usb_configuration config_driver = {
	.label			= "HID Gadget",
	.bind			= do_config,
	.bConfigurationValue	= 1,
	/* .iConfiguration = DYNAMIC */
	.bmAttributes		= USB_CONFIG_ATT_SELFPOWER,
};

static int __ref hid_bind(struct usb_composite_dev *cdev)
{
	struct usb_gadget *gadget = cdev->gadget;
	struct list_head *tmp;
	int status, gcnum, funcs = 0;

	list_for_each(tmp, &hidg_func_list)
		funcs++;

	if (!funcs)
		return -ENODEV;

	/* set up HID */
	status = ghid_setup(cdev->gadget, funcs);
	if (status < 0)
		return status;

	gcnum = usb_gadget_controller_number(gadget);
	if (gcnum >= 0)
		device_desc.bcdDevice = cpu_to_le16(0x0300 | gcnum);
	else
		device_desc.bcdDevice = cpu_to_le16(0x0300 | 0x0099);


	/* Allocate string descriptor numbers ... note that string
	 * contents can be overridden by the composite_dev glue.
	 */

	/* device descriptor strings: manufacturer, product */
	snprintf(manufacturer, sizeof manufacturer, "%s %s with %s",
		init_utsname()->sysname, init_utsname()->release,
		gadget->name);
	status = usb_string_id(cdev);
	if (status < 0)
		return status;
	strings_dev[STRING_MANUFACTURER_IDX].id = status;
	device_desc.iManufacturer = status;

	status = usb_string_id(cdev);
	if (status < 0)
		return status;
	strings_dev[STRING_PRODUCT_IDX].id = status;
	device_desc.iProduct = status;

	/* register our configuration */
	status = usb_add_config(cdev, &config_driver);
	if (status < 0)
		return status;

	dev_info(&gadget->dev, DRIVER_DESC ", version: " DRIVER_VERSION "\n");

	return 0;
}


/****************************** Gadget Bind ******************************/

static int __ref msg_bind(struct usb_composite_dev *cdev)
{
	struct usb_gadget *gadget = cdev->gadget;
	dev_t			dev;
	struct usb_ep		*ep;
	int status;

	g_hidg = kzalloc(sizeof *g_hidg, GFP_KERNEL);
	if (!g_hidg)
		return -ENOMEM;

	ep = usb_ep_autoconfig(gadget, &hidg0_hs_in_ep_desc);
	ep = usb_ep_autoconfig(gadget, &hidg1_hs_in_ep_desc);
	ep = usb_ep_autoconfig(gadget, &hidg2_hs_in_ep_desc);
	ep->driver_data = &config_driver.cdev;	/* claim */
	g_hidg->in_ep = ep;

	g_hidg->req = usb_ep_alloc_request(g_hidg->in_ep, GFP_KERNEL);
	if (!g_hidg->req)
		return -ENOMEM;
	g_hidg->req->buf = kmalloc(HID_BUFLEN, GFP_KERNEL);
	if (!g_hidg->req->buf)
		return -ENOMEM;

	mutex_init(&g_hidg->lock);
	spin_lock_init(&g_hidg->spinlock);
	init_waitqueue_head(&g_hidg->write_queue);
	init_waitqueue_head(&g_hidg->read_queue);
	
	cdev_init(&g_hidg->cdev, &f_hidg_fops);
	dev = MKDEV(major, g_hidg->minor);
	status = cdev_add(&g_hidg->cdev, dev, 1);
	if (status)
		return status;

	device_create(hidg_class, NULL, dev, NULL, "%s%d", "g_hidg", g_hidg->minor);
	
	/* Allocate string descriptor numbers ... note that string
	 * contents can be overridden by the composite_dev glue.
	 */

	/* device descriptor strings: manufacturer, product */
	//snprintf(manufacturer, sizeof manufacturer, "%s %s with %s",
	//         init_utsname()->sysname, init_utsname()->release,
	//         gadget->name);
	status = usb_string_id(cdev);
	if (status < 0)
		return status;
	strings_dev[STRING_MANUFACTURER_IDX].id = status;
	msg_device_desc.iManufacturer = status;

	status = usb_string_id(cdev);
	if (status < 0)
		return status;
	strings_dev[STRING_PRODUCT_IDX].id = status;
	msg_device_desc.iProduct = status;

	status = usb_string_id(cdev);
	if (status < 0)
		return status;
	strings_dev[STRING_CONFIGURATION_IDX].id = status;
	msg_config_driver.iConfiguration = status;

	strings_dev[3].id = STRING_MICROSOFT_OS0_IDX;
	strings_dev[4].id = STRING_MICROSOFT_OS1_IDX;

	/* register our second configuration */
	status = usb_add_config(cdev, &msg_config_driver);
	if (status < 0)
		return status;

	dev_info(&gadget->dev, DRIVER_DESC ", version: " DRIVER_VERSION "\n");
	set_bit(0, &msg_registered);

	//hid_bind(cdev);

	return 0;
}


/****************************** Some noise ******************************/

static int __init hidg_plat_driver_probe(struct platform_device *pdev)
{
	struct hidg_func_descriptor *func = pdev->dev.platform_data;
	struct hidg_func_node *entry;

	if (!func) {
		dev_err(&pdev->dev, "Platform data missing\n");
		return -ENODEV;
	}

	entry = kzalloc(sizeof(*entry), GFP_KERNEL);
	if (!entry)
		return -ENOMEM;

	entry->func = func;
	list_add_tail(&entry->node, &hidg_func_list);

	return 0;
}

static int __devexit hidg_plat_driver_remove(struct platform_device *pdev)
{
	struct hidg_func_node *e, *n;

	list_for_each_entry_safe(e, n, &hidg_func_list, node) {
		list_del(&e->node);
		kfree(e);
	}

	return 0;
}

static struct usb_composite_driver msg_driver = {
	.name		= "g_mass_storage",
	.dev		= &msg_device_desc,
	.strings	= dev_strings,
	.bind		= msg_bind,
};

static struct platform_driver hidg_plat_driver = {
	.remove		= __devexit_p(hidg_plat_driver_remove),
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "hidg",
	},
};

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_AUTHOR("Michal Nazarewicz");
MODULE_LICENSE("GPL");

static int __init msg_init(void)
{
	//int status;

	//platform_device_register(&my_hid);
	//open_1280x720JPG(&UVC_Buff[2], 97549);
	//status = platform_driver_probe(&hidg_plat_driver,
	//			hidg_plat_driver_probe);
	//printk("Louis 1\n");
	//if (status < 0)
	//	return status;
	//printk("Louis 2\n");	
	return usb_composite_register(&msg_driver);
}
module_init(msg_init);

static void msg_cleanup(void)
{
	//platform_driver_unregister(&hidg_plat_driver);
	kfree(g_hidg->req->buf);
	usb_ep_free_request(g_hidg->in_ep, g_hidg->req);
	kfree(g_hidg);
	
	if (test_and_clear_bit(0, &msg_registered))
		usb_composite_unregister(&msg_driver);
}
module_exit(msg_cleanup);
