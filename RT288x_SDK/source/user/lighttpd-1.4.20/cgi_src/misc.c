#include "utils.h"
#include <stdarg.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <linux/wireless.h>
#include "oid.h"

#include "busybox_conf.h"

static void submit_finish(char * input)
{
	int first_item = 1;
	char lang[DATALEN];
	char page[DATALEN];
	strcpy(lang, web_get("lang", input, 0));
	strcpy(page, web_get("page", input, 0));
	web_html_header();
	printf("<!DOCTYPE html>\n");
	printf("<html>\n");
	printf("<head>\n");
	printf("<script language=\"JavaScript\" type=\"text/javascript\">\n");
	printf("function loadpage()\n");
	printf("{\n");
	printf("var url = \"/index.shtml\";\n");
	if (strlen(lang) > 0) {
		printf("url += \"?lang=%s\";", lang);
		first_item = 0;
	}

	if(strlen(page) > 0) {
		if(!strcmp(page, "index")) {
			if(first_item == 1)
				printf("url += \"?page=network_map&time=\"+Math.random()*10000000000000000;\n");
			else
				printf("url += \"&page=network_map&time=\"+Math.random()*10000000000000000;\n");
		} else if (!strcmp(page, "qis")) {
			printf("url = \"/qis/qis.shtml\";\n");
			if (strlen(lang) > 0) {
				printf("url += \"?lang=%s\";", lang);
				printf("url += \"&time=\"+Math.random()*10000000000000000;\n");
			} else {
				printf("url += \"?time=\"+Math.random()*10000000000000000;\n");
			}
		} else if (!strcmp(page, "qis-mode")) {
			printf("url = \"/qis-mode.shtml\";\n");
			if (strlen(lang) > 0) {
				printf("url += \"?lang=%s\";", lang);
				printf("url += \"&time=\"+Math.random()*10000000000000000;\n");
			} else {
				printf("url += \"?time=\"+Math.random()*10000000000000000;\n");
			}
		} else if (!strcmp(page, "qis-bridge")) {
			printf("url = \"/qis-bridge.shtml\";\n");
			if (strlen(lang) > 0) {
				printf("url += \"?lang=%s\";", lang);
				printf("url += \"&time=\"+Math.random()*10000000000000000;\n");
			} else {
				printf("url += \"?time=\"+Math.random()*10000000000000000;\n");
			}
		} else {
			if(first_item == 1)
				printf("url += \"?page=%s?time=\"+Math.random()*10000000000000000;\n", page);
			else
				printf("url += \"&page=%s&time=\"+Math.random()*10000000000000000;\n", page);
		}
	}
	printf("window.location.href = url;\n");
	printf("}\n");
	printf("function initValue()\n");
	printf("{\n");
		printf("setTimeout(\"loadpage();\", 5000);\n");
	printf("}\n");
	printf("</script>\n");
	printf("</head>\n");
	printf("<body onLoad=\"loadpage();\">\n");
	printf("</body>\n");
	printf("</html>\n");
}

void set_lang(char * input)
{
	char cmd[64];

	WebTF(input, "lang", RT2860_NVRAM, "lang", 0);

	sprintf(cmd, "T6-utility lang %s", nvram_bufget(RT2860_NVRAM, "lang"));
	system(cmd);

	submit_finish(input);
}

int get_user_count(void)
{
	FILE * fp;
	FILE * pp;
	int count = 0;
	int found = 0;
	struct in_addr addr;
	char linebuf[256];
	char mac_buf[18];
	struct dhcpOfferedAddr {
		unsigned char hostname[16];
		unsigned char mac[16];
		unsigned long ip;
		unsigned long expires;
	} lease;
	struct ip_list {
		char ip[16];
		unsigned char mac[16];
		struct ip_list * next;
	} * list = NULL;
	struct ip_list * head = NULL;
	struct ip_list * list_tmp = NULL;

	do_system("killall -q -USR1 udhcpd");
	fp = fopen("/var/udhcpd.leases", "r");
	if (NULL == fp)
		return 0;

	while (fread(&lease, 1, sizeof(lease), fp) == sizeof(lease)) {
		sprintf(mac_buf, "%02X:%02X:%02X:%02X:%02X:%02X", lease.mac[0],lease.mac[1],lease.mac[2],lease.mac[3],lease.mac[4],lease.mac[5]);

		if (strncmp(mac_buf, "00:00:00:00:00:00", 17) == 0) {
			//  it is invalidated lease record, I don't know why, but I shouldn't display this record
			continue;
		}

		list_tmp = head;
		found = 0;
		while(list_tmp) {
			if(memcmp(lease.mac, list_tmp->mac, 16) == 0) {
				found = 1;
				break;
			}
			list_tmp = list_tmp->next;
		}
		if (found == 1) {
			//  We find the duplicate record
			continue;
		}

		count++;

		addr.s_addr = lease.ip;
		if(head == NULL) {
			head = (struct ip_list *)malloc(sizeof(struct ip_list));
			list = head;
			list->next = NULL;
			strcpy(head->ip, inet_ntoa(addr));
			memcpy(head->mac, lease.mac, 16);
		} else {
			list->next = (struct ip_list *)malloc(sizeof(struct ip_list));
			list = list->next;
			list->next = NULL;
			strcpy(list->ip, inet_ntoa(addr));
			memcpy(list->mac, lease.mac, 16);
		}
	}
	fclose(fp);

	pp = popen("/sbin/parse-clntlist.sh", "r");
	if(pp != NULL) {
		found = 0;
		memset(linebuf, 0, sizeof(linebuf));
		while(fgets(linebuf, sizeof(linebuf), pp) > 0) {
			linebuf[strlen(linebuf)-1] = '\0';
			list = head;
			while(list != NULL) {
				if(strstr(linebuf, list->ip) != NULL) {
					found = 1;
					break;
				}
				list = list->next;
			}

			if(found == 0) {
				count++;
			}

			fgets(linebuf, sizeof(linebuf), pp);
			fgets(linebuf, sizeof(linebuf), pp);
		}
		pclose(pp);
	}

	list = head;
	while(head != NULL) {
		list = list->next;
		free (head);
		head = list;
	}
	return count;
}

void get_all_user_list()
{
	FILE * fp;
	FILE * pp;
	struct dhcpOfferedAddr {
		unsigned char hostname[16];
		unsigned char mac[16];
		unsigned long ip;
		unsigned long expires;
	} lease;

	int index = 1;
	int found = 0;
	struct in_addr addr;
	unsigned long expires;
	unsigned d, h, m;
	char linebuf[256];
	char mac_buf[18];
	char expire_buf[32];
	char os_buf[64];
	char cmd[128];
	struct ip_list {
		char ip[16];
		unsigned char mac[16];
		struct ip_list * next;
	} * list = NULL;
	struct ip_list * head = NULL;
	struct ip_list * list_tmp = NULL;

	do_system("killall -q -USR1 udhcpd");
	fp = fopen("/var/udhcpd.leases", "r");
	if (NULL == fp)
		return;

	while (fread(&lease, 1, sizeof(lease), fp) == sizeof(lease)) {
		addr.s_addr = lease.ip;

		sprintf(mac_buf, "%02X:%02X:%02X:%02X:%02X:%02X", lease.mac[0],lease.mac[1],lease.mac[2],lease.mac[3],lease.mac[4],lease.mac[5]);

		if (strncmp(mac_buf, "00:00:00:00:00:00", 17) == 0) {
			//  it is invalidated lease record, I don't know why, but I shouldn't display this record
			continue;
		}

		list_tmp = head;
		found = 0;
		while(list_tmp) {
			if(memcmp(lease.mac, list_tmp->mac, 16) == 0) {
				found = 1;
				break;
			}
			list_tmp = list_tmp->next;
		}
		if (found == 1) {
			//  We find the duplicate record
			continue;
		}

		expires = ntohl(lease.expires);
		d = expires / (24*60*60); expires %= (24*60*60);
		h = expires / (60*60); expires %= (60*60);
		m = expires / 60; expires %= 60;
		if (d) {
			sprintf(expire_buf, "%u days %02u:%02u:%02u", d, h, m, (unsigned)expires);
		} else {
			sprintf(expire_buf, "%02u:%02u:%02u", h, m, (unsigned)expires);
		}

		sprintf(cmd, "fingerprint.sh query %s", inet_ntoa(addr));
		pp = popen(cmd, "r");
		if(pp != NULL) {
			memset(os_buf, 0, 64);
			fread(os_buf, 64, 1, pp);
			os_buf[strlen(os_buf)-1] = '\0';
			pclose(pp);
		} else {
			memset(os_buf, 0, 64);
		}

		sprintf(linebuf, "###~U%d~%s~%s~%s~%s~%s~D", index++, lease.hostname, inet_ntoa(addr), mac_buf, expire_buf, os_buf);
		printf("%s\n", linebuf);

		if(head == NULL) {
			head = (struct ip_list *)malloc(sizeof(struct ip_list));
			list = head;
			list->next = NULL;
			strcpy(head->ip, inet_ntoa(addr));
			memcpy(head->mac, lease.mac, 16);
		} else {
			list->next = (struct ip_list *)malloc(sizeof(struct ip_list));
			list = list->next;
			list->next = NULL;
			strcpy(list->ip, inet_ntoa(addr));
			memcpy(list->mac, lease.mac, 16);
		}
	}
	fclose(fp);

	pp = popen("/sbin/check-clnt.sh", "r");
	if(pp != NULL) {
		found = 0;
		memset(cmd, 0, sizeof(cmd));
		//  **  first record  **
		//  Nmap scan report for 10.10.10.3
		//  Host is up (0.0010s latency).
		//  MAC Address: 00:05:1B:AE:02:AD (Magic Control Technology)
		//  **  second record  **
		//  Nmap scan report for 10.10.10.100
		//  Host is up (0.00043s latency).
		//  MAC Address: 20:89:84:4B:F3:BA (Compal Information (kunshan))
		while(fgets(cmd, sizeof(cmd), pp) > 0) {
			cmd[strlen(cmd)-1] = '\0';

			list = head;
			while(list != NULL) {
				if(strstr(cmd, list->ip) != NULL) {
					found = 1;
					break;
				}
				list = list->next;
			}

			if(found == 0) {
				char * p;
				char ip[16];
				int len = 0;
				int i = 0;
				memset(os_buf, '\0', sizeof(os_buf));
				sscanf(cmd, "Nmap scan report for %s", ip);
				//  skip one line
				fgets(cmd, sizeof(cmd), pp);
				fgets(cmd, sizeof(cmd), pp);
				sscanf(cmd, "MAC Address: %s", mac_buf);
				p = strstr(cmd, mac_buf);
				p += strlen(mac_buf);
				p = strchr(p, '(')+1;
				len = (int)p-(int)cmd;
				len = strlen(cmd)-len-2;
				strncpy(os_buf, p, len);
				sprintf(linebuf, "###~U%d'~~%s~%s~~%s~'S'],", index++, ip, mac_buf, os_buf);
				printf("%s\n", linebuf);
			} else {
				fgets(cmd, sizeof(cmd), pp);
				fgets(cmd, sizeof(cmd), pp);
			}
			found = 0;
		}
		pclose(pp);
	}

	list = head;
	while(head != NULL) {
		list = list->next;
		free (head);
		head = list;
	}
}

void get_dhcp_user_list(void)
{
	FILE *fp;
	FILE *pp;
	struct dhcpOfferedAddr {
		unsigned char hostname[16];
		unsigned char mac[16];
		unsigned long ip;
		unsigned long expires;
	} lease;

	int index = 1;
	int i;
	struct in_addr addr;
	unsigned long expires;
	unsigned d, h, m;
	char tmpValue[256];
	char linebuf[256];
	char mac_buf[18];
	char expire_buf[32];
	char os_buf[64];
	char cmd[128];

	do_system("killall -q -USR1 udhcpd");
	fp = fopen("/var/udhcpd.leases", "r");
	if (NULL == fp)
		return;

	while (fread(&lease, 1, sizeof(lease), fp) == sizeof(lease)) {
		addr.s_addr = lease.ip;

		sprintf(mac_buf, "%02X:%02X:%02X:%02X:%02X:%02X", lease.mac[0],lease.mac[1],lease.mac[2],lease.mac[3],lease.mac[4],lease.mac[5]);

		if (strncmp(mac_buf, "00:00:00:00:00:00", 17) == 0) {
			//  it is invalidated lease record, I don't know why, but I shouldn't display this record
			continue;
		}

		expires = ntohl(lease.expires);
		d = expires / (24*60*60); expires %= (24*60*60);
		h = expires / (60*60); expires %= (60*60);
		m = expires / 60; expires %= 60;
		if (d) {
			sprintf(expire_buf, "%u days %02u:%02u:%02u", d, h, m, (unsigned)expires);
		} else {
			sprintf(expire_buf, "%02u:%02u:%02u", h, m, (unsigned)expires);
		}

		sprintf(cmd, "fingerprint.sh query %s", inet_ntoa(addr));
		pp = popen(cmd, "r");
		if(pp != NULL) {
			memset(os_buf, 0, 64);
			fread(os_buf, 64, 1, pp);
			os_buf[strlen(os_buf)-1] = '\0';
			pclose(pp);
		}

		sprintf(linebuf, "###~U%d~%s~%s~%s~%s~%s~D", index++, lease.hostname, inet_ntoa(addr), mac_buf, expire_buf, os_buf);
		printf("%s\n", linebuf);
	}
	fclose(fp);
}

int get_bridge_disconnect(int nvramid)
{
	char linebuf[128];
	char cmd[64];
	FILE * pp;
	int disconnected = 0;

	if (nvramid == RTDEV_NVRAM) {
		sprintf(cmd, "iwpriv apclii0 conn_status");
	} else {
		sprintf(cmd, "iwpriv apcli0 conn_status");
	}

	pp = popen(cmd, "r");
	if(pp != NULL) {
		memset(linebuf, 0, sizeof(linebuf));
		while(fgets(linebuf, sizeof(linebuf), pp) > 0) {
			if (strstr(linebuf, "Connected AP")) {
				if (strstr(linebuf, "Disconnect")) {
					disconnected = 1;
					break;
				}
			}
		}
		pclose(pp);
	}

	return disconnected;
}

char * get_apclient_status(char * input)
{
	static char status[256];
	int bridge_disconnect = 1;
	char ssid[64];
	char bssid[64];
	char authmode[64];
	char cmd[64];
	FILE * pp = NULL;
	char * field;
	int nvramid = 0;
	int channel = 0;

	field = strdup(web_get("nvramid", input, 0));
	if (!strcmp(field, "rtdev"))
		nvramid = RTDEV_NVRAM;
	else
		nvramid = RT2860_NVRAM;

	bridge_disconnect = get_bridge_disconnect(nvramid);

	strcpy(ssid, nvram_bufget(nvramid, "ApCliSsid"));
	convert_string_display(ssid);

	strcpy(bssid, nvram_bufget(nvramid, "ApCliBssid"));
	strcpy(authmode, nvram_bufget(nvramid, "ApCliAuthMode"));

	if (nvramid == RT2860_NVRAM) {
		sprintf(cmd, "iwconfig apcli0|grep Channel");
	} else {
		sprintf(cmd, "iwconfig apclii0|grep Channel");
	}
	pp = popen(cmd, "r");
	if (pp) {
		char linebuf[256];
		fgets(linebuf, sizeof(linebuf), pp);
		sscanf(linebuf, "%*s Channel=%d %*s", &channel);
		pclose(pp);
	}

	//  check is failed or not
	if (bridge_disconnect == 0) {
		//  connect status
		sprintf(status, "%d>%s>%d>%s>%s", bridge_disconnect,
						  ssid,
						  channel,
						  bssid,
						  authmode);
	} else {
		sprintf(status, "1>%s>>>", ssid);
	}

	free(field);
	return status;
}

void set_apclient_ip(char * input)
{
	char cmd[64];
	char * ip = strdup(web_get("ip", input, 2));
	char * mask = strdup(web_get("mask", input, 2));

	sprintf(cmd, "ifconfig br0 %s netmask %s", ip, mask);
	system(cmd);

	free(ip);
	free(mask);
}

void get_apclient_ip(char * input)
{
	FILE * fp = NULL;
	char linebuf[64];
	fp = fopen("/tmp/new_ip.txt", "r");

	if(fp != NULL) {
		memset(linebuf, 0, sizeof(linebuf));
		if(fgets(linebuf, sizeof(linebuf), fp) > 0) {
			linebuf[strlen(linebuf)-1] = '\0';
			printf("%s", linebuf);
		}

		fclose(fp);
		unlink("/tmp/new_ip.txt");
		return;
	}
	printf("");
}

void refresh_aplist(char * input)
{
	char cmd[256];
	char * field;
	int nvramid = 0;

	unlink("/tmp/cur_ip.txt");
	unlink("/tmp/new_ip.txt");

	field = strdup(web_get("nvramid", input, 0));
	if (!strcmp(field, "rtdev"))
		nvramid = RTDEV_NVRAM;
	else
		nvramid = RT2860_NVRAM;

	free(field);

	if (nvramid == RTDEV_NVRAM)
		sprintf(cmd, "iwpriv rai0 set SiteSurvey=1");
	else
		sprintf(cmd, "iwpriv ra0 set SiteSurvey=1");

	system(cmd);
	if (nvramid == RTDEV_NVRAM)
		sleep(3);

	printf("DONE");
}

void get_aplist(char * input)
{
	char linebuf[256];
	char cmd[256];
	char * field;
	int nvramid = 0;
	char show_ssid[186], ssid[186], bssid[20], security[23];
	char mode[9], ext_ch[7], net_type[3];
	char wps[4];
#if defined (FIRST_MT7615_SUPPORT) || defined (SECOND_MT7615_SUPPORT)
	int  no = 0; 
#endif
	int  ch = 0;
	int  signal = 0;
	FILE * pp = NULL;
	char * ptr = NULL;
	int i, space_start;
	int total_ssid_num, get_ssid_times, round;

	field = strdup(web_get("nvramid", input, 0));
	if (!strcmp(field, "rtdev"))
		nvramid = RTDEV_NVRAM;
	else
		nvramid = RT2860_NVRAM;

	if (nvramid == RTDEV_NVRAM)
		sprintf(cmd, "iwpriv apclii0 get_site_survey");
	else
		sprintf(cmd, "iwpriv apcli0 get_site_survey");

	pp = popen(cmd, "r");
	if (pp) {
		if (nvramid == RTDEV_NVRAM)  {
#if defined (FIRST_MT7615_SUPPORT) || defined (SECOND_MT7615_SUPPORT)
			fgets(linebuf, sizeof(linebuf), pp);
			fgets(linebuf, sizeof(linebuf), pp);
			ptr = linebuf;
			total_ssid_num = 0;
			sscanf(ptr, "Total=%d", &total_ssid_num);
			get_ssid_times = (total_ssid_num / 30) + 1;
			fgets(linebuf, sizeof(linebuf), pp);

			for (round = 0; round < get_ssid_times; round++) {
				if (round != 0) {
					memset(cmd, 0, sizeof(cmd));
					snprintf(cmd, sizeof(cmd), "iwpriv apclii0 get_site_survey %d", round*30+1);

					if(pp != NULL)
						pclose(pp);

					if(!(pp = popen(cmd, "r"))) {
						DBG_MSG("execute get_site_survey fail!");
						return;
					}

					fgets(linebuf, sizeof(linebuf), pp);
					fgets(linebuf, sizeof(linebuf), pp);
					fgets(linebuf, sizeof(linebuf), pp);
				}

				while (fgets(linebuf, sizeof(linebuf), pp)) {
					if (strlen(linebuf) < 4)
						break;

					ptr = linebuf;
					sscanf(ptr, "%d %d ", &no, &ch);
					ptr += 41;
					sscanf(ptr, "%20s %23s %d %9s %7s %3s %4s", bssid, security, &signal, mode, ext_ch, net_type, wps);
					ptr = linebuf+8;
					i = 0;
					while (i < 33) {
						if ((ptr[i] == 0x20) && (i == 0 || ptr[i-1] != 0x20))
							space_start = i;
						i++;
					}
					ptr[space_start] = '\0';
					strcpy(ssid, linebuf+8);
					strcpy(show_ssid, linebuf+8);
					convert_string_display(show_ssid);
					printf("%d<%s<%s<%s<%s<%d<%s<%s|", ch, show_ssid, ssid, bssid, security, signal, mode, wps);
				}
			}
#endif
		} else {
			fgets(linebuf, sizeof(linebuf), pp);
			fgets(linebuf, sizeof(linebuf), pp);
			while (fgets(linebuf, sizeof(linebuf), pp)) {
				if (strlen(linebuf) < 4)
					continue;
				ptr = linebuf;
				sscanf(ptr, "%d ", &ch);
				ptr += 37;
				sscanf(ptr, "%20s %23s %d %8s %7s %3s %4s", bssid, security, &signal, mode, ext_ch, net_type, wps);
				ptr = linebuf+4;
				i = 0;
				while (i < 33) {
					if ((ptr[i] == 0x20) && (i == 0 || ptr[i-1] != 0x20))
						space_start = i;
					i++;
				}
				ptr[space_start] = '\0';
				strcpy(ssid, linebuf+4);
				strcpy(show_ssid, linebuf+4);
				convert_string_display(show_ssid);
				printf("%d<%s<%s<%s<%s<%d<%s<%s|", ch, show_ssid, ssid, bssid, security, signal, mode, wps);
			}
		}
		pclose(pp);
	}
	free(field);
}

void get_time(void)
{
	char linebuf[64];
	FILE * pp;
	pp = popen("/bin/date", "r");
	if(pp != NULL) {
		memset(linebuf, 0, sizeof(linebuf));
		if(fgets(linebuf, sizeof(linebuf), pp) > 0) {
			linebuf[strlen(linebuf)-1] = '\0';
			printf("%s", linebuf);
		}

		pclose(pp);
	}
}

#define PROC_MEM_STATISTIC	"/proc/meminfo"
void get_mem_free(void)
{
	char buf[1024], *semiColon;
	FILE *fp = fopen(PROC_MEM_STATISTIC, "r");
	long memsize = 0;

	if(!fp){
		return;
	}

	while(fgets(buf, 1024, fp)){
		if(! (semiColon = strchr(buf, ':'))  )
			continue;

		if (strstr(buf, "MemFree") != NULL) {
			sscanf(buf, "%*s %d kb", &memsize);
			printf("%d", memsize);
			break;
		}
	}
	fclose(fp);
}

#define PROC_IF_STATISTIC	"/proc/net/dev"
void getIfStatistic(char *interface)
{
	int found_flag = 0;
	int skip_line = 2;
	char buf[1024], *field, *semiColon = NULL;
	char data[1024];
	FILE *fp = fopen(PROC_IF_STATISTIC, "r");
	if(!fp){
		DBG_MSG("failed to open /proc/net/dev");
		printf("0\n0\n0\n0\n");
		return;
	}

	while(fgets(buf, 1024, fp)){
		char *ifname;
		if(skip_line != 0){
			skip_line--;
			continue;
		}
		if(! (semiColon = strchr(buf, ':'))  )
			continue;
		*semiColon = '\0';
		ifname = buf;
		ifname = strip_space(ifname);

		if(!strcmp(ifname, interface)){
			found_flag = 1;
			break;
		}
	}
	fclose(fp);

	if (found_flag == 0) {
		DBG_MSG("device not found");
		printf("0\n0\n0\n0\n");
		return;
	}

	semiColon++;

	//  RX byte
	strcpy(data, semiColon);
	if ((field = get_field(data, " ", 0)))
		printf("%lld\n", strtoll(field, NULL, 10));
	else
		printf("0\n");

	//  RX packet
	strcpy(data, semiColon);
	if ((field = get_field(data, " ", 1)))
		printf("%lld\n", strtoll(field, NULL, 10));
	else
		printf("0\n");

	//  TX byte
	strcpy(data, semiColon);
	if ((field = get_field(data, " ", 8)))
		printf("%lld\n", strtoll(field, NULL, 10));
	else
		printf("0\n");

	//  TX packet
	strcpy(data, semiColon);
	if ((field = get_field(data, " ", 9)))
		printf("%lld\n", strtoll(field, NULL, 10));
	else
		printf("0\n");
	return;
}

static void getAllStatistic(char * wanif, char * lanif)
{
	getIfStatistic(wanif);
	getIfStatistic(lanif);
}
#if 1
unsigned int ConvertRssi(long RSSI)
{
    unsigned int signal_quality;
    if (RSSI >= -50)
        signal_quality = 100;
    else if (RSSI >= -80)    // between -50 ~ -80dbm
        signal_quality = (unsigned int)(24 + (RSSI + 80) * 2.6);
    else if (RSSI >= -90)   // between -80 ~ -90dbm
        signal_quality = (unsigned int)((RSSI + 90) * 2.6);
    else    // < -84 dbm
        signal_quality = 0;

    return signal_quality;
}

#define WLAN1_CONF		"2860"
#define WLAN2_CONF		"rtdev"
static void StaInfo(char * conf)
{
	int i, s;
	struct iwreq iwr;
	RT_802_11_MAC_TABLE table = {0};
#if defined (RT2860_TXBF_SUPPORT) || defined (RTDEV_TXBF_SUPPORT)
	char tmpBuff[32];
	char *phyMode[5] = {"CCK", "OFDM", "MM", "GF", "VHT"};
#endif
	char ifname[5];
	int BW = 20;
	int txstream = 0;
	const char * stream;
	int nvramid = 0;

	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (!strcmp(conf, WLAN2_CONF)) {
		strncpy(iwr.ifr_name, "rai0", IFNAMSIZ);
		nvramid = RTDEV_NVRAM;
	} else {
		strncpy(iwr.ifr_name, "ra0", IFNAMSIZ);
		nvramid = RT2860_NVRAM;
	}

	iwr.u.data.pointer = (caddr_t) &table;

	if (s < 0) {
		DBG_MSG("open socket fail!");
		return;
	}

	if (ioctl(s, RTPRIV_IOCTL_GET_MAC_TABLE_STRUCT, &iwr) < 0) {
		DBG_MSG("ioctl -> RTPRIV_IOCTL_GET_MAC_TABLE_STRUCT fail!");
		close(s);
		return;
	}

	close(s);
#if defined (RT2860_TXBF_SUPPORT)
	if (!strcmp(conf, WLAN1_CONF))
		goto advance;
#endif
#if defined (RT2860_TXBF_SUPPORT)
	if (!strcmp(conf, WLAN2_CONF))
		goto advance;
#endif
#if ! defined (RT2860_TXBF_SUPPORT) || ! defined (RTDEV_TXBF_SUPPORT)
	for (i = 0; i < table.Num; i++) {
		printf("<tr><td>%02X:%02X:%02X:%02X:%02X:%02X</td>",
				table.Entry[i].Addr[0], table.Entry[i].Addr[1],
				table.Entry[i].Addr[2], table.Entry[i].Addr[3],
				table.Entry[i].Addr[4], table.Entry[i].Addr[5]);
		printf("<td>%d</td><td>%d</td><td>%d</td>",
				table.Entry[i].Aid, table.Entry[i].Psm, table.Entry[i].MimoPs);
		printf("<td>%d</td><td>%s</td><td>%d</td><td>%d</td></tr>",
				table.Entry[i].TxRate.field.MCS,
				(table.Entry[i].TxRate.field.BW == 0)? "20M":"40M",
				table.Entry[i].TxRate.field.ShortGI, table.Entry[i].TxRate.field.STBC);
	}
	return;
#endif
#if defined (RT2860_TXBF_SUPPORT) || defined (RTDEV_TXBF_SUPPORT)
advance:
	for (i = 0; i < table.Num; i++) {
		RT_802_11_MAC_ENTRY *pe = &(table.Entry[i]);
		unsigned int lastRxRate = pe->LastRxRate;
		unsigned int mcs = pe->LastRxRate & 0x7F;
		int hr, min, sec;

		hr = pe->ConnectedTime/3600;
		min = (pe->ConnectedTime % 3600)/60;
		sec = pe->ConnectedTime - hr*3600 - min*60;

		// MAC Address
		printf("<tr><td style=\"padding:3px 0px 3px 3px\">%02X:%02X:%02X:%02X:%02X:%02X</td>",
				pe->Addr[0], pe->Addr[1], pe->Addr[2], pe->Addr[3],
				pe->Addr[4], pe->Addr[5]);

		// AID, Power Save mode, MIMO Power Save
		printf("<td style=\"padding:3px 0px 3px 3px\" align=\"center\">%d</td><td align=\"center\">%d</td><td align=\"center\">%d</td>",
				pe->Aid, pe->Psm, pe->MimoPs);

		// TX Rate
		if (!strcmp(iwr.ifr_name, "rai0")) {
			if (pe->TxRate.field.BW == 1)
				BW = 40;
			else {
				if (pe->TxRate.field.ShortGI == 1)
					BW = 80;
				else
					BW = 20;
			}
		} else {
			if (pe->TxRate.field.BW == 1)
				BW = 40;
			else
				BW = 20;
		}

		printf("<td style=\"padding:3px 0px 3px 3px\">MCS %d<br>%2dM, %cGI<br>%s%s</td>",
				pe->TxRate.field.MCS & 0x0F,
				BW,
				pe->TxRate.field.ShortGI? 'S': 'L',
				phyMode[pe->TxRate.field.MODE<<1],
				" ");//pe->TxRate.field.STBC? ", STBC": " ");
		// TxBF configuration
		/*printf("<td align=\"center\">%c %c</td>",
				pe->TxRate.field.iTxBF? 'I': '-',
				pe->TxRate.field.eTxBF? 'E': '-');*/

		// RSSI
		if (!strcmp(iwr.ifr_name, "rai0")) {
			int sum = 0;
			int avg = 0;
			stream = nvram_bufget(nvramid, "HT_TxStream");
			if (NULL == stream)
				txstream  = 0;
			else
				txstream = atoi(stream);

			switch(txstream) {
			case 1:
				sum = (int)(pe->AvgRssi0);
				break;
			case 2:
				sum = (int)(pe->AvgRssi0)+(int)(pe->AvgRssi1);
				break;
			case 3:
				sum = (int)(pe->AvgRssi0)+(int)(pe->AvgRssi1)+(int)(pe->AvgRssi2);
				break;
			case 4:
				sum = (int)(pe->AvgRssi0)+(int)(pe->AvgRssi1)+(int)(pe->AvgRssi2)+(int)(pe->AvgRssi3);
				break;
			}

			avg = ConvertRssi(sum/txstream);
			if (avg == 0)
				printf("<td style=\"padding:3px 0px 3px 3px\"><img src=\"/images/UI/rssi0.png\" width=\"30\" height=\"20\" title=\"%d%%\"></td>", avg);
			else if (avg > 0 && avg <= 19)
				printf("<td style=\"padding:3px 0px 3px 3px\"><img src=\"/images/UI/rssi1.png\" width=\"30\" height=\"20\" title=\"%d%%\"></td>", avg);
			else if (avg >= 20 && avg <= 39)
				printf("<td style=\"padding:3px 0px 3px 3px\"><img src=\"/images/UI/rssi2.png\" width=\"30\" height=\"20\" title=\"%d%%\"></td>", avg);
			else if (avg >= 40 && avg <= 59)
				printf("<td style=\"padding:3px 0px 3px 3px\"><img src=\"/images/UI/rssi3.png\" width=\"30\" height=\"20\" title=\"%d%%\"></td>", avg);
			else if (avg >= 60 && avg <= 79)
				printf("<td style=\"padding:3px 0px 3px 3px\"><img src=\"/images/UI/rssi4.png\" width=\"30\" height=\"20\" title=\"%d%%\"></td>", avg);
			else if (avg >= 80 && avg <= 100)
				printf("<td style=\"padding:3px 0px 3px 3px\"><img src=\"/images/UI/rssi5.png\" width=\"30\" height=\"20\" title=\"%d%%\"></td>", avg);
		} else {
			int sum = 0;
			int avg = 0;
			stream = nvram_bufget(nvramid, "HT_TxStream");

			if (NULL == stream)
				txstream  = 0;
			else
				txstream = atoi(stream);

			switch(txstream) {
			case 1:
				sum = (int)(pe->AvgRssi0);
				break;
			case 2:
				sum = (int)(pe->AvgRssi0)+(int)(pe->AvgRssi1);
				break;
			}

			avg = ConvertRssi(sum/txstream);
			if (avg == 0)
				printf("<td style=\"padding:3px 0px 3px 3px\"><img src=\"/images/UI/rssi0.png\" width=\"30\" height=\"20\" title=\"%d%%\"></td>", avg);
			else if (avg > 0 && avg <= 19)
				printf("<td style=\"padding:3px 0px 3px 3px\"><img src=\"/images/UI/rssi1.png\" width=\"30\" height=\"20\" title=\"%d%%\"></td>", avg);
			else if (avg >= 20 && avg <= 39)
				printf("<td style=\"padding:3px 0px 3px 3px\"><img src=\"/images/UI/rssi2.png\" width=\"30\" height=\"20\" title=\"%d%%\"></td>", avg);
			else if (avg >= 40 && avg <= 59)
				printf("<td style=\"padding:3px 0px 3px 3px\"><img src=\"/images/UI/rssi3.png\" width=\"30\" height=\"20\" title=\"%d%%\"></td>", avg);
			else if (avg >= 60 && avg <= 79)
				printf("<td style=\"padding:3px 0px 3px 3px\"><img src=\"/images/UI/rssi4.png\" width=\"30\" height=\"20\" title=\"%d%%\"></td>", avg);
			else if (avg >= 80 && avg <= 100)
				printf("<td style=\"padding:3px 0px 3px 3px\"><img src=\"/images/UI/rssi5.png\" width=\"30\" height=\"20\" title=\"%d%%\"></td>", avg);
		}

		// Per Stream SNR
		/*snprintf(tmpBuff, sizeof(tmpBuff), "%0.1f", pe->StreamSnr[0]*0.25);
		printf("<td>%s<br>", tmpBuff);
		snprintf(tmpBuff, sizeof(tmpBuff), "%0.1f", pe->StreamSnr[1]*0.25); //mcs>7? pe->StreamSnr[1]*0.25: 0.0);
		printf("%s<br>", tmpBuff);
		snprintf(tmpBuff, sizeof(tmpBuff), "%0.1f", pe->StreamSnr[2]*0.25); //mcs>15? pe->StreamSnr[2]*0.25: 0.0);
		printf("%s</td>", tmpBuff);

		// Sounding Response SNR
		if (pe->TxRate.field.eTxBF) {
			snprintf(tmpBuff, sizeof(tmpBuff), "%0.1f", pe->SoundingRespSnr[0]*0.25);
			printf("<td>%s<br>", tmpBuff);
			snprintf(tmpBuff, sizeof(tmpBuff), "%0.1f", pe->SoundingRespSnr[1]*0.25);
			printf("%s<br>", tmpBuff);
			snprintf(tmpBuff, sizeof(tmpBuff), "%0.1f", pe->SoundingRespSnr[2]*0.25);
			printf("%s</td>", tmpBuff);
		}
		else {
			printf("<td align=\"center\">-<br>-<br>-</td>");
		}*/

		// Last RX Rate
		if (!strcmp(iwr.ifr_name, "rai0")) {
			if (((lastRxRate>>7) & 0x01) == 1)
				BW = 40;
			else {
				if (((lastRxRate>>8) & 0x01) == 1)
					BW = 80;
				else
					BW = 20;
			}
			printf("<td style=\"padding:3px 0px 3px 3px\">MCS %d<br>%2dM, %cGI<br>%s%s</td>",
					mcs,  BW,
					((lastRxRate>>8) & 0x1)? 'S': 'L',
					phyMode[(lastRxRate>>13) & 0x7],
					" ");//((lastRxRate>>9) & 0x3)? ", STBC": " ");
		} else {
			printf("<td style=\"padding:3px 0px 3px 3px\">MCS %d<br>%2dM, %cGI<br>%s%s</td>",
					mcs,  ((lastRxRate>>7) & 0x1)? 40: 20,
					((lastRxRate>>8) & 0x1)? 'S': 'L',
					phyMode[(lastRxRate>>13) & 0x7],
					" ");//((lastRxRate>>9) & 0x3)? ", STBC": " ");
		}

		// Connect time
		//printf("<td align=\"center\">%02d:%02d:%02d<br>PER=%d%%</td>", hr, min, sec, pe->TxPER);
		printf("<td style=\"padding:3px 0px 3px 3px\">%02d:%02d:%02d</td>", hr, min, sec);
		printf("</tr>");
	}
#endif
}
#endif

/*
 * NVRAM Query
 */
int chk_wanphy_stat(void)
{
	FILE * pp;
	char buf[32];
	unsigned int sta = 0;
	int ret = 0;

	pp = popen ("/bin/mii_mgr -g -p 31 -r 0x3408", "r");
	if (pp != NULL) {
		if (fgets(buf, sizeof(buf), pp)) {
			sscanf(buf, "Get: %*s = %x", &sta);
			if(sta & 0x01)
				ret = 1;
		}
		pclose(pp);
	}

	return ret;
}

int valid_ip(char * ip)
{
	struct sockaddr_in sa;
	int result = inet_pton(AF_INET, ip, &(sa.sin_addr));

	return result;
}

int qis_wan_detect()
{
	char cmd[1024];
	char hn[128];
	char * wanif = strdup(get_wanif_name());
	char * wanip = NULL;
	char * wanmask = NULL;
	char mode[32];
	int tried = 0;
	int found = 0;
	int stat_code = 0;
	int conn_stat = chk_wanphy_stat();

	memset(hn, '\0', sizeof(hn));
	memset(mode, '\0', sizeof(mode));
	strcpy(hn, nvram_bufget(RT2860_NVRAM, "wan_dhcp_hn"));
	strcpy(mode, nvram_bufget(RT2860_NVRAM, "wanConnectionMode"));

	wanip = get_ipaddr(wanif);
	wanmask  = get_netmask(wanif);

	if (conn_stat == 1) {
		if (strcmp(mode, "DHCP") == 0) {
			//  is valid ip?
			if (valid_ip(wanip) == 0) {
				if (strlen(hn))
					sprintf(cmd, "udhcpc -i %s -h %s -s /sbin/udhcpc.sh -p /var/run/udhcpc.pid 1>/dev/null 2>/dev/null &", wanif, hn);
				else
					sprintf(cmd, "udhcpc -i %s -s /sbin/udhcpc.sh -p /var/run/udhcpc.pid 1>/dev/null 2>/dev/null &", wanif);

				do {
					//  kill previous udhcpc
					do_system("killall -9 udhcpc");
					//  execute it
					system(cmd);
					//  get ip
					wanip = get_ipaddr(wanif);
					//  is valid ip?
					if (valid_ip(wanip) == 1) {
						found = 1;
						stat_code = 0;
						break;
					}
					tried++;
					sleep(3);
				} while (tried < 3);
			} else {
				found = 1;
				stat_code = 0;
			}
		} else {
			if (valid_ip(wanip) == 1) {
				found = 1;
				stat_code = 0;
			}
		}
	} else {
		stat_code = 1;
	}

	web_header();
	if (conn_stat && found == 0)
		stat_code = 2;

	if (stat_code == 0)
		printf("DONE:%s:%s", wanip, wanmask);
	else if (stat_code == 1)
		printf("NOCON::");
	else
		printf("NONE::");

	free(wanif);

	return 0;
}

int set_account(char * input)
{
	char *admuser, *admpass;
	char *old_user;
	char tmp[128];
	int result = 0;

	admuser = admpass = NULL;
	old_user = (char *) nvram_bufget(RT2860_NVRAM, "Login");
	admuser = strdup(web_get("admuser", input, 0));
	admpass = strdup(web_get("admpass", input, 0));

	if (!strlen(admuser)) {
		DBG_MSG("account empty, leave it unchanged");
		printf("0x1101");
		result = -1;
		goto leave;
	}
	if (!strlen(admpass)) {
		DBG_MSG("password empty, leave it unchanged");
		printf("0x1102");
		result = -1;
		goto leave;
	}

	nvram_bufset(RT2860_NVRAM, "Login", admuser);
	nvram_bufset(RT2860_NVRAM, "Password", admpass);
	nvram_commit(RT2860_NVRAM);
leave:
	free_all(2, admuser, admpass);
	return result;
}

void ntp_sync_host(char *time)
{
	if(!time || (!strlen(time)))
		return;
	if(strchr(time, ';'))
		return;
	do_system("date -s %s", time);
}

int set_ntp(char *input)
{
	char *host_time;
	char *tz, *ntpServer, *ntpSync;
	int result = 0;

	host_time = web_get("hostTime", input, 0);
	if (strlen(host_time) > 0) {
		ntp_sync_host(host_time);
	}
	tz = ntpServer = ntpSync = NULL;
	tz = strdup(web_get("time_zone", input, 0));
	ntpServer = strdup(web_get("NTPServerIP", input, 0));
	ntpSync = strdup(web_get("NTPSync", input, 0));

#if 1
	if(!tz || !ntpServer || !ntpSync) {
		printf("0x1201");
		result = -1;
		goto leave;
	}

	if(!strlen(tz)) {
		printf("0x1202");
		result = -1;
		goto leave;
	}

	if(check_semicolon(tz)) {
		printf("0x1203");
		result = -1;
		goto leave;
	}

	if(!strlen(ntpServer)) {
		// user choose to make  NTP server disable
		nvram_bufset(RT2860_NVRAM, "NTPServerIP", "");
		nvram_bufset(RT2860_NVRAM, "NTPSync", "");
	} else {
		if(check_semicolon(ntpServer)) {
			printf("0x1204");
			result = -1;
			goto leave;
		}
		if(!strlen(ntpSync)) {
			printf("0x1205");
			result = -1;
			goto leave;
		}
		if(strtol(ntpSync, NULL, 10) > 300) {
			printf("0x1206");
			result = -1;
			goto leave;
		}
		nvram_bufset(RT2860_NVRAM, "NTPServerIP", ntpServer);
		nvram_bufset(RT2860_NVRAM, "NTPSync", ntpSync);
	}
#else
	nvram_bufset(RT2860_NVRAM, "NTPServerIP", ntpServer);
	nvram_bufset(RT2860_NVRAM, "NTPSync", ntpSync);
#endif
	nvram_bufset(RT2860_NVRAM, "TZ", tz);
	nvram_commit(RT2860_NVRAM);
leave:
	free_all(3, tz, ntpServer, ntpSync);

	return result;
}

int set_wan(char *input)
{
	char *pppoe_opmode, *l2tp_mode, *l2tp_opmode, *pptp_mode, *pptp_opmode;
	int result = 0;

	pppoe_opmode = l2tp_mode = l2tp_opmode = pptp_mode = pptp_opmode = NULL;

	WebTF(input, "opmode", RT2860_NVRAM, "OperationMode", 0);
	WebTF(input, "connectionType", RT2860_NVRAM, "wanConnectionMode", 0);
	/* STATIC */
	WebTF(input, "staticIp", RT2860_NVRAM, "wan_ipaddr", 0);
	WebTF(input, "staticNetmask", RT2860_NVRAM, "wan_netmask", 0);
	WebTF(input, "staticGateway", RT2860_NVRAM, "wan_gateway", 0);
	WebTF(input, "staticPriDns", RT2860_NVRAM, "wan_primary_dns", 0);
	WebTF(input, "staticSecDns", RT2860_NVRAM, "wan_secondary_dns", 0);
	/* DHCP */
	WebTF(input, "wan_dhcp_hn", RT2860_NVRAM, "wan_dhcp_hn", 0);
	/* PPPoE */
	WebTF(input, "pppoeUser", RT2860_NVRAM, "wan_pppoe_user", 0);
	WebTF(input, "pppoePass", RT2860_NVRAM, "wan_pppoe_pass", 0);
	pppoe_opmode = strdup(web_get("pppoeOPMode", input, 0));
	if(pppoe_opmode == NULL) pppoe_opmode="";
	nvram_bufset(RT2860_NVRAM, "wan_pppoe_opmode", pppoe_opmode);
	if (!strncmp(pppoe_opmode, "OnDemand", 8))
		WebTF(input, "pppoeIdleTime", RT2860_NVRAM, "wan_pppoe_optime", 0);
	/* L2TP */
	WebTF(input, "l2tpServer", RT2860_NVRAM, "wan_l2tp_server", 0);
	WebTF(input, "l2tpUser", RT2860_NVRAM, "wan_l2tp_user", 0);
	WebTF(input, "l2tpPass", RT2860_NVRAM, "wan_l2tp_pass", 0);
	l2tp_mode = strdup(web_get("l2tpMode", input, 0));
	if(l2tp_mode == NULL) l2tp_mode="";
	nvram_bufset(RT2860_NVRAM, "wan_l2tp_mode", l2tp_mode);
	if (!strncmp(l2tp_mode, "0", 1)) {
		WebTF(input, "l2tpIp", RT2860_NVRAM, "wan_l2tp_ip", 0);
		WebTF(input, "l2tpNetmask", RT2860_NVRAM, "wan_l2tp_netmask", 0);
		WebTF(input, "l2tpGateway", RT2860_NVRAM, "wan_l2tp_gateway", 0);
	}
	l2tp_opmode = strdup(web_get("l2tpOPMode", input, 0));
	if(l2tp_opmode == NULL) l2tp_opmode="";
	nvram_bufset(RT2860_NVRAM, "wan_l2tp_opmode", l2tp_opmode);
	if (!strncmp(l2tp_opmode, "OnDemand", 8))
		WebTF(input, "l2tpIdleTime", RT2860_NVRAM, "wan_l2tp_optime", 0);
	/* PPTP */
	WebTF(input, "pptpServer", RT2860_NVRAM, "wan_pptp_server", 0);
	WebTF(input, "pptpUser", RT2860_NVRAM, "wan_pptp_user", 0);
	WebTF(input, "pptpPass", RT2860_NVRAM, "wan_pptp_pass", 0);
	pptp_mode = strdup(web_get("pptpMode", input, 0));
	if(pptp_mode == NULL) pptp_mode="";
	nvram_bufset(RT2860_NVRAM, "wan_pptp_mode", pptp_mode);
	if (!strncmp(pptp_mode, "0", 1)) {
		WebTF(input, "pptpIp", RT2860_NVRAM, "wan_pptp_ip", 0);
		WebTF(input, "pptpNetmask", RT2860_NVRAM, "wan_pptp_netmask", 0);
		WebTF(input, "pptpGateway", RT2860_NVRAM, "wan_pptp_gateway", 0);
	}
	WebTF(input, "pptpOPMode", RT2860_NVRAM, "wan_pptp_opmode", 0);
	pptp_opmode = strdup(web_get("pptpOPMode", input, 0));
	if(pptp_opmode == NULL) pptp_opmode="";
	nvram_bufset(RT2860_NVRAM, "wan_pptp_opmode", pptp_opmode);
	if (!strncmp(pptp_opmode, "OnDemand", 8))
		WebTF(input, "pptpIdleTime", RT2860_NVRAM, "wan_pptp_optime", 0);

	/* MAC Clone */
	WebTF(input, "macCloneEnbl", RT2860_NVRAM, "macCloneEnabled", 0);
	WebTF(input, "macCloneMac", RT2860_NVRAM, "macCloneMac", 0);

	nvram_commit(RT2860_NVRAM);
	free_all(5, pppoe_opmode, l2tp_mode, l2tp_opmode, pptp_mode, pptp_opmode);
	return result;
}

int check_wan_lan(char * lan_ip, char * lan_mask, char * wan_ip, char * wan_mask)
{
	in_addr_t lan_addr;
	in_addr_t lan_subnet;
	in_addr_t wan_addr;
	in_addr_t wan_subnet;
	
	lan_addr = inet_addr(lan_ip);
	lan_subnet = inet_addr(lan_mask);
	wan_addr = inet_addr(wan_ip);
	wan_subnet = inet_addr(wan_mask);
	
	if ((lan_addr&lan_subnet) == (wan_addr&wan_subnet)) {
		return 1;
	}
	
	return 0;
}

int set_lan(char * input)
{
	char * ip = NULL;
	char * lanNetmask = NULL;
	char * dhcp_tp = NULL;
	char * dhcp_s = NULL;
	char * dhcp_e = NULL;
	char * dhcp_m = NULL;
	char * wan_ip = NULL;
	char * wan_mask = NULL;
	const char * ctype = NULL;
	int result = 0;

	ip = strdup(web_get("lanIp", input, 0));
	if(ip == NULL)
		ip = "";

	lanNetmask = strdup(web_get("lanNetmask", input, 0));
	if(lanNetmask == NULL)
		lanNetmask = "";

	dhcp_tp = strdup(web_get("lanDhcpType", input, 0));
	if(dhcp_tp == NULL)
		dhcp_tp = "";

	dhcp_s = strdup(web_get("dhcpStart", input, 0));
	if(dhcp_s == NULL)
		dhcp_s = "";

	dhcp_e = strdup(web_get("dhcpEnd", input, 0));
	if(dhcp_e == NULL)
		dhcp_e = "";

	dhcp_m = strdup(web_get("dhcpMask", input, 0));
	if(dhcp_m == NULL)
		dhcp_m = "";

	ctype = nvram_bufget(RT2860_NVRAM, "connectionType");

	/*
	 *      * check static ip address:
	 *           * lan and wan ip should not be the same except in bridge mode
	 */
	wan_ip = strdup(get_ipaddr(get_wanif_name()));
	wan_mask = strdup(get_netmask(get_wanif_name()));
	if (check_wan_lan(ip, lanNetmask, wan_ip, wan_mask) == 1) {
		DBG_MSG("The LAN subnet and the WAN subnet cannot be same");
		printf("0x1401");
		result = -1;
		goto leave;
	}

	//  save settings
	nvram_bufset(RT2860_NVRAM, "lan_ipaddr", ip);
	nvram_bufset(RT2860_NVRAM, "lan_netmask", lanNetmask);

	WebTF(input, "hostname", RT2860_NVRAM, "HostName", 0);

	if (!strncmp(dhcp_tp, "DISABLE", 8))
		nvram_bufset(RT2860_NVRAM, "dhcpEnabled", "0");
	else if (!strncmp(dhcp_tp, "SERVER", 7)) {
		if (-1 == inet_addr(dhcp_s)) {
			DBG_MSG("invalid DHCP Start IP");
			printf("0x1402");
			result = -1;
			goto leave;
		}
		nvram_bufset(RT2860_NVRAM, "dhcpStart", dhcp_s);
		if (-1 == inet_addr(dhcp_e)) {
			DBG_MSG("invalid DHCP End IP");
			printf("0x1403");
			result = -1;
			goto leave;
		}
		nvram_bufset(RT2860_NVRAM, "dhcpEnd", dhcp_e);
		if (-1 == inet_addr(dhcp_m)) {
			DBG_MSG("invalid DHCP Subnet Mask");
			printf("0x1404");
			result = -1;
			goto leave;
		}
		nvram_bufset(RT2860_NVRAM, "dhcpMask", dhcp_m);
		nvram_bufset(RT2860_NVRAM, "dhcpEnabled", "1");
		WebTF(input, "dhcpPriDns", RT2860_NVRAM, "dhcpPriDns", 0);
		WebTF(input, "dhcpSecDns", RT2860_NVRAM, "dhcpSecDns", 0);
		WebTF(input, "dhcpGateway", RT2860_NVRAM, "dhcpGateway", 0);
		WebTF(input, "dhcpLease", RT2860_NVRAM, "dhcpLease", 0);
		WebTF(input, "dhcpStatic1", RT2860_NVRAM, "dhcpStatic1", 0);
		WebTF(input, "dhcpStatic2", RT2860_NVRAM, "dhcpStatic2", 0);
		WebTF(input, "dhcpStatic3", RT2860_NVRAM, "dhcpStatic3", 0);
	}
	nvram_commit(RT2860_NVRAM);
leave:
	free_all(8, ip, lanNetmask, dhcp_tp, dhcp_s, dhcp_e, dhcp_m, wan_ip, wan_mask);

	return result;
}

int set_client(char * input)
{
	char *ssid; 
	int conn_mode = 0;
	char off[4], off5g[4];
	char opmode[4];
	char connmode[4];
	int nvram_id = 0;
	//  remove temp file first
	unlink("/tmp/cur_ip.txt");
	unlink("/tmp/new_ip.txt");

	strcpy(opmode, web_get("opmode", input, 2));
	strcpy(off, web_get("RadioOff_24g", input, 2));
	strcpy(off5g, web_get("RadioOff_5g", input, 2));

	if ((nvram_id = getNvramIndex(web_get("wlan_conf", input, 0))) == -1) {
		return -1;
	}

	//  if all of the wireless interfaces have been turned off, we do nothing and return.
	if (!strcmp(off, "1") && !strcmp(off5g, "1"))
		return 0;

	if (strcmp(opmode, "0") == 0) {
		ssid = web_get("apcli_ssid", input, 2);
		if (strlen(ssid) == 0) {
			free(ssid);
			DBG_MSG("SSID is empty");
			return -1;
		}

		nvram_bufset(nvram_id, "ApCliSsid", ssid);
		free(ssid);
	}

	nvram_bufset(RT2860_NVRAM, "OperationMode", opmode);

	if (strcmp(opmode, "0") == 0) {
		WebTF(input, "apcli_type", RT2860_NVRAM, "ApCliType", 2);  //  2.4G/5G
		nvram_bufset(nvram_id, "ApCliEnable", "1");
		nvram_bufset(nvram_id, "apClient", "1");
		WebTF(input, "apcli_bssid", nvram_id, "ApCliBssid", 2);
		WebTF(input, "apcli_mode", nvram_id, "ApCliAuthMode", 2);
		WebTF(input, "apcli_enc", nvram_id, "ApCliEncrypType", 2);
		WebTF(input, "apcli_wpapsk", nvram_id, "ApCliWPAPSK", 2);
		WebTF(input, "apcli_default_key", nvram_id, "ApCliDefaultKeyID", 2);
		WebTF(input, "apcli_key1type", nvram_id, "ApCliKey1Type", 2);
		WebTF(input, "apcli_key1", nvram_id, "ApCliKey1Str", 2);
	}

	WebTF(input, "hostname", nvram_id, "HostName", 2);
	WebTF(input, "apcli_connmode", nvram_id, "ApCliConnectionMode", 2);
	strcpy(connmode, web_get("apcli_connmode", input, 0)); //  static/DHCP
	if (strlen(connmode) == 0)
		conn_mode = 0;
	else {
		conn_mode = atoi(connmode);
	}

	//  save the ip/mask when we in static ip mode
	if (conn_mode == 1) {
		WebTF(input, "apcli_ipaddr", RT2860_NVRAM, "ApCliIpAddress", 2);
		WebTF(input, "apcli_netmask", RT2860_NVRAM, "ApCliNetmask", 2);
	}

	return 0;
}

void update_checking(void)
{
	FILE * fp = NULL;
	char buf[4];
	fp = fopen("/var/checking", "r");
	if (fp) {
		fgets(buf, sizeof(buf), fp);
		if (strcmp(buf, "1") == 0) {
			//  don't print with '\n', otherwise, we can't check with whole string.
			printf("checking");
		} else {
			printf("checked");
		}
		fclose(fp);
	} else {
		printf("checked");
	}
}

void get_displayfw_st(void)
{
	char cmd[64];
	char buf[64];
	int result = -1;
	FILE * stpp = NULL;
	sprintf(cmd, "cat %s", STATUS_FILE);
	if ((stpp = popen(cmd, "r")) != NULL) {
		memset(buf, '\0', sizeof(buf));
		fgets(buf, sizeof(buf), stpp);
		printf(buf);
		pclose(stpp);
		stpp = NULL;
	} else {
		printf ("2,Status file not found\n");
	}
}

void get_burningfw_st(void)
{
	char cmd[64];
	char buf[64];
	int result = -1;
	FILE * stpp = NULL;
	sprintf(cmd, "cat %s", "/var/mtd_write.log");
	if ((stpp = popen(cmd, "r")) != NULL) {
		memset(buf, '\0', sizeof(buf));
		fgets(buf, sizeof(buf), stpp);
		printf(buf);
		pclose(stpp);
		stpp = NULL;
	}
}

void get_blburningfw_st(void)
{
	char cmd[64];
	char buf[64];
	int result = -1;
	FILE * stpp = NULL;
	sprintf(cmd, "cat %s", "/var/bl_mtd_write.log");
	if ((stpp = popen(cmd, "r")) != NULL) {
		memset(buf, '\0', sizeof(buf));
		fgets(buf, sizeof(buf), stpp);
		printf(buf);
		pclose(stpp);
		stpp = NULL;
	}
}

void check_for_updates()
{
	FILE * fp = NULL;
	char buf[128];
	char * p = NULL;
	unsigned int cur_ver = 0;
	unsigned int new_ver = 0;
	UPDATEINFO fwinfo;
	char display_info[128];
	char ver[10];
	char modelname[32];
	char cmdline[256];

	memset(&fwinfo, '\0', sizeof(UPDATEINFO));

	strcpy(modelname, nvram_bufget(RT2860_NVRAM, "ModelName"));
	//  here we need to use conditional compilation to check another online 
	//  folder for the update checking.
	//  now we only have MCT.
	if (!strcmp(modelname, "IPW611")) {
		sprintf(cmdline, "wget http://update.mct.com.tw/txrx/mct/ipw611/updateinfo.txt -O /var/updateinfo.txt -q");
	}
	//  get info
	unlink("/var/updateinfo.txt");
	if (system(cmdline) < 0) {
		fwinfo.result = 0;
	} else {
		strcpy(ver, nvram_bufget(RT2860_NVRAM, "Version"));
		cur_ver = convert_ver(ver);

		if (cur_ver > 0) {
			parse_info("/var/updateinfo.txt", &fwinfo);
			strcpy(ver, fwinfo.version);
			new_ver = convert_ver(ver);
			if (new_ver > cur_ver) {
				fwinfo.need_update = 1;
			} else {
				fwinfo.need_update = 0;
			}
		} else {
			fwinfo.result = 0;
		}
	}

	//  print result
	printf("{");
	printf("\"result\" : \"%d\",", fwinfo.result);
	printf("\"need_update\" : \"%d\",", fwinfo.need_update);
	printf("\"project_name\" : \"%s\",", fwinfo.project_name);
	printf("\"date\" : \"%s\",", fwinfo.date);
	printf("\"version\" : \"%s\",", fwinfo.version);
	printf("\"rxpath_name\" : \"%s\",", fwinfo.rxpath_name);
	printf("\"rxfile_size\" : \"%d\",", fwinfo.rxfile_size);
	printf("\"txpath_name\" : \"%s\",", fwinfo.txpath_name);
	printf("\"txfile_size\" : \"%d\"", fwinfo.txfile_size);
	printf("}");
}

void get_wget_st(void)
{
	char cmd[64];
	char buf[64];
	int result = -1;
	FILE * stpp = NULL;
	sprintf(cmd, "cat %s", "/var/wget.log");
	if ((stpp = popen(cmd, "r")) != NULL) {
		fgets(buf, sizeof(buf), stpp);
		printf(buf);
		pclose(stpp);
		stpp = NULL;
	}
}

char * extract_t6ver_str(char * dpinfo)
{
	int len = 0;
	char * p = NULL;
	char * p1 = NULL;
	char * ver = NULL;
	int cnt = 0;

	len = strlen(dpinfo);

	//  extract version
	p = dpinfo;
	while(p < (dpinfo+len)) {
		//  find third '>'
		p = strchr(p, '>');
		p++;
		if (++cnt == 3) {
			//  if p >= length of original string that mean we can't get information of T6
			if ((p+1) >= (dpinfo+len)) {
				ver = NULL;
			} else {
				//  extract version
				p1 = strchr(p, '>');
				if (p1) {
					*p1 = '\0';
					ver = p;
				} else {
					ver = NULL;
				}
			}
			break;
		}
	}

	return ver;
}

void checkpw(char * input)
{
	char pw[32];
	char * vpw = NULL;

	vpw = strdup(web_get("Password", input, 2));

	if (vpw == NULL || strlen(vpw) <= 0) {
		printf("{\"check_result\":\"0\"}");
	}

	strcpy(pw, nvram_bufget(RT2860_NVRAM, "Password"));
	if (strcmp(pw, vpw) == 0) {
		printf("{\"check_result\":\"1\"}");
	} else {
		printf("{\"check_result\":\"0\"}");
	}
}

void wan_probing(char * input)
{
	char buf[128];
	int conn_stat = chk_wanphy_stat();
	FILE * pp;
	int found = 0;
	char opmode[4];

	strcpy(opmode, nvram_bufget(RT2860_NVRAM, "OperationMode"));

	if (strcmp(opmode, "0") == 0) {
		/*  it is a workaround, if eth3 has been power down, we can't get IP from it.
		    so , I need to power up it before we do that.  */
		system("ifconfig eth3 up");
	} else if (strcmp(opmode, "2") == 0) {
		/*  it is a workaround, if eth3 stay in br0 when we are Network bridge mode, 
		    we can't get the IP from eth3  */
		do_system("brctl delif br0 eth3");
	}

	if (conn_stat == 1) {
		//  always get eth3 for detect mode
		pp = popen("udhcpc -i eth3 -s /sbin/udhcpc-probing.sh -q -n -T 10", "r");
		if (pp) {
			while(fgets(buf, sizeof(buf), pp) > 0) {
				if (strstr(buf, "\"ip\"") && strstr(buf, "\"subnet\"")) {
					printf("{\n");
					printf("\"status\":\"ip_got\",\n");
					printf("%s", buf);
					printf("}");
					found = 1;
					break;
				}
			}
			pclose(pp);
			if (found == 0) {
				//  can't get IP from server
				printf("{\n");
				printf("\"status\":\"no_ip\"\n");
				printf("}");
			}
		} else {
			//  command failed
			printf("{\n");
			printf("\"status\":\"cmd_failed\"\n");
			printf("}");
		}
	} else {
		// no cable
		printf("{\n");
		printf("\"status\":\"no_cable\"\n");
		printf("}");
	}

	if (strcmp(opmode, "0") == 0) {
		do_system("ifconfig eth3 down");
	} else if (strcmp(opmode, "2") == 0) {
		do_system("brctl addif br0 eth3");
	}
}

static void clear_update_files(void)
{
	unlink("/var/tmpFW");
	unlink("/var/mtd_write.log");
}

int main(int argc, char *argv[])
{
	char * going = NULL;
	char * query_str = NULL;
	char * page = NULL;
	char * len = NULL;
	char * input_buf = NULL;
	char * method = NULL;
	long input_len = 0;;
	int is_post = 0;

	nvram_init(RT2860_NVRAM);
	nvram_init(RTDEV_NVRAM);

	method = getenv("REQUEST_METHOD");

	if (method == NULL) {
		show_error_page();
		goto leave;
	}

	if (strncasecmp(method, "POST", 4) == 0)
		is_post = 1;

	if (is_post == 1) {
		len = getenv("CONTENT_LENGTH");
		if(len == NULL) {
			show_error_page();
			goto leave;
		}

		input_len = strtol(len, NULL, 10);
		input_len &= 0x7FFFFFFF;
		if (input_len <= 0) {
			show_error_page();
			goto leave;
		}
	} else {
		query_str = getenv("QUERY_STRING");

		input_len = strlen(query_str);

		if (input_len <= 0 || query_str == NULL) {
			show_error_page();
			goto leave;
		}
	}

	input_len += 1;

	input_buf = malloc(input_len);
	if(input_buf == NULL) {
		show_error_page();
		goto leave;
	}

	memset(input_buf, 0, input_len);

	if(is_post == 1)
		fgets(input_buf, input_len, stdin);
	else
		strncpy(input_buf, query_str, input_len);

	page = strdup(web_get("page", input_buf, 0));
	if (page == NULL || strlen(page) <= 0) {
		show_error_page();
		goto leave;
	}

	going = strdup(web_get("going", input_buf, 0));
	if(going == NULL || strlen(going) <= 0) {
		show_error_page();
		goto leave;
	}

	if (strcmp(going, "set_lang") == 0) {
		set_lang(input_buf);
	} else if (strcmp(going, "ack") == 0) {
		web_header();
		printf("GO");
	} else if (strcmp(going, "user_list") == 0) {
		web_header();
		get_all_user_list();
	} else if (strcmp(going, "dhcp_user_list") == 0) {
		web_header();
		get_dhcp_user_list();
	} else if (strcmp(going, "user_cnt") == 0) {
		web_header();
		system("/sbin/check-clnt.sh 1>/dev/null 2>/dev/null");
		printf("%d", get_user_count());
	} else if (strcmp(going, "set_apclient_ip") == 0) {
		set_apclient_ip(input_buf);
	} else if (strcmp(going, "get_apclient_ip") == 0) {
		web_header();
		get_apclient_ip(input_buf);
	} else if (strcmp(going, "apclient_status") == 0) {
		char status[256];
		web_header();
		strcpy(status, get_apclient_status(input_buf));
		printf("%s", status);
	} else if (strcmp(going , "get_aplist") == 0) {
		web_header();
		get_aplist(input_buf);
	} else if (strcmp(going, "refresh_aplist") == 0) {
		web_header();
		refresh_aplist(input_buf);
	} else if (strcmp(going, "time") == 0) {
		web_header();
		get_time();
	} else if (strcmp(going, "freemem") == 0) {
		web_header();
		get_mem_free();
	} else if (strcmp(going, "wantxrx") == 0) {
		char *  ethif = strdup(web_get("if", input_buf, 0));
		web_header();
		if (ethif) {
			getIfStatistic(ethif);
			free(ethif);
		} else {
			printf("0\n0\n0\n0\n");
		}
	} else if (strcmp(going, "lantxrx") == 0) {
		char *  ethif = strdup(web_get("if", input_buf, 0));
		web_header();
		if (ethif) {
			getIfStatistic(ethif);
			free(ethif);
		} else {
			printf("0\n0\n0\n0\n");
		}
	} else if (strcmp(going, "txrx") == 0) {
		char *  wanif = strdup(web_get("wanif", input_buf, 0));
		char *  lanif = strdup(web_get("lanif", input_buf, 0));
		if (wanif && lanif) {
			getAllStatistic(wanif, lanif);
			free(wanif);
			free(lanif);
		} else {
			printf("0\n0\n0\n0\n0\n0\n0\n0\n");
			if (wanif)
				free(wanif);
			if (lanif)
				free(lanif);
		}
	} else if (strcmp(going, "allstaInfo") == 0) {
		web_header();
		StaInfo(WLAN1_CONF);
		StaInfo(WLAN2_CONF);
	} else if (strcmp(going, "call_init_system_restart") == 0) {
		do_system("init_system restart");
	} else if (strcmp(going, "init_bridge_restart") == 0) {
		web_header();
		system("init_system restart-bridge");
		printf("DONE");
	} else if (strcmp(going, "update_checking") == 0) {
		web_header();
		update_checking();
	} else if (strcmp(going, "update_displayst") == 0) {
		web_header();
		get_displayfw_st();
	} else if (strcmp(going, "update_burningst") == 0) {
		web_header();
		get_burningfw_st();
	} else if (strcmp(going, "update_blburningst") == 0) {
		web_header();
		get_blburningfw_st();
	} else if (!strcmp(going, "check_for_updates")) {
		web_header();
		check_for_updates();
	} else if (strcmp(going, "update_wgetst") == 0) {
		web_header();
		get_wget_st();
	} else if (!strcmp(going, "clear_all_update_files")) {
		clear_update_files();
	} else {
		show_error_page();
	}

	//  ************************
	//  if we need more operation, we can add here
	//  ************************
leave:
	if (going)
		free(going);
	if (page)
		free(page);
	if (input_buf)
		free(input_buf);

	nvram_close(RT2860_NVRAM);
	nvram_close(RTDEV_NVRAM);

	return 0;
}
