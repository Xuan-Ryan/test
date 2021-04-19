EXEC = mct_gadget
UPDATE_TOOL = updater
OBJS_UPDATE = updater.o
LOADBG_TOOL = loadbg
OBJ_LOADBG = loadbg.o uvcdev.o
OBJS = mct_gadget.o uvcdev.o queue.o
EXEC_TOOL = create_uvc_conf
OBJS_TOOL = create_uvc_conf.o
CFLAGS += -I$(ROOTDIR)/lib/libnvram
CFLAGS += -I$(ROOTDIR)/$(LINUXDIR)/include
CFLAGS += -I$(ROOTDIR)/$(LINUXDIR)/drivers/char/i2s -I$(ROOTDIR)/$(LINUXDIR)/drivers/char
CFLAGS += -I$(ROOTDIR)/user/t6-dongle

LDFLAGS += -lpthread -L$(ROOTDIR)/lib/libnvram

CONF_H  = $(ROOTDIR)/$(LINUXDIR)/include/linux/autoconf.h
I2S_CTRL_H  = $(ROOTDIR)/$(LINUXDIR)/drivers/char/i2s/i2s_ctrl.h

all: clean $(EXEC) $(EXEC_TOOL) $(UPDATE_TOOL) $(LOADBG_TOOL)

$(EXEC):$(OBJS) $(CONF_H) $(I2S_CTRL_H)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) -lnvram -lrt

$(EXEC_TOOL):$(OBJS_TOOL)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS_TOOL)

$(UPDATE_TOOL): $(OBJS_UPDATE)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS_UPDATE)

$(LOADBG_TOOL): $(OBJ_LOADBG)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJ_LOADBG) -lnvram

clean:
	rm -f *.o $(EXEC) $(EXEC_TOOL) $(UPDATE_TOOL) $(LOADBG_TOOL)

romfs:
	for i in $(EXEC) ; do $(ROMFSINST) /bin/$$i ; done
	for i in $(EXEC_TOOL) ; do $(ROMFSINST) /bin/$$i ; done
	for i in $(UPDATE_TOOL) ; do $(ROMFSINST) /bin/$$i ; done
	for i in $(LOADBG_TOOL) ; do $(ROMFSINST) /bin/$$i ; done
	$(ROMFSINST) bg /home/bg/.
