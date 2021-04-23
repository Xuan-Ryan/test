#include "utils.h"
#include <stdarg.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <linux/wireless.h>
#include <wait.h>

#include "busybox_conf.h"
#include "oid.h"

/*  Firmware file name, including full path name  */
char sys_fw[128] = "";
char sys_info[128] = "";
char t6_fw[128] = "";
char t6_info[128] = "";
char bootloader[128] = "";
char bootloader_info[128] = "";

char * get_usbmp()
{
	static char buf[256] = "";
	FILE * pp = NULL;
	char cmd[256];
	sprintf(cmd, "df | grep \"/media/\" | sed 's/^.*media/\\/media/g'");

	if((pp = popen(cmd, "r")) == NULL) {
		DBG_MSG("popen()df error!\r\n");
		return "";
	}

	fgets(buf, sizeof(buf), pp);

	buf[strlen(buf)-1] = '\0';

	return buf;
}

int test_usb()
{
	char mp[64];
	char cmd[256];
	char buf[256];
	char * p = NULL;
	FILE * pp;

	strcpy(mp, get_usbmp());

	if (strlen(mp)) {
		sprintf(cmd, "dd if=/dev/zero of=%s/online_test.bin bs=1M count=50 2>&1", mp);
		pp = popen(cmd, "r");
		if (pp) {
			while(fgets(buf, sizeof(buf), pp)) {
				if ((p=strstr(buf, "seconds,"))) {
					pclose(pp);
					while(*(p++) != ' ');
					printf("1-%s", p);
					return 0;
				}
			}
			pclose(pp);
			printf("2-3");  //  get wrong result
		} else {
			printf("2-2");  //  failed to run command
		}
	} else {
		printf("2-1");  //  no mount point got
	}
	return 1;
}

int test_led()
{
	system("gpio l 45 4000 0 1 0 4000;gpio l 46 4000 0 1 0 4000;gpio l 47 4000 0 1 0 4000;gpio l 48 4000 0 1 0 4000");
	printf("DONE");
}

int test_trigger()
{
	system("/usr/bin/T6-utility video");
	printf("DONE");
}

static int check_wanphy_st(void)
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
			else
				ret = 2;  //  not connected
		}
		pclose(pp);
		return ret;
	}
	return ret;
}

static int check_lanphy_st(int idx)  //  idx = 0~3
{
	FILE * pp;
	char buf[32];
	char cmd[128];
	static char output[16] = "";
	unsigned int sta = 0;
	int stat = 0;

	sprintf(cmd, "/bin/mii_mgr -g -p 31 -r 0x%x", 0x3008+idx*0x100);
	pp = popen (cmd, "r");
	if (pp != NULL) {
		if (fgets(buf, sizeof(buf), pp)) {
			sscanf(buf, "Get: %*s = %x", &sta);
			if(sta & 0x01)
				stat = 1;
			else
				stat = 2;  //  not connected
		}
		pclose(pp);
	} else {
		stat = 0;
	}

	return stat;
}

static char * get_speed(int index)
{
	FILE * pp = NULL;
	static char buf[512];
	char * p = NULL;

	if (index == 0) {
		pp = popen("iperf -c 192.168.2.254 -n 10485760 -y c 2>&1", "r");
	} else if (index == 1) {
		pp = popen("iperf -c 10.10.10.1 -n 10485760 -y c 2>&1", "r");
	} else if (index == 2) {
		pp = popen("iperf -c 10.10.10.2 -n 10485760 -y c 2>&1", "r");
	} else if (index == 3) {
		pp = popen("iperf -c 10.10.10.3 -n 10485760 -y c 2>&1", "r");
	} else if (index == 4) {
		pp = popen("iperf -c 10.10.10.4 -n 10485760 -y c 2>&1", "r");
	} else if (index == 5) {
		pp = popen("iperf -c 10.10.10.5 -n 10485760 -y c 2>&1", "r");
	} else if (index == 6) {
		pp = popen("iperf -c 10.10.10.6 -n 10485760 -y c 2>&1", "r");
	} 
	if (pp != NULL) {
		if (fgets(buf, sizeof(buf), pp) != NULL) {
			if (strstr(buf, "connect failed")) {
				pclose(pp);
				return "Connect Failed";
			} else {
				p = strrchr(buf, ',');
				p++;
				p[strlen(p)-1] = '\0';
			}
		}
		pclose(pp);
	}

	if (p == NULL)
		return "";
	return p;
}

int test_ethernet()
{
	printf("%d-", check_wanphy_st());
	printf("%s,", get_speed(0));
	printf("%d-", check_lanphy_st(0));
	printf("%s,", get_speed(1));
	printf("%d-", check_lanphy_st(1));
	printf("%s,", get_speed(2));
	printf("%d-", check_lanphy_st(2));
	printf("%s,", get_speed(3));
	printf("%d-", check_lanphy_st(3));
	printf("%s", get_speed(4));
}

static int check_wifi_connection(int idx)  //  0~1
{
	int i, s;
	struct iwreq iwr;
	RT_802_11_MAC_TABLE table = {0};
	char ifname[5];
	int BW = 20;

	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (idx == 0)
		strncpy(iwr.ifr_name, "ra0", IFNAMSIZ);
	else if (idx == 1)
		strncpy(iwr.ifr_name, "rai0", IFNAMSIZ);
	else
		strncpy(iwr.ifr_name, "ra0", IFNAMSIZ);

	iwr.u.data.pointer = (caddr_t) &table;
	if (s < 0) {
		return 0;
	}

	if (ioctl(s, RTPRIV_IOCTL_GET_MAC_TABLE_STRUCT, &iwr) < 0) {
		close(s);
		return 0;
	}

	close(s);

	if (table.Num > 0)
		return 1;

	return 2;
}

int test_wifi()
{
	printf("%d-", check_wifi_connection(0));
	printf("%s,", get_speed(5));
	printf("%d-", check_wifi_connection(1));
	printf("%s", get_speed(6));
}

struct online_test_item {
	int usb_test;
	char usb_speed[32];
	int wan_test;
	char wan_speed[32];
	int lan1_test;
	char lan1_speed[32];
	int lan2_test;
	char lan2_speed[32];
	int lan3_test;
	char lan3_speed[32];
	int lan4_test;
	char lan4_speed[32];
	int wifi1_test;
	char wifi1_speed[32];
	int wifi2_test;
	char wifi2_speed[32];
};

struct online_test_item item_result;
static const char * result_path = "/var/online_test.bin";

static void get_devinfo()
{
	char * tmp;
	nvram_init(RT2860_NVRAM);
	nvram_init(RTDEV_NVRAM);

	printf("{");
	printf("\"ModelName\":\"%s\",", nvram_bufget(RT2860_NVRAM, "ModelName"));
	printf("\"Type\":\"%s\",", nvram_bufget(RT2860_NVRAM, "Type"));
	printf("\"Version\":\"%s\",", nvram_bufget(RT2860_NVRAM, "Version"));
	printf("\"ssid_5g\":\"%s\",", nvram_bufget(RTDEV_NVRAM, "SSID1"));
	tmp = get_macaddr("eth2");
	if(tmp == NULL) tmp = "";
	printf("\"mac_eth\":\"%s\",", tmp);
	tmp = get_macaddr("rai0");
	if(tmp == NULL) tmp = "";
	printf("\"mac_5g\":\"%s\"", tmp);
	printf("}");

	nvram_close(RT2860_NVRAM);
	nvram_close(RTDEV_NVRAM);
}

int get_result()
{
	FILE * fp = NULL;

	get_devinfo();

	/*fp = fopen (result_path, "r");
	if (fp) {
		fread(&item_result, 1, sizeof(item_result), fp);
		printf("%d-", item_result.usb_test);
		printf("%s>", item_result.usb_speed);
		printf("%d-", item_result.wan_test);
		printf("%s,", item_result.wan_speed);
		printf("%d-", item_result.lan1_test);
		printf("%s,", item_result.lan1_speed);
		printf("%d-", item_result.lan2_test);
		printf("%s,", item_result.lan2_speed);
		printf("%d-", item_result.lan3_test);
		printf("%s,", item_result.lan3_speed);
		printf("%d-", item_result.lan4_test);
		printf("%s>", item_result.lan4_speed);
		printf("%d-", item_result.wifi1_test);
		printf("%s,", item_result.wifi1_speed);
		printf("%d-", item_result.wifi2_test);
		printf("%s", item_result.wifi2_speed);
		fclose(fp);
	} else {
		printf("...");
	}*/
}

int re_test()
{
	FILE * pp = NULL;

	pp = popen("/bin/online_test", "r");
	pclose(pp);
}

void set_region(char * input)
{
	char * nvram_str = NULL;
	int nvram_id;
	char * country_region = NULL;

	nvram_str = strdup(web_get("nvram_id", input, 0));
	if (nvram_str == NULL || strlen(nvram_str) <= 0) {
		goto leave;
	}

	if (strcmp(nvram_str, "rtdev") == 0)
		nvram_id = RTDEV_NVRAM;
	else
		nvram_id = RT2860_NVRAM;

	country_region = strdup(web_get("country_region", input, 0));
	if (country_region == NULL || strlen(country_region) <= 0) {
		goto leave;
	}

	nvram_init(nvram_id);

	if (strcmp(nvram_str, "rtdev") == 0) {
		//  5G
		nvram_bufset(nvram_id, "CountryRegionABand", country_region);
	} else  //  2.4G
		nvram_bufset(nvram_id, "CountryRegion", country_region);

	nvram_close(nvram_id);
	printf("DONE");
leave:
	if (nvram_str)
		free(nvram_str);
	if (country_region)
		free(country_region);
}

void set_model_name(char * input)
{
	int nvram_id;
	char * model_name = NULL;

	nvram_id = RT2860_NVRAM;

	model_name = strdup(web_get("ModelName", input, 0));
	if (model_name == NULL || strlen(model_name) <= 0) {
		goto leave;
	}

	nvram_init(nvram_id);

	nvram_bufset(nvram_id, "ModelName", model_name);

	nvram_close(nvram_id);
	printf("DONE");
leave:
	if (model_name)
		free(model_name);
}

/*  firmware file updating  */
inline void clear_update_files(void)
{
	unlink("/var/checking");
	unlink("/var/mtd_write.log");
}

inline unsigned int getMTDPartSize(char *part)
{
	char buf[128], name[32], size[32], dev[32], erase[32];
	unsigned int result=0;
	FILE *fp = fopen("/proc/mtd", "r");
	if(!fp){
		fprintf(stderr, "mtd support not enable?");
		return 0;
	}
	while(fgets(buf, sizeof(buf), fp)){
		sscanf(buf, "%32s %32s %32s %32s", dev, size, erase, name);
		if(!strcmp(name, part)){
			result = strtol(size, NULL, 16);
			break;
		}
	}
	fclose(fp);
	return result;
}

static int check(char *imagefile, int offset, int len, char *err_msg)
{
	struct stat sbuf;

	int  data_len;
	char *data;
	unsigned char *ptr;
	unsigned long checksum;
	char cmd[128];

	image_header_t header;
	image_header_t *hdr = &header;
	int ifd;

	if ((unsigned)len < sizeof(image_header_t)) {
		sprintf (err_msg, "Bad size: \"%s\" is no valid image\n", imagefile);
		return 0;
	}

	ifd = open(imagefile, O_RDONLY);
	if(!ifd){
		sprintf (err_msg, "Can't open %s: %s\n", imagefile, strerror(errno));
		close(ifd);
		return 0;
	}

	if (fstat(ifd, &sbuf) < 0) {
		close(ifd);
		sprintf (err_msg, "Can't stat %s: %s\n", imagefile, strerror(errno));
		return 0;
	}

	ptr = (unsigned char *) mmap(0, sbuf.st_size, PROT_READ, MAP_SHARED, ifd, 0);
	if ((caddr_t)ptr == (caddr_t)-1) {
		close(ifd);
		sprintf (err_msg, "Can't mmap %s: %s\n", imagefile, strerror(errno));
		return 0;
	}
	ptr += offset;

	/*
	 *  handle Header CRC32
	 */
	memcpy (hdr, ptr, sizeof(image_header_t));

	if (ntohl(hdr->ih_magic) != IH_MAGIC) {
		munmap(ptr, len);
		close(ifd);
		sprintf (err_msg, "Bad Magic Number: \"%s\" is no valid image\n", imagefile);
		return 0;
	}

	data = (char *)hdr;

	checksum = ntohl(hdr->ih_hcrc);
	hdr->ih_hcrc = htonl(0);	/* clear for re-calculation */

	if (crc32 (0, data, sizeof(image_header_t)) != checksum) {
		munmap(ptr, len);
		close(ifd);
		sprintf (err_msg, "*** Warning: \"%s\" has bad header checksum!\n", imagefile);
		return 0;
	}

	/*
	 *  handle Data CRC32
	 */
	data = (char *)(ptr + sizeof(image_header_t));
	data_len  = len - sizeof(image_header_t) ;

	if (crc32 (0, data, data_len) != ntohl(hdr->ih_dcrc)) {
		munmap(ptr, len);
		close(ifd);
		sprintf (err_msg, "*** Warning: \"%s\" has corrupted data!\n", imagefile);
		return 0;
	}

#if 1
	/*
	 * compare MTD partition size and image size
	 */
#if defined (CONFIG_RT2880_ROOTFS_IN_RAM)
	if(len > getMTDPartSize("\"Kernel\"")){
		munmap(ptr, len);
		close(ifd);
		sprintf(err_msg, "*** Warning: the image file(0x%x) is bigger than Kernel MTD partition.\n", len);
		return 0;
	}
#elif defined (CONFIG_RT2880_ROOTFS_IN_FLASH)
#ifdef CONFIG_ROOTFS_IN_FLASH_NO_PADDING
	if(len > getMTDPartSize("\"Kernel_RootFS\"")){
		munmap(ptr, len);
		close(ifd);
		sprintf(err_msg, "*** Warning: the image file(0x%x) is bigger than Kernel_RootFS MTD partition.\n", len);
		return 0;
	}
#else
	if(len < CONFIG_MTD_KERNEL_PART_SIZ){
		munmap(ptr, len);
		close(ifd);
		sprintf(err_msg, "*** Warning: the image file(0x%x) size doesn't make sense.\n", len);
		return 0;
	}

	if((len - CONFIG_MTD_KERNEL_PART_SIZ) > getMTDPartSize("\"RootFS\"")){
		munmap(ptr, len);
		close(ifd);
		sprintf(err_msg, "*** Warning: the image file(0x%x) is bigger than RootFS MTD partition.\n", len - CONFIG_MTD_KERNEL_PART_SIZ);
		return 0;
	}
#endif
#else
#if defined (CONFIG_NVRAM_RW_FILE)
#else
//#error "goahead: no CONFIG_RT2880_ROOTFS defined!"
#endif
#endif
#endif
	snprintf(cmd, sizeof(cmd),"nvram_set 2860 Version \"%s\"", extract_version(hdr->ih_name));
	system(cmd);
	munmap(ptr, len);
	close(ifd);

	return 1;
}

static int check_sum(char * data, int size)
{
	int i = 0;
	int result = 0;

	for (i=0; i<size; i+=4) {
		result += *((int *)(data+i));
	}

	return result;
}

static int check_trigger(char *imagefile, int offset, int len, char *err_msg)
{
	struct stat sbuf;

	int  data_len;
	char *data;
	unsigned char *ptr;
	unsigned long checksum;
	char cmd[128];
	trigger_header_t header;
	trigger_header_t *hdr = &header;
	int ifd;

	if ((unsigned)len < sizeof(trigger_header_t)) {
		sprintf (err_msg, "Bad size: \"%s\" is no valid image\n", imagefile);
		return 0;
	}

	ifd = open(imagefile, O_RDONLY);
	if(!ifd){
		sprintf (err_msg, "Can't open %s: %s\n", imagefile, strerror(errno));
		close(ifd);
		return 0;
	}

	if (fstat(ifd, &sbuf) < 0) {
		close(ifd);
		sprintf (err_msg, "Can't stat %s: %s\n", imagefile, strerror(errno));
		return 0;
	}

	ptr = (unsigned char *) mmap(0, sbuf.st_size, PROT_READ, MAP_SHARED, ifd, 0);
	if ((caddr_t)ptr == (caddr_t)-1) {
		close(ifd);
		sprintf (err_msg, "Can't mmap %s: %s\n", imagefile, strerror(errno));
		return 0;
	}
	ptr += offset;

	/*
	 *  handle Header CRC32
	 */
	memcpy (hdr, ptr, sizeof(trigger_header_t));

	if (strcmp(hdr->tag, "IMG_")) {
		munmap(ptr, len);
		close(ifd);
		sprintf (err_msg, "Bad Magic Number: \"%s\" is no valid image\n", imagefile);
		return 0;
	}

	data = (char *)(ptr + sizeof(trigger_header_t));
	data_len  = len - sizeof(trigger_header_t) ;

	checksum = hdr->checksum;
	hdr->checksum = 0;	/* clear for re-calculation */
	//  check sum
	if (check_sum(data, hdr->size-sizeof(trigger_header_t)) != checksum) {
		munmap(ptr, len);
		close(ifd);
		sprintf (err_msg, "*** Warning: \"%s\" has bad header checksum!\n", imagefile);
		return 0;
	}

	munmap(ptr, len);
	close(ifd);

	return 1;
}

inline int mtd_write_firmware(char *filename, int offset, int len)
{
	char cmd[512];
	int status;
#if defined (CONFIG_RT2880_FLASH_8M) || defined (CONFIG_RT2880_FLASH_16M)
	/* workaround: erase 8k sector by myself instead of mtd_erase */
	/* this is for bottom 8M NOR flash only */
	snprintf(cmd, sizeof(cmd), "/bin/flash -f 0x400000 -l 0x40ffff");
	system(cmd);
#endif

#if defined (CONFIG_RT2880_ROOTFS_IN_RAM)
	snprintf(cmd, sizeof(cmd), "/bin/mtd_write -o %d -l %d write \"%s\" Kernel", offset, len, filename);
	status = system(cmd);
#elif defined (CONFIG_RT2880_ROOTFS_IN_FLASH)
  #ifdef CONFIG_ROOTFS_IN_FLASH_NO_PADDING
	snprintf(cmd, sizeof(cmd), "/bin/mtd_write -o %d -l %d write %s Kernel_RootFS", offset, len, filename);
	status = system(cmd);
  #else
	snprintf(cmd, sizeof(cmd), "/bin/mtd_write -o %d -l %d write %s Kernel", offset,  CONFIG_MTD_KERNEL_PART_SIZ, filename);
	status = system(cmd);
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
		return -1;
	if(len > CONFIG_MTD_KERNEL_PART_SIZ ){
		snprintf(cmd, sizeof(cmd), "/bin/mtd_write -o %d -l %d write %s RootFS", offset + CONFIG_MTD_KERNEL_PART_SIZ, len - CONFIG_MTD_KERNEL_PART_SIZ, filename);
		status = system(cmd);
	}
  #endif
#elif defined (CONFIG_MTK_EMMC_SUPPORT)
	snprintf(cmd, sizeof(cmd), "/bin/mtd_write -o %d -l %d write %s bootimg", offset, len, filename);
	status = system(cmd);
#elif (defined (CONFIG_MTK_MTD_NAND) && defined (CONFIG_ARCH_MT7623))
	snprintf(cmd, sizeof(cmd), "/bin/mtd_write -o %d -l %d write %s boot", offset, len, filename);
	status = system(cmd);
#else
	fprintf(stderr, "goahead: no CONFIG_RT2880_ROOTFS defined!");
#endif
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
		return -1;
	return 0;
}

void do_burning(void)
{
	char cmd[256];
	char err_msg[256];
	struct stat st;
	UPDATEINFO info;

	if (strlen(bootloader)) {
		parse_info(bootloader_info, &info);
		snprintf(cmd, sizeof(cmd), "/bin/mtd_write -o 0 -l %d write \"%s\" Bootloader", info.file_size, bootloader);
		system(cmd);
		sleep(3);
	}

	if (strlen(sys_fw)) {
		parse_info(sys_info, &info);
		mtd_write_firmware(sys_fw, 0, info.file_size);
	}

	if (strlen(t6_fw)) {
		parse_info(t6_info, &info);
		snprintf(cmd, sizeof(cmd), "%s \"%s\" %s", "/usr/bin/t6usbupdate", t6_fw, STATUS_FILE);
		system(cmd);
	}
}

void write_checkst(int st)
{
	FILE * fp = NULL;
	fp = fopen("/var/checking", "w");
	if (fp) {
		fprintf(fp, "%d", st);
		fclose(fp);
	}
}

int check_fw_format()
{
	char cmd[256];
	char err_msg[256];
	struct stat st;
	UPDATEINFO info;

	memset(&info, '\0', sizeof(UPDATEINFO));

	if (strlen(bootloader)) {
		if (stat(bootloader_info, &st) < 0) {
			printf("FAILED:NO_BOOTLOADERINFO");
			return -1;
		}
		parse_info(bootloader_info, &info);
		if (stat(bootloader, &st) == 0) {
			if (st.st_size != info.file_size) {
				printf("FAILED:WRONG_BOOTLOADER");
				return -1;
			}
		} else {
			printf("FAILED:NO_BOOTLOADER");
			return -1;
		}
	}

	if (strlen(sys_fw)) {
		if (stat(sys_info, &st) < 0) {
			printf("FAILED:NO_SYSINFO");
			return -1;
		}
		parse_info(sys_info, &info);

		if (stat(sys_fw, &st) == 0) {
			if (!check(sys_fw, 0, info.file_size, err_msg)) {
				printf("FAILED:WRONG_SYSFW");
				//  if we got the invalid format fw, we have to remove all files
				clear_update_files();
				return -1;
			}
		} else {
			printf("FAILED:NO_SYSFW");
			return -1;
		}
	}

	if (strlen(t6_fw)) {
		if (stat(t6_info, &st) < 0) {
			printf("FAILED:NO_T6INFO");
			return -1;
		}
		parse_info(t6_info, &info);

		if (stat(t6_fw, &st) == 0) {
			if (!check_trigger(t6_fw, 0, info.file_size, err_msg)) {
				printf("FAILED:WRONG_T6FW");
				//  if we got the invalid format fw, we have to remove all files
				clear_update_files();
				return -1;
			}
		} else {
			printf("FAILED:NO_T6FW");
			return -1;
		}
	}
	return 0;
}

void find_fw_file(void)
{
	FILE * fp_mount = fopen(MOUNT_INFO, "r");
	FILE * fp_info = NULL;
	FILE * fp_t6info = NULL;
	char cmd[256];
	char part[30], path[50];
	char full_name[256];
	char t6full_name[256];
	static char file_name[128] = "";
	char line_buf[256];
	char * p = NULL;
	int found = 0;

	if (fp_mount == NULL) {
		DBG_MSG("opne %s fail!", MOUNT_INFO);
		return;
	}

	while(fscanf(fp_mount, "%30s %50s %*s %*s %*s %*s\n", part, path) != EOF) {
		struct dirent *dirp;
		struct stat statbuf;

		if (strncmp(path, "/media/", 7) != 0) {
			continue;
		}

		sprintf(full_name, "%s/online_update/system/updateinfo.txt", path);
		if (lstat(full_name, &statbuf) == 0) {
			fp_info = fopen(full_name, "r");
			if (fp_info) {
				while(fgets(line_buf, sizeof(line_buf), fp_info)) {
					if (strncmp(line_buf, "PathName", 8) == 0) {
						p = strrchr(line_buf, '/');
						if (p) {
							//  keep it into the info buffer
							strcpy(sys_info, full_name);
							p++;
							remove_carriage(p);
							sprintf(full_name, "%s/online_update/system/%s", path, p);
							if (lstat(full_name, &statbuf) == 0) {
								strcpy(sys_fw, full_name);
								found = 1;
							} else {
								DBG_MSG("failed to stat %s", full_name);
							}
						} else {
							DBG_MSG("Wrong text format in updateinfo.txt");
						}
						break;
					}
				}
				fclose(fp_info);
			} else {
				DBG_MSG("failed to open %s", full_name);
			}
		} else {
			DBG_MSG("cannot find %s", full_name);
		}
		sprintf(full_name, "%s/online_update/t6/updateinfo.txt", path);
		if (lstat(full_name, &statbuf) == 0) {
			fp_info = fopen(full_name, "r");
			if (fp_info) {
				while(fgets(line_buf, sizeof(line_buf), fp_info)) {
					if (strncmp(line_buf, "PathName", 8) == 0) {
						p = strrchr(line_buf, '/');
						if (p) {
							//  keep it into the into buffer
							strcpy(t6_info, full_name);
							p++;
							remove_carriage(p);
							sprintf(full_name, "%s/online_update/t6/%s", path, p);
							if (lstat(full_name, &statbuf) == 0) {
								strcpy(t6_fw, full_name);
								found = 1;
							} else {
								DBG_MSG("failed to stat %s", full_name);
							}
							break;
						} else {
							DBG_MSG("Wrong text format in updateinfo.txt");
						}
					}
				}
				fclose(fp_info);
			} else {
				DBG_MSG("failed to open %s", full_name);
			}
		} else {
			DBG_MSG("cannot find %s", full_name);
		}
		sprintf(full_name, "%s/online_update/bootloader/updateinfo.txt", path);
		if (lstat(full_name, &statbuf) == 0) {
			fp_info = fopen(full_name, "r");
			if (fp_info) {
				while(fgets(line_buf, sizeof(line_buf), fp_info)) {
					if (strncmp(line_buf, "PathName", 8) == 0) {
						//  keep it into the into buffer
						strcpy(bootloader_info, full_name);
						p = strrchr(line_buf, '=');
						if (p) {
							p+=2;
							remove_carriage(p);
							sprintf(full_name, "%s/online_update/bootloader/%s", path, p);
							if (lstat(full_name, &statbuf) == 0) {
								strcpy(bootloader, full_name);
								found = 1;
							} else {
								DBG_MSG("failed to stat %s", full_name);
							}
							break;
						} else {
							DBG_MSG("Wrong text format in updateinfo.txt");
						}
					}
				}
				fclose(fp_info);
			} else {
				DBG_MSG("failed to open %s", full_name);
			}
		} else {
			DBG_MSG("cannot find %s", full_name);
		}

		if (found == 1)
			break;
	}

	if(fp_mount)
		fclose(fp_mount);
}

/*  find the FW file and call checking function  */
void do_auto_burning(char * input)
{
	//  find the firmware file
	find_fw_file();
	if (strlen(sys_fw) == 0 && strlen(t6_fw) == 0 && strlen(bootloader) == 0) {
		printf("NO_FILE");
		return;
	}

	write_checkst(1);
	if (check_fw_format()) {
		unlink("/var/checking");
		return;
	}
	write_checkst(0);

	do_burning();
	sleep(2);
	clear_update_files();
	printf("DONE");
}

int main(int argc, char *argv[])
{
	char * item = NULL;
	char * page = NULL;
	char * len = NULL;
	char * input_buf = NULL;
	long input_len = 0;;

	len = getenv("CONTENT_LENGTH");
	if(len == NULL) {
		goto leave;
	}

	input_len = strtol(len, NULL, 10);
	input_len &= 0x7FFFFFFF;
	if (input_len <= 0) {
		goto leave;
	}

	input_len += 1;

	input_buf = malloc(input_len);
	if(input_buf == NULL) {
		goto leave;
	}

	memset(input_buf, 0, input_len);

	fgets(input_buf, input_len, stdin);

	page = strdup(web_get("page", input_buf, 0));
	if (page == NULL || strlen(page) <= 0) {
		goto leave;
	}

	item = strdup(web_get("item", input_buf, 0));
	if(item == NULL || strlen(item) <= 0) {
		goto leave;
	}

	web_header();
	if (strcmp(item, "init_ssid") == 0) {
		system("init_onlinetest.sh");
		printf("DONE");
	} else if (strcmp(item, "result") == 0) {
		get_result();
	} else if (strcmp(item, "re-test") == 0) {
		re_test();
	} else if (strcmp(item, "USB") == 0) {
		test_usb();
	} else if (strcmp(item, "LED") == 0) {
		test_led();
	} else if (strcmp(item, "trigger") == 0) {
		test_trigger();
	} else if (strcmp(item, "ethernet") == 0) {
		test_ethernet();
	} else if (strcmp(item, "wifi") == 0) {
		test_wifi();
	} else if (strcmp(item, "country_region") == 0) {
		set_region(input_buf);
	} else if (strcmp(item, "model_name") == 0) {
		set_model_name(input_buf);
	} else if (strcmp(item, "auto_update") == 0) {
		do_auto_burning(input_buf);
	} else if (strcmp(item, "stop_service") == 0) {
		do_system("killall mct_gadget");
		do_system("killall uvcclient");
	} else if (strcmp(item, "onlinetest") == 0) {
		/*  TBD  */
	} else {
		printf("Unknown Command");
	}

leave:
	if (item)
		free(item);
	if (page)
		free(page);
	if (input_buf)
		free(input_buf);

	return 0;
}
