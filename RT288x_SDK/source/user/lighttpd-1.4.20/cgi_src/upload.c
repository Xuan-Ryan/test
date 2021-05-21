#include <stdarg.h>
#include "upload.h"
#include <string.h>

#define LOGFILE	"/dev/console"
#define DBG_MSG(fmt, arg...)	do {	FILE *log_fp = fopen(LOGFILE, "w+"); \
						fprintf(log_fp, "%s:%s:%d:" fmt "\n", __FILE__, __func__, __LINE__, ##arg); \
						fclose(log_fp); \
					} while(0)

#define REFRESH_TIMEOUT		"2000"		/* 40000 = 40 secs*/

#define RFC_ERROR "RFC1867 error"
#define UPLOAD_FILE "/var/tmpFW"
#define MEM_SIZE	1024
#define MEM_HALT	512
#define CMDLEN		256

static char cmd[CMDLEN];
void do_system(char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vsprintf(cmd, fmt, ap);
	va_end(ap);
	sprintf(cmd, "%s 1>%s 2>&1", cmd, LOGFILE);
	system(cmd);

	return;
}

void *memmem(const void *buf, size_t buf_len, const void *byte_line, size_t byte_line_len)
{
	unsigned char *bl = (unsigned char *)byte_line;
	unsigned char *bf = (unsigned char *)buf;
	unsigned char *p  = bf;

	while (byte_line_len <= (buf_len - (p - bf))){
		unsigned int b = *bl & 0xff;
		if ((p = (unsigned char *) memchr(p, b, buf_len - (p - bf))) != NULL){
			if ( (memcmp(p, byte_line, byte_line_len)) == 0)
				return p;
			else
				p++;
		}else{
			break;
		}
	}
	return NULL;
}

#if defined (UPLOAD_FIRMWARE_SUPPORT)

/*
 *  taken from "mkimage -l" with few modified....
 */
int check(char *imagefile, int offset, int len, char *err_msg)
{
	struct stat sbuf;

	int  data_len;
	char *data;
	unsigned char *ptr;
	unsigned long checksum;

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
#error "goahead: no CONFIG_RT2880_ROOTFS defined!"
#endif
#endif

	munmap(ptr, len);
	close(ifd);

	return 1;
}


#endif /* UPLOAD_FIRMWARE_SUPPORT */

/*
 * I'm too lazy to use popen() instead of system()....
 * ( note:  static buffer used)
 */
#define DEFAULT_LAN_IP "10.10.10.254"
char *getLanIP(void)
{
	static char buf[64];
	char *nl;
	FILE *fp;

	memset(buf, 0, sizeof(buf));
	if( (fp = popen("nvram_get 2860 lan_ipaddr", "r")) == NULL )
		goto error;

	if(!fgets(buf, sizeof(buf), fp)){
		pclose(fp);
		goto error;
	}

	if(!strlen(buf)){
		pclose(fp);
		goto error;
	}
	pclose(fp);

	if(nl = strchr(buf, '\n'))
		*nl = '\0';

	return buf;

error:
	fprintf(stderr, "warning, cant find lan ip\n");
	return DEFAULT_LAN_IP;
}


void javascriptUpdate(int success)
{
	printf("<script language=\"JavaScript\" type=\"text/javascript\">");
	if(success){
		printf(" \
function update(){ \
  console.info('%s'); \
  window.history.replaceState({}, '', '/');\
}", getLanIP(), REFRESH_TIMEOUT);
	}else{
		printf("function update(){ window.history.replaceState({}, '', '/'); }");
	}
	printf("</script>");
}

inline void webFoot(void)
{
	printf("</body></html>\n");
}

char *getvalue(char * key)
{
	static char buf[64];
	char cmd[128];
	char *nl;
	FILE *fp;

	memset(buf, 0, sizeof(buf));
	sprintf(cmd, "nvram_get 2860 %s", key);
	if( (fp = popen(cmd, "r")) == NULL )
		goto error;

	if(!fgets(buf, sizeof(buf), fp)){
		pclose(fp);
		goto error;
	}

	if(!strlen(buf)){
		pclose(fp);
		goto error;
	}
	pclose(fp);

	if(nl = strchr(buf, '\n'))
		*nl = '\0';

	return buf;

error:
	return "..";
}

int check_OEM(char * ih_name, char * oem)
{
	int ret = 0;
	char * p = NULL;
	
	//  IPW611:RX:1.0.1.210519
	p = strrchr(ih_name, ':');
	if (p) {
		p += 1;
		*(p+1) = '\0';
		if (strcmp(p, oem) == 0)
			ret = 1;
	}
	return ret;
}

int check_model(char * ih_name, char * model)
{
	int ret = 0;
	char * p = NULL;
	
	//  IPW611:RX:1.0.1.210519
	p = strchr(ih_name, ':');
	if (p) {
		*p = '\0';
		if (strcmp(ih_name, model) == 0)
			ret = 1;
	}
	return ret;
}
	
#define	MAX_BUFFER_SIZE		1024*1024
void read_remainder(char * buffer, int totalsize, int receive_size)
{
	while(receive_size < totalsize) {
		if (totalsize-receive_size > MAX_BUFFER_SIZE) {
			fread(buffer, 1, MAX_BUFFER_SIZE, stdin);
			receive_size += MAX_BUFFER_SIZE;
		} else {
			int remainder = totalsize-receive_size;
			fread(buffer, 1, remainder, stdin);
			receive_size += remainder;
		}
	}
}

int main (int argc, char *argv[])
{
	char *file_begin, *file_end;
	char *line_begin, *line_end;
	char err_msg[256];
	char *boundary; int boundary_len;
	FILE *fp = fopen(UPLOAD_FILE, "w");
	long inLen;
	char *inStr;
	long receive_size = 0;
	long write_remainder = 0;
	long real_file_size = 0;
	image_header_t * header;
	char buf[32];
	char * p = NULL;

	//  red LED quickly flash
	do_system("gpio l 52 0 4000 0 1 4000");
	do_system("gpio l 14 1 1 1 1 300");

	inLen = strtol(getenv("CONTENT_LENGTH"), NULL, 10) + 1;
	printf("Content-Type:text/plain;charset=utf-8\n\n");

	//  allocate 1M bytes
	inStr = malloc(MAX_BUFFER_SIZE);
	//  read the first block
	fread(inStr, 1, MAX_BUFFER_SIZE, stdin);
	//  count the receive size
	receive_size += MAX_BUFFER_SIZE;

	line_begin = inStr;

	//  extract the boundary and keep it
	//  ex: ------WebKitFormBoundaryXVNs7VyzAAnxkHCm
	if ((line_end = strstr(line_begin, "\r\n")) == 0) {
		printf("%s(%d)", RFC_ERROR, 1);
		free(inStr);
		fclose(fp);
		return -1;
	}
	boundary_len = line_end - line_begin;
	boundary = malloc(boundary_len+1);
	if (NULL == boundary) {
		printf("boundary allocate %d faul!\n", boundary_len);
		goto err;
	}
	memset(boundary, 0, boundary_len+1);
	memcpy(boundary, line_begin, boundary_len);
	//  shift to next section
	//  ex: Content-Disposition: form-data; name="filename"; filename="IPW611T-1.0.0.210427.bin"
	line_begin = line_end + 2;
	if ((line_end = strstr(line_begin, "\r\n")) == 0) {
		printf("%s(%d)", RFC_ERROR, 2);
		goto err;
	}
	if(strncasecmp(line_begin, "content-disposition: form-data;", strlen("content-disposition: form-data;"))) {
		printf("%s(%d)", RFC_ERROR, 3);
		goto err;
	}
	//  shift to name="filename";....
	//  if we can't find the ';' that means we got the wrong format
	line_begin += strlen("content-disposition: form-data;") + 1;
	if (!(line_begin=strchr(line_begin, ';'))){
		printf("We dont support multi-field upload.");
		goto err;
	}
	//  shift to filename="..."
	//  if we can't find the "filename=" thaa means we got the wrong format
	line_begin += 2;
	if (strncasecmp(line_begin, "filename=", strlen("filename="))) {
		printf("%s(%d)", RFC_ERROR, 4);
		goto err;
	}

	// We may check a string  "Content-Type: application/octet-stream" here,
	// but if our firmware extension name is the same with other known ones, 
	// the browser would use other content-type instead.
	// So we dont check Content-type here...
	//  shift to next section
	//  ex: Content-Type: application/octet-stream
	line_begin = line_end + 2;
	//  find 2 times "\r\n"
	//  if not that means we got the wrong file content
	if ((line_end = strstr(line_begin, "\r\n")) == 0) {
		printf("%s(%d)", RFC_ERROR, 5);
		goto err;
	}
	line_begin = line_end + 2;
	if ((line_end = strstr(line_begin, "\r\n")) == 0) {
		printf("%s(%d)", RFC_ERROR, 6);
		goto err;
	}
	//  shift to the start of the firmware file
	line_begin = line_end + 2;

	header = (image_header_t *)line_begin;
	//  get the kernel size from header
	if (inLen <= (ntohl(header->ih_ksz)+64)) {
		printf("Size not correct!");
		goto err;
	}

	strcpy(buf, header->ih_name);
	//  IPW611:RX:1.0.1.210519
	if (check_OEM(buf, getvalue("ManufactureCode")) == 0) {
		printf("Wrong OEM Code!");
		goto err;
	}

	strcpy(buf, header->ih_name);
	if (check_model(buf, getvalue("ModelName")) == 0) {
		printf("Wrong Model!");
		goto err;
	}

	if (strstr(header->ih_name, getvalue("Type")) == NULL) {
		printf("Wrong TX/RX type!");
		goto err;
	}

	//  need to add the size of header
	write_remainder = real_file_size = ntohl(header->ih_ksz)+64;
	//  write the first 1M bytes to the temp file, but needs to reduce the header of HTML
	fwrite(line_begin, 1, MAX_BUFFER_SIZE-(line_begin-inStr), fp);
	

	//  count the file size
	write_remainder -= MAX_BUFFER_SIZE-(line_begin-inStr);
	while(write_remainder) {
		memset(inStr, 0, MAX_BUFFER_SIZE);
		if (write_remainder >= MAX_BUFFER_SIZE) {
			fread(inStr, 1, MAX_BUFFER_SIZE, stdin);
			fwrite(inStr, 1, MAX_BUFFER_SIZE, fp);
			receive_size += MAX_BUFFER_SIZE;
			write_remainder -= MAX_BUFFER_SIZE;
		} else {
			fread(inStr, 1, write_remainder, stdin);
			fwrite(inStr, 1, write_remainder, fp);
			receive_size += write_remainder;
			write_remainder = 0;
			//  recevice the last data and ingore it
			fread(inStr, 1, inLen-receive_size, stdin);
			receive_size += (inLen-receive_size);
		}
	}
	if (receive_size != inLen) {
		if (inLen-receive_size > 0)
			fread(inStr, 1, inLen-receive_size, stdin);
	}

	// examination
#if defined (UPLOAD_FIRMWARE_SUPPORT)
	if(!check(UPLOAD_FILE, 0, real_file_size, err_msg) ){
		unlink(UPLOAD_FILE);
		printf("%s", err_msg);
		goto err;
	}

	/*
	 * write the current linux version into flash.
	 */
	write_flash_kernel_version(UPLOAD_FILE, 0);
#ifdef CONFIG_RT2880_DRAM_8M
	system("killall goahead");
#endif
	// flash write
	if( mtd_write_firmware(UPLOAD_FILE, 0, real_file_size) == -1){
		//printf("mtd_write fatal error! The corrupted image has ruined the flash!!");
		unlink(UPLOAD_FILE);
		printf("FAILED-WRITE");
		goto err;
	}
#elif defined (UPLOAD_BOOTLOADER_SUPPORT)
		mtd_write_bootloader(UPLOAD_FILE, 0, real_file_size);
#else
#error "no upload support defined!"
#endif

	unlink(UPLOAD_FILE);
	printf("DONE");
	free(boundary);
	free(inStr);
	fclose(fp);
	//  green LED
	do_system("gpio l 14 0 4000 0 1 4000");
	do_system("gpio l 52 4000 0 1 0 4000");
	exit(0);

err:
	read_remainder(inStr, inLen, receive_size);
	free(boundary);
	free(inStr);
	fclose(fp);
	//  red LED
	do_system("gpio l 14 4000 0 1 0 4000");
	do_system("gpio l 52 0 4000 0 1 4000");
	exit(-1);
}

