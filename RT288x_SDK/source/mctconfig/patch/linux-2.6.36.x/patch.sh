echo 'start patch Kernel files'
echo '-------------------------------------------'
echo 'start patch drivers'
echo ''
echo 'start patch char device'
cp drivers/char/ralink_gpio.c ../../../linux-2.6.36.x/drivers/char/ralink_gpio.c
cp drivers/char/i2s_ctrl.h ../../../linux-2.6.36.x/drivers/char/i2s_ctrl.h
#echo 'start patch rlt_wifi'
#cp drivers/rlt_wifi/ap_cfg.c ../../../linux-2.6.36.x/drivers/net/wireless/rlt_wifi/ap/ap_cfg.c
#cp drivers/rlt_wifi/ap_ioctl.c ../../../linux-2.6.36.x/drivers/net/wireless/rlt_wifi/os/linux/ap_ioctl.c
#cp drivers/rlt_wifi/rt_os.h ../../../linux-2.6.36.x/drivers/net/wireless/rlt_wifi/include/os/rt_os.h
#cp drivers/rlt_wifi/rtmp_cmd.h ../../../linux-2.6.36.x/drivers/net/wireless/rlt_wifi/include/rtmp_cmd.h
#cp drivers/rlt_wifi/ee_flash.c ../../../linux-2.6.36.x/drivers/net/wireless/rlt_wifi/common/ee_flash.c
#echo ''
#echo 'start patch raeth'
#cp drivers/raeth/raether.c ../../../linux-2.6.36.x/drivers/net/raeth/raether.c
#echo ''
#echo 'start patch mtd'
#cp drivers/mtd/ralink_spi.c ../../../linux-2.6.36.x/drivers/mtd/ralink/ralink_spi.c
echo ''
echo 'end of patch Kernel files'
echo '-------------------------------------------'
