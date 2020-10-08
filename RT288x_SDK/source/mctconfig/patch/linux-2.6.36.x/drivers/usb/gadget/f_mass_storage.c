/*
 * f_mass_storage.c -- Mass Storage USB Composite Function
 *
 * Copyright (C) 2003-2008 Alan Stern
 * Copyright (C) 2009 Samsung Electronics
 *                    Author: Michal Nazarewicz <m.nazarewicz@samsung.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The names of the above-listed copyright holders may not be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * ALTERNATIVELY, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") as published by the Free Software
 * Foundation, either version 2 of that License or (at your option) any
 * later version.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


/*
 * The Mass Storage Function acts as a USB Mass Storage device,
 * appearing to the host as a disk drive or as a CD-ROM drive.  In
 * addition to providing an example of a genuinely useful composite
 * function for a USB device, it also illustrates a technique of
 * double-buffering for increased throughput.
 *
 * Function supports multiple logical units (LUNs).  Backing storage
 * for each LUN is provided by a regular file or a block device.
 * Access for each LUN can be limited to read-only.  Moreover, the
 * function can indicate that LUN is removable and/or CD-ROM.  (The
 * later implies read-only access.)
 *
 * MSF is configured by specifying a fsg_config structure.  It has the
 * following fields:
 *
 *	nluns		Number of LUNs function have (anywhere from 1
 *				to FSG_MAX_LUNS which is 8).
 *	luns		An array of LUN configuration values.  This
 *				should be filled for each LUN that
 *				function will include (ie. for "nluns"
 *				LUNs).  Each element of the array has
 *				the following fields:
 *	->filename	The path to the backing file for the LUN.
 *				Required if LUN is not marked as
 *				removable.
 *	->ro		Flag specifying access to the LUN shall be
 *				read-only.  This is implied if CD-ROM
 *				emulation is enabled as well as when
 *				it was impossible to open "filename"
 *				in R/W mode.
 *	->removable	Flag specifying that LUN shall be indicated as
 *				being removable.
 *	->cdrom		Flag specifying that LUN shall be reported as
 *				being a CD-ROM.
 *
 *	lun_name_format	A printf-like format for names of the LUN
 *				devices.  This determines how the
 *				directory in sysfs will be named.
 *				Unless you are using several MSFs in
 *				a single gadget (as opposed to single
 *				MSF in many configurations) you may
 *				leave it as NULL (in which case
 *				"lun%d" will be used).  In the format
 *				you can use "%d" to index LUNs for
 *				MSF's with more than one LUN.  (Beware
 *				that there is only one integer given
 *				as an argument for the format and
 *				specifying invalid format may cause
 *				unspecified behaviour.)
 *	thread_name	Name of the kernel thread process used by the
 *				MSF.  You can safely set it to NULL
 *				(in which case default "file-storage"
 *				will be used).
 *
 *	vendor_name
 *	product_name
 *	release		Information used as a reply to INQUIRY
 *				request.  To use default set to NULL,
 *				NULL, 0xffff respectively.  The first
 *				field should be 8 and the second 16
 *				characters or less.
 *
 *	can_stall	Set to permit function to halt bulk endpoints.
 *				Disabled on some USB devices known not
 *				to work correctly.  You should set it
 *				to true.
 *
 * If "removable" is not set for a LUN then a backing file must be
 * specified.  If it is set, then NULL filename means the LUN's medium
 * is not loaded (an empty string as "filename" in the fsg_config
 * structure causes error).  The CD-ROM emulation includes a single
 * data track and no audio tracks; hence there need be only one
 * backing file per LUN.  Note also that the CD-ROM block length is
 * set to 512 rather than the more common value 2048.
 *
 *
 * MSF includes support for module parameters.  If gadget using it
 * decides to use it, the following module parameters will be
 * available:
 *
 *	file=filename[,filename...]
 *			Names of the files or block devices used for
 *				backing storage.
 *	ro=b[,b...]	Default false, boolean for read-only access.
 *	removable=b[,b...]
 *			Default true, boolean for removable media.
 *	cdrom=b[,b...]	Default false, boolean for whether to emulate
 *				a CD-ROM drive.
 *	luns=N		Default N = number of filenames, number of
 *				LUNs to support.
 *	stall		Default determined according to the type of
 *				USB device controller (usually true),
 *				boolean to permit the driver to halt
 *				bulk endpoints.
 *
 * The module parameters may be prefixed with some string.  You need
 * to consult gadget's documentation or source to verify whether it is
 * using those module parameters and if it does what are the prefixes
 * (look for FSG_MODULE_PARAMETERS() macro usage, what's inside it is
 * the prefix).
 *
 *
 * Requirements are modest; only a bulk-in and a bulk-out endpoint are
 * needed.  The memory requirement amounts to two 16K buffers, size
 * configurable by a parameter.  Support is included for both
 * full-speed and high-speed operation.
 *
 * Note that the driver is slightly non-portable in that it assumes a
 * single memory/DMA buffer will be useable for bulk-in, bulk-out, and
 * interrupt-in endpoints.  With most device controllers this isn't an
 * issue, but there may be some with hardware restrictions that prevent
 * a buffer from being used by more than one endpoint.
 *
 *
 * The pathnames of the backing files and the ro settings are
 * available in the attribute files "file" and "ro" in the lun<n> (or
 * to be more precise in a directory which name comes from
 * "lun_name_format" option!) subdirectory of the gadget's sysfs
 * directory.  If the "removable" option is set, writing to these
 * files will simulate ejecting/loading the medium (writing an empty
 * line means eject) and adjusting a write-enable tab.  Changes to the
 * ro setting are not allowed when the medium is loaded or if CD-ROM
 * emulation is being used.
 *
 * When a LUN receive an "eject" SCSI request (Start/Stop Unit),
 * if the LUN is removable, the backing file is released to simulate
 * ejection.
 *
 *
 * This function is heavily based on "File-backed Storage Gadget" by
 * Alan Stern which in turn is heavily based on "Gadget Zero" by David
 * Brownell.  The driver's SCSI command interface was based on the
 * "Information technology - Small Computer System Interface - 2"
 * document from X3T9.2 Project 375D, Revision 10L, 7-SEP-93,
 * available at <http://www.t10.org/ftp/t10/drafts/s2/s2-r10l.pdf>.
 * The single exception is opcode 0x23 (READ FORMAT CAPACITIES), which
 * was based on the "Universal Serial Bus Mass Storage Class UFI
 * Command Specification" document, Revision 1.0, December 14, 1998,
 * available at
 * <http://www.usb.org/developers/devclass_docs/usbmass-ufi10.pdf>.
 */


/*
 *				Driver Design
 *
 * The MSF is fairly straightforward.  There is a main kernel
 * thread that handles most of the work.  Interrupt routines field
 * callbacks from the controller driver: bulk- and interrupt-request
 * completion notifications, endpoint-0 events, and disconnect events.
 * Completion events are passed to the main thread by wakeup calls.  Many
 * ep0 requests are handled at interrupt time, but SetInterface,
 * SetConfiguration, and device reset requests are forwarded to the
 * thread in the form of "exceptions" using SIGUSR1 signals (since they
 * should interrupt any ongoing file I/O operations).
 *
 * The thread's main routine implements the standard command/data/status
 * parts of a SCSI interaction.  It and its subroutines are full of tests
 * for pending signals/exceptions -- all this polling is necessary since
 * the kernel has no setjmp/longjmp equivalents.  (Maybe this is an
 * indication that the driver really wants to be running in userspace.)
 * An important point is that so long as the thread is alive it keeps an
 * open reference to the backing file.  This will prevent unmounting
 * the backing file's underlying filesystem and could cause problems
 * during system shutdown, for example.  To prevent such problems, the
 * thread catches INT, TERM, and KILL signals and converts them into
 * an EXIT exception.
 *
 * In normal operation the main thread is started during the gadget's
 * fsg_bind() callback and stopped during fsg_unbind().  But it can
 * also exit when it receives a signal, and there's no point leaving
 * the gadget running when the thread is dead.  At of this moment, MSF
 * provides no way to deregister the gadget when thread dies -- maybe
 * a callback functions is needed.
 *
 * To provide maximum throughput, the driver uses a circular pipeline of
 * buffer heads (struct fsg_buffhd).  In principle the pipeline can be
 * arbitrarily long; in practice the benefits don't justify having more
 * than 2 stages (i.e., double buffering).  But it helps to think of the
 * pipeline as being a long one.  Each buffer head contains a bulk-in and
 * a bulk-out request pointer (since the buffer can be used for both
 * output and input -- directions always are given from the host's
 * point of view) as well as a pointer to the buffer and various state
 * variables.
 *
 * Use of the pipeline follows a simple protocol.  There is a variable
 * (fsg->next_buffhd_to_fill) that points to the next buffer head to use.
 * At any time that buffer head may still be in use from an earlier
 * request, so each buffer head has a state variable indicating whether
 * it is EMPTY, FULL, or BUSY.  Typical use involves waiting for the
 * buffer head to be EMPTY, filling the buffer either by file I/O or by
 * USB I/O (during which the buffer head is BUSY), and marking the buffer
 * head FULL when the I/O is complete.  Then the buffer will be emptied
 * (again possibly by USB I/O, during which it is marked BUSY) and
 * finally marked EMPTY again (possibly by a completion routine).
 *
 * A module parameter tells the driver to avoid stalling the bulk
 * endpoints wherever the transport specification allows.  This is
 * necessary for some UDCs like the SuperH, which cannot reliably clear a
 * halt on a bulk endpoint.  However, under certain circumstances the
 * Bulk-only specification requires a stall.  In such cases the driver
 * will halt the endpoint and set a flag indicating that it should clear
 * the halt in software during the next device reset.  Hopefully this
 * will permit everything to work correctly.  Furthermore, although the
 * specification allows the bulk-out endpoint to halt when the host sends
 * too much data, implementing this would cause an unavoidable race.
 * The driver will always use the "no-stall" approach for OUT transfers.
 *
 * One subtle point concerns sending status-stage responses for ep0
 * requests.  Some of these requests, such as device reset, can involve
 * interrupting an ongoing file I/O operation, which might take an
 * arbitrarily long time.  During that delay the host might give up on
 * the original ep0 request and issue a new one.  When that happens the
 * driver should not notify the host about completion of the original
 * request, as the host will no longer be waiting for it.  So the driver
 * assigns to each ep0 request a unique tag, and it keeps track of the
 * tag value of the request associated with a long-running exception
 * (device-reset, interface-change, or configuration-change).  When the
 * exception handler is finished, the status-stage response is submitted
 * only if the current ep0 request tag is equal to the exception request
 * tag.  Thus only the most recently received ep0 request will get a
 * status-stage response.
 *
 * Warning: This driver source file is too long.  It ought to be split up
 * into a header file plus about 3 separate .c files, to handle the details
 * of the Gadget, USB Mass Storage, and SCSI protocols.
 */


/* #define VERBOSE_DEBUG */
/* #define DUMP_MSGS */


#include <linux/blkdev.h>
#include <linux/completion.h>
#include <linux/dcache.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fcntl.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/kref.h>
#include <linux/kthread.h>
#include <linux/limits.h>
#include <linux/rwsem.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/freezer.h>
#include <linux/utsname.h>

#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>

#include "gadget_chips.h"

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>

#include <linux/net.h>
#include <net/sock.h>
#include <linux/tcp.h>
#include <linux/in.h>
#include <asm/uaccess.h>
#include <linux/socket.h>
#include <linux/slab.h>
#include "mnspdef.h"
#include <linux/inetdevice.h>
#include <linux/rtnetlink.h>
#include <linux/vmalloc.h>
/*------------------------------------------------------------------------*/

#define FSG_DRIVER_DESC		"Mass Storage Function"
#define FSG_DRIVER_VERSION	"2009/09/11"

static const char fsg_string_interface[] = "Mass Storage";


#define FSG_NO_INTR_EP 1
#define FSG_NO_DEVICE_STRINGS    1
#define FSG_NO_OTG               1
#define FSG_NO_INTR_EP           1

#include "storage_common.c"


/*-------------------------------------------------------------------------*/

struct fsg_dev;
struct fsg_common;

u8 CSW_dft_status;
/* FSF callback functions */
struct fsg_operations {
	/* Callback function to call when thread exits.  If no
	 * callback is set or it returns value lower then zero MSF
	 * will force eject all LUNs it operates on (including those
	 * marked as non-removable or with prevent_medium_removal flag
	 * set). */
	int (*thread_exits)(struct fsg_common *common);

	/* Called prior to ejection.  Negative return means error,
	 * zero means to continue with ejection, positive means not to
	 * eject. */
	int (*pre_eject)(struct fsg_common *common,
			 struct fsg_lun *lun, int num);
	/* Called after ejection.  Negative return means error, zero
	 * or positive is just a success. */
	int (*post_eject)(struct fsg_common *common,
			  struct fsg_lun *lun, int num);
};


/* Data shared by all the FSG instances. */
struct fsg_common {
	struct usb_gadget	*gadget;
	struct fsg_dev		*fsg, *new_fsg;
	wait_queue_head_t	fsg_wait;

	/* filesem protects: backing files in use */
	struct rw_semaphore	filesem;

	/* lock protects: state, all the req_busy's */
	spinlock_t		lock;

	struct usb_ep		*ep0;		/* Copy of gadget->ep0 */
	struct usb_request	*ep0req;	/* Copy of cdev->req */
	unsigned int		ep0_req_tag;

	struct fsg_buffhd	*next_buffhd_to_fill;
	struct fsg_buffhd	*next_buffhd_to_drain;
	struct fsg_buffhd	buffhds[FSG_NUM_BUFFERS];

	int			cmnd_size;
	u8			cmnd[MAX_COMMAND_SIZE];

	unsigned int		nluns;
	unsigned int		lun;
	struct fsg_lun		*luns;
	struct fsg_lun		*curlun;

	unsigned int		bulk_out_maxpacket;
	enum fsg_state		state;		/* For exception handling */
	unsigned int		exception_req_tag;

	enum data_direction	data_dir;
	u32			data_size;
	u32			data_size_from_cmnd;
	u32			tag;
	u32			residue;
	u32			usb_amount_left;
	u32         datatype;

	unsigned int		can_stall:1;
	unsigned int		free_storage_on_release:1;
	unsigned int		phase_error:1;
	unsigned int		short_packet_received:1;
	unsigned int		bad_lun_okay:1;
	unsigned int		running:1;

	int			thread_wakeup_needed;
	struct completion	thread_notifier;
	struct task_struct	*thread_task;

	/* Callback functions. */
	const struct fsg_operations	*ops;
	/* Gadget's private data. */
	void			*private_data;

	/* Vendor (8 chars), product (16 chars), release (4
	 * hexadecimal digits) and NUL byte */
	char inquiry_string[8 + 16 + 4 + 1];

	struct kref		ref;
};


struct fsg_config {
	unsigned nluns;
	struct fsg_lun_config {
		const char *filename;
		char ro;
		char removable;
		char cdrom;
	} luns[FSG_MAX_LUNS];

	const char		*lun_name_format;
	const char		*thread_name;

	/* Callback functions. */
	const struct fsg_operations	*ops;
	/* Gadget's private data. */
	void			*private_data;

	const char *vendor_name;		/*  8 characters or less */
	const char *product_name;		/* 16 characters or less */
	u16 release;

	char			can_stall;
};


struct fsg_dev {
	struct usb_function	function;
	struct usb_gadget	*gadget;	/* Copy of cdev->gadget */
	struct fsg_common	*common;

	u16			interface_number;

	unsigned int		bulk_in_enabled:1;
	unsigned int		bulk_out_enabled:1;

	unsigned long		atomic_bitflags;
#define IGNORE_BULK_OUT		0

	struct usb_ep		*bulk_in;
	struct usb_ep		*bulk_out;
};


static inline int __fsg_is_set(struct fsg_common *common,
			       const char *func, unsigned line)
{
	if (common->fsg)
		return 1;
	ERROR(common, "common->fsg is NULL in %s at %u\n", func, line);
	WARN_ON(1);
	return 0;
}

#define fsg_is_set(common) likely(__fsg_is_set(common, __func__, __LINE__))


static inline struct fsg_dev *fsg_from_func(struct usb_function *f)
{
	return container_of(f, struct fsg_dev, function);
}


typedef void (*fsg_routine_t)(struct fsg_dev *);

static int exception_in_progress(struct fsg_common *common)
{
	return common->state > FSG_STATE_IDLE;
}

/* Make bulk-out requests be divisible by the maxpacket size */
static void set_bulk_out_req_length(struct fsg_common *common,
		struct fsg_buffhd *bh, unsigned int length)
{
	unsigned int	rem;

	bh->bulk_out_intended_length = length;
	rem = length % common->bulk_out_maxpacket;
	if (rem > 0)
		length += common->bulk_out_maxpacket - rem;
	bh->outreq->length = length;
}

/*-------------------------------------------------------------------------*/

static int fsg_set_halt(struct fsg_dev *fsg, struct usb_ep *ep)
{
	const char	*name;

	if (ep == fsg->bulk_in)
		name = "bulk-in";
	else if (ep == fsg->bulk_out)
		name = "bulk-out";
	else
		name = ep->name;
	DBG(fsg, "%s set halt\n", name);
	return usb_ep_set_halt(ep);
}


/*-------------------------------------------------------------------------*/

/* These routines may be called in process context or in_irq */

/* Caller must hold fsg->lock */
void wakeup_thread(struct fsg_common *common)
{
	/* Tell the main thread that something has happened */
	common->thread_wakeup_needed = 1;
	if (common->thread_task)
		wake_up_process(common->thread_task);
}


static void raise_exception(struct fsg_common *common, enum fsg_state new_state)
{
	unsigned long		flags;

	/* Do nothing if a higher-priority exception is already in progress.
	 * If a lower-or-equal priority exception is in progress, preempt it
	 * and notify the main thread by sending it a signal. */
	spin_lock_irqsave(&common->lock, flags);
	if (common->state <= new_state) {
		common->exception_req_tag = common->ep0_req_tag;
		common->state = new_state;
		if (common->thread_task)
			send_sig_info(SIGUSR1, SEND_SIG_FORCED,
				      common->thread_task);
	}
	spin_unlock_irqrestore(&common->lock, flags);
}


/*-------------------------------------------------------------------------*/

static int ep0_queue(struct fsg_common *common)
{
	int	rc;

	rc = usb_ep_queue(common->ep0, common->ep0req, GFP_ATOMIC);
	common->ep0->driver_data = common;
	if (rc != 0 && rc != -ESHUTDOWN) {
		/* We can't do much more than wait for a reset */
		WARNING(common, "error in submission: %s --> %d\n",
			common->ep0->name, rc);
	}
	return rc;
}

/*-------------------------------------------------------------------------*/

/* Bulk and interrupt endpoint completion handlers.
 * These always run in_irq. */
unsigned char g_incomplete;
static void bulk_in_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct fsg_common	*common = ep->driver_data;
	struct fsg_buffhd	*bh = req->context;

	//printk("bulk===___===in:%d\n", req->actual);
	if (req->status || req->actual != req->length)
		DBG(common, "%s --> %d, %u/%u\n", __func__,
				req->status, req->actual, req->length);
	if (req->status == -ECONNRESET)		/* Request was cancelled */
		usb_ep_fifo_flush(ep);

	/* Hold the lock while we update the request and buffer states */
	smp_wmb();
	spin_lock(&common->lock);
	bh->inreq_busy = 0;
	bh->state = BUF_STATE_EMPTY;
	//wakeup_thread(common);//for kernel
	g_incomplete = 1;
	wake_up(&gUVCwq);//for user
	spin_unlock(&common->lock);
}

static void bulk_out_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct fsg_common	*common = ep->driver_data;
	struct fsg_buffhd	*bh = req->context;

	//printk("bulk===___===out:%d\n", req->actual);
	if (req->status || req->actual != bh->bulk_out_intended_length)
		DBG(common, "%s --> %d, %u/%u\n", __func__,
				req->status, req->actual,
				bh->bulk_out_intended_length);
	if (req->status == -ECONNRESET)		/* Request was cancelled */
		usb_ep_fifo_flush(ep);

	/* Hold the lock while we update the request and buffer states */
	smp_wmb();
	spin_lock(&common->lock);
	bh->outreq_busy = 0;
	bh->state = BUF_STATE_FULL;
	//wakeup_thread(common);
	spin_unlock(&common->lock);
}

/*const unsigned char ReportDescHSMouse[] = {
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
};*/

/*-------------------------------------------------------------------------*/

/* Ep0 class-specific handlers.  These always run in_irq. */

static int fsg_setup(struct usb_function *f,
		const struct usb_ctrlrequest *ctrl)
{
	struct fsg_dev		*fsg = fsg_from_func(f);
	struct usb_request	*req = fsg->common->ep0req;
	u16			w_index = le16_to_cpu(ctrl->wIndex);
	u16			w_value = le16_to_cpu(ctrl->wValue);
	u16			w_length = le16_to_cpu(ctrl->wLength);
	int status = 0;
	printk("fsg_setup_0x%x\n", (ctrl->bRequest));
	if (!fsg_is_set(fsg->common))
		return -EOPNOTSUPP;

	switch (ctrl->bRequest) {

	case USB_BULK_RESET_REQUEST:
		if (ctrl->bRequestType !=
		    (USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE))
			break;
		if (w_index != fsg->interface_number || w_value != 0)
			return -EDOM;

		/* Raise an exception to stop the current operation
		 * and reinitialize our state. */
		DBG(fsg, "bulk reset request\n");
		raise_exception(fsg->common, FSG_STATE_RESET);
		return DELAYED_STATUS;

	case USB_BULK_GET_MAX_LUN_REQUEST:
		if (ctrl->bRequestType !=
		    (USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE))
			break;
		if (w_index != fsg->interface_number || w_value != 0)
			return -EDOM;
		VDBG(fsg, "get max LUN\n");
		*(u8 *) req->buf = fsg->common->nluns - 1;

		/* Respond with data/status */
		req->length = min((u16)1, w_length);
		return ep0_queue(fsg->common);
	case 0x0A:
		w_length = 0x00;
		goto respond;
		break;
	case 0x09:
		req->length = 1;
		return ep0_queue(fsg->common);
		break;
	case USB_REQ_GET_DESCRIPTOR:
		switch(ctrl->bRequestType)
		{
			case ((USB_DIR_IN | USB_TYPE_STANDARD | USB_RECIP_INTERFACE)):
				switch (w_value >> 8) {
				case HID_DT_REPORT:
					VDBG(fsg, "USB_REQ_GET_DESCRIPTOR: REPORT\n");
					switch(w_index)
					{
						case 1:
							w_length = 0x379;
							memcpy(req->buf, ReportDescHSMouse, w_length);
							break;
						case 2:
							w_length = 0x3e;
							memcpy(req->buf, ReportDescHSKeyboard, w_length);
							break;
						case 3:
							w_length = 0x65;
							memcpy(req->buf, ReportDescHSNone, w_length);
							break;
					}
					goto respond;
					break;

				default:
					VDBG(fsg, "Unknown decriptor request 0x%x\n",
						 w_value >> 8);
					goto stall;
					break;
				}

			break;
		}
		return;
	}

	VDBG(fsg,
	     "unknown class-specific control req "
	     "%02x.%02x v%04x i%04x l%u\n",
	     ctrl->bRequestType, ctrl->bRequest,
	     le16_to_cpu(ctrl->wValue), w_index, w_length);
	return -EOPNOTSUPP;
stall:
	return -EOPNOTSUPP;

respond:
	req->zero = 0;
	req->length = w_length;
	status = usb_ep_queue(fsg->common->ep0, req, GFP_ATOMIC);
	if (status < 0)
		ERROR(fsg, "usb_ep_queue error on ep0 %d\n", w_value);
	return status;
}


/*-------------------------------------------------------------------------*/

/* All the following routines run in process context */


/* Use this for bulk or interrupt transfers, not ep0 */
static void start_transfer(struct fsg_dev *fsg, struct usb_ep *ep,
		struct usb_request *req, int *pbusy,
		enum fsg_buffer_state *state)
{
	int	rc;

	if (ep == fsg->bulk_in)
	{
		//printk("bulk-in-req\n");
	}
	if (ep == fsg->bulk_out)
	{
		//printk("bulk-out-req\n");
	}

	spin_lock_irq(&fsg->common->lock);
	*pbusy = 1;
	*state = BUF_STATE_BUSY;
	spin_unlock_irq(&fsg->common->lock);
	rc = usb_ep_queue(ep, req, GFP_KERNEL);
	if (rc != 0) {
		*pbusy = 0;
		*state = BUF_STATE_EMPTY;

		/* We can't do much more than wait for a reset */

		/* Note: currently the net2280 driver fails zero-length
		 * submissions if DMA is enabled. */
		if (rc != -ESHUTDOWN && !(rc == -EOPNOTSUPP &&
						req->length == 0))
			WARNING(fsg, "error in submission: %s --> %d\n",
					ep->name, rc);
	}
}

#define START_TRANSFER_OR(common, ep_name, req, pbusy, state)		\
	if (fsg_is_set(common))						\
		start_transfer((common)->fsg, (common)->fsg->ep_name,	\
			       req, pbusy, state);			\
	else

#define START_TRANSFER(common, ep_name, req, pbusy, state)		\
	START_TRANSFER_OR(common, ep_name, req, pbusy, state) (void)0



static int sleep_thread(struct fsg_common *common)
{
	int	rc = 0;

	/* Wait until a signal arrives or we are woken up */
	for (;;) {
		try_to_freeze();
		set_current_state(TASK_INTERRUPTIBLE);
		if (signal_pending(current)) {
			rc = -EINTR;
			break;
		}
		if (common->thread_wakeup_needed)
			break;
		schedule();
	}
	__set_current_state(TASK_RUNNING);
	common->thread_wakeup_needed = 0;
	return rc;
}


/*-------------------------------------------------------------------------*/

static int do_read(struct fsg_common *common)
{
	struct fsg_lun		*curlun = common->curlun;
	u32			lba;
	struct fsg_buffhd	*bh;
	int			rc;
	u32			amount_left;
	loff_t			file_offset, file_offset_tmp;
	unsigned int		amount;
	unsigned int		partial_page;
	ssize_t			nread;

	/* Get the starting Logical Block Address and check that it's
	 * not too big */
	if (common->cmnd[0] == SC_READ_6)
		lba = get_unaligned_be24(&common->cmnd[1]);
	else {
		lba = get_unaligned_be32(&common->cmnd[2]);

		/* We allow DPO (Disable Page Out = don't save data in the
		 * cache) and FUA (Force Unit Access = don't read from the
		 * cache), but we don't implement them. */
		if ((common->cmnd[1] & ~0x18) != 0) {
			curlun->sense_data = SS_INVALID_FIELD_IN_CDB;
			return -EINVAL;
		}
	}
	if (lba >= curlun->num_sectors) {
		curlun->sense_data = SS_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE;
		return -EINVAL;
	}
	file_offset = ((loff_t) lba) << 9;

	/* Carry out the file reads */
	amount_left = common->data_size_from_cmnd;
	if (unlikely(amount_left == 0))
		return -EIO;		/* No default reply */

	for (;;) {

		/* Figure out how much we need to read:
		 * Try to read the remaining amount.
		 * But don't read more than the buffer size.
		 * And don't try to read past the end of the file.
		 * Finally, if we're not at a page boundary, don't read past
		 *	the next page.
		 * If this means reading 0 then we were asked to read past
		 *	the end of file. */
		amount = min(amount_left, FSG_BUFLEN);
		amount = min((loff_t) amount,
				curlun->file_length - file_offset);
		partial_page = file_offset & (PAGE_CACHE_SIZE - 1);
		if (partial_page > 0)
			amount = min(amount, (unsigned int) PAGE_CACHE_SIZE -
					partial_page);

		/* Wait for the next buffer to become available */
		bh = common->next_buffhd_to_fill;
		while (bh->state != BUF_STATE_EMPTY) {
			rc = sleep_thread(common);
			if (rc)
				return rc;
		}

		/* If we were asked to read past the end of file,
		 * end with an empty buffer. */
		if (amount == 0) {
			curlun->sense_data =
					SS_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE;
			curlun->sense_data_info = file_offset >> 9;
			curlun->info_valid = 1;
			bh->inreq->length = 0;
			bh->state = BUF_STATE_FULL;
			break;
		}

		/* Perform the read */
		file_offset_tmp = file_offset;
		nread = vfs_read(curlun->filp,
				(char __user *) bh->buf,
				amount, &file_offset_tmp);
		VLDBG(curlun, "file read %u @ %llu -> %d\n", amount,
				(unsigned long long) file_offset,
				(int) nread);
		if (signal_pending(current))
			return -EINTR;

		if (nread < 0) {
			LDBG(curlun, "error in file read: %d\n",
					(int) nread);
			nread = 0;
		} else if (nread < amount) {
			LDBG(curlun, "partial file read: %d/%u\n",
					(int) nread, amount);
			nread -= (nread & 511);	/* Round down to a block */
		}
		file_offset  += nread;
		amount_left  -= nread;
		common->residue -= nread;
		bh->inreq->length = nread;
		bh->state = BUF_STATE_FULL;

		/* If an error occurred, report it and its position */
		if (nread < amount) {
			curlun->sense_data = SS_UNRECOVERED_READ_ERROR;
			curlun->sense_data_info = file_offset >> 9;
			curlun->info_valid = 1;
			break;
		}

		if (amount_left == 0)
			break;		/* No more left to read */

		/* Send this buffer and go read some more */
		bh->inreq->zero = 0;
		START_TRANSFER_OR(common, bulk_in, bh->inreq,
			       &bh->inreq_busy, &bh->state)
			/* Don't know what to do if
			 * common->fsg is NULL */
			return -EIO;
		common->next_buffhd_to_fill = bh->next;
	}

	return -EIO;		/* No default reply */
}


/*-------------------------------------------------------------------------*/
struct usb_udp_pingpong {
	unsigned char* buf;
	struct usb_udp_pingpong* prev;
	struct usb_udp_pingpong* next;
	u32	status;
	u32 validblockcount;
};

enum pingpong_buf_status {
	PINGPONG_IDLE = 0,
	PINGPONG_BUSY = 1,
};

#define UDP_DATABLOCK_SIZE 52*1024
struct socket *g_Tconn_socket = NULL;
struct socket *g_Uconn_socket = NULL;
struct socket *g_UCURconn_socket = NULL;
struct socket *g_TCURconn_socket = NULL;
struct socket *g_TROMconn_socket = NULL;
struct sockaddr_in g_Tsaddr;
struct sockaddr_in g_Usaddr;
struct sockaddr_in g_UCURsaddr;
struct sockaddr_in g_TCURsaddr;
struct sockaddr_in g_TROMsaddr;
struct usb_udp_pingpong* pingpong[2];
struct usb_udp_pingpong* stable_bps_pingpong[3];
unsigned char g_destip[5] = {192,168,2,185,'\0'};
static unsigned int from_bc_uint_destip = 0;
static unsigned int from_rx_current_bps = 10*2*1024*1024;//2sec broadcast once
unsigned int udp_send_total_bytes = 0;
static unsigned int interval_total_bytes = 0;
static unsigned int prev_interval_total_bytes = 0;
struct usb_udp_pingpong* g_temp_bps_pingpong;
unsigned char *g_updatefwbuf;
module_param(from_bc_uint_destip, uint, S_IRWXU|S_IRWXG);
module_param(from_rx_current_bps, uint, S_IRWXU|S_IRWXG);
module_param(interval_total_bytes, uint, S_IRWXU|S_IRWXG);

int ksocket_send(struct socket *sock, struct sockaddr_in *addr, unsigned char *buf, int len)

{
      struct msghdr msg;
      struct iovec iov;

      mm_segment_t oldfs;
      int size = 0;

      if (sock->sk==NULL)
         return 0;

      iov.iov_base = buf;
      iov.iov_len = len;

      msg.msg_flags = 0;
      msg.msg_name = addr;

      msg.msg_namelen  = sizeof(struct sockaddr_in);
      msg.msg_control = NULL;

      msg.msg_controllen = 0;
      msg.msg_iov = &iov;

      msg.msg_iovlen = 1;
      msg.msg_control = NULL;

      oldfs = get_fs();
      set_fs(KERNEL_DS);

      size = sock_sendmsg(sock,&msg,len);
      set_fs(oldfs);

      return size;
}

int ksocket_receive(struct socket* sock, struct sockaddr_in* addr, unsigned char* buf, int len)

{
      struct msghdr msg;
      struct iovec iov;

      mm_segment_t oldfs;
      int size = 0;

      if (sock->sk==NULL) return 0;

      iov.iov_base = buf;
      iov.iov_len = len;

      msg.msg_flags = 0;
      msg.msg_name = addr;

      msg.msg_namelen  = sizeof(struct sockaddr_in);
      msg.msg_control = NULL;

      msg.msg_controllen = 0;
      msg.msg_iov = &iov;

      msg.msg_iovlen = 1;
      msg.msg_control = NULL;

      oldfs = get_fs();
      set_fs(KERNEL_DS);

      size = sock_recvmsg(sock,&msg,len,msg.msg_flags);

      set_fs(oldfs);

      return size;
}

unsigned char g_udp_bulk_XactId = 0;
unsigned char g_current_stall = 0;
uint64_t g_currentHtimestamp = 0;
uint64_t g_prevHtimestamp = 0;
int g_udpTransfer_flag = 0;

int open_1280x720JPG(unsigned char *buf, unsigned int len)
{
    struct file *fp;
    mm_segment_t fs;
    loff_t pos;
	int ret;
    printk("open jpg\n");
    fp =filp_open("/home/KArtboardbg0A.jpg",O_RDWR | O_CREAT,0666);
    if(IS_ERR(fp)){
        printk("create file error\n");
        return -1;
    }
    fs =get_fs();
    set_fs(KERNEL_DS);
    pos = 0;
	ret = vfs_read(fp,(char __user *)buf, len, &pos);
	printk("ret:%d\n",ret);
    filp_close(fp,NULL);
    set_fs(fs);
    return 0;
}

int store_FW_by_USB(unsigned char *buf, unsigned int len)
{
    struct file *fp;
    mm_segment_t fs;
    loff_t pos;
	int ret;
    printk("store fw\n");
    fp =filp_open("/var/usbFW",O_RDWR | O_CREAT,0666);
    if(IS_ERR(fp)){
        printk("create file error\n");
        return -1;
    }
    fs =get_fs();
    set_fs(KERNEL_DS);
    pos = 0;
	ret = vfs_write(fp,(char __user *)buf, len, &pos);
	printk("ret:%d\n",ret);
    filp_close(fp,NULL);
    set_fs(fs);
    return 0;
}

int update_FW_by_USB(unsigned int len)
{
#if 0
	char cmd[512];
	int result = 0;
	char *argv[] = {"/bin/mtd_write", cmd, NULL};
	static char *envp[] = {
	    "HOME=/",
	    "TERM=linux",
	    "PATH=/sbin:/bin:/usr/sbin:/usr/bin", NULL };

	snprintf(cmd, sizeof(cmd), "-o %d -l %d write %s Kernel", 0, len, "/var/usbFW");

	printk("Call user now \n");
	result = call_usermodehelper(argv[0], argv, envp, UMH_WAIT_PROC);
	printk("The result of call_usermodehelper is %d\n", result);  
	return 0;
#else
	struct file *fp;
    mm_segment_t fs;
    loff_t pos;
	int ret;
    printk("update fw\n");
    fp =filp_open("/var/updateFW/fw_len",O_RDWR | O_CREAT,0666);
    if(IS_ERR(fp)){
        printk("create file error\n");
        return -1;
    }
    fs =get_fs();
    set_fs(KERNEL_DS);
    pos = 0;
	ret = vfs_write(fp,(char __user *)&len, 4, &pos);
	printk("ret:%d\n",ret);
    filp_close(fp,NULL);
    set_fs(fs);
    return 0;
#endif
}

uint64_t GetTimeStamp() {
    struct timeval tv;
    do_gettimeofday(&tv);
    return tv.tv_sec*(uint64_t)1000000+tv.tv_usec;
}

u32 reg_read(u32 addr)
{
	return ioread32( (void __iomem *)0x0 + addr);
}

void reg_write(u32 addr, u32 value)
{
	iowrite32(value, (void __iomem *)0x0 + addr);
}

unsigned int g_a_offset = 0;
unsigned int g_v_offset = 0;
unsigned int g_led_net_send_total_block = 0;
static int do_write(struct fsg_common *common)
{
	struct fsg_lun		*curlun = common->curlun;
	u32			lba;
	struct fsg_buffhd	*bh;
	int			get_some_more;
	u32			amount_left_to_req, amount_left_to_write, udp_block_write;
	loff_t			usb_offset, file_offset, file_offset_tmp;
	unsigned int		amount;
	unsigned int		partial_page;
	ssize_t			nwritten;
	int			rc;
	int rxsucc, udp_send_size;
	MNSPXACTHDR udp_hdr;
	WIFIDONGLEROMHDR rom_hdr;
	UINT32 temp_rom_offset = 0;
	UINT32 temp_XactOffset = 0;
	u32 drop_frame = 0;
	u32 last_frame_block_count = 0;
	struct usb_udp_pingpong* temppingpong;
	temppingpong = pingpong[0];
	udp_block_write = UDP_DATABLOCK_SIZE - USB_BULK_CB_WRAP_LEN - sizeof(udp_hdr);
	//printk("do_write1\n");
	//if (curlun->ro) {
	//	curlun->sense_data = SS_WRITE_PROTECTED;
	//	return -EINVAL;
	//}
	//spin_lock(&curlun->filp->f_lock);
	//curlun->filp->f_flags &= ~O_SYNC;	/* Default is not to wait */
	//spin_unlock(&curlun->filp->f_lock);

	/* Get the starting Logical Block Address and check that it's
	 * not too big */
	//if (common->cmnd[0] == SC_WRITE_6)
	//	lba = get_unaligned_be24(&common->cmnd[1]);
	//else {
	//	lba = get_unaligned_be32(&common->cmnd[2]);

		/* We allow DPO (Disable Page Out = don't save data in the
		 * cache) and FUA (Force Unit Access = write directly to the
		 * medium).  We don't implement DPO; we implement FUA by
		 * performing synchronous output. */
	//	if (common->cmnd[1] & ~0x18) {
	//		curlun->sense_data = SS_INVALID_FIELD_IN_CDB;
	//		return -EINVAL;
	//	}
	//	if (common->cmnd[1] & 0x08) {	/* FUA */
	//		spin_lock(&curlun->filp->f_lock);
	//		curlun->filp->f_flags |= O_SYNC;
	//		spin_unlock(&curlun->filp->f_lock);
	//	}
	//}
	/*if (lba >= curlun->num_sectors) {
		curlun->sense_data = SS_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE;
		return -EINVAL;
	}

	/* Carry out the file writes */
	//get_some_more = 1;
	//file_offset = usb_offset = ((loff_t) lba) << 9;
	amount_left_to_req = common->data_size_from_cmnd;
	udp_hdr.Tag = MNSP_TAG;
	udp_hdr.XactType = MNSP_XACTTYPE_VIDEO;
	udp_hdr.HdrSize = sizeof(udp_hdr);
	udp_hdr.XactId = g_udp_bulk_XactId;
	udp_hdr.PayloadLength = (amount_left_to_req + USB_BULK_CB_WRAP_LEN)>(UDP_DATABLOCK_SIZE - sizeof(udp_hdr))?(UDP_DATABLOCK_SIZE - sizeof(udp_hdr)):(amount_left_to_req + USB_BULK_CB_WRAP_LEN);
	udp_hdr.TotalLength = amount_left_to_req + USB_BULK_CB_WRAP_LEN;
	udp_hdr.XactOffset = temp_XactOffset;
	if(common->datatype == 0x00)//video
	{
		memcpy(&temppingpong->buf[g_v_offset+0], &udp_hdr, sizeof(udp_hdr));
	}
	else if(common->datatype == 0x03)//audio
	{
		memcpy(&stable_bps_pingpong[0]->buf[g_a_offset+0], &udp_hdr, sizeof(udp_hdr));
	}
	else if(common->datatype == 0x05)//rom
	{
		
	}
	temp_XactOffset = temp_XactOffset + USB_BULK_CB_WRAP_LEN;
	g_udp_bulk_XactId++;
	//amount_left_to_write = common->data_size_from_cmnd;
	//printk("do_write12\n");
	while(amount_left_to_req)
	{
		bh = common->next_buffhd_to_fill;
		rxsucc = 1;
		if(bh->state == BUF_STATE_EMPTY)
		{
			rxsucc = 0;
		}
		else
		{
			rxsucc = 1;
		}
		while (rxsucc) {
			rc = sleep_thread(common);
			if (rc)
				return rc;
			if(bh->state == BUF_STATE_EMPTY)
			{
				rxsucc = 0;
			}
			else
			{
				rxsucc = 1;
			}
		}

		/* Queue a request to read a Bulk-only CBW */
		//set_bulk_out_req_length(common, bh, USB_BULK_CB_WRAP_LEN);
		if(amount_left_to_req > FSG_BUFLEN)
		{
			amount_left_to_write = FSG_BUFLEN;
		}
		else
		{
			amount_left_to_write = amount_left_to_req;
		}
		bh->outreq->length = amount_left_to_write;
		bh->outreq->short_not_ok = 1;
		//printk("I_DATA%d:%d\n",bh->outreq->length, bh->state);
		START_TRANSFER_OR(common, bulk_out, bh->outreq,
				  &bh->outreq_busy, &bh->state)
		/* Don't know what to do if common->fsg is NULL */
		//return -EIO;

		/* We will drain the buffer in software, which means we
		 * can reuse it for the next filling.  No need to advance
		 * next_buffhd_to_fill. */

		/* Wait for the CBW to arrive */
		//while (bh->state != BUF_STATE_FULL) {
		//	printk("123456\n");
		//	rc = sleep_thread(common);
		//	if (rc)
		//		return rc;
		//}
		rxsucc = 1;
		if(bh->state == BUF_STATE_FULL)
		{
			rxsucc = 0;
		}
		else
		{
			rxsucc = 1;
		}
		while (rxsucc) {
			//printk("123456\n");
			rc = sleep_thread(common);
			if (rc)
				return rc;
			if(bh->state == BUF_STATE_FULL)
			{
				rxsucc = 0;
			}
			else
			{
				rxsucc = 1;
			}
		}
		//printk("rc3:%d,%d\n",rc, bh->state);
		smp_rmb();
		//rc = fsg_is_set(common) ? received_cbw(common->fsg, bh) : -EIO;
		//=================
		//deal net transfer
		//len = sock_recvmsg(sock, &msg, max_size, 0);
		//hex_dump(bh->outreq->buf, 512);
		//printk("0_0x%x\n",g_Uconn_socket);
		//=================
		if(common->datatype == 0x05)//rom
		{
			if(temp_rom_offset == 0x40000)
			{
				memcpy(&g_updatefwbuf[USB_BULK_CB_WRAP_LEN + temp_rom_offset], bh->outreq->buf, amount_left_to_write);
				memcpy(&rom_hdr, bh->outreq->buf, sizeof(rom_hdr));
				temp_rom_offset = temp_rom_offset + amount_left_to_write;
				printk("dest:%d\n",rom_hdr.FW_for_dest);
				printk("rom size:%d\n",rom_hdr.PayloadLength);
			}
			else
			{
				memcpy(&g_updatefwbuf[USB_BULK_CB_WRAP_LEN + temp_rom_offset], bh->outreq->buf, amount_left_to_write);
				temp_rom_offset = temp_rom_offset + amount_left_to_write;				
				printk("left size:%d\n",amount_left_to_req);
			}
			amount_left_to_req = amount_left_to_req - amount_left_to_write;
		}

		if(udp_block_write >= amount_left_to_write)
		{
			if(common->datatype == 0x00)//video
			{
				memcpy(&temppingpong->buf[g_v_offset+UDP_DATABLOCK_SIZE-udp_block_write], bh->outreq->buf, amount_left_to_write);
				udp_block_write = udp_block_write - amount_left_to_write;
				temp_XactOffset = temp_XactOffset + amount_left_to_write;
				amount_left_to_req = amount_left_to_req - amount_left_to_write;
			}
			else if(common->datatype == 0x03)//audio
			{
				memcpy(&stable_bps_pingpong[0]->buf[g_a_offset+UDP_DATABLOCK_SIZE-udp_block_write], bh->outreq->buf, amount_left_to_write);
				udp_block_write = udp_block_write - amount_left_to_write;
				temp_XactOffset = temp_XactOffset + amount_left_to_write;
				amount_left_to_req = amount_left_to_req - amount_left_to_write;
			}
			else if(common->datatype == 0x05)//rom
			{

			}
		}
		else
		{
			memcpy(&temppingpong->buf[UDP_DATABLOCK_SIZE-udp_block_write], bh->outreq->buf, udp_block_write);
			temp_XactOffset = temp_XactOffset + udp_block_write;
			//printk("udp_hdr:%d, %d, %d\n", udp_hdr.PayloadLength, udp_hdr.XactOffset, common->datatype);
			if(g_udpTransfer_flag)
			{
				if(common->datatype == 0x00)//video
				{
					g_v_offset = g_v_offset + 26*1024;
					if(g_v_offset == 52*1024)
					{
						udp_send_size = ksocket_send(g_Uconn_socket, &g_Usaddr, temppingpong->buf, UDP_DATABLOCK_SIZE);
						g_v_offset = 0;
						g_led_net_send_total_block = g_led_net_send_total_block + udp_send_size;
					}
					//udp_send_size = ksocket_send(g_Uconn_socket, &g_Usaddr, temppingpong->buf, UDP_DATABLOCK_SIZE);
				}
				else if(common->datatype == 0x03)//audio
				{
					//memcpy(&stable_bps_pingpong[0]->buf[g_a_offset], temppingpong->buf, 2*1024);
					g_a_offset = g_a_offset + 2*1024;
					if(g_a_offset == 60*1024)
					{
						udp_send_size = ksocket_send(g_Uconn_socket, &g_Usaddr, stable_bps_pingpong[0]->buf, 60*1024);
						g_a_offset = 0;
						g_led_net_send_total_block = g_led_net_send_total_block + udp_send_size;
					}
					//udp_send_size = ksocket_send(g_Uconn_socket, &g_Usaddr, temppingpong->buf, 2*1024);
				}
				else if(common->datatype == 0x05)//rom
				{

				}
			}
			//printk("60K ready %lld\n", GetTimeStamp());
#if 0
			if(g_temp_bps_pingpong->status == PINGPONG_IDLE && drop_frame == 0)
			{
				memcpy(&g_temp_bps_pingpong->buf[g_temp_bps_pingpong->validblockcount*UDP_DATABLOCK_SIZE], temppingpong->buf, UDP_DATABLOCK_SIZE);
				g_temp_bps_pingpong->validblockcount++;
			}
			else
			{
				//reg_write(((0xB0120000) + 0x800), 0x3C000063);
				//usb_ep_set_halt(common->fsg->bulk_out);
				drop_frame = 1;
				g_temp_bps_pingpong->prev->status = PINGPONG_IDLE;
				memcpy(&g_temp_bps_pingpong->prev->buf[last_frame_block_count*UDP_DATABLOCK_SIZE], temppingpong->buf, UDP_DATABLOCK_SIZE);
				last_frame_block_count++;
				g_temp_bps_pingpong->prev->validblockcount = last_frame_block_count;
			}
#endif
			//printk("size:%d\n", udp_send_size);
			temppingpong = temppingpong->next;
			//udp_hdr.Tag = MNSP_TAG;
			//udp_hdr.XactType = MNSP_XACTTYPE_VIDEO;
			//udp_hdr.HdrSize = sizeof(udp_hdr);
			//udp_hdr.XactId = udp_hdr.XactId + 1;
			amount_left_to_req = amount_left_to_req - amount_left_to_write;
			udp_hdr.PayloadLength = (amount_left_to_req + amount_left_to_write - udp_block_write)>(UDP_DATABLOCK_SIZE - sizeof(udp_hdr))?(UDP_DATABLOCK_SIZE - sizeof(udp_hdr)):(amount_left_to_req + amount_left_to_write - udp_block_write);
			//udp_hdr.TotalLength = UDP_DATABLOCK_SIZE;
			udp_hdr.XactOffset = temp_XactOffset;
			//printk("temp_XactOffset1:%d, %d, %d\n", temp_XactOffset, amount_left_to_req, udp_block_write);
			memcpy(temppingpong->buf, &udp_hdr, sizeof(udp_hdr));
			memcpy(&temppingpong->buf[sizeof(udp_hdr)], &bh->outreq->buf[udp_block_write], amount_left_to_write - udp_block_write);
			temp_XactOffset = temp_XactOffset + amount_left_to_write - udp_block_write;
			udp_block_write = UDP_DATABLOCK_SIZE - amount_left_to_write + udp_block_write - sizeof(udp_hdr);
		}

		//amount_left_to_req = amount_left_to_req - amount_left_to_write;
		if(!udp_block_write || !amount_left_to_req)
		{
			//printk("udp_hdr:%d, %d, %d\n", udp_hdr.PayloadLength, udp_hdr.XactOffset, common->datatype);
			if(g_udpTransfer_flag)
			{
				if(common->datatype == 0x00)//video
				{
					g_v_offset = g_v_offset + 26*1024;
					if(g_v_offset == 52*1024)
					{
						udp_send_size = ksocket_send(g_Uconn_socket, &g_Usaddr, temppingpong->buf, UDP_DATABLOCK_SIZE);
						g_v_offset = 0;
						g_led_net_send_total_block = g_led_net_send_total_block + udp_send_size;
					}
					//udp_send_size = ksocket_send(g_Uconn_socket, &g_Usaddr, temppingpong->buf, UDP_DATABLOCK_SIZE);
				}
				else if(common->datatype == 0x03)//audio
				{
					//memcpy(&stable_bps_pingpong[0]->buf[g_a_offset], temppingpong->buf, 2*1024);
					g_a_offset = g_a_offset + 2*1024;
					if(g_a_offset == 60*1024)
					{
						udp_send_size = ksocket_send(g_Uconn_socket, &g_Usaddr, stable_bps_pingpong[0]->buf, 60*1024);
						g_a_offset = 0;
						g_led_net_send_total_block = g_led_net_send_total_block + udp_send_size;
					}
					//udp_send_size = ksocket_send(g_Uconn_socket, &g_Usaddr, temppingpong->buf, 2*1024);
				}
				else if(common->datatype == 0x05)//rom
				{

				}
			}
			//printk("60K ready %lld\n", GetTimeStamp());
#if 0
			if(g_temp_bps_pingpong->status == PINGPONG_IDLE && drop_frame == 0)
			{
				memcpy(&g_temp_bps_pingpong->buf[g_temp_bps_pingpong->validblockcount*UDP_DATABLOCK_SIZE], temppingpong->buf, UDP_DATABLOCK_SIZE);
				g_temp_bps_pingpong->validblockcount++;
			}
			else
			{
				//reg_write(((0xB0120000) + 0x800), 0x3C000063);
				//usb_ep_set_halt(common->fsg->bulk_out);
				drop_frame = 1;
				g_temp_bps_pingpong->prev->status = PINGPONG_IDLE;
				memcpy(&g_temp_bps_pingpong->prev->buf[last_frame_block_count*UDP_DATABLOCK_SIZE], temppingpong->buf, UDP_DATABLOCK_SIZE);
				last_frame_block_count++;
				g_temp_bps_pingpong->prev->validblockcount = last_frame_block_count;
			}
#endif
			//printk("size:%d\n", udp_send_size);
			temppingpong = temppingpong->next;
			//udp_hdr.Tag = MNSP_TAG;
			//udp_hdr.XactType = MNSP_XACTTYPE_VIDEO;
			//udp_hdr.HdrSize = sizeof(udp_hdr);
			//udp_hdr.XactId = udp_hdr.XactId + 1;
			udp_hdr.PayloadLength = (amount_left_to_req)>(UDP_DATABLOCK_SIZE - sizeof(udp_hdr))?(UDP_DATABLOCK_SIZE - sizeof(udp_hdr)):(amount_left_to_req);
			//udp_hdr.TotalLength = UDP_DATABLOCK_SIZE;
			udp_hdr.XactOffset = temp_XactOffset;
			//printk("temp_XactOffset2:%d, %d, %d\n", temp_XactOffset, amount_left_to_req, udp_block_write);
			memcpy(temppingpong->buf, &udp_hdr, sizeof(udp_hdr));
			udp_block_write = UDP_DATABLOCK_SIZE - sizeof(udp_hdr);
		}
		bh->state = BUF_STATE_EMPTY;
		//while(1)
		//{}
#if 0
		if(amount_left_to_req == 0)
		{
			if(g_temp_bps_pingpong->status == PINGPONG_IDLE && drop_frame == 0)
			{
				printk("send id:%d, bs:%d\n", udp_hdr.XactId, g_temp_bps_pingpong->validblockcount);
				g_temp_bps_pingpong->status = PINGPONG_BUSY;
				g_temp_bps_pingpong = g_temp_bps_pingpong->next;
			}
			else if(drop_frame == 1)
			{
				printk("over id:%d, bs:%d\n", udp_hdr.XactId, g_temp_bps_pingpong->prev->validblockcount);
				if(g_current_stall == 0)
				{
					//usb_ep_set_halt(common->fsg->bulk_out);
					g_current_stall = 1;
				}
				g_temp_bps_pingpong->prev->status = PINGPONG_BUSY;
			}
		}
#endif
	}
	//printk("type:%d\n",common->datatype);
	if(common->datatype == 0x05)
	{
		usb_ep_disable(common->fsg->bulk_out);
		printk("dest:%d\n",rom_hdr.FW_for_dest);
		if(rom_hdr.FW_for_dest == FW_DEST_TX)
		{
			printk("TX update\n");
			store_FW_by_USB(&g_updatefwbuf[USB_BULK_CB_WRAP_LEN + 512 + 0x40000],rom_hdr.PayloadLength);
			update_FW_by_USB(rom_hdr.PayloadLength);
		}
		else if(rom_hdr.FW_for_dest == FW_DEST_RX || rom_hdr.FW_for_dest == FW_DEST_T6)
		{
			printk("RX T6 update\n");
			udp_send_size = ksocket_send(g_TROMconn_socket, &g_TROMsaddr, g_updatefwbuf, common->data_size_from_cmnd + USB_BULK_CB_WRAP_LEN);
			printk("FW Trans:%d\n",udp_send_size);
			g_led_net_send_total_block = g_led_net_send_total_block + udp_send_size;
		}
	}
	//usb_ep_disable(common->fsg->bulk_out);
	//msleep(5000);
	//usb_ep_enable(common->fsg->bulk_out, &fsg_hs_bulk_out_desc);
	//printk("frame E %lld\n", GetTimeStamp());
#if 0
	while (amount_left_to_write > 0) {
		printk("do_write2\n");
		/* Queue a request for more data from the host */
		bh = common->next_buffhd_to_fill;
		if (bh->state == BUF_STATE_EMPTY && get_some_more) {
			printk("do_write3\n");
			/* Figure out how much we want to get:
			 * Try to get the remaining amount.
			 * But don't get more than the buffer size.
			 * And don't try to go past the end of the file.
			 * If we're not at a page boundary,
			 *	don't go past the next page.
			 * If this means getting 0, then we were asked
			 *	to write past the end of file.
			 * Finally, round down to a block boundary. */
			amount = min(amount_left_to_req, FSG_BUFLEN);
			//amount = min((loff_t) amount, curlun->file_length -
			//		usb_offset);
			//partial_page = usb_offset & (PAGE_CACHE_SIZE - 1);
			//if (partial_page > 0)
			//	amount = min(amount,
	//(unsigned int) PAGE_CACHE_SIZE - partial_page);

			if (amount == 0) {
				get_some_more = 0;
				//curlun->sense_data =
				//	SS_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE;
				//curlun->sense_data_info = usb_offset >> 9;
				//curlun->info_valid = 1;
				continue;
			}
			amount -= (amount & 511);
			if (amount == 0) {

				/* Why were we were asked to transfer a
				 * partial block? */
				get_some_more = 0;
				continue;
			}

			/* Get the next buffer */
			//usb_offset += amount;
			common->usb_amount_left -= amount;
			amount_left_to_req -= amount;
			if (amount_left_to_req == 0)
				get_some_more = 0;

			/* amount is always divisible by 512, hence by
			 * the bulk-out maxpacket size */
			bh->outreq->length = amount;
			bh->bulk_out_intended_length = amount;
			bh->outreq->short_not_ok = 1;
			printk("I_Data%d,%d\n",bh->outreq->length, common->data_size);
			START_TRANSFER_OR(common, bulk_out, bh->outreq,
					  &bh->outreq_busy, &bh->state)
				/* Don't know what to do if
				 * common->fsg is NULL */
				//return -EIO;
			common->next_buffhd_to_fill = bh->next;
			continue;
		}

		/* Write the received data to the backing file */
		bh = common->next_buffhd_to_drain;
		if (bh->state == BUF_STATE_EMPTY && !get_some_more)
			break;			/* We stopped early */
		if (bh->state == BUF_STATE_FULL) {
			smp_rmb();
			common->next_buffhd_to_drain = bh->next;
			bh->state = BUF_STATE_EMPTY;

			/* Did something go wrong with the transfer? */
			if (bh->outreq->status != 0) {
				curlun->sense_data = SS_COMMUNICATION_FAILURE;
				curlun->sense_data_info = file_offset >> 9;
				curlun->info_valid = 1;
				break;
			}

			amount = bh->outreq->actual;
			//if (curlun->file_length - file_offset < amount) {
			//	LERROR(curlun,
	//"write %u @ %llu beyond end %llu\n",
	//amount, (unsigned long long) file_offset,
	//(unsigned long long) curlun->file_length);
	//			amount = curlun->file_length - file_offset;
	//		}

			/* Perform the write */
			//file_offset_tmp = file_offset;

			
			//nwritten = vfs_write(curlun->filp,
			//		(char __user *) bh->buf,
			//		amount, &file_offset_tmp);
			nwritten = amount;
			VLDBG(curlun, "file write %u @ %llu -> %d\n", amount,
					(unsigned long long) file_offset,
					(int) nwritten);
			if (signal_pending(current))
				return -EINTR;		/* Interrupted! */

			if (nwritten < 0) {
				LDBG(curlun, "error in file write: %d\n",
						(int) nwritten);
				nwritten = 0;
			} else if (nwritten < amount) {
				LDBG(curlun, "partial file write: %d/%u\n",
						(int) nwritten, amount);
				nwritten -= (nwritten & 511);
				/* Round down to a block */
			}
			file_offset += nwritten;
			amount_left_to_write -= nwritten;
			common->residue -= nwritten;

			/* If an error occurred, report it and its position */
			if (nwritten < amount) {
				curlun->sense_data = SS_WRITE_ERROR;
				curlun->sense_data_info = file_offset >> 9;
				curlun->info_valid = 1;
				break;
			}

			/* Did the host decide to stop early? */
			if (bh->outreq->actual != bh->outreq->length) {
				common->short_packet_received = 1;
				break;
			}
			continue;
		}

		/* Wait for something to happen */
		rc = sleep_thread(common);
		if (rc)
			return rc;
	}
#endif
	return common->data_size_from_cmnd;		/* No default reply */
}


/*-------------------------------------------------------------------------*/

static int do_synchronize_cache(struct fsg_common *common)
{
	struct fsg_lun	*curlun = common->curlun;
	int		rc;

	/* We ignore the requested LBA and write out all file's
	 * dirty data buffers. */
	rc = fsg_lun_fsync_sub(curlun);
	if (rc)
		curlun->sense_data = SS_WRITE_ERROR;
	return 0;
}


/*-------------------------------------------------------------------------*/

static void invalidate_sub(struct fsg_lun *curlun)
{
	struct file	*filp = curlun->filp;
	struct inode	*inode = filp->f_path.dentry->d_inode;
	unsigned long	rc;

	rc = invalidate_mapping_pages(inode->i_mapping, 0, -1);
	VLDBG(curlun, "invalidate_mapping_pages -> %ld\n", rc);
}

static int do_verify(struct fsg_common *common)
{
	struct fsg_lun		*curlun = common->curlun;
	u32			lba;
	u32			verification_length;
	struct fsg_buffhd	*bh = common->next_buffhd_to_fill;
	loff_t			file_offset, file_offset_tmp;
	u32			amount_left;
	unsigned int		amount;
	ssize_t			nread;

	/* Get the starting Logical Block Address and check that it's
	 * not too big */
	lba = get_unaligned_be32(&common->cmnd[2]);
	if (lba >= curlun->num_sectors) {
		curlun->sense_data = SS_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE;
		return -EINVAL;
	}

	/* We allow DPO (Disable Page Out = don't save data in the
	 * cache) but we don't implement it. */
	if (common->cmnd[1] & ~0x10) {
		curlun->sense_data = SS_INVALID_FIELD_IN_CDB;
		return -EINVAL;
	}

	verification_length = get_unaligned_be16(&common->cmnd[7]);
	if (unlikely(verification_length == 0))
		return -EIO;		/* No default reply */

	/* Prepare to carry out the file verify */
	amount_left = verification_length << 9;
	file_offset = ((loff_t) lba) << 9;

	/* Write out all the dirty buffers before invalidating them */
	fsg_lun_fsync_sub(curlun);
	if (signal_pending(current))
		return -EINTR;

	invalidate_sub(curlun);
	if (signal_pending(current))
		return -EINTR;

	/* Just try to read the requested blocks */
	while (amount_left > 0) {

		/* Figure out how much we need to read:
		 * Try to read the remaining amount, but not more than
		 * the buffer size.
		 * And don't try to read past the end of the file.
		 * If this means reading 0 then we were asked to read
		 * past the end of file. */
		amount = min(amount_left, FSG_BUFLEN);
		amount = min((loff_t) amount,
				curlun->file_length - file_offset);
		if (amount == 0) {
			curlun->sense_data =
					SS_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE;
			curlun->sense_data_info = file_offset >> 9;
			curlun->info_valid = 1;
			break;
		}

		/* Perform the read */
		file_offset_tmp = file_offset;
		nread = vfs_read(curlun->filp,
				(char __user *) bh->buf,
				amount, &file_offset_tmp);
		VLDBG(curlun, "file read %u @ %llu -> %d\n", amount,
				(unsigned long long) file_offset,
				(int) nread);
		if (signal_pending(current))
			return -EINTR;

		if (nread < 0) {
			LDBG(curlun, "error in file verify: %d\n",
					(int) nread);
			nread = 0;
		} else if (nread < amount) {
			LDBG(curlun, "partial file verify: %d/%u\n",
					(int) nread, amount);
			nread -= (nread & 511);	/* Round down to a sector */
		}
		if (nread == 0) {
			curlun->sense_data = SS_UNRECOVERED_READ_ERROR;
			curlun->sense_data_info = file_offset >> 9;
			curlun->info_valid = 1;
			break;
		}
		file_offset += nread;
		amount_left -= nread;
	}
	return 0;
}


/*-------------------------------------------------------------------------*/
static unsigned char InquiryResponse[]                        = { 0x00, 0x80, 0x05, 0x02, 0x5B, 0x00, 0x00, 0x00,
                                                                  0x42, 0x55, 0x46, 0x46, 0x41, 0x4C, 0x4F, 0x20, // Vendor Identification field contains 8 bytes
                                                                  0x55, 0x53, 0x42, 0x20, 0x46, 0x6C, 0x61, 0x73,
                                                                  0x68, 0x20, 0x44, 0x69, 0x73, 0x6B, 0x20, 0x20, // Product Identification field contains 16 bytes
                                                                  0x31, 0x2E, 0x30, 0x30, 0x00, 0x00, 0x00, 0x00, // Product RevisionLevel field contains 4 bytes
                                                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                                  0x00, 0x00, 0x04, 0x60, 0x04, 0x61, 0x04, 0x62,
                                                                  0x04, 0x63, 0x04, 0x60, 0x04, 0x61, 0x04, 0x62,
                                                                  0x04, 0x63, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                                };

static int do_inquiry(struct fsg_common *common, struct fsg_buffhd *bh)
{
	struct fsg_lun *curlun = common->curlun;
	u8	*buf = (u8 *) bh->buf;

	if (!curlun) {		/* Unsupported LUNs are okay */
		common->bad_lun_okay = 1;
		memset(buf, 0, 36);
		buf[0] = 0x7f;		/* Unsupported, no device-type */
		buf[4] = 31;		/* Additional length */
		memcpy(buf, InquiryResponse, sizeof(InquiryResponse));
		CSW_dft_status = 0x00;
		return sizeof(InquiryResponse);
	}

	buf[0] = curlun->cdrom ? TYPE_CDROM : TYPE_DISK;
	buf[1] = curlun->removable ? 0x80 : 0;
	buf[2] = 2;		/* ANSI SCSI level 2 */
	buf[3] = 2;		/* SCSI-2 INQUIRY data format */
	buf[4] = 31;		/* Additional length */
	buf[5] = 0;		/* No special options */
	buf[6] = 0;
	buf[7] = 0;
	memcpy(buf, InquiryResponse, sizeof(InquiryResponse));
	CSW_dft_status = 0x00;
	return sizeof(InquiryResponse);
}

static unsigned char RequestSenseResponse[18]                 = { 0x70, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x3A, 0x00, 0x00, 0x00, 0x00, 0x00 };

static int do_request_sense(struct fsg_common *common, struct fsg_buffhd *bh)
{
	struct fsg_lun	*curlun = common->curlun;
	u8		*buf = (u8 *) bh->buf;
	u32		sd, sdinfo;
	int		valid;

	/*
	 * From the SCSI-2 spec., section 7.9 (Unit attention condition):
	 *
	 * If a REQUEST SENSE command is received from an initiator
	 * with a pending unit attention condition (before the target
	 * generates the contingent allegiance condition), then the
	 * target shall either:
	 *   a) report any pending sense data and preserve the unit
	 *	attention condition on the logical unit, or,
	 *   b) report the unit attention condition, may discard any
	 *	pending sense data, and clear the unit attention
	 *	condition on the logical unit for that initiator.
	 *
	 * FSG normally uses option a); enable this code to use option b).
	 */
#if 0
	if (curlun && curlun->unit_attention_data != SS_NO_SENSE) {
		curlun->sense_data = curlun->unit_attention_data;
		curlun->unit_attention_data = SS_NO_SENSE;
	}
#endif

	if (!curlun) {		/* Unsupported LUNs are okay */
		common->bad_lun_okay = 1;
		sd = SS_LOGICAL_UNIT_NOT_SUPPORTED;
		sdinfo = 0;
		valid = 0;
	} else {
		sd = curlun->sense_data;
		sdinfo = curlun->sense_data_info;
		valid = curlun->info_valid << 7;
		curlun->sense_data = SS_NO_SENSE;
		curlun->sense_data_info = 0;
		curlun->info_valid = 0;
	}

	memset(buf, 0, 18);
	buf[0] = valid | 0x70;			/* Valid, current error */
	buf[2] = SK(sd);
	put_unaligned_be32(sdinfo, &buf[3]);	/* Sense information */
	buf[7] = 18 - 8;			/* Additional sense length */
	buf[12] = ASC(sd);
	buf[13] = ASCQ(sd);
	memcpy(buf, RequestSenseResponse, sizeof(RequestSenseResponse));
	CSW_dft_status = 0x00;
	return sizeof(RequestSenseResponse);
}

static unsigned char ReadCapacity10Response[8]                = { ((16384-1)>>24)&0xFF, ((16384-1)>>16)&0xFF, ((16384-1)>>8)&0xFF, ((16384-1))&0xFF, 0x00, (512>>16)&0xFF, (512>>8)&0xFF, (512)&0xFF };

static int do_read_capacity(struct fsg_common *common, struct fsg_buffhd *bh)
{
	struct fsg_lun	*curlun = common->curlun;
	u32		lba = get_unaligned_be32(&common->cmnd[2]);
	int		pmi = common->cmnd[8];
	u8		*buf = (u8 *) bh->buf;

	/* Check the PMI and LBA fields */
	if (pmi > 1 || (pmi == 0 && lba != 0)) {
		curlun->sense_data = SS_INVALID_FIELD_IN_CDB;
		return -EINVAL;
	}

	memcpy(buf, ReadCapacity10Response, sizeof(ReadCapacity10Response));
	CSW_dft_status = 0x01;
	return sizeof(ReadCapacity10Response);
}


static int do_read_header(struct fsg_common *common, struct fsg_buffhd *bh)
{
	struct fsg_lun	*curlun = common->curlun;
	int		msf = common->cmnd[1] & 0x02;
	u32		lba = get_unaligned_be32(&common->cmnd[2]);
	u8		*buf = (u8 *) bh->buf;

	if (common->cmnd[1] & ~0x02) {		/* Mask away MSF */
		curlun->sense_data = SS_INVALID_FIELD_IN_CDB;
		return -EINVAL;
	}
	if (lba >= curlun->num_sectors) {
		curlun->sense_data = SS_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE;
		return -EINVAL;
	}

	memset(buf, 0, 8);
	buf[0] = 0x01;		/* 2048 bytes of user data, rest is EC */
	store_cdrom_address(&buf[4], msf, lba);
	return 8;
}


static int do_read_toc(struct fsg_common *common, struct fsg_buffhd *bh)
{
	struct fsg_lun	*curlun = common->curlun;
	int		msf = common->cmnd[1] & 0x02;
	int		start_track = common->cmnd[6];
	u8		*buf = (u8 *) bh->buf;

	if ((common->cmnd[1] & ~0x02) != 0 ||	/* Mask away MSF */
			start_track > 1) {
		curlun->sense_data = SS_INVALID_FIELD_IN_CDB;
		return -EINVAL;
	}

	memset(buf, 0, 20);
	buf[1] = (20-2);		/* TOC data length */
	buf[2] = 1;			/* First track number */
	buf[3] = 1;			/* Last track number */
	buf[5] = 0x16;			/* Data track, copying allowed */
	buf[6] = 0x01;			/* Only track is number 1 */
	store_cdrom_address(&buf[8], msf, 0);

	buf[13] = 0x16;			/* Lead-out track is data */
	buf[14] = 0xAA;			/* Lead-out track number */
	store_cdrom_address(&buf[16], msf, curlun->num_sectors);
	return 20;
}

static unsigned char ModeSense1C00Response[0xC0]              = { 0x0F, 0x00, 0x00, 0x00, 0x1C, 0x0A, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                                  0x31, 0x30, 0x39, 0x32, 0x39, 0x38, 0x30, 0x30, 0x30, 0x30, 0x30, 0x33, 0x34, 0x39, 0x00, 0x00,
                                                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                                };

static int do_mode_sense(struct fsg_common *common, struct fsg_buffhd *bh)
{
	struct fsg_lun	*curlun = common->curlun;
	int		mscmnd = common->cmnd[0];
	u8		*buf = (u8 *) bh->buf;
	u8		*buf0 = buf;
	int		pc, page_code;
	int		changeable_values, all_pages;
	int		valid_page = 0;
	int		len, limit;

	memcpy(buf, ModeSense1C00Response, sizeof(ModeSense1C00Response));
	CSW_dft_status = 0x01;
	return sizeof(ModeSense1C00Response);
	
	if ((common->cmnd[1] & ~0x08) != 0) {	/* Mask away DBD */
		curlun->sense_data = SS_INVALID_FIELD_IN_CDB;
		return -EINVAL;
	}
	pc = common->cmnd[2] >> 6;
	page_code = common->cmnd[2] & 0x3f;
	if (pc == 3) {
		curlun->sense_data = SS_SAVING_PARAMETERS_NOT_SUPPORTED;
		return -EINVAL;
	}
	changeable_values = (pc == 1);
	all_pages = (page_code == 0x3f);

	/* Write the mode parameter header.  Fixed values are: default
	 * medium type, no cache control (DPOFUA), and no block descriptors.
	 * The only variable value is the WriteProtect bit.  We will fill in
	 * the mode data length later. */
	memset(buf, 0, 8);
	if (mscmnd == SC_MODE_SENSE_6) {
		buf[2] = (curlun->ro ? 0x80 : 0x00);		/* WP, DPOFUA */
		buf += 4;
		limit = 255;
	} else {			/* SC_MODE_SENSE_10 */
		buf[3] = (curlun->ro ? 0x80 : 0x00);		/* WP, DPOFUA */
		buf += 8;
		limit = 65535;		/* Should really be FSG_BUFLEN */
	}

	/* No block descriptors */

	/* The mode pages, in numerical order.  The only page we support
	 * is the Caching page. */
	if (page_code == 0x08 || all_pages) {
		valid_page = 1;
		buf[0] = 0x08;		/* Page code */
		buf[1] = 10;		/* Page length */
		memset(buf+2, 0, 10);	/* None of the fields are changeable */

		if (!changeable_values) {
			buf[2] = 0x04;	/* Write cache enable, */
					/* Read cache not disabled */
					/* No cache retention priorities */
			put_unaligned_be16(0xffff, &buf[4]);
					/* Don't disable prefetch */
					/* Minimum prefetch = 0 */
			put_unaligned_be16(0xffff, &buf[8]);
					/* Maximum prefetch */
			put_unaligned_be16(0xffff, &buf[10]);
					/* Maximum prefetch ceiling */
		}
		buf += 12;
	}

	/* Check that a valid page was requested and the mode data length
	 * isn't too long. */
	len = buf - buf0;
	if (!valid_page || len > limit) {
		curlun->sense_data = SS_INVALID_FIELD_IN_CDB;
		return -EINVAL;
	}

	/*  Store the mode data length */
	if (mscmnd == SC_MODE_SENSE_6)
		buf0[0] = len - 1;
	else
		put_unaligned_be16(len - 2, buf0);
	CSW_dft_status = 0x01;
	return len;
}


static int do_start_stop(struct fsg_common *common)
{
	struct fsg_lun	*curlun = common->curlun;
	int		loej, start;

	if (!curlun) {
		return -EINVAL;
	} else if (!curlun->removable) {
		curlun->sense_data = SS_INVALID_COMMAND;
		return -EINVAL;
	} else if ((common->cmnd[1] & ~0x01) != 0 || /* Mask away Immed */
		   (common->cmnd[4] & ~0x03) != 0) { /* Mask LoEj, Start */
		curlun->sense_data = SS_INVALID_FIELD_IN_CDB;
		return -EINVAL;
	}

	loej  = common->cmnd[4] & 0x02;
	start = common->cmnd[4] & 0x01;

	/* Our emulation doesn't support mounting; the medium is
	 * available for use as soon as it is loaded. */
	if (start) {
		if (!fsg_lun_is_open(curlun)) {
			curlun->sense_data = SS_MEDIUM_NOT_PRESENT;
			return -EINVAL;
		}
		return 0;
	}

	/* Are we allowed to unload the media? */
	if (curlun->prevent_medium_removal) {
		LDBG(curlun, "unload attempt prevented\n");
		curlun->sense_data = SS_MEDIUM_REMOVAL_PREVENTED;
		return -EINVAL;
	}

	if (!loej)
		return 0;

	/* Simulate an unload/eject */
	if (common->ops && common->ops->pre_eject) {
		int r = common->ops->pre_eject(common, curlun,
					       curlun - common->luns);
		if (unlikely(r < 0))
			return r;
		else if (r)
			return 0;
	}

	up_read(&common->filesem);
	down_write(&common->filesem);
	fsg_lun_close(curlun);
	up_write(&common->filesem);
	down_read(&common->filesem);

	return common->ops && common->ops->post_eject
		? min(0, common->ops->post_eject(common, curlun,
						 curlun - common->luns))
		: 0;
}


static int do_prevent_allow(struct fsg_common *common)
{
	struct fsg_lun	*curlun = common->curlun;
	int		prevent;

	if (!common->curlun) {
		return -EINVAL;
	} else if (!common->curlun->removable) {
		common->curlun->sense_data = SS_INVALID_COMMAND;
		return -EINVAL;
	}

	prevent = common->cmnd[4] & 0x01;
	if ((common->cmnd[4] & ~0x01) != 0) {	/* Mask away Prevent */
		curlun->sense_data = SS_INVALID_FIELD_IN_CDB;
		return -EINVAL;
	}

	if (curlun->prevent_medium_removal && !prevent)
		fsg_lun_fsync_sub(curlun);
	curlun->prevent_medium_removal = prevent;
	return 0;
}

unsigned char FormatCapacitiesResponse[]               = { 0x00, 0x00, 0x00, 0x08, (16384>>24)&0xFF, (16384>>16)&0xFF, (16384>>8)&0xFF, (16384)&0xFF, 0x00, (512>>16)&0xFF, (512>>8)&0xFF, (512)&0xFF,
                                                                  0x41, 0x4C, 0x4F, 0x20, 0x55, 0x53, 0x42, 0x20, 0x46, 0x6C, 0x61, 0x73, 0x68, 0x20, 0x44, 0x69,
                                                                  0x73, 0x6B, 0x20, 0x20, 0x31, 0x2E, 0x30, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x60,
                                                                  0x04, 0x61, 0x04, 0x62, 0x04, 0x63, 0x04, 0x60, 0x04, 0x61, 0x04, 0x62, 0x04, 0x63, 0x00, 0x00,
                                                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                                };

static int do_read_format_capacities(struct fsg_common *common,
			struct fsg_buffhd *bh)
{
	struct fsg_lun	*curlun = common->curlun;
	u8		*buf = (u8 *) bh->buf;

	buf[0] = buf[1] = buf[2] = 0;
	buf[3] = 8;	/* Only the Current/Maximum Capacity Descriptor */
	buf += 4;

	put_unaligned_be32(curlun->num_sectors, &buf[0]);
						/* Number of blocks */
	put_unaligned_be32(512, &buf[4]);	/* Block length */
	buf[4] = 0x02;				/* Current capacity */
	memcpy(buf, FormatCapacitiesResponse, sizeof(FormatCapacitiesResponse));
	CSW_dft_status = 0x01;
	return sizeof(FormatCapacitiesResponse);
}


static int do_mode_select(struct fsg_common *common, struct fsg_buffhd *bh)
{
	struct fsg_lun	*curlun = common->curlun;

	/* We don't support MODE SELECT */
	if (curlun)
		curlun->sense_data = SS_INVALID_COMMAND;
	return -EINVAL;
}


/*-------------------------------------------------------------------------*/

static int halt_bulk_in_endpoint(struct fsg_dev *fsg)
{
	int	rc;

	rc = fsg_set_halt(fsg, fsg->bulk_in);
	if (rc == -EAGAIN)
		VDBG(fsg, "delayed bulk-in endpoint halt\n");
	while (rc != 0) {
		if (rc != -EAGAIN) {
			WARNING(fsg, "usb_ep_set_halt -> %d\n", rc);
			rc = 0;
			break;
		}

		/* Wait for a short time and then try again */
		if (msleep_interruptible(100) != 0)
			return -EINTR;
		rc = usb_ep_set_halt(fsg->bulk_in);
	}
	return rc;
}

static int wedge_bulk_in_endpoint(struct fsg_dev *fsg)
{
	int	rc;

	DBG(fsg, "bulk-in set wedge\n");
	rc = usb_ep_set_wedge(fsg->bulk_in);
	if (rc == -EAGAIN)
		VDBG(fsg, "delayed bulk-in endpoint wedge\n");
	while (rc != 0) {
		if (rc != -EAGAIN) {
			WARNING(fsg, "usb_ep_set_wedge -> %d\n", rc);
			rc = 0;
			break;
		}

		/* Wait for a short time and then try again */
		if (msleep_interruptible(100) != 0)
			return -EINTR;
		rc = usb_ep_set_wedge(fsg->bulk_in);
	}
	return rc;
}

static int pad_with_zeros(struct fsg_dev *fsg)
{
	struct fsg_buffhd	*bh = fsg->common->next_buffhd_to_fill;
	u32			nkeep = bh->inreq->length;
	u32			nsend;
	int			rc;

	bh->state = BUF_STATE_EMPTY;		/* For the first iteration */
	fsg->common->usb_amount_left = nkeep + fsg->common->residue;
	while (fsg->common->usb_amount_left > 0) {

		/* Wait for the next buffer to be free */
		while (bh->state != BUF_STATE_EMPTY) {
			rc = sleep_thread(fsg->common);
			if (rc)
				return rc;
		}

		nsend = min(fsg->common->usb_amount_left, FSG_BUFLEN);
		memset(bh->buf + nkeep, 0, nsend - nkeep);
		bh->inreq->length = nsend;
		bh->inreq->zero = 0;
		start_transfer(fsg, fsg->bulk_in, bh->inreq,
				&bh->inreq_busy, &bh->state);
		bh = fsg->common->next_buffhd_to_fill = bh->next;
		fsg->common->usb_amount_left -= nsend;
		nkeep = 0;
	}
	return 0;
}

static int throw_away_data(struct fsg_common *common)
{
	struct fsg_buffhd	*bh;
	u32			amount;
	int			rc;

	for (bh = common->next_buffhd_to_drain;
	     bh->state != BUF_STATE_EMPTY || common->usb_amount_left > 0;
	     bh = common->next_buffhd_to_drain) {

		/* Throw away the data in a filled buffer */
		if (bh->state == BUF_STATE_FULL) {
			smp_rmb();
			bh->state = BUF_STATE_EMPTY;
			common->next_buffhd_to_drain = bh->next;

			/* A short packet or an error ends everything */
			if (bh->outreq->actual != bh->outreq->length ||
					bh->outreq->status != 0) {
				raise_exception(common,
						FSG_STATE_ABORT_BULK_OUT);
				return -EINTR;
			}
			continue;
		}

		/* Try to submit another request if we need one */
		bh = common->next_buffhd_to_fill;
		if (bh->state == BUF_STATE_EMPTY
		 && common->usb_amount_left > 0) {
			amount = min(common->usb_amount_left, FSG_BUFLEN);

			/* amount is always divisible by 512, hence by
			 * the bulk-out maxpacket size */
			bh->outreq->length = amount;
			bh->bulk_out_intended_length = amount;
			bh->outreq->short_not_ok = 1;
			START_TRANSFER_OR(common, bulk_out, bh->outreq,
					  &bh->outreq_busy, &bh->state)
				/* Don't know what to do if
				 * common->fsg is NULL */
				return -EIO;
			common->next_buffhd_to_fill = bh->next;
			common->usb_amount_left -= amount;
			continue;
		}

		/* Otherwise wait for something to happen */
		rc = sleep_thread(common);
		if (rc)
			return rc;
	}
	return 0;
}


static int finish_reply(struct fsg_common *common)
{
	struct fsg_buffhd	*bh = common->next_buffhd_to_fill;
	int			rc = 0;

	switch (common->data_dir) {
	case DATA_DIR_NONE:
		break;			/* Nothing to send */

	/* If we don't know whether the host wants to read or write,
	 * this must be CB or CBI with an unknown command.  We mustn't
	 * try to send or receive any data.  So stall both bulk pipes
	 * if we can and wait for a reset. */
	case DATA_DIR_UNKNOWN:
		if (!common->can_stall) {
			/* Nothing */
		} else if (fsg_is_set(common)) {
			fsg_set_halt(common->fsg, common->fsg->bulk_out);
			rc = halt_bulk_in_endpoint(common->fsg);
		} else {
			/* Don't know what to do if common->fsg is NULL */
			rc = -EIO;
		}
		break;

	/* All but the last buffer of data must have already been sent */
	case DATA_DIR_TO_HOST:
		if (common->data_size == 0) {
			/* Nothing to send */

		/* If there's no residue, simply send the last buffer */
		} else if (common->residue == 0) {
			bh->inreq->zero = 0;
			START_TRANSFER_OR(common, bulk_in, bh->inreq,
					  &bh->inreq_busy, &bh->state)
				return -EIO;
			common->next_buffhd_to_fill = bh->next;

		/* For Bulk-only, if we're allowed to stall then send the
		 * short packet and halt the bulk-in endpoint.  If we can't
		 * stall, pad out the remaining data with 0's. */
		} else if (common->can_stall) {
			bh->inreq->zero = 1;
			START_TRANSFER_OR(common, bulk_in, bh->inreq,
					  &bh->inreq_busy, &bh->state)
				/* Don't know what to do if
				 * common->fsg is NULL */
				rc = -EIO;
			common->next_buffhd_to_fill = bh->next;
			if (common->fsg)
				rc = halt_bulk_in_endpoint(common->fsg);
		} else if (fsg_is_set(common)) {
			rc = pad_with_zeros(common->fsg);
		} else {
			/* Don't know what to do if common->fsg is NULL */
			rc = -EIO;
		}
		break;

	/* We have processed all we want from the data the host has sent.
	 * There may still be outstanding bulk-out requests. */
	case DATA_DIR_FROM_HOST:
		if (common->residue == 0) {
			/* Nothing to receive */

		/* Did the host stop sending unexpectedly early? */
		} else if (common->short_packet_received) {
			raise_exception(common, FSG_STATE_ABORT_BULK_OUT);
			rc = -EINTR;

		/* We haven't processed all the incoming data.  Even though
		 * we may be allowed to stall, doing so would cause a race.
		 * The controller may already have ACK'ed all the remaining
		 * bulk-out packets, in which case the host wouldn't see a
		 * STALL.  Not realizing the endpoint was halted, it wouldn't
		 * clear the halt -- leading to problems later on. */
#if 0
		} else if (common->can_stall) {
			if (fsg_is_set(common))
				fsg_set_halt(common->fsg,
					     common->fsg->bulk_out);
			raise_exception(common, FSG_STATE_ABORT_BULK_OUT);
			rc = -EINTR;
#endif

		/* We can't stall.  Read in the excess data and throw it
		 * all away. */
		} else {
			rc = throw_away_data(common);
		}
		break;
	}
	return rc;
}

wait_queue_head_t gUVCwq;
int send_UVB_bulk_usr(struct fsg_common *common, unsigned int length, unsigned char *UVCbuf)
{
	struct fsg_lun		*curlun = common->curlun;
	struct fsg_buffhd	*bh;
	struct bulk_cs_wrap	*csw;
	int			rc;
	u8			status = USB_STATUS_PASS;
	u32			sd, sdinfo = 0;

	/* Wait for the next buffer to become available */
	bh = common->next_buffhd_to_fill;
	//while (bh->state != BUF_STATE_EMPTY) {
	//	interruptible_sleep_on(&gUVCwq);
	//}

	memcpy(bh->buf, UVCbuf, length);
	//csw = (void *)bh->buf;

	//csw->Signature = cpu_to_le32(USB_BULK_CS_SIG);
	//csw->Tag = common->tag;
	//csw->Residue = cpu_to_le32(common->residue);
	//csw->Status = CSW_dft_status;

	bh->inreq->length = length;
	bh->state = BUF_STATE_FULL;
	bh->inreq->zero = 0;
	START_TRANSFER_OR(common, bulk_in, bh->inreq,
			  &bh->inreq_busy, &bh->state)
		/* Don't know what to do if common->fsg is NULL */
		return -EIO;

	common->next_buffhd_to_fill = bh->next;
	return 0;
}

int send_UVB_bulk(struct fsg_common *common, unsigned int length, unsigned char *UVCbuf)
{
	struct fsg_lun		*curlun = common->curlun;
	struct fsg_buffhd	*bh;
	struct bulk_cs_wrap	*csw;
	int			rc;
	u8			status = USB_STATUS_PASS;
	u32			sd, sdinfo = 0;

	/* Wait for the next buffer to become available */
	bh = common->next_buffhd_to_fill;
	while (bh->state != BUF_STATE_EMPTY) {
		rc = sleep_thread(common);
		if (rc)
			return rc;
	}

	memcpy(bh->buf, UVCbuf, length);
	//csw = (void *)bh->buf;

	//csw->Signature = cpu_to_le32(USB_BULK_CS_SIG);
	//csw->Tag = common->tag;
	//csw->Residue = cpu_to_le32(common->residue);
	//csw->Status = CSW_dft_status;

	bh->inreq->length = length;
	bh->state = BUF_STATE_FULL;
	bh->inreq->zero = 0;
	START_TRANSFER_OR(common, bulk_in, bh->inreq,
			  &bh->inreq_busy, &bh->state)
		/* Don't know what to do if common->fsg is NULL */
		return -EIO;

	common->next_buffhd_to_fill = bh->next;
	return 0;
}

int Enable_UVC_Bulk(struct fsg_common *common)
{
	usb_ep_enable(common->fsg->bulk_in, &fsg_hs_bulk_in_desc);
}

int Disable_UVC_Bulk(struct fsg_common *common)
{
	usb_ep_disable(common->fsg->bulk_in);
}

static int send_status(struct fsg_common *common)
{
	struct fsg_lun		*curlun = common->curlun;
	struct fsg_buffhd	*bh;
	struct bulk_cs_wrap	*csw;
	int			rc;
	u8			status = USB_STATUS_PASS;
	u32			sd, sdinfo = 0;

	/* Wait for the next buffer to become available */
	bh = common->next_buffhd_to_fill;
	while (bh->state != BUF_STATE_EMPTY) {
		rc = sleep_thread(common);
		if (rc)
			return rc;
	}

	if (curlun) {
		sd = curlun->sense_data;
		sdinfo = curlun->sense_data_info;
	} else if (common->bad_lun_okay)
		sd = SS_NO_SENSE;
	else
		sd = SS_LOGICAL_UNIT_NOT_SUPPORTED;

	if (common->phase_error) {
		DBG(common, "sending phase-error status\n");
		status = USB_STATUS_PHASE_ERROR;
		sd = SS_INVALID_COMMAND;
	} else if (sd != SS_NO_SENSE) {
		DBG(common, "sending command-failure status\n");
		status = USB_STATUS_FAIL;
		VDBG(common, "  sense data: SK x%02x, ASC x%02x, ASCQ x%02x;"
				"  info x%x\n",
				SK(sd), ASC(sd), ASCQ(sd), sdinfo);
	}

	/* Store and send the Bulk-only CSW */
	csw = (void *)bh->buf;

	csw->Signature = cpu_to_le32(USB_BULK_CS_SIG);
	csw->Tag = common->tag;
	csw->Residue = cpu_to_le32(common->residue);
	csw->Status = CSW_dft_status;

	bh->inreq->length = USB_BULK_CS_WRAP_LEN;
	bh->inreq->zero = 0;
	START_TRANSFER_OR(common, bulk_in, bh->inreq,
			  &bh->inreq_busy, &bh->state)
		/* Don't know what to do if common->fsg is NULL */
		return -EIO;

	common->next_buffhd_to_fill = bh->next;
	return 0;
}


/*-------------------------------------------------------------------------*/

/* Check whether the command is properly formed and whether its data size
 * and direction agree with the values we already have. */
static int check_command(struct fsg_common *common, int cmnd_size,
		enum data_direction data_dir, unsigned int mask,
		int needs_medium, const char *name)
{
	int			i;
	int			lun = common->cmnd[1] >> 5;
	static const char	dirletter[4] = {'u', 'o', 'i', 'n'};
	char			hdlen[20];
	struct fsg_lun		*curlun;

	hdlen[0] = 0;
	if (common->data_dir != DATA_DIR_UNKNOWN)
		sprintf(hdlen, ", H%c=%u", dirletter[(int) common->data_dir],
				common->data_size);
	printk("SCSI command: %s;  Dc=%d, D%c=%u;  Hc=%d%s\n",
	     name, cmnd_size, dirletter[(int) data_dir],
	     common->data_size_from_cmnd, common->cmnd_size, hdlen);

	/* We can't reply at all until we know the correct data direction
	 * and size. */
	if (common->data_size_from_cmnd == 0)
		data_dir = DATA_DIR_NONE;
	if (common->data_size < common->data_size_from_cmnd) {
		/* Host data size < Device data size is a phase error.
		 * Carry out the command, but only transfer as much as
		 * we are allowed. */
		common->data_size_from_cmnd = common->data_size;
		common->phase_error = 1;
	}
	common->residue = common->data_size;
	common->usb_amount_left = common->data_size;

	/* Conflicting data directions is a phase error */
	if (common->data_dir != data_dir
	 && common->data_size_from_cmnd > 0) {
		common->phase_error = 1;
		return -EINVAL;
	}

	/* Verify the length of the command itself */
	if (cmnd_size != common->cmnd_size) {

		/* Special case workaround: There are plenty of buggy SCSI
		 * implementations. Many have issues with cbw->Length
		 * field passing a wrong command size. For those cases we
		 * always try to work around the problem by using the length
		 * sent by the host side provided it is at least as large
		 * as the correct command length.
		 * Examples of such cases would be MS-Windows, which issues
		 * REQUEST SENSE with cbw->Length == 12 where it should
		 * be 6, and xbox360 issuing INQUIRY, TEST UNIT READY and
		 * REQUEST SENSE with cbw->Length == 10 where it should
		 * be 6 as well.
		 */
		if (cmnd_size <= common->cmnd_size) {
			DBG(common, "%s is buggy! Expected length %d "
			    "but we got %d\n", name,
			    cmnd_size, common->cmnd_size);
			cmnd_size = common->cmnd_size;
		} else {
			common->phase_error = 1;
			return -EINVAL;
		}
	}

	/* Check that the LUN values are consistent */
	if (common->lun != lun)
		DBG(common, "using LUN %d from CBW, not LUN %d from CDB\n",
		    common->lun, lun);

	/* Check the LUN */
	if (common->lun >= 0 && common->lun < common->nluns) {
		curlun = &common->luns[common->lun];
		common->curlun = curlun;
		if (common->cmnd[0] != SC_REQUEST_SENSE) {
			curlun->sense_data = SS_NO_SENSE;
			curlun->sense_data_info = 0;
			curlun->info_valid = 0;
		}
	} else {
		common->curlun = NULL;
		curlun = NULL;
		common->bad_lun_okay = 0;

		/* INQUIRY and REQUEST SENSE commands are explicitly allowed
		 * to use unsupported LUNs; all others may not. */
		if (common->cmnd[0] != SC_INQUIRY &&
		    common->cmnd[0] != SC_REQUEST_SENSE) {
			DBG(common, "unsupported LUN %d\n", common->lun);
			return -EINVAL;
		}
	}

	/* If a unit attention condition exists, only INQUIRY and
	 * REQUEST SENSE commands are allowed; anything else must fail. */
	if (curlun && curlun->unit_attention_data != SS_NO_SENSE &&
			common->cmnd[0] != SC_INQUIRY &&
			common->cmnd[0] != SC_REQUEST_SENSE) {
		curlun->sense_data = curlun->unit_attention_data;
		curlun->unit_attention_data = SS_NO_SENSE;
		return -EINVAL;
	}

	/* Check that only command bytes listed in the mask are non-zero */
	common->cmnd[1] &= 0x1f;			/* Mask away the LUN */
	for (i = 1; i < cmnd_size; ++i) {
		if (common->cmnd[i] && !(mask & (1 << i))) {
			if (curlun)
				curlun->sense_data = SS_INVALID_FIELD_IN_CDB;
			return -EINVAL;
		}
	}

	/* If the medium isn't mounted and the command needs to access
	 * it, return an error. */
	if (curlun && !fsg_lun_is_open(curlun) && needs_medium) {
		curlun->sense_data = SS_MEDIUM_NOT_PRESENT;
		return -EINVAL;
	}

	return 0;
}

static int do_read0(struct fsg_common *common, struct fsg_buffhd *bh)
{
	u8	*buf = (u8 *) bh->buf;
	memcpy(buf, InquiryResponse, sizeof(InquiryResponse));
	return 512;
}

unsigned char g_hid_m_buffer[8] = {0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
unsigned char g_hid_k_buffer[8] = {0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00};

struct f_hidg *g_hidg;
struct fsg_common cursor_udp_common;
struct fsg_common cursor_tcp_common;
struct fsg_common hid_udp_common;
struct fsg_common ksocket_send_common;
struct fsg_common led_blink_lv_common;

struct fsg_common *g_userspaceUVC_common;

static void my_f_hidg_req_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct f_hidg *hidg = g_hidg;
	//printk("hid_f\n");
	if (req->status != 0) {
		ERROR(hidg->func.config->cdev,
			"End Point Request ERROR: %d\n", req->status);
	}

	hidg->write_pending = 0;
	wake_up(&hidg->write_queue);
	wakeup_thread(&hid_udp_common);
}

ssize_t my_f_hidg_write(const char __user *buffer, size_t count)
{
	struct f_hidg *hidg  = g_hidg;
	ssize_t status;
	//printk("hid_w\n");
	mutex_lock(&hidg->lock);

	memcpy(hidg->req->buf, buffer, count);
	
	hidg->req->status   = 0;
	hidg->req->zero     = 0;
	hidg->req->length   = count;
	hidg->req->complete = my_f_hidg_req_complete;
	hidg->req->context  = hidg;
	hidg->write_pending = 1;

	status = usb_ep_queue(hidg->in_ep, hidg->req, GFP_ATOMIC);
	if (status < 0) {
		ERROR(hidg->func.config->cdev,
			"usb_ep_queue error on int endpoint %zd\n", status);
		hidg->write_pending = 0;
		wake_up(&hidg->write_queue);
	} else {
		status = count;
	}
	//printk("my_f_hidg_write0\n");
	mutex_unlock(&hidg->lock);

	return status;
}

static int do_scsi_command(struct fsg_common *common)
{
	struct fsg_buffhd	*bh;
	int			rc;
	int			reply = -EINVAL;
	int			i;
	static char		unknown[16];

	//dump_cdb(common);
	//printk("do_scsi_command1\n");
	/* Wait for the next buffer to become available for data or status */
	bh = common->next_buffhd_to_fill;
	common->next_buffhd_to_drain = bh;
	while (bh->state != BUF_STATE_EMPTY) {
		rc = sleep_thread(common);
		if (rc)
			return rc;
	}
	common->phase_error = 0;
	common->short_packet_received = 0;

	//down_read(&common->filesem);	/* We're using the backing file */

	common->data_size_from_cmnd = common->data_size;
				//get_unaligned_be16(&common->cmnd[7]) << 9;
		//reply = check_command(common, 10, DATA_DIR_FROM_HOST,
		//		      (1<<1) | (0xf<<2) | (3<<7), 1,
		//		      "WRITE(10)");
		//if (reply == 0)
			reply = do_write(common);
	
	/*switch (common->cmnd[0]) {

	case SC_INQUIRY:
		common->data_size_from_cmnd = common->cmnd[4];
		if(common->data_size_from_cmnd > sizeof(InquiryResponse))
			common->data_size_from_cmnd = sizeof(InquiryResponse);
		common->data_size = common->data_size_from_cmnd;
		reply = check_command(common, 6, DATA_DIR_TO_HOST,
				      (1<<4), 0,
				      "INQUIRY");
		reply = 0;
		if (reply == 0)
			reply = do_inquiry(common, bh);
		break;

	case SC_MODE_SELECT_6:
		common->data_size_from_cmnd = common->cmnd[4];
		reply = check_command(common, 6, DATA_DIR_FROM_HOST,
				      (1<<1) | (1<<4), 0,
				      "MODE SELECT(6)");
		if (reply == 0)
			reply = do_mode_select(common, bh);
		break;

	case SC_MODE_SELECT_10:
		common->data_size_from_cmnd =
			get_unaligned_be16(&common->cmnd[7]);
		reply = check_command(common, 10, DATA_DIR_FROM_HOST,
				      (1<<1) | (3<<7), 0,
				      "MODE SELECT(10)");
		if (reply == 0)
			reply = do_mode_select(common, bh);
		break;

	case SC_MODE_SENSE_6:
		common->data_size_from_cmnd = common->cmnd[4];
		reply = check_command(common, 6, DATA_DIR_TO_HOST,
				      (1<<1) | (1<<2) | (1<<4), 0,
				      "MODE SENSE(6)");
		reply = 0;
		if (reply == 0)
			reply = do_mode_sense(common, bh);
		break;

	case SC_MODE_SENSE_10:
		common->data_size_from_cmnd =
			get_unaligned_be16(&common->cmnd[7]);
		reply = check_command(common, 10, DATA_DIR_TO_HOST,
				      (1<<1) | (1<<2) | (3<<7), 0,
				      "MODE SENSE(10)");
		if (reply == 0)
			reply = do_mode_sense(common, bh);
		break;

	case SC_PREVENT_ALLOW_MEDIUM_REMOVAL:
		common->data_size_from_cmnd = 0;
		reply = check_command(common, 6, DATA_DIR_NONE,
				      (1<<4), 0,
				      "PREVENT-ALLOW MEDIUM REMOVAL");
		if (reply == 0)
			reply = do_prevent_allow(common);
		CSW_dft_status = 0x01;
		break;

	case SC_READ_6:
		i = common->cmnd[4];
		common->data_size_from_cmnd = (i == 0 ? 256 : i) << 9;
		reply = check_command(common, 6, DATA_DIR_TO_HOST,
				      (7<<1) | (1<<4), 1,
				      "READ(6)");
		if (reply == 0)
			reply = do_read(common);
		break;

	case SC_READ_10:
		common->data_size_from_cmnd =
				get_unaligned_be16(&common->cmnd[7]) << 9;
		//common->data_size_from_cmnd = 0;
		//common->data_size = 0;
		reply = check_command(common, 10, DATA_DIR_TO_HOST,
				      (1<<1) | (0xf<<2) | (3<<7), 1,
				      "READ(10)");
		//halt_bulk_in_endpoint(common->fsg);
		reply = 0;
		if (reply == 0)
			reply = do_read0(common, bh);
		CSW_dft_status = 0x01;
		break;

	case SC_READ_12:
		common->data_size_from_cmnd =
				get_unaligned_be32(&common->cmnd[6]) << 9;
		reply = check_command(common, 12, DATA_DIR_TO_HOST,
				      (1<<1) | (0xf<<2) | (0xf<<6), 1,
				      "READ(12)");
		if (reply == 0)
			reply = do_read(common);
		break;

	case SC_READ_CAPACITY:
		common->data_size_from_cmnd = 8;
		reply = check_command(common, 10, DATA_DIR_TO_HOST,
				      (0xf<<2) | (1<<8), 1,
				      "READ CAPACITY");
		reply = 0;
		if (reply == 0)
			reply = do_read_capacity(common, bh);
		break;

	case SC_READ_HEADER:
		if (!common->curlun || !common->curlun->cdrom)
			goto unknown_cmnd;
		common->data_size_from_cmnd =
			get_unaligned_be16(&common->cmnd[7]);
		reply = check_command(common, 10, DATA_DIR_TO_HOST,
				      (3<<7) | (0x1f<<1), 1,
				      "READ HEADER");
		if (reply == 0)
			reply = do_read_header(common, bh);
		break;

	case SC_READ_TOC:
		if (!common->curlun || !common->curlun->cdrom)
			goto unknown_cmnd;
		common->data_size_from_cmnd =
			get_unaligned_be16(&common->cmnd[7]);
		reply = check_command(common, 10, DATA_DIR_TO_HOST,
				      (7<<6) | (1<<1), 1,
				      "READ TOC");
		if (reply == 0)
			reply = do_read_toc(common, bh);
		break;

	case SC_READ_FORMAT_CAPACITIES:
		common->data_size_from_cmnd =
			get_unaligned_be16(&common->cmnd[7]);
		reply = check_command(common, 10, DATA_DIR_TO_HOST,
				      (3<<7), 1,
				      "READ FORMAT CAPACITIES");
		reply = 0;
		if (reply == 0)
			reply = do_read_format_capacities(common, bh);
		break;

	case SC_REQUEST_SENSE:
		common->data_size_from_cmnd = common->cmnd[4];
		reply = check_command(common, 6, DATA_DIR_TO_HOST,
				      (1<<4), 0,
				      "REQUEST SENSE");
		if (reply == 0)
			reply = do_request_sense(common, bh);
		break;

	case SC_START_STOP_UNIT:
		common->data_size_from_cmnd = 0;
		reply = check_command(common, 6, DATA_DIR_NONE,
				      (1<<1) | (1<<4), 0,
				      "START-STOP UNIT");
		if (reply == 0)
			reply = do_start_stop(common);
		break;

	case SC_SYNCHRONIZE_CACHE:
		common->data_size_from_cmnd = 0;
		reply = check_command(common, 10, DATA_DIR_NONE,
				      (0xf<<2) | (3<<7), 1,
				      "SYNCHRONIZE CACHE");
		if (reply == 0)
			reply = do_synchronize_cache(common);
		break;

	case SC_TEST_UNIT_READY:
		common->data_size_from_cmnd = 0;
		reply = check_command(common, 6, DATA_DIR_NONE,
				0, 1,
				"TEST UNIT READY");
		CSW_dft_status = 0x01;
		break;

	/* Although optional, this command is used by MS-Windows.  We
	 * support a minimal version: BytChk must be 0. */
	/*case SC_VERIFY:
		common->data_size_from_cmnd = 0;
		reply = check_command(common, 10, DATA_DIR_NONE,
				      (1<<1) | (0xf<<2) | (3<<7), 1,
				      "VERIFY");
		if (reply == 0)
			reply = do_verify(common);
		break;

	case SC_WRITE_6:
		i = common->cmnd[4];
		common->data_size_from_cmnd = (i == 0 ? 256 : i) << 9;
		reply = check_command(common, 6, DATA_DIR_FROM_HOST,
				      (7<<1) | (1<<4), 1,
				      "WRITE(6)");
		if (reply == 0)
			reply = do_write(common);
		break;

	case SC_WRITE_10:
		common->data_size_from_cmnd =
				get_unaligned_be16(&common->cmnd[7]) << 9;
		reply = check_command(common, 10, DATA_DIR_FROM_HOST,
				      (1<<1) | (0xf<<2) | (3<<7), 1,
				      "WRITE(10)");
		if (reply == 0)
			reply = do_write(common);
		break;

	case SC_WRITE_12:
		common->data_size_from_cmnd =
				get_unaligned_be32(&common->cmnd[6]) << 9;
		reply = check_command(common, 12, DATA_DIR_FROM_HOST,
				      (1<<1) | (0xf<<2) | (0xf<<6), 1,
				      "WRITE(12)");
		if (reply == 0)
			reply = do_write(common);
		break;

	/* Some mandatory commands that we recognize but don't implement.
	 * They don't mean much in this setting.  It's left as an exercise
	 * for anyone interested to implement RESERVE and RELEASE in terms
	 * of Posix locks. */
	/*case SC_FORMAT_UNIT:
	case SC_RELEASE:
	case SC_RESERVE:
	case SC_SEND_DIAGNOSTIC:
		/* Fall through */

	/*default:
unknown_cmnd:
		common->data_size_from_cmnd = 0;
		sprintf(unknown, "Unknown x%02x", common->cmnd[0]);
		reply = check_command(common, common->cmnd_size,
				      DATA_DIR_UNKNOWN, 0xff, 0, unknown);
		if (reply == 0) {
			common->curlun->sense_data = SS_INVALID_COMMAND;
			reply = -EINVAL;
		}
		CSW_dft_status = 0x01;
		break;
	}*/
	//printk("do_scsi_command2\n");
	up_read(&common->filesem);
	//printk("do_scsi_command3\n");
	if (reply == -EINTR || signal_pending(current))
		return -EINTR;

	/* Set up the single reply buffer for finish_reply() */
	if (reply == -EINVAL)
		reply = 0;		/* Error reply length */
	if (reply >= 0 && common->data_dir == DATA_DIR_TO_HOST) {
		reply = min((u32) reply, common->data_size_from_cmnd);
		bh->inreq->length = reply;
		bh->state = BUF_STATE_FULL;
		common->residue -= reply;
	}				/* Otherwise it's already set */

	return 0;
}


/*-------------------------------------------------------------------------*/

static int received_cbw(struct fsg_dev *fsg, struct fsg_buffhd *bh)
{
	struct usb_request	*req = bh->outreq;
	struct fsg_bulk_cb_wrap	*cbw = req->buf;
	struct fsg_common	*common = fsg->common;
	//hex_dump(req->buf, 32);
	/* Was this a real packet?  Should it be ignored? */
	if (req->status || test_bit(IGNORE_BULK_OUT, &fsg->atomic_bitflags))
		return -EINVAL;

	/* Is the CBW valid? */
	//if (req->actual != USB_BULK_CB_WRAP_LEN ||
	//		cbw->Signature != cpu_to_le32(
	//			USB_BULK_CB_SIG)) {
	//	DBG(fsg, "invalid CBW: len %u sig 0x%x\n",
	//			req->actual,
	//			le32_to_cpu(cbw->Signature));

		/* The Bulk-only spec says we MUST stall the IN endpoint
		 * (6.6.1), so it's unavoidable.  It also says we must
		 * retain this state until the next reset, but there's
		 * no way to tell the controller driver it should ignore
		 * Clear-Feature(HALT) requests.
		 *
		 * We aren't required to halt the OUT endpoint; instead
		 * we can simply accept and discard any data received
		 * until the next reset. */
	//	wedge_bulk_in_endpoint(fsg);
	//	set_bit(IGNORE_BULK_OUT, &fsg->atomic_bitflags);
	//	return -EINVAL;
	//}

	/* Is the CBW meaningful? */
	//if (cbw->Lun >= FSG_MAX_LUNS || cbw->Flags & ~USB_BULK_IN_FLAG ||
	//		cbw->Length <= 0 || cbw->Length > MAX_COMMAND_SIZE) {
	//	DBG(fsg, "non-meaningful CBW: lun = %u, flags = 0x%x, "
	//			"cmdlen %u\n",
	//			cbw->Lun, cbw->Flags, cbw->Length);

		/* We can do anything we want here, so let's stall the
		 * bulk pipes if we are allowed to. */
	//	if (common->can_stall) {
	//		fsg_set_halt(fsg, fsg->bulk_out);
	//		halt_bulk_in_endpoint(fsg);
	//	}
	//	return -EINVAL;
	//}

	/* Save the command for later */
	//common->cmnd_size = cbw->Length;
	//memcpy(common->cmnd, cbw->CDB, common->cmnd_size);
	//if (cbw->Flags & USB_BULK_IN_FLAG)
		common->data_dir = DATA_DIR_FROM_HOST;
	//else
		common->data_dir = DATA_DIR_FROM_HOST;
	common->data_size = cbw->Length;//(u8)(req->buf[12])+(u8)((req->buf[13])<<8)+(u8)((req->buf[14])<<16)+(u8)((req->buf[15])<<24);//le32_to_cpu(cbw->DataTransferLength);
	common->datatype = cbw->Signature;
	//printk("common->data_size0x%x\n", common->data_size);
	if (common->data_size == 0)
		common->data_dir = DATA_DIR_FROM_HOST;
	//common->lun = cbw->Lun;
	//common->tag = cbw->Tag;
	return 0;
}

static int get_next_command(struct fsg_common *common)
{
	struct fsg_buffhd	*bh;
	int			rc = 0;
	int rxsucc = 1;
	u32			amount_left_to_req, amount_left_to_write;
	MNSPXACTHDR udp_hdr;
	/* Wait for the next buffer to become available */
	bh = common->next_buffhd_to_fill;
	while (bh->state != BUF_STATE_EMPTY) {
		rc = sleep_thread(common);
		if (rc)
			return rc;
	}

	/* Queue a request to read a Bulk-only CBW */
	//set_bulk_out_req_length(common, bh, USB_BULK_CB_WRAP_LEN);
	bh->outreq->length = USB_BULK_CB_WRAP_LEN;
	bh->outreq->short_not_ok = 0;
	//printk("I_CMD%d:%d\n",bh->outreq->length, bh->state);
	START_TRANSFER_OR(common, bulk_out, bh->outreq,
			  &bh->outreq_busy, &bh->state)
		/* Don't know what to do if common->fsg is NULL */
		//return -EIO;

	/* We will drain the buffer in software, which means we
	 * can reuse it for the next filling.  No need to advance
	 * next_buffhd_to_fill. */

	/* Wait for the CBW to arrive */
	//while (bh->state != BUF_STATE_FULL) {
	//	printk("123456\n");
	//	rc = sleep_thread(common);
	//	if (rc)
	//		return rc;
	//}
	if(bh->state == BUF_STATE_FULL)
	{
		rxsucc = 0;
	}
	else
	{
		rxsucc = 1;
	}
	while (rxsucc) {
		rc = sleep_thread(common);
		if (rc)
			return rc;
		if(bh->state == BUF_STATE_FULL)
		{
			rxsucc = 0;
		}
		else
		{
			rxsucc = 1;
		}
	}
	//printk("rc1:%d,%d\n",rc, bh->state);
	smp_rmb();
	rc = fsg_is_set(common) ? received_cbw(common->fsg, bh) : -EIO;

	if(common->datatype == 0x00)//video
	{
		//if(g_prevHtimestamp == 0)
		//{
		//	g_udpTransfer_flag = 1;
		//	g_currentHtimestamp = GetTimeStamp();
		//	g_prevHtimestamp = g_currentHtimestamp;
		//}
		//else
		//{
		//	if(g_udpTransfer_flag == 1)
		//	{
		//		g_prevHtimestamp = g_currentHtimestamp;
		//	}
		//	g_currentHtimestamp = GetTimeStamp();
			//printk("%d, %d\n",(common->fsg->common->data_size/1024), ((((u32)(g_currentHtimestamp - g_prevHtimestamp))/1000)*(((skip_rate_control*1024)/8)/1000)));
		//	if((common->fsg->common->data_size/1024) > ((((u32)(g_currentHtimestamp - g_prevHtimestamp))/1000)*(((skip_rate_control*1024)/8)/1000)))
		//	{
		//		printk("skip\n");
		//		printk("%d, %d\n",(common->fsg->common->data_size/1024), ((((u32)(g_currentHtimestamp - g_prevHtimestamp))/1000)*(((skip_rate_control*1024)/8)/1000)));
		//		g_udpTransfer_flag = 0;
		//	}
		//	else
		//	{
				g_udpTransfer_flag = 1;
		//	}
		//}
	}
	else if(common->datatype == 0x03)//audio
	{
		g_udpTransfer_flag = 1;
	}
	else if(common->datatype == 0x05)//rom
	{
		g_udpTransfer_flag = 1;
	}

	if(common->datatype == 0x00)//video
	{
		memcpy(&pingpong[0]->buf[g_v_offset+0], &udp_hdr, sizeof(udp_hdr));
		memcpy(&pingpong[0]->buf[g_v_offset+sizeof(udp_hdr)], bh->outreq->buf, USB_BULK_CB_WRAP_LEN);
	}
	else if(common->datatype == 0x03)//audio
	{
		memcpy(&stable_bps_pingpong[0]->buf[g_a_offset+0], &udp_hdr, sizeof(udp_hdr));
		memcpy(&stable_bps_pingpong[0]->buf[g_a_offset+sizeof(udp_hdr)], bh->outreq->buf, USB_BULK_CB_WRAP_LEN);
	}
	else if(common->datatype == 0x05)//rom
	{
		g_updatefwbuf = vmalloc(common->data_size + USB_BULK_CB_WRAP_LEN);
		printk("g_updatefwbuf:%p\n",g_updatefwbuf);
		memcpy(g_updatefwbuf, bh->outreq->buf, USB_BULK_CB_WRAP_LEN);
	}
	//memcpy(pingpong[0]->buf, &udp_hdr, sizeof(udp_hdr));
	//memcpy(&pingpong[0]->buf[sizeof(udp_hdr)], bh->outreq->buf, USB_BULK_CB_WRAP_LEN);
	bh->state = BUF_STATE_EMPTY;
	//printk("rc2:%d\n",rc);
#if 0
	amount_left_to_req = common->data_size;
	//amount_left_to_write = common->data_size_from_cmnd;
	printk("do_write12\n");
	while(amount_left_to_req)
	{
		//bh = common->next_buffhd_to_fill;
		rxsucc = 1;
		if(bh->state == BUF_STATE_EMPTY)
		{
			rxsucc = 0;
		}
		else
		{
			rxsucc = 1;
		}
		while (rxsucc) {
			rc = sleep_thread(common);
			if (rc)
				return rc;
			if(bh->state == BUF_STATE_EMPTY)
			{
				rxsucc = 0;
			}
			else
			{
				rxsucc = 1;
			}
		}

		/* Queue a request to read a Bulk-only CBW */
		//set_bulk_out_req_length(common, bh, USB_BULK_CB_WRAP_LEN);
		if(amount_left_to_req > FSG_BUFLEN)
		{
			amount_left_to_write = FSG_BUFLEN;
		}
		else
		{
			amount_left_to_write = amount_left_to_req;
		}
		bh->outreq->length = amount_left_to_write;
		bh->outreq->short_not_ok = 0;
		bh->outreq->zero = 1;
		printk("I_DATA%d:%d\n",bh->outreq->length, bh->state);
		START_TRANSFER_OR(common, bulk_out, bh->outreq,
				  &bh->outreq_busy, &bh->state)
		/* Don't know what to do if common->fsg is NULL */
		//return -EIO;

		/* We will drain the buffer in software, which means we
		 * can reuse it for the next filling.  No need to advance
		 * next_buffhd_to_fill. */

		/* Wait for the CBW to arrive */
		//while (bh->state != BUF_STATE_FULL) {
		//	printk("123456\n");
		//	rc = sleep_thread(common);
		//	if (rc)
		//		return rc;
		//}
		rxsucc = 1;
		if(bh->state == BUF_STATE_FULL)
		{
			rxsucc = 0;
		}
		else
		{
			rxsucc = 1;
		}
		while (rxsucc) {
			//printk("123456\n");
			rc = sleep_thread(common);
			if (rc)
				return rc;
			if(bh->state == BUF_STATE_FULL)
			{
				rxsucc = 0;
			}
			else
			{
				rxsucc = 1;
			}
		}
		printk("rc3:%d,%d\n",rc, bh->state);
		smp_rmb();
		//rc = fsg_is_set(common) ? received_cbw(common->fsg, bh) : -EIO;
		//deal net transfer
		amount_left_to_req = amount_left_to_req - amount_left_to_write;
		bh->state = BUF_STATE_EMPTY;
	}
#endif
	return rc;
}


/*-------------------------------------------------------------------------*/

static int enable_endpoint(struct fsg_common *common, struct usb_ep *ep,
		const struct usb_endpoint_descriptor *d)
{
	int	rc;

	ep->driver_data = common;
	rc = usb_ep_enable(ep, d);
	if (rc)
		ERROR(common, "can't enable %s, result %d\n", ep->name, rc);
	return rc;
}

static int alloc_request(struct fsg_common *common, struct usb_ep *ep,
		struct usb_request **preq)
{
	*preq = usb_ep_alloc_request(ep, GFP_ATOMIC);
	if (*preq)
		return 0;
	ERROR(common, "can't allocate request for %s\n", ep->name);
	return -ENOMEM;
}

/* Reset interface setting and re-init endpoint state (toggle etc). */
static int do_set_interface(struct fsg_common *common, struct fsg_dev *new_fsg)
{
	const struct usb_endpoint_descriptor *d;
	struct fsg_dev *fsg;
	int i, rc = 0;

	if (common->running)
		DBG(common, "reset interface\n");

reset:
	/* Deallocate the requests */
	if (common->fsg) {
		fsg = common->fsg;

		for (i = 0; i < FSG_NUM_BUFFERS; ++i) {
			struct fsg_buffhd *bh = &common->buffhds[i];

			if (bh->inreq) {
				usb_ep_free_request(fsg->bulk_in, bh->inreq);
				bh->inreq = NULL;
			}
			if (bh->outreq) {
				usb_ep_free_request(fsg->bulk_out, bh->outreq);
				bh->outreq = NULL;
			}
		}

		/* Disable the endpoints */
		if (fsg->bulk_in_enabled) {
			usb_ep_disable(fsg->bulk_in);
			fsg->bulk_in_enabled = 0;
		}
		if (fsg->bulk_out_enabled) {
			usb_ep_disable(fsg->bulk_out);
			fsg->bulk_out_enabled = 0;
		}

		common->fsg = NULL;
		wake_up(&common->fsg_wait);
	}

	common->running = 0;
	if (!new_fsg || rc)
		return rc;

	common->fsg = new_fsg;
	fsg = common->fsg;

	printk("fsg enable ep\n");
	/* Enable the endpoints */
	d = fsg_ep_desc(common->gadget,
			&fsg_fs_bulk_in_desc, &fsg_hs_bulk_in_desc);
	rc = enable_endpoint(common, fsg->bulk_in, d);
	if (rc)
		goto reset;
	fsg->bulk_in_enabled = 1;

	d = fsg_ep_desc(common->gadget,
			&fsg_fs_bulk_out_desc, &fsg_hs_bulk_out_desc);
	rc = enable_endpoint(common, fsg->bulk_out, d);
	if (rc)
		goto reset;
	usb_ep_enable(g_hidg->in_ep, &hidg0_hs_in_ep_desc);
	fsg->bulk_out_enabled = 1;
	common->bulk_out_maxpacket = le16_to_cpu(d->wMaxPacketSize);
	clear_bit(IGNORE_BULK_OUT, &fsg->atomic_bitflags);

	/* Allocate the requests */
	for (i = 0; i < FSG_NUM_BUFFERS; ++i) {
		struct fsg_buffhd	*bh = &common->buffhds[i];

		rc = alloc_request(common, fsg->bulk_in, &bh->inreq);
		if (rc)
			goto reset;
		rc = alloc_request(common, fsg->bulk_out, &bh->outreq);
		if (rc)
			goto reset;
		bh->inreq->buf = bh->outreq->buf = bh->buf;
		bh->inreq->context = bh->outreq->context = bh;
		bh->inreq->complete = bulk_in_complete;
		bh->outreq->complete = bulk_out_complete;
	}

	common->running = 1;
	for (i = 0; i < common->nluns; ++i)
		common->luns[i].unit_attention_data = SS_RESET_OCCURRED;
	return rc;
}


/****************************** ALT CONFIGS ******************************/


static int fsg_set_alt(struct usb_function *f, unsigned intf, unsigned alt)
{
	struct fsg_dev *fsg = fsg_from_func(f);
	fsg->common->new_fsg = fsg;
	raise_exception(fsg->common, FSG_STATE_CONFIG_CHANGE);
	return 0;
}

static void fsg_disable(struct usb_function *f)
{
	struct fsg_dev *fsg = fsg_from_func(f);
	fsg->common->new_fsg = NULL;
	raise_exception(fsg->common, FSG_STATE_CONFIG_CHANGE);
}


/*-------------------------------------------------------------------------*/

static void handle_exception(struct fsg_common *common)
{
	siginfo_t		info;
	int			i;
	struct fsg_buffhd	*bh;
	enum fsg_state		old_state;
	struct fsg_lun		*curlun;
	unsigned int		exception_req_tag;

	/* Clear the existing signals.  Anything but SIGUSR1 is converted
	 * into a high-priority EXIT exception. */
	for (;;) {
		int sig =
			dequeue_signal_lock(current, &current->blocked, &info);
		if (!sig)
			break;
		if (sig != SIGUSR1) {
			if (common->state < FSG_STATE_EXIT)
				DBG(common, "Main thread exiting on signal\n");
			raise_exception(common, FSG_STATE_EXIT);
		}
	}

	/* Cancel all the pending transfers */
	if (likely(common->fsg)) {
		for (i = 0; i < FSG_NUM_BUFFERS; ++i) {
			bh = &common->buffhds[i];
			if (bh->inreq_busy)
				usb_ep_dequeue(common->fsg->bulk_in, bh->inreq);
			if (bh->outreq_busy)
				usb_ep_dequeue(common->fsg->bulk_out,
					       bh->outreq);
		}

		/* Wait until everything is idle */
		for (;;) {
			int num_active = 0;
			for (i = 0; i < FSG_NUM_BUFFERS; ++i) {
				bh = &common->buffhds[i];
				num_active += bh->inreq_busy + bh->outreq_busy;
			}
			if (num_active == 0)
				break;
			if (sleep_thread(common))
				return;
		}

		/* Clear out the controller's fifos */
		if (common->fsg->bulk_in_enabled)
			usb_ep_fifo_flush(common->fsg->bulk_in);
		if (common->fsg->bulk_out_enabled)
			usb_ep_fifo_flush(common->fsg->bulk_out);
	}

	/* Reset the I/O buffer states and pointers, the SCSI
	 * state, and the exception.  Then invoke the handler. */
	spin_lock_irq(&common->lock);

	for (i = 0; i < FSG_NUM_BUFFERS; ++i) {
		bh = &common->buffhds[i];
		bh->state = BUF_STATE_EMPTY;
	}
	common->next_buffhd_to_fill = &common->buffhds[0];
	common->next_buffhd_to_drain = &common->buffhds[0];
	exception_req_tag = common->exception_req_tag;
	old_state = common->state;

	if (old_state == FSG_STATE_ABORT_BULK_OUT)
		common->state = FSG_STATE_STATUS_PHASE;
	else {
		for (i = 0; i < common->nluns; ++i) {
			curlun = &common->luns[i];
			curlun->prevent_medium_removal = 0;
			curlun->sense_data = SS_NO_SENSE;
			curlun->unit_attention_data = SS_NO_SENSE;
			curlun->sense_data_info = 0;
			curlun->info_valid = 0;
		}
		common->state = FSG_STATE_IDLE;
	}
	spin_unlock_irq(&common->lock);

	/* Carry out any extra actions required for the exception */
	switch (old_state) {
	case FSG_STATE_ABORT_BULK_OUT:
		send_status(common);
		spin_lock_irq(&common->lock);
		if (common->state == FSG_STATE_STATUS_PHASE)
			common->state = FSG_STATE_IDLE;
		spin_unlock_irq(&common->lock);
		break;

	case FSG_STATE_RESET:
		/* In case we were forced against our will to halt a
		 * bulk endpoint, clear the halt now.  (The SuperH UDC
		 * requires this.) */
		if (!fsg_is_set(common))
			break;
		if (test_and_clear_bit(IGNORE_BULK_OUT,
				       &common->fsg->atomic_bitflags))
			usb_ep_clear_halt(common->fsg->bulk_in);

		if (common->ep0_req_tag == exception_req_tag)
			ep0_queue(common);	/* Complete the status stage */

		/* Technically this should go here, but it would only be
		 * a waste of time.  Ditto for the INTERFACE_CHANGE and
		 * CONFIG_CHANGE cases. */
		/* for (i = 0; i < common->nluns; ++i) */
		/*	common->luns[i].unit_attention_data = */
		/*		SS_RESET_OCCURRED;  */
		break;

	case FSG_STATE_CONFIG_CHANGE:
		do_set_interface(common, common->new_fsg);
		break;

	case FSG_STATE_EXIT:
	case FSG_STATE_TERMINATED:
		do_set_interface(common, NULL);		/* Free resources */
		spin_lock_irq(&common->lock);
		common->state = FSG_STATE_TERMINATED;	/* Stop the thread */
		spin_unlock_irq(&common->lock);
		break;

	case FSG_STATE_INTERFACE_CHANGE:
	case FSG_STATE_DISCONNECT:
	case FSG_STATE_COMMAND_PHASE:
	case FSG_STATE_DATA_PHASE:
	case FSG_STATE_STATUS_PHASE:
	case FSG_STATE_IDLE:
		break;
	}
}


/*-------------------------------------------------------------------------*/
#define CURSOR_TCP_PORT 22672
#define TCP_PORT 22671
#define UDP_PORT 22670
#define CURSOR_PORT 22669
#define JWR_ROM_TCP_PORT    22800

u32 create_address(u8 *ip)
{
        u32 addr = 0;
        int i;

        for(i=0; i<4; i++)
        {
                addr += ip[i];
                if(i==3)
                        break;
                addr <<= 8;
        }
        return addr;
}

#define WACOM_TOUCH_LEN 64
static unsigned char hid_udp_buf[WACOM_TOUCH_LEN];
static unsigned char usb_in_idx = 0;
static unsigned char socket_out_idx = 0;

static int stable_bps_ksocket_send_thread(struct fsg_common	*common)
{
	struct usb_udp_pingpong* ksocket_send_pingpong = stable_bps_pingpong[0];
	u32 i;
	int udp_send_size;
	printk("start stable_bps_ksocket...\n");
	while(1)
	{
#if 0
		if(prev_interval_total_bytes != interval_total_bytes)
		{
			if(interval_total_bytes == 1*1024*1024)
			{
				from_rx_current_bps = interval_total_bytes + 1*4*1024*1024;
			}
			else
			{
				if(interval_total_bytes > udp_send_total_bytes)
				{
					from_rx_current_bps = from_rx_current_bps + 1*2*1024*1024;
					if(from_rx_current_bps > 10*2*1024*1024)
					{
						from_rx_current_bps = 10*2*1024*1024;
					}
				}
				else if(interval_total_bytes == udp_send_total_bytes)
				{
					from_rx_current_bps = from_rx_current_bps + 1*2*1024*1024;
					if(from_rx_current_bps > 10*2*1024*1024)
					{
						from_rx_current_bps = 10*2*1024*1024;
					}
				}
				else if((udp_send_total_bytes - interval_total_bytes) < (1*1024*1024))
				{
					from_rx_current_bps = from_rx_current_bps + 1*1024*1024;
					if(from_rx_current_bps > 10*2*1024*1024)
					{
						from_rx_current_bps = 10*2*1024*1024;
					}
				}
				else if((udp_send_total_bytes - interval_total_bytes) > (1*2*1024*1024))
				{
					from_rx_current_bps = from_rx_current_bps - 1*1024*1024;
					if(from_rx_current_bps < (1*1024*1024 + 1*4*1024*1024))
					{
						from_rx_current_bps = (1*1024*1024 + 1*4*1024*1024);
					}
				}
				else
				{
					from_rx_current_bps = from_rx_current_bps - 1*512*1024;
					if(from_rx_current_bps < (1*1024*1024 + 1*4*1024*1024))
					{
						from_rx_current_bps = (1*1024*1024 + 1*4*1024*1024);
					}
				}
				udp_send_total_bytes = 0;
			}
			prev_interval_total_bytes = interval_total_bytes;
		}
#endif
		//reg_write(((0xB0120000) + 0x800), 0x3F000060);
		if(ksocket_send_pingpong->status == PINGPONG_BUSY)
		{
			for(i=0;i<ksocket_send_pingpong->validblockcount;i++)
			{
				udp_send_size = ksocket_send(g_Uconn_socket, &g_Usaddr, &ksocket_send_pingpong->buf[i*UDP_DATABLOCK_SIZE], UDP_DATABLOCK_SIZE);
				udp_send_total_bytes = udp_send_total_bytes + udp_send_size;
				msleep(2*1000*UDP_DATABLOCK_SIZE/from_rx_current_bps);
			}
			printk("ksocket_out:%d,Bps:%d\n",ksocket_send_pingpong->buf[6], from_rx_current_bps);
			ksocket_send_pingpong->validblockcount = 0;
			ksocket_send_pingpong->status = PINGPONG_IDLE;
			ksocket_send_pingpong = ksocket_send_pingpong->next;
			//reg_write(((0xB0120000) + 0x800), 0x3F000063);
			if(g_current_stall == 1)
			{
				//usb_ep_clear_halt(common->fsg->bulk_out);
				g_current_stall = 0;
			}
		}
		else
		{
			msleep(2*1000*UDP_DATABLOCK_SIZE/from_rx_current_bps);
		}
	}
}

static int hid_udp_main_thread(void)
{
	int ret = 0;
	unsigned int axis_x, axis_y;
	printk("start hid_udp...\n");
	while(1)
	{
		ret = ksocket_receive(g_Tconn_socket, &g_Tsaddr, hid_udp_buf, WACOM_TOUCH_LEN);
		//printk("get hid_udp...\n");
		axis_x = hid_udp_buf[4]+(hid_udp_buf[5]<<8);
		axis_y = hid_udp_buf[6]+(hid_udp_buf[7]<<8);
		if(axis_x > 0x5890)
			printk("X:%d\n",axis_x);
		if(axis_y > 0x3280)
			printk("Y:%d\n",axis_y);
		my_f_hidg_write(hid_udp_buf, ret);
		sleep_thread(&hid_udp_common);
	}
}
extern unsigned char t6_vendor_cmd;
spinlock_t cursor_lock;
unsigned long cursor_flags;
unsigned char shapeusbin[10] = {0,0,0,0,0,0,0,0,0,0};
static int cursor_udp_main_thread(void)
{
	int ret = 0;
	int udp_send_size;
	struct fsg_common common;
	printk("start cursor_udp...\n");
	while(1)
	{
		sleep_thread(&cursor_udp_common);
		if(g_UCURconn_socket && g_TCURconn_socket)
		{
			udp_send_size = ksocket_send(g_UCURconn_socket, &g_UCURsaddr, g_udp_cursor_buf, 64);
			udp_send_total_bytes = udp_send_total_bytes + udp_send_size;
			g_udp_control_XactId++;
			g_led_net_send_total_block = g_led_net_send_total_block + udp_send_size;
		}
		smp_wmb();
		spin_unlock_irqrestore(&cursor_lock, cursor_flags);
	}
}

static int cursor_tcp_main_thread(void)
{
	int ret = 0;
	int udp_send_size;
	int i;
	struct fsg_common common;
	MNSPXACTHDR udp_hdr, tcp_hdr;
	printk("start cursor_tcp...\n");
	while(1)
	{
		sleep_thread(&cursor_tcp_common);
		if(g_UCURconn_socket && g_TCURconn_socket)
		{
			if(t6_vendor_cmd == VENDOR_REQ_SET_CURSOR_SHAPE || t6_vendor_cmd == VENDOR_REQ_SET_CURSOR1_SHAPE || t6_vendor_cmd == VENDOR_REQ_SET_CURSOR2_SHAPE || t6_vendor_cmd == VENDOR_REQ_SET_CURSOR1_STATE || t6_vendor_cmd == VENDOR_REQ_SET_CURSOR2_STATE)
			{
				for(i=0;i<10;i++)
				{
					if(shapeusbin[i])
					{
						do
						{
							udp_send_size = ksocket_send(g_UCURconn_socket, &g_UCURsaddr, g_udp_cursor_shape_buf[i], 20*1024);
							udp_send_total_bytes = udp_send_total_bytes + udp_send_size;
							g_udp_control_XactId++;
							g_led_net_send_total_block = g_led_net_send_total_block + udp_send_size;
							ret = ksocket_receive(g_TCURconn_socket, &g_TCURsaddr, g_tcp_cursor_buf, 32);
						}while(ret < 0);
						memcpy(&udp_hdr, g_tcp_cursor_buf, 32);
						shapeusbin[udp_hdr.Flags] = 0;
					}
				}
			}
			if(t6_vendor_cmd == VENDOR_REQ_SET_CURSOR1_STATE || t6_vendor_cmd == VENDOR_REQ_SET_CURSOR2_STATE)
			{
				do
				{
					memcpy(&tcp_hdr, g_udp_cursor_state_buf, 32);
					udp_send_size = ksocket_send(g_UCURconn_socket, &g_UCURsaddr, g_udp_cursor_state_buf, 64);
					udp_send_total_bytes = udp_send_total_bytes + udp_send_size;
					g_udp_control_XactId++;
					g_led_net_send_total_block = g_led_net_send_total_block + udp_send_size;
					ret = ksocket_receive(g_TCURconn_socket, &g_TCURsaddr, g_tcp_cursor_buf, 32);
					memcpy(&udp_hdr, g_tcp_cursor_buf, 32);
					if(tcp_hdr.XactId == udp_hdr.XactId)
					{

					}
					else
					{
						ret = -1;
					}
				}while(ret < 0);
			}
		}
		smp_wmb();
		spin_unlock_irqrestore(&cursor_lock, cursor_flags);
	}
}

unsigned char g_led_blink_lv = 0;
unsigned char g_led_blink_lv_prev = 0;
static int led_blink_lv_main_thread(void)
{
	struct file *fp;
    mm_segment_t fs;
    loff_t pos;
	int ret;
	printk("start led_blink_lv...\n");
	while(1)
	{
		if(g_led_net_send_total_block > (8*1024*1024))
		{
			g_led_blink_lv = 1;
		}
		else if(g_led_net_send_total_block > (6*1024*1024))
		{
			g_led_blink_lv = 3;
		}
		else if(g_led_net_send_total_block > (4*1024*1024))
		{
			g_led_blink_lv = 5;
		}
		else if(g_led_net_send_total_block > (2*1024*1024))
		{
			g_led_blink_lv = 7;
		}
		else if(g_led_net_send_total_block > 0)
		{
			g_led_blink_lv = 9;
		}
		else
		{
			g_led_blink_lv = 0;
		}
		g_led_net_send_total_block = 0;
	
		if(g_led_blink_lv_prev == g_led_blink_lv)
		{

		}
		else
		{
			fp =filp_open("/var/ledblinklv/lv",O_RDWR | O_CREAT,0666);
		    if(IS_ERR(fp)){
		        printk("create file error\n");
		        return -1;
		    }
		    fs =get_fs();
		    set_fs(KERNEL_DS);
		    pos = 0;
			ret = vfs_write(fp,(char __user *)&g_led_blink_lv, 1, &pos);
			printk("ret:%d\n",ret);
		    filp_close(fp,NULL);
		    set_fs(fs);

			g_led_blink_lv_prev = g_led_blink_lv;
		}
		msleep(1000);
	}
}

#define SERVER_IP_NAME  "br0"
int tcp_buf_n = 4*1024*1024;
int udp_buf_n = 4*1024*1024;
int curudp_buf_n = 4*1024*1024;
int curtcp_buf_n = 4*1024*1024;
int romtcp_buf_n = 4*1024*1024;

u32 get_default_ipaddr_by_devname(struct socket *sock, const char *devname)
{
	u32 addr;
	struct net *init_net;
	struct net_device *dev;
	if (!devname)
		return 0;
	/* find netdev by name, increment refcnt */
	init_net = sock_net(sock->sk);
	dev=dev_get_by_name(init_net, devname);
	if (!dev)
		return 0;
	/* get ip addr from rtable (global scope) */
	addr = inet_select_addr(dev, 0, RT_SCOPE_UNIVERSE);
	/* decrement netdev refcnt */
	dev_put(dev);
	return addr;
}

unsigned char UVC_Buff[0x20000];

static int fsg_main_thread(void *common_)
{
    int ret = -1;
	struct fsg_common	*common = common_;
	struct task_struct	*hid_udp_thread_task;
	struct task_struct	*cursor_udp_thread_task;
	struct task_struct	*cursor_tcp_thread_task;
	struct task_struct	*ksocket_send_thread_task;
	struct task_struct	*led_blink_lv_thread_task;
	struct timeval tv;
	unsigned int test_jpg_offset;
	
	pingpong[0] = kzalloc(sizeof *pingpong[0], GFP_KERNEL);
	pingpong[1] = kzalloc(sizeof *pingpong[1], GFP_KERNEL);
	pingpong[0]->buf = kmalloc(UDP_DATABLOCK_SIZE, GFP_KERNEL);
	pingpong[1]->buf = kmalloc(UDP_DATABLOCK_SIZE, GFP_KERNEL);
	pingpong[0]->next = pingpong[1];
	pingpong[1]->next = pingpong[0];
	pingpong[0]->status = PINGPONG_IDLE;
	pingpong[1]->status = PINGPONG_IDLE;

	stable_bps_pingpong[0] = kzalloc(sizeof *stable_bps_pingpong[0], GFP_KERNEL);
	stable_bps_pingpong[1] = kzalloc(sizeof *stable_bps_pingpong[1], GFP_KERNEL);
	stable_bps_pingpong[2] = kzalloc(sizeof *stable_bps_pingpong[2], GFP_KERNEL);
	//stable_bps_pingpong[3] = kzalloc(sizeof *stable_bps_pingpong[3], GFP_KERNEL);
	//stable_bps_pingpong[4] = kzalloc(sizeof *stable_bps_pingpong[4], GFP_KERNEL);
	stable_bps_pingpong[0]->buf = kmalloc(60*1024, GFP_KERNEL);
	stable_bps_pingpong[1]->buf = kmalloc(60*1024, GFP_KERNEL);
	stable_bps_pingpong[2]->buf = kmalloc(60*1024, GFP_KERNEL);
	//stable_bps_pingpong[3]->buf = kmalloc(1024*1024, GFP_KERNEL);
	//stable_bps_pingpong[4]->buf = kmalloc(1024*1024, GFP_KERNEL);
	stable_bps_pingpong[0]->prev = stable_bps_pingpong[2];
	stable_bps_pingpong[1]->prev = stable_bps_pingpong[0];
	stable_bps_pingpong[2]->prev = stable_bps_pingpong[1];
	//stable_bps_pingpong[3]->prev = stable_bps_pingpong[2];
	//stable_bps_pingpong[4]->prev = stable_bps_pingpong[3];
	stable_bps_pingpong[0]->next = stable_bps_pingpong[1];
	stable_bps_pingpong[1]->next = stable_bps_pingpong[2];
	stable_bps_pingpong[2]->next = stable_bps_pingpong[0];
	//stable_bps_pingpong[3]->next = stable_bps_pingpong[4];
	//stable_bps_pingpong[4]->next = stable_bps_pingpong[0];
	stable_bps_pingpong[0]->status = PINGPONG_IDLE;
	stable_bps_pingpong[1]->status = PINGPONG_IDLE;
	stable_bps_pingpong[2]->status = PINGPONG_IDLE;
	//stable_bps_pingpong[3]->status = PINGPONG_IDLE;
	//stable_bps_pingpong[4]->status = PINGPONG_IDLE;
	stable_bps_pingpong[0]->validblockcount = 0;
	stable_bps_pingpong[1]->validblockcount = 0;
	stable_bps_pingpong[2]->validblockcount = 0;
	//stable_bps_pingpong[3]->validblockcount = 0;
	//stable_bps_pingpong[4]->validblockcount = 0;
	g_temp_bps_pingpong = stable_bps_pingpong[0];
	
	/* Allow the thread to be killed by a signal, but set the signal mask
	 * to block everything but INT, TERM, KILL, and USR1. */
	allow_signal(SIGINT);
	allow_signal(SIGTERM);
	allow_signal(SIGKILL);
	allow_signal(SIGUSR1);
	printk("from_bc_uint_destip:%x %x\n",from_bc_uint_destip,htonl(create_address(g_destip)));
	
	DECLARE_WAIT_QUEUE_HEAD(recv_wait);
	ret = sock_create(AF_INET, SOCK_DGRAM, IPPROTO_UDP, &g_Tconn_socket);
	if(ret < 0)
	{
		printk(" *** Tmtp | Error: %d while creating first socket. | setup_connection *** \n", ret);
		//goto err;
	}
	ret = sock_create(AF_INET, SOCK_DGRAM, IPPROTO_UDP, &g_Uconn_socket);
	if(ret < 0)
	{
		printk(" *** Umtp | Error: %d while creating first socket. | setup_connection *** \n", ret);
		//goto err;
	}
	ret = sock_create(AF_INET, SOCK_DGRAM, IPPROTO_UDP, &g_UCURconn_socket);
	if(ret < 0)
	{
		printk(" *** UCURmtp | Error: %d while creating first socket. | setup_connection *** \n", ret);
		//goto err;
	}
	ret = sock_create(AF_INET, SOCK_DGRAM, IPPROTO_UDP, &g_TCURconn_socket);
	if(ret < 0)
	{
		printk(" *** TCURmtp | Error: %d while creating first socket. | setup_connection *** \n", ret);
		//goto err;
	}
	ret = sock_create(PF_INET, SOCK_STREAM, IPPROTO_TCP, &g_TROMconn_socket);
	if(ret < 0)
	{
		printk(" *** TROMmtp | Error: %d while creating first socket. | setup_connection *** \n", ret);
		//goto err;
	}
	memset(&g_Tsaddr, 0, sizeof(g_Tsaddr));
    g_Tsaddr.sin_family = AF_INET;
    g_Tsaddr.sin_port = htons(TCP_PORT);
	g_Tsaddr.sin_addr.s_addr = get_default_ipaddr_by_devname(g_Tconn_socket, SERVER_IP_NAME);
	memset(&g_Usaddr, 0, sizeof(g_Usaddr));
    g_Usaddr.sin_family = AF_INET;
    g_Usaddr.sin_port = htons(UDP_PORT);
    //g_Usaddr.sin_addr.s_addr = htonl(create_address(g_destip));
	g_Usaddr.sin_addr.s_addr = from_bc_uint_destip;
	memset(&g_UCURsaddr, 0, sizeof(g_UCURsaddr));
    g_UCURsaddr.sin_family = AF_INET;
    g_UCURsaddr.sin_port = htons(CURSOR_PORT);
    //g_UCURsaddr.sin_addr.s_addr = htonl(create_address(g_destip));
	g_UCURsaddr.sin_addr.s_addr = from_bc_uint_destip;
	memset(&g_TCURsaddr, 0, sizeof(g_TCURsaddr));
    g_TCURsaddr.sin_family = AF_INET;
    g_TCURsaddr.sin_port = htons(CURSOR_TCP_PORT);
    //g_TCURsaddr.sin_addr.s_addr = htonl(create_address(g_destip));
	g_TCURsaddr.sin_addr.s_addr = get_default_ipaddr_by_devname(g_TCURconn_socket, SERVER_IP_NAME);
	memset(&g_TROMsaddr, 0, sizeof(g_TROMsaddr));
    g_TROMsaddr.sin_family = AF_INET;
    g_TROMsaddr.sin_port = htons(JWR_ROM_TCP_PORT);
    //g_TROMsaddr.sin_addr.s_addr = htonl(create_address(g_destip));
	g_TROMsaddr.sin_addr.s_addr = from_bc_uint_destip;

	ret = sock_setsockopt(g_Tconn_socket, SOL_SOCKET, SO_RCVBUF, &tcp_buf_n, sizeof(tcp_buf_n));
    if(ret && (ret != -EINPROGRESS))
    {
        printk(" *** Tmtp | Error: %d setsockopt. | option *** \n", ret);
        //goto err;
    }
	ret = sock_setsockopt(g_Uconn_socket, SOL_SOCKET, SO_SNDBUF, &udp_buf_n, sizeof(udp_buf_n));
    if(ret && (ret != -EINPROGRESS))
    {
        printk(" *** Umtp | Error: %d setsockopt. | option *** \n", ret);
        //goto err;
    }
	ret = sock_setsockopt(g_UCURconn_socket, SOL_SOCKET, SO_SNDBUF, &curudp_buf_n, sizeof(curudp_buf_n));
    if(ret && (ret != -EINPROGRESS))
    {
        printk(" *** UCURmtp | Error: %d setsockopt. | option *** \n", ret);
        //goto err;
    }
	ret = sock_setsockopt(g_TCURconn_socket, SOL_SOCKET, SO_RCVBUF, &curtcp_buf_n, sizeof(curtcp_buf_n));
    if(ret && (ret != -EINPROGRESS))
    {
        printk("0 *** TCURmtp | Error: %d setsockopt. | option *** \n", ret);
        //goto err;
    }
	ret = sock_setsockopt(g_TROMconn_socket, SOL_SOCKET, SO_SNDBUF, &romtcp_buf_n, sizeof(romtcp_buf_n));
    if(ret && (ret != -EINPROGRESS))
    {
        printk(" *** TROMmtp | Error: %d setsockopt. | option *** \n", ret);
        //goto err;
    }
	tv.tv_sec =  1;  
	tv.tv_usec = 0;  
	ret = sock_setsockopt(g_TCURconn_socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(struct timeval));
	if(ret && (ret != -EINPROGRESS))
	{
		printk("1 *** TCURmtp | Error: %d setsockopt. | option *** \n", ret);
		//goto err;
	}
	ret = g_Tconn_socket->ops->bind(g_Tconn_socket, (struct sockaddr *)&g_Tsaddr, sizeof(g_Tsaddr));
	if(ret && (ret != -EINPROGRESS))
    {
        printk(" *** Tmtp | Error: %d while bind using conn socket. | setup_connection *** \n", ret);
        //goto err;
    }
	ret = g_TCURconn_socket->ops->bind(g_TCURconn_socket, (struct sockaddr *)&g_TCURsaddr, sizeof(g_TCURsaddr));
	if(ret && (ret != -EINPROGRESS))
    {
        printk(" *** TCURmtp | Error: %d while bind using conn socket. | setup_connection *** \n", ret);
        //goto err;
    }
	ret = g_TROMconn_socket->ops->connect(g_TROMconn_socket, (struct sockaddr *)&g_TROMsaddr, sizeof(g_TROMsaddr), O_RDWR);
    if(ret && (ret != -EINPROGRESS))
    {
        printk(" *** TROMmtp | Error: %d while connecting using conn socket. | setup_connection *** \n", ret);
        //goto err;
    }

	/* Allow the thread to be frozen */
	set_freezable();

	/* Arrange for userspace references to be interpreted as kernel
	 * pointers.  That way we can pass a kernel pointer to a routine
	 * that expects a __user pointer and it will work okay. */
	set_fs(get_ds());
	hid_udp_thread_task = kthread_create(hid_udp_main_thread, NULL, "hid_udp");
	hid_udp_common.thread_task = hid_udp_thread_task;
	wake_up_process(hid_udp_thread_task);
	cursor_udp_thread_task = kthread_create(cursor_udp_main_thread, NULL, "cursor_udp");
	cursor_udp_common.thread_task = cursor_udp_thread_task;
	wake_up_process(cursor_udp_thread_task);
	cursor_tcp_thread_task = kthread_create(cursor_tcp_main_thread, NULL, "cursor_tcp");
	cursor_tcp_common.thread_task = cursor_tcp_thread_task;
	wake_up_process(cursor_tcp_thread_task);
	ksocket_send_thread_task = kthread_create(stable_bps_ksocket_send_thread, common, "ksocket_send");
	ksocket_send_common.thread_task = ksocket_send_thread_task;
	wake_up_process(ksocket_send_thread_task);

	led_blink_lv_thread_task = kthread_create(led_blink_lv_main_thread, NULL, "led_blink_lv");
	led_blink_lv_common.thread_task = led_blink_lv_thread_task;
	wake_up_process(led_blink_lv_thread_task);

	g_userspaceUVC_common = common;
	open_1280x720JPG(&UVC_Buff[2], 97549);
	UVC_Buff[0] = 0x02;
	UVC_Buff[1] = 0x82;
		
	/* The main loop */
	while (common->state != FSG_STATE_TERMINATED) {
		if (exception_in_progress(common) || signal_pending(current)) {
			handle_exception(common);
			continue;
		}

		if (!common->running) {
			sleep_thread(common);
			continue;
		}
		//printk();

		printk("sleep uvc kthread\n");
		sleep_thread(common);
		test_jpg_offset = 0;
		while(1)
		{
			printk("usb replug\n");
			handle_exception(common);
			sleep_thread(common);
#if 0
			printk("send 512B\n");
			send_UVB_bulk(g_userspaceUVC_common, 512, &UVC_Buff[test_jpg_offset]);
			test_jpg_offset = test_jpg_offset + 512;
			if((test_jpg_offset+512) > 97551)
			{
				//common->state = FSG_STATE_IDLE;
				send_UVB_bulk(g_userspaceUVC_common, 97551-test_jpg_offset, &UVC_Buff[test_jpg_offset]);
				if(UVC_Buff[1] == 0x82)
				{
					UVC_Buff[1] = 0x83;
				}
				else
				{
					UVC_Buff[1] = 0x82;
				}
				test_jpg_offset = 0;
			}
			//common->state = FSG_STATE_IDLE;
#endif
		}
		
		if (get_next_command(common))
			continue;

		spin_lock_irq(&common->lock);
		if (!exception_in_progress(common))
			common->state = FSG_STATE_DATA_PHASE;
		spin_unlock_irq(&common->lock);

		if (do_scsi_command(common) || finish_reply(common))
			continue;

		spin_lock_irq(&common->lock);
		//if (!exception_in_progress(common))
		//	common->state = FSG_STATE_STATUS_PHASE;
		spin_unlock_irq(&common->lock);

		//if (send_status(common))
		//	continue;
		
		//if(common->cmnd[0] == 0x00)
		//{
			//my_f_hidg_write(g_hid_m_buffer, 4);
			//my_f_hidg_write(g_hid_k_buffer, 8);
		//}

		spin_lock_irq(&common->lock);
		if (!exception_in_progress(common))
			common->state = FSG_STATE_IDLE;
		spin_unlock_irq(&common->lock);
	}

	spin_lock_irq(&common->lock);
	common->thread_task = NULL;
	spin_unlock_irq(&common->lock);

	if (!common->ops || !common->ops->thread_exits
	 || common->ops->thread_exits(common) < 0) {
		struct fsg_lun *curlun = common->luns;
		unsigned i = common->nluns;

		down_write(&common->filesem);
		for (; i--; ++curlun) {
			if (!fsg_lun_is_open(curlun))
				continue;

			fsg_lun_close(curlun);
			curlun->unit_attention_data = SS_MEDIUM_NOT_PRESENT;
		}
		up_write(&common->filesem);
	}

	/* Let the unbind and cleanup routines know the thread has exited */
	complete_and_exit(&common->thread_notifier, 0);
}


/*************************** DEVICE ATTRIBUTES ***************************/

/* Write permission is checked per LUN in store_*() functions. */
static DEVICE_ATTR(ro, 0644, fsg_show_ro, fsg_store_ro);
static DEVICE_ATTR(file, 0644, fsg_show_file, fsg_store_file);


/****************************** FSG COMMON ******************************/

static void fsg_common_release(struct kref *ref);

static void fsg_lun_release(struct device *dev)
{
	/* Nothing needs to be done */
}

static inline void fsg_common_get(struct fsg_common *common)
{
	kref_get(&common->ref);
}

static inline void fsg_common_put(struct fsg_common *common)
{
	kref_put(&common->ref, fsg_common_release);
}


static struct fsg_common *fsg_common_init(struct fsg_common *common,
					  struct usb_composite_dev *cdev,
					  struct fsg_config *cfg)
{
	struct usb_gadget *gadget = cdev->gadget;
	struct fsg_buffhd *bh;
	struct fsg_lun *curlun;
	struct fsg_lun_config *lcfg;
	int nluns, i, rc;
	char *pathbuf;

	/* Find out how many LUNs there should be */
	nluns = cfg->nluns;
	if (nluns < 1 || nluns > FSG_MAX_LUNS) {
		dev_err(&gadget->dev, "invalid number of LUNs: %u\n", nluns);
		return ERR_PTR(-EINVAL);
	}

	/* Allocate? */
	if (!common) {
		common = kzalloc(sizeof *common, GFP_KERNEL);
		if (!common)
			return ERR_PTR(-ENOMEM);
		common->free_storage_on_release = 1;
	} else {
		memset(common, 0, sizeof common);
		common->free_storage_on_release = 0;
	}

	common->ops = cfg->ops;
	common->private_data = cfg->private_data;

	common->gadget = gadget;
	common->ep0 = gadget->ep0;
	common->ep0req = cdev->req;

	/* Maybe allocate device-global string IDs, and patch descriptors */
	if (fsg_strings[FSG_STRING_INTERFACE].id == 0) {
		rc = usb_string_id(cdev);
		if (unlikely(rc < 0))
			goto error_release;
		fsg_strings[FSG_STRING_INTERFACE].id = rc;
		fsg_intf_desc.iInterface = 0x00;//rc;
	}

	/* Create the LUNs, open their backing files, and register the
	 * LUN devices in sysfs. */
	curlun = kzalloc(nluns * sizeof *curlun, GFP_KERNEL);
	if (unlikely(!curlun)) {
		rc = -ENOMEM;
		goto error_release;
	}
	common->luns = curlun;

	init_rwsem(&common->filesem);

	for (i = 0, lcfg = cfg->luns; i < nluns; ++i, ++curlun, ++lcfg) {
		curlun->cdrom = !!lcfg->cdrom;
		curlun->ro = lcfg->cdrom || lcfg->ro;
		curlun->removable = lcfg->removable;
		curlun->dev.release = fsg_lun_release;
		curlun->dev.parent = &gadget->dev;
		/* curlun->dev.driver = &fsg_driver.driver; XXX */
		dev_set_drvdata(&curlun->dev, &common->filesem);
		dev_set_name(&curlun->dev,
			     cfg->lun_name_format
			   ? cfg->lun_name_format
			   : "lun%d",
			     i);

		rc = device_register(&curlun->dev);
		if (rc) {
			INFO(common, "failed to register LUN%d: %d\n", i, rc);
			common->nluns = i;
			goto error_release;
		}

		rc = device_create_file(&curlun->dev, &dev_attr_ro);
		if (rc)
			goto error_luns;
		rc = device_create_file(&curlun->dev, &dev_attr_file);
		if (rc)
			goto error_luns;

		if (lcfg->filename) {
			rc = fsg_lun_open(curlun, lcfg->filename);
			if (rc)
				goto error_luns;
		} else if (!curlun->removable) {
			ERROR(common, "no file given for LUN%d\n", i);
			rc = -EINVAL;
			goto error_luns;
		}
	}
	common->nluns = nluns;


	/* Data buffers cyclic list */
	bh = common->buffhds;
	i = FSG_NUM_BUFFERS;
	goto buffhds_first_it;
	do {
		bh->next = bh + 1;
		++bh;
buffhds_first_it:
		bh->buf = kmalloc(FSG_BUFLEN, GFP_KERNEL);
		if (unlikely(!bh->buf)) {
			rc = -ENOMEM;
			goto error_release;
		}
	} while (--i);
	bh->next = common->buffhds;


	/* Prepare inquiryString */
	if (cfg->release != 0xffff) {
		i = cfg->release;
	} else {
		i = usb_gadget_controller_number(gadget);
		if (i >= 0) {
			i = 0x0300 + i;
		} else {
			WARNING(common, "controller '%s' not recognized\n",
				gadget->name);
			i = 0x0399;
		}
	}
#define OR(x, y) ((x) ? (x) : (y))
	snprintf(common->inquiry_string, sizeof common->inquiry_string,
		 "%-8s%-16s%04x",
		 OR(cfg->vendor_name, "Linux   "),
		 /* Assume product name dependent on the first LUN */
		 OR(cfg->product_name, common->luns->cdrom
				     ? "File-Stor Gadget"
				     : "File-CD Gadget  "),
		 i);


	/* Some peripheral controllers are known not to be able to
	 * halt bulk endpoints correctly.  If one of them is present,
	 * disable stalls.
	 */
	common->can_stall = cfg->can_stall &&
		!(gadget_is_at91(common->gadget));


	spin_lock_init(&common->lock);
	kref_init(&common->ref);


	/* Tell the thread to start working */
	common->thread_task =
		kthread_create(fsg_main_thread, common,
			       OR(cfg->thread_name, "t6dongle_udp"));
	if (IS_ERR(common->thread_task)) {
		rc = PTR_ERR(common->thread_task);
		goto error_release;
	}
	init_completion(&common->thread_notifier);
	init_waitqueue_head(&common->fsg_wait);
#undef OR


	/* Information */
	INFO(common, FSG_DRIVER_DESC ", version: " FSG_DRIVER_VERSION "\n");
	INFO(common, "Number of LUNs=%d\n", common->nluns);

	pathbuf = kmalloc(PATH_MAX, GFP_KERNEL);
	for (i = 0, nluns = common->nluns, curlun = common->luns;
	     i < nluns;
	     ++curlun, ++i) {
		char *p = "(no medium)";
		if (fsg_lun_is_open(curlun)) {
			p = "(error)";
			if (pathbuf) {
				p = d_path(&curlun->filp->f_path,
					   pathbuf, PATH_MAX);
				if (IS_ERR(p))
					p = "(error)";
			}
		}
		LINFO(curlun, "LUN: %s%s%sfile: %s\n",
		      curlun->removable ? "removable " : "",
		      curlun->ro ? "read only " : "",
		      curlun->cdrom ? "CD-ROM " : "",
		      p);
	}
	kfree(pathbuf);

	DBG(common, "I/O thread pid: %d\n", task_pid_nr(common->thread_task));

	wake_up_process(common->thread_task);

	return common;


error_luns:
	common->nluns = i + 1;
error_release:
	common->state = FSG_STATE_TERMINATED;	/* The thread is dead */
	/* Call fsg_common_release() directly, ref might be not
	 * initialised */
	fsg_common_release(&common->ref);
	return ERR_PTR(rc);
}


static void fsg_common_release(struct kref *ref)
{
	struct fsg_common *common = container_of(ref, struct fsg_common, ref);

	/* If the thread isn't already dead, tell it to exit now */
	if (common->state != FSG_STATE_TERMINATED) {
		raise_exception(common, FSG_STATE_EXIT);
		wait_for_completion(&common->thread_notifier);

		/* The cleanup routine waits for this completion also */
		complete(&common->thread_notifier);
	}

	if (likely(common->luns)) {
		struct fsg_lun *lun = common->luns;
		unsigned i = common->nluns;

		/* In error recovery common->nluns may be zero. */
		for (; i; --i, ++lun) {
			device_remove_file(&lun->dev, &dev_attr_ro);
			device_remove_file(&lun->dev, &dev_attr_file);
			fsg_lun_close(lun);
			device_unregister(&lun->dev);
		}

		kfree(common->luns);
	}

	{
		struct fsg_buffhd *bh = common->buffhds;
		unsigned i = FSG_NUM_BUFFERS;
		do {
			kfree(bh->buf);
		} while (++bh, --i);
	}

	if (common->free_storage_on_release)
		kfree(common);
}


/*-------------------------------------------------------------------------*/


static void fsg_unbind(struct usb_configuration *c, struct usb_function *f)
{
	struct fsg_dev		*fsg = fsg_from_func(f);
	struct fsg_common	*common = fsg->common;

	DBG(fsg, "unbind\n");
	if (fsg->common->fsg == fsg) {
		fsg->common->new_fsg = NULL;
		raise_exception(fsg->common, FSG_STATE_CONFIG_CHANGE);
		/* FIXME: make interruptible or killable somehow? */
		wait_event(common->fsg_wait, common->fsg != fsg);
	}

	kfree(pingpong[0]->buf);
	kfree(pingpong[1]->buf);
	kfree(pingpong[0]);
	kfree(pingpong[1]);
	sock_release(g_Tconn_socket);
	sock_release(g_Uconn_socket);
	sock_release(g_UCURconn_socket);
	g_Tconn_socket = NULL;
	g_Uconn_socket = NULL;
	g_UCURconn_socket = NULL;
	kthread_stop(hid_udp_common.thread_task);
	kthread_stop(cursor_udp_common.thread_task);

	fsg_common_put(common);
	usb_free_descriptors(fsg->function.descriptors);
	usb_free_descriptors(fsg->function.hs_descriptors);
	kfree(fsg);
}


static int fsg_bind(struct usb_configuration *c, struct usb_function *f)
{
	struct fsg_dev		*fsg = fsg_from_func(f);
	struct usb_gadget	*gadget = c->cdev->gadget;
	int			i;
	struct usb_ep		*ep;

	fsg->gadget = gadget;

	/* New interface */
	i = usb_interface_id(c, f);
	if (i < 0)
		return i;
	fsg_intf_desc.bInterfaceNumber = 0;//i;
	fsg->interface_number = i;

	/* Find all the endpoints we will use */
	ep = usb_ep_autoconfig(gadget, &fsg_fs_bulk_in_desc);
	if (!ep)
		goto autoconf_fail;
	ep->driver_data = fsg->common;	/* claim the endpoint */
	fsg->bulk_in = ep;

	ep = usb_ep_autoconfig(gadget, &fsg_fs_bulk_out_desc);
	if (!ep)
		goto autoconf_fail;
	ep->driver_data = fsg->common;	/* claim the endpoint */
	fsg->bulk_out = ep;

	/* Copy descriptors */
	f->descriptors = usb_copy_descriptors(fsg_fs_function);
	if (unlikely(!f->descriptors))
		return -ENOMEM;

	if (gadget_is_dualspeed(gadget)) {
		/* Assume endpoint addresses are the same for both speeds */
		fsg_hs_bulk_in_desc.bEndpointAddress =
			fsg_fs_bulk_in_desc.bEndpointAddress;
		fsg_hs_bulk_out_desc.bEndpointAddress =
			fsg_fs_bulk_out_desc.bEndpointAddress;
		M_uvc_control_header.wTotalLength = sizeof(M_uvc_control_header) + sizeof(M_uvc_camera_terminal) + sizeof(M_uvc_processing) + sizeof(M_uvc_output_terminal) + sizeof(M_uvc_extension);
		M_uvc_input_header.wTotalLength = sizeof(M_uvc_input_header) + sizeof(M_uvc_format_mjpg) + sizeof(M_uvc_frame_mjpg_720p) + sizeof(M_uvc_frame_mjpg_1080p) + sizeof(M_uvc_color_matching_mjpeg) + sizeof(M_uvc_format_h264) + sizeof(M_uvc_frame_h264_720p) + sizeof(M_uvc_frame_h264_1080p) + sizeof(M_uvc_color_matching_h264);
		f->hs_descriptors = usb_copy_descriptors(fsg_hs_function);
		if (unlikely(!f->hs_descriptors)) {
			usb_free_descriptors(f->descriptors);
			return -ENOMEM;
		}
	}

	return 0;

autoconf_fail:
	ERROR(fsg, "unable to autoconfigure all endpoints\n");
	return -ENOTSUPP;
}


/****************************** ADD FUNCTION ******************************/

static struct usb_gadget_strings *fsg_strings_array[] = {
	&fsg_stringtab,
	NULL,
};

static int fsg_bind_config(struct usb_composite_dev *cdev,
			   struct usb_configuration *c,
			   struct fsg_common *common)
{
	struct fsg_dev *fsg;
	int rc;

	fsg = kzalloc(sizeof *fsg, GFP_KERNEL);
	if (unlikely(!fsg))
		return -ENOMEM;

	fsg->function.name        = FSG_DRIVER_DESC;
	fsg->function.strings     = fsg_strings_array;
	fsg->function.bind        = fsg_bind;
	fsg->function.unbind      = fsg_unbind;
	fsg->function.setup       = fsg_setup;
	fsg->function.set_alt     = fsg_set_alt;
	fsg->function.disable     = fsg_disable;

	fsg->common               = common;
	/* Our caller holds a reference to common structure so we
	 * don't have to be worry about it being freed until we return
	 * from this function.  So instead of incrementing counter now
	 * and decrement in error recovery we increment it only when
	 * call to usb_add_function() was successful. */

	rc = usb_add_function(c, &fsg->function);
	if (unlikely(rc))
		kfree(fsg);
	else
		fsg_common_get(fsg->common);
	return rc;
}

static inline int __deprecated __maybe_unused
fsg_add(struct usb_composite_dev *cdev,
	struct usb_configuration *c,
	struct fsg_common *common)
{
	return fsg_bind_config(cdev, c, common);
}


/************************* Module parameters *************************/


struct fsg_module_parameters {
	char		*file[FSG_MAX_LUNS];
	int		ro[FSG_MAX_LUNS];
	int		removable[FSG_MAX_LUNS];
	int		cdrom[FSG_MAX_LUNS];

	unsigned int	file_count, ro_count, removable_count, cdrom_count;
	unsigned int	luns;	/* nluns */
	int		stall;	/* can_stall */
};


#define _FSG_MODULE_PARAM_ARRAY(prefix, params, name, type, desc)	\
	module_param_array_named(prefix ## name, params.name, type,	\
				 &prefix ## params.name ## _count,	\
				 S_IRUGO);				\
	MODULE_PARM_DESC(prefix ## name, desc)

#define _FSG_MODULE_PARAM(prefix, params, name, type, desc)		\
	module_param_named(prefix ## name, params.name, type,		\
			   S_IRUGO);					\
	MODULE_PARM_DESC(prefix ## name, desc)

#define FSG_MODULE_PARAMETERS(prefix, params)				\
	_FSG_MODULE_PARAM_ARRAY(prefix, params, file, charp,		\
				"names of backing files or devices");	\
	_FSG_MODULE_PARAM_ARRAY(prefix, params, ro, bool,		\
				"true to force read-only");		\
	_FSG_MODULE_PARAM_ARRAY(prefix, params, removable, bool,	\
				"true to simulate removable media");	\
	_FSG_MODULE_PARAM_ARRAY(prefix, params, cdrom, bool,		\
				"true to simulate CD-ROM instead of disk"); \
	_FSG_MODULE_PARAM(prefix, params, luns, uint,			\
			  "number of LUNs");				\
	_FSG_MODULE_PARAM(prefix, params, stall, bool,			\
			  "false to prevent bulk stalls")


static void
fsg_config_from_params(struct fsg_config *cfg,
		       const struct fsg_module_parameters *params)
{
	struct fsg_lun_config *lun;
	unsigned i;

	/* Configure LUNs */
	cfg->nluns =
		min(params->luns ?: (params->file_count ?: 1u),
		    (unsigned)FSG_MAX_LUNS);
	for (i = 0, lun = cfg->luns; i < cfg->nluns; ++i, ++lun) {
		lun->ro = !!params->ro[i];
		lun->cdrom = !!params->cdrom[i];
		lun->removable = /* Removable by default */
			params->removable_count <= i || params->removable[i];
		lun->filename =
			params->file_count > i && params->file[i][0]
			? params->file[i]
			: 0;
	}

	/* Let MSF use defaults */
	cfg->lun_name_format = 0;
	cfg->thread_name = 0;
	cfg->vendor_name = 0;
	cfg->product_name = 0;
	cfg->release = 0xffff;

	cfg->ops = NULL;
	cfg->private_data = NULL;

	/* Finalise */
	cfg->can_stall = params->stall;
}

static inline struct fsg_common *
fsg_common_from_params(struct fsg_common *common,
		       struct usb_composite_dev *cdev,
		       const struct fsg_module_parameters *params)
	__attribute__((unused));
static inline struct fsg_common *
fsg_common_from_params(struct fsg_common *common,
		       struct usb_composite_dev *cdev,
		       const struct fsg_module_parameters *params)
{
	struct fsg_config cfg;
	fsg_config_from_params(&cfg, params);
	return fsg_common_init(common, cdev, &cfg);
}

