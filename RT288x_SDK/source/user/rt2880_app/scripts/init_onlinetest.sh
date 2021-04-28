#!/bin/sh

calibrated=`nvram_get 2860 Calibrated`

if [ "$calibrated" = "0" ]; then
	#set=`iwconfig ra0|grep "online_test_uvc_5g"`
	#if [ -z "$set" ]; then
	#	iwpriv rai0 set SSID="online_test_uvc_5g"
	#	iwpriv rai0 set WPAPSK="12345678"
	#fi
fi
