#!/bin/sh
. /sbin/config.sh
. /sbin/global.sh

ip=`nvram_get 2860 lan_ipaddr`
nm=`nvram_get 2860 lan_netmask`

interval=10
count=0
connected="0"
disconnect="0"

green_led()
{
	# light up green LED
	gpio l 52 4000 0 1 0 4000
	# turn off red LED
	gpio l 14 0 4000 0 1 4000
}

red_led()
{
	# turn off green LED
	gpio l 52 0 4000 0 1 4000
	# light up red LED
	gpio l 14 4000 0 1 0 4000
}

orange_led()
{
	# light up green LED
	gpio l 52 4000 0 1 0 4000
	# light up red LED
	gpio l 14 4000 0 1 0 4000
}

rm /tmp/timeout 1>/dev/null 2>/dev/null
opmode=`nvram_get 2860 OperationMode`
if [ "$opmode" = "1" ]; then
	# for the AP server to check whether the client is connected or not.
	# and also control the LED to indicate the status.
	disconnect=`web rtdev wifi clnConnected`
	if [ "$disconnect" != "0" ]; then
		# connected
		if [ -e /tmp/mctgadget_connected ]; then
			green_led
		else
			orange_led
		fi
	else
		# disconnected
		red_led
	fi

	interval=2
	while :
	do
		disconnect=`web rtdev wifi clnConnected`
		if [ "$disconnect" != "0" ]; then
			# connected
			if [ "$connected" = "0" ]; then
				if [ -e /tmp/mctgadget_connected ]; then
					green_led
				else
					orange_led
				fi
			fi
			interval=10
			connected="1"
		else
			# disconnected
			if [ "$connected" = "1" ]; then
				red_led
			fi
			connected="0"
			interval=2
		fi
		sleep $interval
	done
else
	# for WiFi client to check whether connect to AP or not.
	# and also control LED to indicate the status.
	disconnect=`iwpriv apclii0 conn_status|grep ApClii0|grep Disconnect`
	if [ -n "$disconnect" ]; then
		red_led
		iwpriv apclii0 set SiteSurvey=1 1>/dev/null 2>/dev/null
		sleep 1
		iwpriv apclii0 get_site_survey 1>/dev/null 2>/dev/null
		iwpriv apclii0 set ApCliAutoConnect=1 1>/dev/null 2>/dev/null
	else
		if [ -e /tmp/uvcclient_disconnect ]; then
			orange_led
		else
			green_led
		fi
	fi

	while :
	do
		if [ ! -e /tmp/doing_wps ]; then
			# check connection via conn_status command
			disconnect=`iwpriv apclii0 conn_status|grep ApClii0|grep Disconnect`
			if [ -n "$disconnect" ]; then
				if [ "$connected" = "1" ]; then
					connected="0"
					red_led
					count=0
				fi
				#  disconnect, do connection again
				ssid=`nvram_get rtdev ApCliSsid`
				if [ -n "$ssid" ]; then
					count=$((count+1))
					if [ "$count" -gt "10" ]; then
						#  waiting for 20's and do auto-connect again
						ifconfig apclii0 down 1>/dev/null
						ifconfig rai0 down up 1>/dev/null
						ifconfig apclii0 up 1>/dev/null
						iwpriv apclii0 set ApCliAutoConnect=1 1>/dev/null
						count=0
					fi
				fi
				interval=2
			else
				count=0
				#  store the SSID from AP
				#  ex: ApClii0 Connected AP : 00:05:1B:00:01:02   SSID:UVC-79366D
				CUR_SSID=`iwpriv apclii0 conn_status|awk -F ' ' '{print $6}'|awk -F ':' '{print $2}'`
				if [ -n "$CUR_SSID" ]; then
					if [ "$connected" = "0" ]; then
						connected="1"
						ORG_SSID=`nvram_get rtdev ApCliSsid`
						SSID=`iwpriv apclii0 stat|grep SSID`
						SSID=`echo $SSID|awk -F '=' '{print $2}'`
						SSID=`echo $SSID|xargs`
						AUTHTYPE=`iwpriv apclii0 stat|grep AuthType`
						AUTHTYPE=`echo $AUTHTYPE|awk -F '=' '{print $2}'`
						AUTHTYPE=`echo $AUTHTYPE|xargs`
						ENCRYPTYPE=`iwpriv apclii0 stat|grep EncrypType`
						ENCRYPTYPE=`echo $ENCRYPTYPE|awk -F '=' '{print $2}'`
						ENCRYPTYPE=`echo $ENCRYPTYPE|xargs`
						KEY=`iwpriv apclii0 stat|grep "Key "`
						KEY=`echo $KEY|awk -F '=' '{print $2}'`
						KEY=`echo $KEY|xargs`
						if [ -n "$SSID" ] && [ "$SSID" != "$ORG_SSID" ]; then
							nvram_set rtdev ApCliSsid "$SSID"
		
							#if [ -n `echo $AUTHTYPE|grep WPA2` ]; then
								#nvram_set rtdev ApCliAuthMode WPA2PSK
								#nvram_set rtdev ApCliWPAPSK "$KEY"
							#elif [ -n `echo $AUTHTYPE|grep WPA` ]; then
								#nvram_set rtdev ApCliAuthMode WPAPSK
								#nvram_set rtdev ApCliWPAPSK "$KEY"
							#elif [ -n `echo $AUTHTYPE|grep SHARED` ]; then
								#nvram_set rtdev ApCliAuthMode SHARED
								#nvram_set rtdev ApCliKey1Str "$KEY"
							#elif [ -n `echo $AUTHTYPE|grep OPEN` ]; then
								#nvram_set rtdev ApCliAuthMode OPEN
								#nvram_set rtdev ApCliKey1Str "$KEY"
							#else
								#nvram_set rtdev ApCliAuthMode WPA2PSK
								#nvram_set rtdev ApCliWPAPSK "$KEY"
							#fi
		
							#if [ -n `echo $ENCRYPTYPE|grep AES` ]; then
								#nvram_set rtdev ApCliEncrypType AES
							#elif [ -n `echo $ENCRYPTYPE|grep TKIP` ]; then
								#nvram_set rtdev ApCliEncrypType AES
							#elif [ -n `echo $ENCRYPTYPE|grep WEP` ]; then
								#nvram_set rtdev ApCliEncrypType WEP
							#elif [ -n `echo $ENCRYPTYPE|grep NONE` ]; then
								#nvram_set rtdev ApCliEncrypType NONE
							#else
								#nvram_set rtdev ApCliEncrypType AES
							#fi
						fi

						if [ -e /tmp/uvcclient_disconnect ]; then
							orange_led
						else
							green_led
						fi
					fi
				fi
				interval=10
			fi
		else
			interval=10
		fi
		sleep $interval
	done
fi
