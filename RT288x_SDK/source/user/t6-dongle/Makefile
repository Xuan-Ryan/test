

obj-m := usbdongledrv.o 
usbdongledrv-objs = usb-kernel-driver/usbdongle.o



KDIR := $(ROOTDIR)/$(LINUXDIR) 
PWD	:=$(shell pwd)
EXTRA_CFLAGS += -mhard-float
LDFLAGS=

CFLAGS = -I$(ROOTDIR)/lib/libusb-1.0.0 -I$(ROOTDIR)/user/alsa-lib-1.0.27.2/alsa_install/include
CFLAGS += -I$(ROOTDIR)/lib/libffmpeg-0.5
CFLAGS += -I$(ROOTDIR)/lib/include 
 
#CFLAGS += -I$(ROOTDIR)/user/t6-dongle/v4l2-ctl/include
LIBS = -L$(ROOTDIR)/lib/lib -L$(ROOTDIR)/lib/libusb-1.0.0 -L$(ROOTDIR)/user/alsa-lib-1.0.27.2/alsa_install/lib  -lasound -lusb -lm -ldl -lpthread -lrt -lz -lavcodec -lavformat -lavutil 

#INC += -I$(ROOTDIR)/lib/libusb-1.0.0/libusb
INC += -I$(ROOTDIR)/lib/libusb-0.1.12
LIBA += -L$(ROOTDIR)/lib/libusb-0.1.12 -lc -lpthread -lusb -lm 
#LIBA += -L$(ROOTDIR)/lib/libusb-1.0.0 -lc -lpthread -lusb -lm
all:
#	$(MAKE) -C $(KDIR) ARCH=$(ARCH) SUBDIRS=$(PWD) modules
#	$(CC) -o Lantousb Lantousb.c tcpserver.c -lc -lpthread -lm 
#	$(CC) -o displayserver displayserver.c tcpserver.c queue.c t6usbdongle.c t6libusb.c $(INC) $(LIBA)
#	$(CC) -o t6usbupdate t6usbupdate.c tcpserver.c  -lc -lpthread -lm
#	$(CC) -o mctousb  mctousb.c queue.c t6usbdongle.c tcpserver.c -lc -lpthread -lm -L$(ROOTDIR)/lib/libusb-0.1.12 -I$(ROOTDIR)/lib/libusb-0.1.12 
#	$(CC) -o aoatousb aoaserver/usbaccessory.c $(LIBS) $(CFLAGS)
#	$(CC) -o uvcserver uvcserver.c libv4l2/*.c libv4lconvert/*.c libv4lconvert/processing/*.c libv4lconvert/control/*.c $(LIBS) $(CFLAGS)
#	$(CC) -o T6-utility T6-utility.c t6libusb.c parse_edid.c t6usbdongle.c $(INC) $(LIBA)
#	$(CC) -o hid-example hid-example.c  -lc -lpthread -lm
#	$(CC) -o aoamode aoasw/aoasw.c -lusb -L$(ROOTDIR)/lib/libusb-0.1.12 -I$(ROOTDIR)/lib/libusb-0.1.12 
#	$(CC) -o uvcclient uvcmain.c v4l2uvc.c alsauvc.c $(LIBS) $(CFLAGS)
	$(CC) -o uvcclient uvchost/uvchost.c uvchost/alsauvc.c uvchost/network.c uvchost/uvcdescription.c $(LIBS) $(CFLAGS)	
#	$(CC) -o uvcserver uvcserver.c $(LIBS) $(CFLAGS)  
romfs:
#	 cp -af usbdongledrv.ko $(ROOTDIR)/romfs/lib/modules
#	 cp -af aoadrv.ko $(ROOTDIR)/romfs/lib/modules
#	 cp -af ds_detect.sh $(ROOTDIR)/romfs/home
#	 cp -af network_detect.sh $(ROOTDIR)/romfs/home 
#	 $(ROMFSINST) Lantousb /usr/bin 
#	 $(ROMFSINST) mctousb /usr/bin 
#	 $(ROMFSINST) aoatousb /usr/bin 	
#	 $(ROMFSINST) T6-utility /usr/bin 
#	 $(ROMFSINST) aoamode /usr/bin 
#	 $(ROMFSINST) displayserver /usr/bin 
	 $(ROMFSINST) uvcclient /usr/bin 
#	 $(ROMFSINST) uvcserver /usr/bin 
#	 $(ROMFSINST) hid-example /usr/bin 
clean:
	$(MAKE) -C $(KDIR) ARCH=$(ARCH) SUBDIRS=$(PWD) clean
	rm -f *.bak modules.order 
	rm -f Lantousb *.o *.bak
	rm -f mctousb *.o *.bak
	rm -f aoatousb *.o *.bak



