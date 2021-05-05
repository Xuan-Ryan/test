#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/autoconf.h>
#include "config/autoconf.h"
#if defined CONFIG_USER_LIGHTY
#include <sys/time.h>
#include "oid.h"
#include <arpa/inet.h>
#include <linux/wireless.h>
#endif
#include "nvram.h"

#if defined (CONFIG_RALINK_GPIO) || defined (CONFIG_RALINK_GPIO_MODULE)
#include "ralink_gpio.h"
#define GPIO_DEV "/dev/gpio"
#endif


#ifdef CONFIG_RT2880_L2_MANAGE
int ramad_start(void);
#endif
static char *saved_pidfile;

void loadDefault(int chip_id)
{
    switch(chip_id)
    {
    case 2860:
	system("ralink_init clear 2860");
#if defined CONFIG_LAN_WAN_SUPPORT || defined CONFIG_MAC_TO_MAC_MODE 
	system("ralink_init renew 2860 /etc_ro/Wireless/RT2860AP/RT2860_default_vlan");
#elif defined(CONFIG_ICPLUS_PHY)
	system("ralink_init renew 2860 /etc_ro/Wireless/RT2860AP/RT2860_default_oneport");
#else
	system("ralink_init renew 2860 /etc_ro/Wireless/RT2860AP/RT2860_default_novlan");
#endif
#if defined CONFIG_MTK_VOIP
	system("ralink_init clear voip");
	system("ralink_init renew voip /etc_ro/voip_default_settings");
#endif
	break;
    case 2880:
	system("ralink_init clear rtdev");
#if defined (CONFIG_RTDEV)
	system("ralink_init renew rtdev /etc_ro/Wireless/iNIC/RT2860AP.dat");
#elif defined (CONFIG_RTDEV_PLC)
	system("ralink_init renew rtdev /etc_ro/PLC/plc_default.dat");
#endif
	break;
    case 2561: 
	system("ralink_init clear rtdev");
	system("ralink_init renew rtdev /etc_ro/Wireless/RT61AP/RT2561_default");
	break;
    default:
	printf("%s:Wrong chip id\n",__FUNCTION__);
	break;
    }
}

#if defined CONFIG_USER_LIGHTY
#if defined WSC_SUPPORT
#if defined (CONFIG_RALINK_GPIO) || defined (CONFIG_RALINK_GPIO_MODULE)
#if defined (CONFIG_RALINK_RT2880)
#define WPS_AP_PBC_LED_GPIO     13   // 0 ~ 24( or disable this feature by undefine it)

#elif defined (CONFIG_RALINK_RT3883)
#define WPS_AP_PBC_LED_GPIO     0   // 0 ~ 24( or disable this feature by undefine it)

#elif defined (CONFIG_RALINK_RT6855A)
#if defined (CONFIG_RT6855A_PCIE_PORT0_ENABLE)
#define WPS_AP_PBC_LED_GPIO     32      // rt6855 WPS LED
#else
#define WPS_AP_PBC_LED_GPIO     21      // rt6856 WPS LED
#endif

#elif defined (CONFIG_RALINK_MT7620)
#define WPS_AP_PBC_LED_GPIO_GREEN     52      // MT7620 WPS LED
#define WPS_AP_PBC_LED_GPIO_RED       14

#elif defined (CONFIG_RALINK_MT7628)
#define WPS_AP_PBC_LED_GPIO     37       // MT7628 WPS LED

#else
#define WPS_AP_PBC_LED_GPIO     14   // 0 ~ 24( or disable this feature by undefine it)
#endif
#define LED_ON                          1
#define LED_OFF                         0
#define WPS_LED_RESET                   1
#define WPS_LED_PROGRESS                2
#define WPS_LED_ERROR                   3
#define WPS_LED_SESSION_OVERLAP         4
#define WPS_LED_SUCCESS                 5
#define LedReset()                  {ledWps(WPS_AP_PBC_LED_GPIO_GREEN, WPS_LED_RESET);ledWps(WPS_AP_PBC_LED_GPIO_RED, WPS_LED_SUCCESS);}
#define LedInProgress()             {ledWps(WPS_AP_PBC_LED_GPIO_GREEN, WPS_LED_PROGRESS);ledWps(WPS_AP_PBC_LED_GPIO_RED, WPS_LED_PROGRESS);}
#define LedError()                  {ledWps(WPS_AP_PBC_LED_GPIO_GREEN, WPS_LED_RESET);ledWps(WPS_AP_PBC_LED_GPIO_RED, WPS_LED_SUCCESS);}
#define LedSessionOverlapDetected() {ledWps(WPS_AP_PBC_LED_GPIO_GREEN, WPS_LED_RESET);ledWps(WPS_AP_PBC_LED_GPIO_RED, WPS_LED_SESSION_OVERLAP);}
#define LedSuccess()                {ledWps(WPS_AP_PBC_LED_GPIO_GREEN, WPS_LED_SUCCESS);ledWps(WPS_AP_PBC_LED_GPIO_RED, WPS_LED_SUCCESS);}
#else
#define LedReset()
#define LedInProgress()
#define LedError()
#define LedSessionOverlapDetected()
#define LedSuccess()
#endif

#define WPS_AP_CATCH_CONFIGURED_TIMER	100
#define WPS_AP_TIMEOUT_SECS 		120000
static int wsc_timeout_counter;
static int g_wps_timer_state;
static int triggered_signal;
int setTimer(int microsec, void ((*sigroutine)(int)))
{
	struct itimerval value, ovalue;

	signal(SIGALRM, sigroutine);
	value.it_value.tv_sec = 0;
	value.it_value.tv_usec = microsec;
	value.it_interval.tv_sec = 0;
	value.it_interval.tv_usec = microsec;
	return setitimer(ITIMER_REAL, &value, &ovalue);
}

void stopTimer(void)
{
	struct itimerval value, ovalue;

	value.it_value.tv_sec = 0;
	value.it_value.tv_usec = 0;
	value.it_interval.tv_sec = 0;
	value.it_interval.tv_usec = 0;
	setitimer(ITIMER_REAL, &value, &ovalue);
}

static void resetTimerAll(void)
{
	stopTimer();
	g_wps_timer_state = 0;
}

#if defined (CONFIG_RALINK_GPIO) || defined (CONFIG_RALINK_GPIO_MODULE)
static int gpioLedSet(int gpio, unsigned int on, unsigned int off,
		unsigned int blinks, unsigned int rests, unsigned int times)
{
	int fd;
	ralink_gpio_led_info led;

	if (gpio < 0 || gpio >= RALINK_GPIO_NUMBER ||
			on > RALINK_GPIO_LED_INFINITY ||
			off > RALINK_GPIO_LED_INFINITY ||
			blinks > RALINK_GPIO_LED_INFINITY ||
			rests > RALINK_GPIO_LED_INFINITY ||
			times > RALINK_GPIO_LED_INFINITY) {
		return -1;
	}
	led.gpio = gpio;
	led.on = on;
	led.off = off;
	led.blinks = blinks;
	led.rests = rests;
	led.times = times;

	fd = open(GPIO_DEV, O_RDONLY);
	if (fd < 0) {
		perror(GPIO_DEV);
		return -1;
	}
	if (ioctl(fd, RALINK_GPIO_LED_SET, &led) < 0) {
		perror("ioctl");
		close(fd);
		return -1;
	}
	close(fd);
	return 0;
}

int ledWps(int gpio, int mode)
{
	switch (mode) {
	case WPS_LED_RESET:
		return gpioLedSet(gpio, 0, RALINK_GPIO_LED_INFINITY, 1, 1, RALINK_GPIO_LED_INFINITY);
		break;
	case WPS_LED_PROGRESS:
		return gpioLedSet(gpio, 2, 1, RALINK_GPIO_LED_INFINITY, 1, RALINK_GPIO_LED_INFINITY);
		break;
	case WPS_LED_ERROR:
		return gpioLedSet(gpio, 1, 1, RALINK_GPIO_LED_INFINITY, 1, RALINK_GPIO_LED_INFINITY);
		break;
	case WPS_LED_SESSION_OVERLAP:
		return gpioLedSet(gpio, 1, 1, 10, 5, RALINK_GPIO_LED_INFINITY);
		break;
	case WPS_LED_SUCCESS:
		gpioLedSet(gpio, RALINK_GPIO_LED_INFINITY, 1, 1, 1, 1); //  3000 = 300s
		break;
	}
	return 0;
}
#endif

static void configAP(char *ifname, int nvram_idx)
{
	WSC_PROFILE *wsc_profile;
	char temp[65];

	if ((wsc_profile = malloc(sizeof(WSC_PROFILE))) == NULL) {
		perror("malloc wsc_profile");
		return;
	}
	getWscProfile(ifname, wsc_profile);
	nvram_init(nvram_idx);
	nvram_bufset(nvram_idx, "WscConfigured", "1");

	nvram_bufset(nvram_idx, "SSID1", wsc_profile->Profile[0].SSID.Ssid);
	nvram_bufset(nvram_idx, "WscSSID", wsc_profile->Profile[0].SSID.Ssid);

	switch (wsc_profile->Profile[0].AuthType) {
	case 0x0002:
		nvram_bufset(nvram_idx, "AuthMode", "WPAPSK");
		break;
	case 0x0004:
		nvram_bufset(nvram_idx, "AuthMode", "SHARED");
		break;
	case 0x0008:
		nvram_bufset(nvram_idx, "AuthMode", "WPA");
		break;
	case 0x0010:
		nvram_bufset(nvram_idx, "AuthMode", "WPA2");
		break;
	case 0x0020:
		nvram_bufset(nvram_idx, "AuthMode", "WPA2PSK");
		break;
	case 0x0022:
		nvram_bufset(nvram_idx, "AuthMode", "WPAPSKWPA2PSK");
		break;
	case 0x0001:
	default:
		nvram_bufset(nvram_idx, "AuthMode", "OPEN");
	}
	switch (wsc_profile->Profile[0].EncrType) {
	case 0x0002:
		nvram_bufset(nvram_idx, "EncrypType", "WEP");
		if ((wsc_profile->Profile[0].KeyLength == 10) || 
		    (wsc_profile->Profile[0].KeyLength == 26)) {
			/* Key Entry Method == HEX */
			nvram_bufset(nvram_idx, "Key1Type", "0");
			nvram_bufset(nvram_idx, "Key2Type", "0");
			nvram_bufset(nvram_idx, "Key3Type", "0");
			nvram_bufset(nvram_idx, "Key4Type", "0");
		} else {
			/* Key Entry Method == ASCII */
			nvram_bufset(nvram_idx, "Key1Type", "1");
			nvram_bufset(nvram_idx, "Key2Type", "1");
			nvram_bufset(nvram_idx, "Key3Type", "1");
			nvram_bufset(nvram_idx, "Key4Type", "1");
		}
		if (wsc_profile->Profile[0].KeyIndex == 1) {
			nvram_bufset(nvram_idx, "Key1Str1", wsc_profile->Profile[0].Key);
			nvram_bufset(nvram_idx, "DefaultKeyID", "1");
		} else if (wsc_profile->Profile[0].KeyIndex == 2) {
			nvram_bufset(nvram_idx, "Key2Str1", wsc_profile->Profile[0].Key);
			nvram_bufset(nvram_idx, "DefaultKeyID", "2");
		} else if (wsc_profile->Profile[0].KeyIndex == 3) {
			nvram_bufset(nvram_idx, "Key3Str1", wsc_profile->Profile[0].Key);
			nvram_bufset(nvram_idx, "DefaultKeyID", "3");
		} else if (wsc_profile->Profile[0].KeyIndex == 4) {
			nvram_bufset(nvram_idx, "Key4Str1", wsc_profile->Profile[0].Key);
			nvram_bufset(nvram_idx, "DefaultKeyID", "4");
		}
		break;
	case 0x0004:
		nvram_bufset(nvram_idx, "EncrypType", "TKIP");
		nvram_bufset(nvram_idx, "DefaultKeyID", "2");
		memset(temp, 0, 65);
		memcpy(temp, wsc_profile->Profile[0].Key, wsc_profile->Profile[0].KeyLength);
		nvram_bufset(nvram_idx, "WPAPSK1", temp);
		break;
	case 0x0008:
		nvram_bufset(nvram_idx, "EncrypType", "AES");
		nvram_bufset(nvram_idx, "DefaultKeyID", "2");
		memset(temp, 0, 65);
		memcpy(temp, wsc_profile->Profile[0].Key, wsc_profile->Profile[0].KeyLength);
		nvram_bufset(nvram_idx, "WPAPSK1", temp);
		break;
	case 0x000C:
		nvram_bufset(nvram_idx, "EncrypType", "TKIPAES");
		nvram_bufset(nvram_idx, "DefaultKeyID", "2");
		memset(temp, 0, 65);
		memcpy(temp, wsc_profile->Profile[0].Key, wsc_profile->Profile[0].KeyLength);
		nvram_bufset(nvram_idx, "WPAPSK1", temp); 
		break;
	case 0x0001:
	default:
		nvram_bufset(nvram_idx, "EncrypType", "NONE");
		nvram_bufset(nvram_idx, "DefaultKeyID", "1");
	}

	if (wsc_profile->Profile[0].AuthType == 0x0002 &&
			wsc_profile->Profile[0].EncrType == 0x0004) {
		nvram_bufset(nvram_idx, "AuthMode", "WPAPSKWPA2PSK");
		nvram_bufset(nvram_idx, "EncrypType", "TKIPAES");
	}
	nvram_bufset(nvram_idx, "IEEE8021X", "0");
	nvram_commit(nvram_idx);

	free(wsc_profile);
}

int ap_oid_query_info(unsigned long OidQueryCode, int socket_id, char *DeviceName, void *ptr, unsigned long PtrLength)
{
	struct iwreq wrq;

	strcpy(wrq.ifr_name, DeviceName);
	wrq.u.data.length = PtrLength;
	wrq.u.data.pointer = (caddr_t) ptr;
	wrq.u.data.flags = OidQueryCode;

	return (ioctl(socket_id, RT_PRIV_IOCTL, &wrq));
}

static void config_acl(char *ifname, int nvram_idx)
{
	int i, socket_id = socket(AF_INET, SOCK_DGRAM, 0);
	char temp[18];
	PRT_802_11_ACL alc_list = malloc(sizeof(RT_802_11_ACL));

	ap_oid_query_info(OID_802_11_ACL_LIST, socket_id, ifname, alc_list, sizeof(RT_802_11_ACL));
	for (i=0; i < alc_list->Num; i++)
	{
		sprintf(temp, "%02X:%02X:%02X:%02X:%02X:%02X",
				alc_list->Entry[i].Addr[0], alc_list->Entry[i].Addr[1], alc_list->Entry[i].Addr[2],
				alc_list->Entry[i].Addr[3], alc_list->Entry[i].Addr[4], alc_list->Entry[i].Addr[5]);
		nvram_init(nvram_idx);
		set_nth_value_flash(nvram_idx, i, "AccessControlList0", temp);
		nvram_commit(nvram_idx);
		nvram_close(nvram_idx);
	}
	close(socket_id);
	free(alc_list);
}

static void configAPClient(int nvram_idx, WSC_CONFIGURED_VALUE * wsc_value)
{
	char authmod[32];
	char encryp[32];

	printf("WscConfigured = %d\n", wsc_value->WscConfigured); // 1 un-configured; 2 configured
	printf("WscSssid = %s\n", wsc_value->WscSsid);
	printf("WscAuthMode = %d\n", wsc_value->WscAuthMode); // mandatory, 0x01: open, 0x02: wpa-psk, 0x04: shared, 0x08:wpa, 0x10: wpa2
	printf("WscEncrypType = %d\n", wsc_value->WscEncrypType);  // 0x01: none, 0x02: wep, 0x04: tkip, 0x08: aes
	printf("DefaultKeyIdx = %d\n", wsc_value->DefaultKeyIdx);
	printf("WscWPAKey = %s\n", wsc_value->WscWPAKey);
/*
	nvram_set(nvram_idx, "ApCliSsid", wsc_value->WscSsid);
	if (wsc_value->WscAuthMode & 0x0022)  //  PSK and WPA2
		strcpy(authmod, "WPA2PSK");
	else if (wsc_value->WscAuthMode & 0x0082)  //  WPAPSK
		strcpy(authmod, "WPAPSK");
	else if (wsc_value->WscAuthMode & 0x0004)  //  shared
		strcpy(authmod, "SHARED");
	else if (wsc_value->WscAuthMode & 0x0001)  //  open
		strcpy(authmod, "OPEN");
	else
		strcpy(authmod, "WPA2PSK");
	nvram_set(nvram_idx, "ApCliAuthMode", authmod);
	if (wsc_value->WscEncrypType & 0x0008) //  AES
		strcpy(encryp, "AES");
	else if (wsc_value->WscEncrypType & 0x0004) //  TKIP
		strcpy(encryp, "TKIP");
	else if (wsc_value->WscEncrypType & 0x0002) //  WEP
		strcpy(encryp, "WEP");
	else if (wsc_value->WscEncrypType & 0x0001) //  NONE
		strcpy(encryp, "NONE");
	else
		strcpy(encryp, "AES");

	nvram_set(nvram_idx, "ApCliEncrypType", encryp);
	nvram_set(nvram_idx, "ApCliWPAPSK", wsc_value->WscWPAKey);*/
}

static void queryWPSStatus(int signo)
{
	int WscStatus = 0, nvram_idx;
	struct _WSC_CONFIGURED_VALUE *wsc_value;
	char ifname[5];
	const char *opmode = nvram_get(RT2860_NVRAM, "OperationMode");

	if (triggered_signal == SIGWINCH) {
		if (!strcmp(opmode, "1")) {
			//  this is RX
			strcpy(ifname, "rai0");
		} else {
			//  this is TX
			strcpy(ifname, "apclii0");
		}
		nvram_idx = RTDEV_NVRAM;
	} else {
		if (!strcmp(opmode, "1")) {
			//  this is RX
			strcpy(ifname, "ra0");
		} else {
			//  this is TX
			strcpy(ifname, "apcli0");
		}
		nvram_idx = RT2860_NVRAM;
	}
	WscStatus = getWscStatus(ifname);
printf("%s: WscStatus = %x\n", __FUNCTION__, WscStatus);
	wsc_timeout_counter += WPS_AP_CATCH_CONFIGURED_TIMER;
	if (g_wps_timer_state == 0) {
		resetTimerAll();
		system("rm /tmp/doing_wps");
		return;
	}
	if (wsc_timeout_counter > WPS_AP_TIMEOUT_SECS) {  //  timeout 120's
		resetTimerAll();
		if (!strcmp(opmode, "1")) {
			LedSuccess();
		} else {
			LedError();
		}
		system("rm /tmp/doing_wps");
		return;
	}

	switch (WscStatus) {
	case 0x109:	/* PBC_SESSION_OVERLAP */
		resetTimerAll();
		LedSessionOverlapDetected();
		system("rm /tmp/doing_wps");
		break;
	case 34:	/* CONFIGURED */
		resetTimerAll();
		LedSuccess();
		if ((wsc_value = malloc(sizeof(struct _WSC_CONFIGURED_VALUE))) == NULL) {
			perror("malloc wsc_value");
			system("rm /tmp/doing_wps");
			return;
		}
		getCurrentWscProfile(ifname, wsc_value, sizeof(WSC_CONFIGURED_VALUE));
		if (!strcmp(opmode, "1")) {
			//  this is RX
			if ((strcmp(nvram_bufget(nvram_idx, "WCNTest"), "1") == 0) ||
			    (wsc_value->WscConfigured == 2)) {
				//configAP(ifname, nvram_idx);
			}
		} else {
			//  this is TX
			/*
			 *  NOTE: if you want to restrict our TX can only connect to our RX,
			 *        I think the way we can do is only check it by SSID name here.
			 *        But, not write related code yet, if you want to do this, 
			 *        add the code here.
			 *        MAYBE we need to do this via MAC address check.
			 */
			configAPClient(nvram_idx, wsc_value);
		}
		system("rm /tmp/doing_wps");
		free(wsc_value);
		//config_acl(ifname, nvram_idx);
		break;
	case 2:		/* FAIL */
	case 1:		/* IDLE */
		resetTimerAll();
		if (!strcmp(opmode, "1")) {
			LedSuccess();
		} else {
			LedReset();
		}
		system("rm /tmp/doing_wps");
	}
}

static void triggerWPS(int signo)
{
	triggered_signal = signo;
	resetTimerAll();
	wsc_timeout_counter = 0;
	g_wps_timer_state = 1;
	setTimer(WPS_AP_CATCH_CONFIGURED_TIMER * 1000, queryWPSStatus);  //  100ms per check
	//  this part of codes related to WEB cgi call
	//LedInProgress();
	system("date > /tmp/doing_wps");
}

#endif

static void dhcpcHandler(int signo)
{
#ifdef CONFIG_IP_NF_FILTER
	system("/etc_ro/lighttpd/www/cgi-bin/firewall.cgi init");
#endif
	system("/etc_ro/lighttpd/www/cgi-bin/internet.cgi ripd");
	system("/sbin/config-igmpproxy.sh");
#ifdef CONFIG_RALINKAPP_SWQOS
	system("/etc_ro/lighttpd/www/cgi-bin/qos.cgi init");
#endif
#ifdef CONFIG_RALINKAPP_HWQOS
	system("/etc_ro/lighttpd/www/cgi-bin/qos.cgi init");
#endif
#if defined (CONFIG_IPV6)
	system("/etc_ro/lighttpd/www/cgi-bin/internet.cgi ipv6");
#endif

#if defined (CONFIG_RA_NAT_HW)
	/* remove all binding entries after getting new IP */
	const char *hwnat_ebl = nvram_bufget(RT2860_NVRAM, "hwnatEnabled");

	if(0 == strcmp(hwnat_ebl, "1")) {
		system("rmmod hw_nat");
		system("insmod hw_nat");
	}
#endif
}

static void hotPluglerHandler(int signo)
{
	system("/etc_ro/lighttpd/www/cgi-bin/nas.cgi init");
	system("/etc_ro/lighttpd/www/cgi-bin/usb.cgi init");
}

#endif

#if defined WSC_SUPPORT
static void doWPSHandler(int signo)
{
#if defined CONFIG_RALINK_RT2880
	int gopid;
	FILE *fp = fopen("/var/run/goahead.pid", "r");

	if (NULL == fp) {
		printf("nvram: goAhead is not running\n");
		return;
	}
	fscanf(fp, "%d", &gopid);
	if (gopid < 2) {
		printf("nvram: goAhead pid(%d) <= 1\n", gopid);
		return;
	}

	//send SIGUSR1 signal to goAhead for WPSPBCStart();
	printf("notify goahead to start WPS PBC..\n");
	kill(gopid, SIGUSR1);
	fclose(fp);
#elif defined CONFIG_USER_LIGHTY
	char cmd[128];
	sprintf(cmd, "%s %d", 
		"/etc_ro/lighttpd/www/cgi-bin/wireless.cgi wps_pbc",
		RTDEV_NVRAM);
	LedInProgress();
	system(cmd);
	triggerWPS(SIGWINCH);
#endif
}
#endif

static int get_region(int nvram_id, char * key)
{
	int region = 0;
	const char * buf = NULL;
	nvram_init(nvram_id);
	buf = nvram_bufget(nvram_id, key);
	if (buf)
		region = atoi(buf);
	nvram_close(nvram_id);
	return region;
}

static void set_region(int nvram_id, char * key, int region)
{
	char value[16];
	nvram_init(nvram_id);
	sprintf(value, "%d", region);
	nvram_bufset(nvram_id, key, value);
	nvram_commit(nvram_id);
	nvram_close(nvram_id);
}

static void loadDefaultHandler(int signo)
{
	int region5g = 0;
	char model_name[64];
	char calibrated[4];

	printf("load default and reboot..\n");
	
	nvram_init(RT2860_NVRAM);
	strcpy(model_name, nvram_bufget(RT2860_NVRAM, "ModelName"));
	strcpy(calibrated, nvram_bufget(RT2860_NVRAM, "Calibrated"));
	nvram_close(RT2860_NVRAM);
	
	nvram_init(RTDEV_NVRAM);
	region5g = get_region(RTDEV_NVRAM, "CountryRegionABand");
	nvram_close(RTDEV_NVRAM);

	loadDefault(2860);
#if defined (CONFIG_RTDEV) || \
	defined (CONFIG_RTDEV_PLC)
	loadDefault(2880);
#elif defined (CONFIG_RT2561_AP) || defined (CONFIG_RT2561_AP_MODULE)
	loadDefault(2561);
#endif
	nvram_init(RT2860_NVRAM);
	nvram_bufset(RT2860_NVRAM, "ModelName", model_name);
	nvram_bufset(RT2860_NVRAM, "Calibrated", calibrated);
	nvram_close(RT2860_NVRAM);

	nvram_init(RTDEV_NVRAM);
	set_region(RTDEV_NVRAM, "CountryRegionABand", region5g);
	nvram_close(RTDEV_NVRAM);

	system("reboot");
}

enum {
	RX,
	TX
};

static void send_signal(int type)
{
	FILE * pidfile = NULL;
	char buf[16];
	int pid = 0;
	if (type == RX) {
		pidfile = fopen("/var/run/mct_gadget.pid", "r");
	} else {
		pidfile = fopen("/var/run/uvcclient.pid", "r");
	}
	
	if (pidfile) {
		if (fread(buf, 1, sizeof(buf), pidfile) > 0) {
			pid = atoi(buf);
			kill(pid, SIGUSR1);
		}
		fclose(pidfile);
	}
}

static void start_stop_play(int signo)
{
	const char *opmode = nvram_get(RT2860_NVRAM, "OperationMode");
	if (!strcmp(opmode, "1")) {
		//  RX
		//  send signal to mct_gadget
		send_signal(RX);
	} else {
		//  TX
		//  send signal to uvcclient
		send_signal(TX);
	}
}

static void signal_handler(void)
{
	signal(SIGPIPE, SIG_IGN);
	signal(SIGTTIN, loadDefaultHandler);
	signal(SIGUSR1, start_stop_play);
#if defined WSC_SUPPORT
	signal(SIGUSR2, doWPSHandler);
#endif
#if defined CONFIG_USER_LIGHTY
	signal(SIGTSTP, dhcpcHandler);
#if defined WSC_SUPPORT
	signal(SIGXFSZ, triggerWPS);
	signal(SIGWINCH, triggerWPS);
#endif
#if (defined CONFIG_USB) || (defined CONFIG_MMC)
	//signal(SIGTTIN, hotPluglerHandler);
#endif
	//signal(SIGXFSZ, doWPSSingleTriggerHandler);
#endif
}

#if defined (CONFIG_RALINK_GPIO) || defined (CONFIG_RALINK_GPIO_MODULE) 
static int initGpio(unsigned int gpio_num)
{
	int fd;
	ralink_gpio_reg_info info;

	fd = open(GPIO_DEV, O_RDONLY);
	if (fd < 0) {
		perror(GPIO_DEV);
		return -1;
	}
	info.pid = getpid();
	info.irq = gpio_num;
	//set gpio direction to input
#if !defined (CONFIG_RALINK_RT6855A)
	if (info.irq < 24) {
		if (ioctl(fd, RALINK_GPIO_SET_DIR_IN, (1<<info.irq)) < 0)
			goto ioctl_err;
	}
#if defined (RALINK_GPIO_HAS_2722)
	else if (22 <= info.irq) {
		if (ioctl(fd, RALINK_GPIO2722_SET_DIR_IN, (1<<(info.irq-22))) < 0)
			goto ioctl_err;
	}
#elif defined (RALINK_GPIO_HAS_4524)
	else if (24 <= info.irq && info.irq < 40) {
		if (ioctl(fd, RALINK_GPIO3924_SET_DIR_IN, (1<<(info.irq-24))) < 0)
			goto ioctl_err;
	}
	else {
		if (ioctl(fd, RALINK_GPIO4540_SET_DIR_IN, (1<<(info.irq-40))) < 0)
			goto ioctl_err;
	}
#elif defined (RALINK_GPIO_HAS_5124)
	else if (24 <= info.irq && info.irq < 40) {
		if (ioctl(fd, RALINK_GPIO3924_SET_DIR_IN, (1<<(info.irq-24))) < 0)
			goto ioctl_err;
	}
	else {
		if (ioctl(fd, RALINK_GPIO5140_SET_DIR_IN, (1<<(info.irq-40))) < 0)
			goto ioctl_err;
	}
#elif defined (RALINK_GPIO_HAS_9524) || defined (RALINK_GPIO_HAS_7224)
	else if (24 <= info.irq && info.irq < 40) {
		if (ioctl(fd, RALINK_GPIO3924_SET_DIR_IN, (1<<(info.irq-24))) < 0)
			goto ioctl_err;
	}
	else if (40 <= info.irq && info.irq < 72) {
		if (ioctl(fd, RALINK_GPIO7140_SET_DIR_IN, (1<<(info.irq-40))) < 0)
			goto ioctl_err;
	}
	else {
#if defined (RALINK_GPIO_HAS_7224)
		if (ioctl(fd, RALINK_GPIO72_SET_DIR_IN, (1<<(info.irq-72))) < 0)
#else
		if (ioctl(fd, RALINK_GPIO9572_SET_DIR_IN, (1<<(info.irq-72))) < 0)
#endif
			goto ioctl_err;
	}
#endif
#else
	if (info.irq < 16) {
		if (ioctl(fd, RALINK_GPIO_SET_DIR_IN, info.irq) < 0)
			goto ioctl_err;
	} else {
		goto ioctl_err;
	}
#endif
	//enable gpio interrupt
	if (ioctl(fd, RALINK_GPIO_ENABLE_INTP) < 0)
		goto ioctl_err;

	//register my information
	if (ioctl(fd, RALINK_GPIO_REG_IRQ, &info) < 0)
		goto ioctl_err;
	close(fd);
	return 0;

ioctl_err:
	perror("ioctl");
	close(fd);
	return -1;
}
#endif

static void pidfile_delete(void)
{
	if (saved_pidfile) unlink(saved_pidfile);
}

int pidfile_acquire(const char *pidfile)
{
	int pid_fd;
	if (!pidfile) return -1;

	pid_fd = open(pidfile, O_CREAT | O_WRONLY, 0644);
	if (pid_fd < 0) {
		printf("Unable to open pidfile %s\n", pidfile);
	} else {
		lockf(pid_fd, F_LOCK, 0);
		if (!saved_pidfile)
			atexit(pidfile_delete);
		saved_pidfile = (char *) pidfile;
	}
	return pid_fd;
}

void pidfile_write_release(int pid_fd)
{
	FILE *out;

	if (pid_fd < 0) return;

	if ((out = fdopen(pid_fd, "w")) != NULL) {
		fprintf(out, "%d\n", getpid());
		fclose(out);
	}
	lockf(pid_fd, F_UNLCK, 0);
	close(pid_fd);
}

static int running = 1;

int main(int argc,char **argv)
{
	pid_t pid;
	int fd;

	nvram_init(RT2860_NVRAM);
#if defined (CONFIG_RTDEV) || \
	defined (CONFIG_RT2561_AP) || defined (CONFIG_RT2561_AP_MODULE)
	nvram_init(RTDEV_NVRAM);
#endif
	if (strcmp(nvram_bufget(RT2860_NVRAM, "WebInit"),"1")) {
		loadDefault(2860);
	}
	
	if (strcmp(nvram_bufget(RTDEV_NVRAM, "WebInit"),"1")) {
#if defined (CONFIG_RTDEV) || \
    defined (CONFIG_RTDEV_PLC)
		loadDefault(2880);
#elif defined (CONFIG_RT2561_AP) || defined (CONFIG_RT2561_AP_MODULE)
		loadDefault(2561);
#endif
	}
	nvram_close(RT2860_NVRAM);
#if defined (CONFIG_RTDEV) || \
	defined (CONFIG_RT2561_AP) || defined (CONFIG_RT2561_AP_MODULE)
	nvram_close(RTDEV_NVRAM);
#endif

#if defined (CONFIG_RALINK_GPIO) || defined (CONFIG_RALINK_GPIO_MODULE) 
#if defined (CONFIG_RALINK_RT2880)
	/* RT2880 reset default & WPS PBC */
	if ((err = initGpio(0) != 0))
		exit(EXIT_FAILURE);
#elif defined (CONFIG_RALINK_RT3052) && ((CONFIG_RALINK_I2S) || defined (CONFIG_RALINK_I2S_MODULE))
	if ((err = initGpio(43) != 0))
		exit(EXIT_FAILURE);
#elif defined (CONFIG_RALINK_RT3883)
	/* RT3883 reset default */
	if ((err = initGpio(27) != 0))
		exit(EXIT_FAILURE);
#if defined CONFIG_USER_LIGHTY && defined WSC_SUPPORT
	/* RT3883 WPS PBC */
	if ((err = initGpio(26) != 0))
		exit(EXIT_FAILURE);
#endif
#elif defined (CONFIG_RALINK_RT6855A)
#if defined (CONFIG_RT6855A_PCIE_PORT0_ENABLE)
	/* RT6855 reset default */
	if (initGpio(0) != 0)
		exit(EXIT_FAILURE);
#if defined CONFIG_USER_LIGHTY && defined WSC_SUPPORT
	/* RT6855 WPS PBC */
	if (initGpio(1) != 0)
		exit(EXIT_FAILURE);
#endif
#else
	/* RT6856 reset default */
	if (initGpio(2) != 0)
		exit(EXIT_FAILURE);
#if defined CONFIG_USER_LIGHTY && defined WSC_SUPPORT
	/* RT6856 WPS PBC */
	if (initGpio(6) != 0)
		exit(EXIT_FAILURE);
#endif
#endif
#elif defined (CONFIG_RALINK_MT7620)
	/* MT7620 reset default */
	/*if (initGpio(1) != 0)
		exit(EXIT_FAILURE);*/
#if defined CONFIG_USER_LIGHTY && defined WSC_SUPPORT
	/* MT7620 WPS PBC */
	if (initGpio(44) != 0)
		exit(EXIT_FAILURE);
#endif
#elif defined (CONFIG_RALINK_MT7621)
	/* MT7621 reset default & WPS PBC */
	if (initGpio(18) != 0)
		exit(EXIT_FAILURE);
#elif defined (CONFIG_RALINK_MT7628)
	/* MT7628 reset default */
	if (initGpio(38) != 0)	// RFB-V10/V11: GPIO #45, RFB-V12: GPIO #38 
		exit(EXIT_FAILURE);
#if defined CONFIG_USER_LIGHTY && defined WSC_SUPPORT
	/* MT7628 WPS PBC */
	if (initGpio(38) != 0)	// RFB-V10/V11: GPIO #46, RFB-V12: GPIO #38
		exit(EXIT_FAILURE);
#endif
#else
	/* RT2883, RT3052, RT3352, RT5350 reset default */
	if (initGpio(10) != 0)
		exit(EXIT_FAILURE);
#if defined CONFIG_USER_LIGHTY && defined WSC_SUPPORT
	/* RT2883, RT3052, RT3352, RT5350 WPS PBC */
	if (initGpio(0) != 0)
		exit(EXIT_FAILURE);
#endif
#endif	
#endif
	signal_handler();

	fd = pidfile_acquire("/var/run/nvramd.pid");
	pidfile_write_release(fd);

#ifdef CONFIG_RT2880_L2_MANAGE
	//start the management daemon (blocking)
	ramad_start();
#else
	while (running == 1) {
		pause();
	}
#endif

	exit(EXIT_SUCCESS);
}
