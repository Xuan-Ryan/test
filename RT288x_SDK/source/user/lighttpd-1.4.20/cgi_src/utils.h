#ifndef HAVE_UTILS_H
#define HAVE_UTILS_H

#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/mman.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nvram.h"
#include "linux_conf.h"
#include "user_conf.h"

#include <dirent.h>
#include <sys/stat.h>
#if defined CONFIG_USER_STORAGE
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#define DATALEN		65
#define CMDLEN		256
#define IF_NAMESIZE     16
#define LOG_MAX 	16384

#define SYSTEM_COMMAND_LOG	"/var/system_command.log"
#define LOGFILE	"/dev/console"
#define MOUNT_INFO      "/proc/mounts"

struct media_config {
	char path[40];
};

/*
 * FOR CGI
 */
inline int get_one_port(void);
int get_ifip(char *ifname, char *if_addr);
int get_nth_value(int index, char *value, char delimit, char *result, int len);
char* get_field(char *a_line, char *delim, int count);
int get_nums(char *value, char delimit);
int get_if_live(char *ifname);
int delete_nth_value(int index[], int count, char *value, char delimit);
void set_nth_value_flash(int nvram, int index, char *flash_key, char *value);
char *get_lanif_name(void);
char *get_wanif_name(void);
char *get_macaddr(char *ifname);
char *get_mac_last3addr(char *ifname);
char *get_ipaddr(char *ifname);
char *get_netmask(char *ifname);
int get_index_routingrule(const char *rrs, char *dest, char *netmask, char *interface);
#if defined CONFIG_USER_STORAGE
FILE *fetch_media_cfg(struct media_config *cfg, int write);
#endif

int is_ipnetmask_valid(char *str);
int is_ip_valid(char *str);
int is_mac_valid(char *str);
int is_oneport_only(void);

char *strip_space(char *str);
char *racat(char *s, int i);
int netmask_aton(const char *ip);
void convert_string_display(char *str);
void do_system(char *fmt, ...);
int check_semicolon(char *str);
void free_all(int num, char *fmt, ...);

unsigned int convert_rssi_signal_quality(long rssi);

/* 
 * FOR PROCESS
 */
void update_flash_8021x(int nvram);

/* 
 * FOR WEB
 */
void web_redirect(const char *url);
void web_redirect_wholepage(const char *url);
void web_back_parentpage(void);
void web_debug_header(void);
void web_debug_header_no_cache(void);
char *web_get(char *tag, char *input, int dbg);
#define WebTF(str, tag, nvram, entry, dbg) 	nvram_bufset(nvram, entry, web_get(tag, str, dbg))

#define WebNthTF(str, tag, nvram, index, entry, dbg) 	set_nth_value_flash(nvram, index, entry, web_get(tag, str, dbg))

/* Set the same value into all BSSID */
#define SetAllValues(bssid_num, nvram, flash_key, value)	do {	char buffer[24]; int i=1;                       \
									strcpy(buffer, value);                          \
									while (i++ < bssid_num)                         \
									sprintf(buffer, "%s;%s", buffer, value);    \
									nvram_bufset(nvram, #flash_key, buffer);        \
								} while(0)

#define DBG_MSG(fmt, arg...)	do {	FILE *log_fp = fopen(LOGFILE, "w+"); \
						fprintf(log_fp, "%s:%s:%d:" fmt "\n", __FILE__, __func__, __LINE__, ##arg); \
						fclose(log_fp); \
					} while(0)
/*
 *  Uboot image header format
 *  (ripped from mkimage.c/image.h)
 */
#define STATUS_FILE "/var/updateSta"
#define IH_MAGIC    0x27051956
#define IH_NMLEN    32-4
typedef struct image_header {
    uint32_t    ih_magic;   /* Image Header Magic Number    */
    uint32_t    ih_hcrc;    /* Image Header CRC Checksum    */
    uint32_t    ih_time;    /* Image Creation Timestamp */
    uint32_t    ih_size;    /* Image Data Size      */
    uint32_t    ih_load;    /* Data  Load  Address      */
    uint32_t    ih_ep;      /* Entry Point Address      */
    uint32_t    ih_dcrc;    /* Image Data CRC Checksum  */
    uint8_t     ih_os;      /* Operating System     */
    uint8_t     ih_arch;    /* CPU architecture     */
    uint8_t     ih_type;    /* Image Type           */
    uint8_t     ih_comp;    /* Compression Type     */
    uint8_t     ih_name[IH_NMLEN];  /* Image Name       */
    uint32_t	ih_ksz;     /* Kernel Part Size		*/
} image_header_t;

typedef struct trigger_header {
	uint8_t     tag[4];
	uint32_t    size;
	uint32_t    checksum;
	uint8_t     version;
	uint8_t     reserved[3];
} trigger_header_t;

typedef struct _updateinfo {
	int result;
	int need_update;
	char project_name[256];
	char date[32];
	char version[64];
	char rxpath_name[PATH_MAX];
	unsigned int rxfile_size;
	char txpath_name[PATH_MAX];
	unsigned int txfile_size;
} UPDATEINFO;

int convert_ver(const char * ver);
int convert_t6ver(const char * ver);
unsigned int extract_t6ver(char * dpinfo);
void parse_info(char * file_name, UPDATEINFO * info);
char * extract_version(char * str);
void remove_carriage(char * str);

void lookupAllList(char *dir_path);
void lookupSelectList(void);
#endif


//#if defined (CONFIG_RT2860V2_STA_DPB) || defined (CONFIG_RT2860V2_STA_ETH_CONVERT) || \
	defined (CONFIG_RLT_STA_SUPPORT) || defined (CONFIG_MT_STA_SUPPORT) || \
	defined (CONFIG_RT2860V2_STA) || defined (CONFIG_RT2860V2_STA_MODULE)
#if defined (RT2860_STA_SUPPORT)
#ifndef FALSE
#define FALSE	(0==1)
#endif

#ifndef TRUE
#define TRUE	(1==1)
#endif
        
int OidQueryInformation(unsigned long OidQueryCode, int socket_id, char *DeviceName, void *ptr, unsigned long PtrLength);
int OidSetInformation(unsigned long OidQueryCode, int socket_id, char *DeviceName, void *ptr, unsigned long PtrLength);
#endif
