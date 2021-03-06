echo 'start patch Kernel files'
echo '-------------------------------------------'
echo 'start patch drivers'
echo ''
echo 'start patch char device'
cp drivers/char/ralink_gpio.c ../../../linux-2.6.36.x/drivers/char/ralink_gpio.c
cp drivers/char/ralink_gdma.c ../../../linux-2.6.36.x/drivers/char/ralink_gdma.c
cp drivers/char/i2s/i2s_ctrl.h ../../../linux-2.6.36.x/drivers/char/i2s/i2s_ctrl.h
cp drivers/char/i2s/i2s_ctrl.c ../../../linux-2.6.36.x/drivers/char/i2s/i2s_ctrl.c
echo ''
echo 'start patch usb gadget driver'
cp drivers/usb/gadget/composite.c ../../../linux-2.6.36.x/drivers/usb/gadget/composite.c
cp drivers/usb/gadget/f_mass_storage.c ../../../linux-2.6.36.x/drivers/usb/gadget/f_mass_storage.c
cp drivers/usb/gadget/hid.c ../../../linux-2.6.36.x/drivers/usb/gadget/hid.c
cp drivers/usb/gadget/mass_storage.c ../../../linux-2.6.36.x/drivers/usb/gadget/mass_storage.c
cp drivers/usb/gadget/rt_udc.h ../../../linux-2.6.36.x/drivers/usb/gadget/rt_udc.h
cp drivers/usb/gadget/rt_udc_pdma.c ../../../linux-2.6.36.x/drivers/usb/gadget/rt_udc_pdma.c
cp drivers/usb/gadget/storage_common.c ../../../linux-2.6.36.x/drivers/usb/gadget/storage_common.c
cp drivers/usb/gadget/include/video.h ../../../linux-2.6.36.x/include/linux/usb/video.h
echo 'start patch UVC driver'
cp drivers/media/video/uvc/uvc_ctrl.c ../../../linux-2.6.36.x/drivers/media/video/uvc/uvc_ctrl.c
cp drivers/media/video/uvc/uvc_video.c ../../../linux-2.6.36.x/drivers/media/video/uvc/uvc_video.c
cp drivers/media/video/uvc/uvc_v4l2.c ../../../linux-2.6.36.x/drivers/media/video/uvc/uvc_v4l2.c
cp drivers/media/video/uvc/uvc_driver.c ../../../linux-2.6.36.x/drivers/media/video/uvc/uvc_driver.c
cp drivers/media/video/uvc/uvcvideo.h ../../../linux-2.6.36.x/drivers/media/video/uvc/uvcvideo.h
cp drivers/usb/core/hub.c ../../../linux-2.6.36.x/drivers/usb/core/hub.c
echo 'start patch network related driver'
echo 'start patch raeth'
cp drivers/net/raeth/raether.c ../../../linux-2.6.36.x/drivers/net/raeth/raether.c
echo 'start patch rlt_wifi'
cp drivers/net/wireless/rlt_wifi/common/ee_flash.c ../../../linux-2.6.36.x/drivers/net/wireless/rlt_wifi/common/ee_flash.c
cp drivers/net/wireless/rlt_wifi/common/wsc.c ../../../linux-2.6.36.x/drivers/net/wireless/rlt_wifi/common/wsc.c
cp drivers/net/wireless/rlt_wifi/common/ba_action.c ../../../linux-2.6.36.x/drivers/net/wireless/rlt_wifi/common/ba_action.c
cp drivers/net/wireless/rlt_wifi/ap/ap_cfg.c ../../../linux-2.6.36.x/drivers/net/wireless/rlt_wifi/ap/ap_cfg.c
cp drivers/net/wireless/rlt_wifi/os/linux/ap_ioctl.c ../../../linux-2.6.36.x/drivers/net/wireless/rlt_wifi/os/linux/ap_ioctl.c
cp drivers/net/wireless/rlt_wifi/include/os/rt_os.h ../../../linux-2.6.36.x/drivers/net/wireless/rlt_wifi/include/os/rt_os.h
cp drivers/net/wireless/rlt_wifi/include/rtmp_cmd.h ../../../linux-2.6.36.x/drivers/net/wireless/rlt_wifi/include/rtmp_cmd.h
echo 'start patch include'
cp include/linux/usb/video.h ../../../linux-2.6.36.x/include/linux/usb/video.h
cp include/linux/videodev2.h ../../../linux-2.6.36.x/include/linux/videodev2.h
echo 'start patch Kconfig'
cp mct ../../../linux-2.6.36.x/. -r
cp arch/mips/Kconfig ../../../linux-2.6.36.x/arch/mips/Kconfig
#echo 'start patch mtd'
#cp drivers/mtd/ralink_spi.c ../../../linux-2.6.36.x/drivers/mtd/ralink/ralink_spi.c
echo ''
echo 'end of patch Kernel files'
echo '-------------------------------------------'
