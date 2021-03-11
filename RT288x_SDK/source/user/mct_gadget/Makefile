EXEC = mct_gadget
OBJS = mct_gadget.o uvcdev.o
EXEC_TOOL = create_uvc_conf
OBJS_TOOL = create_uvc_conf.o
CFLAGS += -I$(ROOTDIR)/lib/libnvram
CFLAGS += -I$(ROOTDIR)/$(LINUXDIR)/include
CFLAGS += -I$(ROOTDIR)/$(LINUXDIR)/drivers/char/i2s -I$(ROOTDIR)/$(LINUXDIR)/drivers/char

LDFLAGS += -lpthread -L$(ROOTDIR)/lib/libnvram

CONF_H  = $(ROOTDIR)/$(LINUXDIR)/include/linux/autoconf.h
I2S_CTRL_H  = $(ROOTDIR)/$(LINUXDIR)/drivers/char/i2s/i2s_ctrl.h

all: clean $(EXEC) $(EXEC_TOOL)

$(EXEC):$(OBJS) $(CONF_H) $(I2S_CTRL_H)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) -lnvram -lrt

$(EXEC_TOOL):$(OBJS_TOOL)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS_TOOL)

clean:
	rm -f *.o $(EXEC)

romfs:
	for i in $(EXEC) ; do $(ROMFSINST) /bin/$$i ; done
	for i in $(EXEC_TOOL) ; do $(ROMFSINST) /bin/$$i ; done
	$(ROMFSINST) bg /home/bg/.
