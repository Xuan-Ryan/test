#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <syslog.h>

#include "i2s_ctrl.h"
#include "linux/autoconf.h" //kernel config

#define I2S_PID_FILENAME       "/var/run/openi2s.pid"
//#define MIC_IN_PATH

typedef struct rtp_header
{
	uint16_t cc:4;
	uint16_t extbit:1;
	uint16_t padbit:1;
	uint16_t version:2;
	uint16_t paytype:7;
	uint16_t markbit:1;

	uint16_t seq_number;
	uint32_t timestamp;
	uint32_t ssrc;
	//uint32_t csrc[16];
} rtp_header_t;

int i2s_fd;
void *fdm, *fdm_in;
int tx_pause_stat;
int rx_pause_stat;
int demo_case;
int execute_mode;
int fsize;
int sockfd;
int done;
struct sockaddr_in addr;

void *shtxbuf[MAX_I2S_PAGE];
void *shrxbuf[MAX_I2S_PAGE];
unsigned char* sbuf;
struct stat i2s_stat;
#if defined(CONFIG_I2S_MMAP)
#else
char txbuffer[I2S_PAGE_SIZE+(I2S_PAGE_SIZE>>1)];
char rxbuffer[I2S_PAGE_SIZE];
#endif
static char	*i2s_opt_pid_filename = I2S_PID_FILENAME;

#define PORT 9930

/* Mode definition*/
#define PLAYBACK_MODE		0
#define RECORD_MODE		1
#define PLAYBACK_NETWORK	2
#define UDP_SERVER		3
#define UDP_CLINT		4
#define LOOPBACK_MODE		5
#define NONE_MODE		9
#define INTERNAL_LBK_DMA	10
//#define INTERNAL_LBK_FIFO	11
#define	EXTERNAL_LBK1		13
#define	EXTERNAL_LBK2		14
#define CODEC_BYPASS		15
#define	CODEC_EXLBK		17

void usage()
{
#if defined(MIC_IN_PATH)
#if defined(CONFIG_RALINK_MT7628)
	printf("Usage of i2scmd:\n");
	printf("i2scmd [0] [srate] [tvol] [wlen] [endian] < playback file ------------------------>Playback\n");
	printf("       [1] [srate] [rvol] [boostganin] [micin] [size] [wlen] [endian] ------------>Record\n");
	printf("       [3] [srate] [tvol] [rvol] [boostgain] [micin] [wlen] [endian] ------------->UDP server\n");
	printf("       [4] [srate] [tvol] [rvol] [boostgain] [micin] [server ip] [wlen] [endian]-->UDP client\n");
#else
	printf("Usage of i2scmd:\n");
	printf("i2scmd [0] [srate] [tvol] < playback file -------------------------> Playback\n");
	printf("       [1] [srate] [rvol] [boostganin] [micin] [size] -------------> Record\n");
	printf("       [3] [srate] [tvol] [rvol] [boostgain] [micin] --------------> UDP server\n");
	printf("       [4] [srate] [tvol] [rvol] [boostgain] [micin] [server ip] --> UDP client\n");
#endif
#else
#if defined(CONFIG_RALINK_MT7628)
	printf("Usage:  [cmd] [srate] [vol] [wordlen] [endian fmt]< playback file\n");
	printf("        [cmd] [srate] [vol] [size] [wordlen] [endian fmt]\n");
	printf("        cmd = 0|1 - i2s raw playback|record\n");
	printf("        srate = 8000|16000|32000|44100|48000 Hz playback sampling rate\n");
	printf("        vol = -10~2 db playback volumn\n");
	printf("      	wordlen = 16|24 bit\n");
	printf("	endian fmt = 1|0 - little|big endian\n");
#else
	printf("Usage: [cmd] [srate] [vol] < playback file\n");
	printf("       [cmd] [srate] [vol] [size]\n");
	printf("       cmd = 0|1 - i2s raw playback|record\n");
	printf("       srate = 8000|16000|32000|44100|48000 Hz playback sampling rate\n");
	printf("       vol = -10~2 db playback volumn\n");

#endif
#endif
}

#if defined(CONFIG_I2S_MMAP)
static int i2s_txbuffer_mmap(void)
{
	int i;
	for(i = 0; i < MAX_I2S_PAGE; i++)
	{
		printf("i = %d, tsize=%d, offset=%d\n", i, I2S_PAGE_SIZE, i*I2S_PAGE_SIZE);

		if (I2S_PAGE_SIZE >= I2S_MIN_PAGE_SIZE)
		{
			shtxbuf[i] = mmap(0, I2S_PAGE_SIZE, PROT_WRITE, MAP_SHARED, i2s_fd, i*I2S_PAGE_SIZE);
			if (shtxbuf[i] == MAP_FAILED)
			{
				printf("i2scmd:failed to mmap..\n");
				exit(1);
			}
			memset(shtxbuf[i], 0, I2S_PAGE_SIZE);
		}
		else
		{
			shtxbuf[i] = mmap(0, I2S_MIN_PAGE_SIZE, PROT_WRITE, MAP_SHARED, i2s_fd, i*I2S_MIN_PAGE_SIZE);
			if (shtxbuf[i] == MAP_FAILED)
			{
				printf("i2scmd:failed to mmap..\n");
				exit(1);
			}
			memset(shtxbuf[i], 0, I2S_MIN_PAGE_SIZE);
		}
	}

	return 0;
}

static int i2s_rxbuffer_mmap(void)
{
	int i;

	for(i = 0; i < MAX_I2S_PAGE; i++)
	{
		printf("rsize=%d\n", I2S_PAGE_SIZE);

		if (I2S_PAGE_SIZE >= I2S_MIN_PAGE_SIZE)
		{
			shrxbuf[i] = mmap(0, I2S_PAGE_SIZE, PROT_READ, MAP_SHARED, i2s_fd, i*I2S_PAGE_SIZE);
			if (shrxbuf[i] == MAP_FAILED)
			{
				printf("i2scmd:failed to mmap..\n");
				exit(1);
			}
		}
		else
		{
			shrxbuf[i] = mmap(0, I2S_MIN_PAGE_SIZE, PROT_READ, MAP_SHARED, i2s_fd, i*I2S_MIN_PAGE_SIZE);
			if (shrxbuf[i] == MAP_FAILED)
			{
				printf("i2scmd:failed to mmap..\n");
				exit(1);
			}

		}
	}
	return 0;
}

static int i2s_txbuffer_munmap(void)
{
	int i;

	for(i = 0; i < MAX_I2S_PAGE; i++)
	{
		if (I2S_PAGE_SIZE >= I2S_MIN_PAGE_SIZE)
		{
			if(munmap(shtxbuf[i], I2S_PAGE_SIZE)!=0)
			{
				printf("i2scmd : munmap i2s mmap faild\n");
			}
		}
		else
		{
			if(munmap(shtxbuf[i], I2S_MIN_PAGE_SIZE)!=0)
			{
				printf("i2scmd : munmap i2s mmap faild\n");
			}
		}
	}
	return 0;
}

static int i2s_rxbuffer_munmap(void)
{
	int i;

	for(i = 0; i < MAX_I2S_PAGE; i++)
	{
		if (I2S_PAGE_SIZE >= I2S_MIN_PAGE_SIZE)
		{
			if(munmap(shrxbuf[i], I2S_PAGE_SIZE)!=0)
			{
				printf("i2scmd : munmap i2s mmap faild\n");
			}
		}
		else
		{
			if(munmap(shrxbuf[i], I2S_MIN_PAGE_SIZE)!=0)
			{
				printf("i2scmd : munmap i2s mmap faild\n");
			}
		}
	}
	return 0;
}
#endif

void i2s_rtpplayback_handler(int signum)
{
	int i;
	switch(signum)
	{
	case SIGINT:
		ioctl(i2s_fd, I2S_DISABLE, 0);
#if defined(CONFIG_I2S_MMAP)
		for(i = 0; i < MAX_I2S_PAGE; i++)
		{
			if (I2S_PAGE_SIZE >= I2S_MIN_PAGE_SIZE)
			{
		    		munmap(shtxbuf[i], I2S_PAGE_SIZE);
			}
			else
			{
				munmap(shtxbuf[i], I2S_MIN_PAGE_SIZE);
			}
		}
#endif
		close(i2s_fd);
		unlink(i2s_opt_pid_filename);
		break;
	default:
		break;
	}

	return;
}

void i2s_udp_handler(int signum)
{
	switch(signum)
        {
        case SIGINT:
		printf("SIGINT\n");
		printf("***SIGINT: udp stop***\n");
		done=1;
		ioctl(i2s_fd, I2S_RX_STOP, 0);
                ioctl(i2s_fd, I2S_TX_STOP, 0);
#if defined(CONFIG_I2S_MMAP)
                i2s_txbuffer_munmap();
		i2s_rxbuffer_munmap();
#endif
		close(sockfd);
                close(i2s_fd);
		unlink(i2s_opt_pid_filename);
                printf("I2S UDP STOP!!\n");
		break;
	default:
		break;
	}
	return;
}

void i2s_ctrl_handler(int signum)
{
	int i;
	switch(signum)
	{
	case SIGINT:
		printf("SIGINT\n");
		if (execute_mode == PLAYBACK_MODE) {
			printf("***SIGINT: playback***\n");
			ioctl(i2s_fd, I2S_TX_STOP, 0);
#if defined(CONFIG_I2S_MMAP)
			i2s_txbuffer_munmap();
#endif
			munmap(fdm, i2s_stat.st_size);
			close(i2s_fd);
			unlink(i2s_opt_pid_filename);
			printf("I2S TX STOP!!\n");
		} else if (execute_mode == RECORD_MODE) {
			printf("***SIGINT: record***\n");
			ioctl(i2s_fd, I2S_RX_STOP, 0);
#if defined(CONFIG_I2S_MMAP)
			i2s_rxbuffer_munmap();
#endif
			munmap(fdm, fsize);
			close(i2s_fd);
			unlink(i2s_opt_pid_filename);
			printf("I2S RX Stop!!\n");
		}
		else if (execute_mode == INTERNAL_LBK_DMA)
		{
			printf("***SIGINT: inlbk***\n");
			ioctl(i2s_fd, I2S_TX_STOP, 0);
			ioctl(i2s_fd, I2S_RX_STOP, 0);
#if defined(CONFIG_I2S_MMAP)
			i2s_txbuffer_munmap();
			i2s_rxbuffer_munmap();
#endif
			munmap(fdm_in, fsize);
			munmap(fdm, i2s_stat.st_size);
			close(i2s_fd);
			unlink(i2s_opt_pid_filename);
			printf("Stop internal lbk!\n");
		}
		else if ((execute_mode == EXTERNAL_LBK1)||(execute_mode == EXTERNAL_LBK2))
		{
			printf("***SIGINT: exlbk***\n");
			if (execute_mode == EXTERNAL_LBK2)
			{
				ioctl(i2s_fd, I2S_TX_STOP, 0);
			}
			ioctl(i2s_fd, I2S_RX_STOP, 0);
#if defined(CONFIG_I2S_MMAP)
			i2s_txbuffer_munmap();
			i2s_rxbuffer_munmap();
#endif
			munmap(fdm_in, fsize);
			close(i2s_fd);
			unlink(i2s_opt_pid_filename);
			printf("Stop external lbk!\n");
		}
		else if (execute_mode == LOOPBACK_MODE)
		{
			done = 1;
		}
                break;
	case SIGUSR1:
		if(tx_pause_stat == 0)
		{
                	ioctl(i2s_fd, I2S_TX_PAUSE, 0);
			tx_pause_stat = 1;
                	printf("I2S TX PAUSE....\n");
		}
		else if (tx_pause_stat == 1)
		{
                	ioctl(i2s_fd, I2S_TX_RESUME, 0);
			tx_pause_stat = 0;
                	printf("I2S TX RESUME....\n");
		}
		break;
	case SIGUSR2:
		if(rx_pause_stat == 0)
		{
                	ioctl(i2s_fd, I2S_RX_PAUSE, 0);
			rx_pause_stat = 1;
                	printf("I2S RX PAUSE....\n");
		}
		else if (rx_pause_stat == 1)
		{
                	ioctl(i2s_fd, I2S_RX_RESUME, 0);
			rx_pause_stat = 0;
                	printf("I2S RX RESUME....\n");
		}
		break;
	default:
                break;
        }

	return;
}

void endian_swap(unsigned short * x)
{
	*(x) = (*(x)>>24) |
	    ((*(x)<<8) & 0x00FF0000) |
	    ((*(x)>>8) & 0x0000FF00) |
	    (*(x)<<24);
}

static const unsigned char BitReverseTable256[] =
{
 0x00, 0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0, 0x10, 0x90, 0x50, 0xD0, 0x30, 0xB0, 0x70, 0xF0,
 0x08, 0x88, 0x48, 0xC8, 0x28, 0xA8, 0x68, 0xE8, 0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8, 0x78, 0xF8,
 0x04, 0x84, 0x44, 0xC4, 0x24, 0xA4, 0x64, 0xE4, 0x14, 0x94, 0x54, 0xD4, 0x34, 0xB4, 0x74, 0xF4,
 0x0C, 0x8C, 0x4C, 0xCC, 0x2C, 0xAC, 0x6C, 0xEC, 0x1C, 0x9C, 0x5C, 0xDC, 0x3C, 0xBC, 0x7C, 0xFC,
 0x02, 0x82, 0x42, 0xC2, 0x22, 0xA2, 0x62, 0xE2, 0x12, 0x92, 0x52, 0xD2, 0x32, 0xB2, 0x72, 0xF2,
 0x0A, 0x8A, 0x4A, 0xCA, 0x2A, 0xAA, 0x6A, 0xEA, 0x1A, 0x9A, 0x5A, 0xDA, 0x3A, 0xBA, 0x7A, 0xFA,
 0x06, 0x86, 0x46, 0xC6, 0x26, 0xA6, 0x66, 0xE6, 0x16, 0x96, 0x56, 0xD6, 0x36, 0xB6, 0x76, 0xF6,
 0x0E, 0x8E, 0x4E, 0xCE, 0x2E, 0xAE, 0x6E, 0xEE, 0x1E, 0x9E, 0x5E, 0xDE, 0x3E, 0xBE, 0x7E, 0xFE,
 0x01, 0x81, 0x41, 0xC1, 0x21, 0xA1, 0x61, 0xE1, 0x11, 0x91, 0x51, 0xD1, 0x31, 0xB1, 0x71, 0xF1,
 0x09, 0x89, 0x49, 0xC9, 0x29, 0xA9, 0x69, 0xE9, 0x19, 0x99, 0x59, 0xD9, 0x39, 0xB9, 0x79, 0xF9,
 0x05, 0x85, 0x45, 0xC5, 0x25, 0xA5, 0x65, 0xE5, 0x15, 0x95, 0x55, 0xD5, 0x35, 0xB5, 0x75, 0xF5,
 0x0D, 0x8D, 0x4D, 0xCD, 0x2D, 0xAD, 0x6D, 0xED, 0x1D, 0x9D, 0x5D, 0xDD, 0x3D, 0xBD, 0x7D, 0xFD,
 0x03, 0x83, 0x43, 0xC3, 0x23, 0xA3, 0x63, 0xE3, 0x13, 0x93, 0x53, 0xD3, 0x33, 0xB3, 0x73, 0xF3,
 0x0B, 0x8B, 0x4B, 0xCB, 0x2B, 0xAB, 0x6B, 0xEB, 0x1B, 0x9B, 0x5B, 0xDB, 0x3B, 0xBB, 0x7B, 0xFB,
 0x07, 0x87, 0x47, 0xC7, 0x27, 0xA7, 0x67, 0xE7, 0x17, 0x97, 0x57, 0xD7, 0x37, 0xB7, 0x77, 0xF7,
 0x0F, 0x8F, 0x4F, 0xCF, 0x2F, 0xAF, 0x6F, 0xEF, 0x1F, 0x9F, 0x5F, 0xDF, 0x3F, 0xBF, 0x7F, 0xFF
};

void reverse_order(char * dest, char * src, size_t n)
{
	int i = 0;
	unsigned short * inptr = (unsigned short *)src;
	unsigned short * outptr = (unsigned short *)dest;
	for (i=0; i<n; i+=2) {
		unsigned char * p = (unsigned char *) &(*inptr);
		unsigned char * q = (unsigned char *) &(*outptr);
		q[1] = BitReverseTable256[p[0]];
		q[0] = BitReverseTable256[p[1]];
		//q[1] = BitReverseTable256[p[2]];
		//q[0] = BitReverseTable256[p[3]];
		//if (i < 20)
		//	printf("%X, %X\n", *((unsigned int *)dest), *((unsigned int *)src));
		inptr++;
		outptr++;
	}
	//printf("%X, %X\n", *((unsigned short *)--dest), *((unsigned short *)--src));
}

void swap_endian(char * dest, char * src, size_t n)
{
	int i = 0;
	unsigned short * inptr = (unsigned short *)src;
	unsigned short * outptr = (unsigned short *)dest;
	for (i=0; i<n; i+=2) {
		unsigned char * p = (unsigned char *) &(*inptr);
		unsigned char * q = (unsigned char *) &(*outptr);
		q[1] = p[0];//(~p[0]+1)&0xff;
		q[0] = p[1];//(~p[1]+1)&0xff;
		//q[1] = p[2];
		//q[0] = p[3];
		inptr++;
		outptr++;
	}
	//printf("%X, %X\n", *(--dest), *(--src));
}

void two_complement(char * dest, char * src, size_t n)
{
	int i = 0;
	for (i=0; i<n; i++) {
		*(dest++) = (~(*(src++))+1)&0xff;
	}
	//printf("%X, %X\n", *(--dest), *(--src));
}

//#define	WRITE_TO_FILE		1
int main(int argc, char *argv[])
{
	FILE* fp;
	int fd, fd_in;
	char* pBuf;
	char* pBuf_in;
	int pos;
	int nRet=0,nLen,i=0,j;
	int index = 0;
	unsigned char packet[1600];
	int fd_pid;
	char pidstr[10];

	tx_pause_stat = 0;
	rx_pause_stat = 0;
	execute_mode = NONE_MODE;

	printf("This is Ralink I2S Command Program...\n");
	fflush(stdout);

	i2s_fd = open("/dev/i2s0", O_RDWR|O_SYNC);
	if(i2s_fd<0)
	{
		printf("i2scmd:open i2s driver failed (%d)...exit\n",i2s_fd);
		return -1;
	}
	if(argc < 2)
	{
		usage();
		goto EXIT;
	}

	fd_pid = open(i2s_opt_pid_filename, O_WRONLY | O_CREAT | O_EXCL, 0660);

	if (fd_pid < 0) {
		if (errno == EEXIST) {
			fprintf(stderr, "File %s already exists. Is %s already running?\n",
			i2s_opt_pid_filename, argv[0]);
		} else {
			fprintf(stderr, "File %s: %m", i2s_opt_pid_filename);
		}
		exit(1);
	}

	/* We write the PID file AFTER the double-fork */
	sprintf(&pidstr[0], "%d", getpid());
	if (write(fd_pid, &pidstr[0], strlen(pidstr)) < 0)
		syslog(LOG_WARNING, "Failed to write pid file %s", i2s_opt_pid_filename);
	close(fd_pid);

	switch(strtoul(argv[1], NULL ,10))
	{
	case 0:
#if defined(CONFIG_RALINK_MT7628)
		if(argc < 6)
#else
		if(argc < 4)
#endif
		{
			usage();
	 		goto EXIT;
		}
		if (fstat(STDIN_FILENO, &i2s_stat) == -1 ||i2s_stat.st_size == 0)
			goto EXIT;
		ioctl(i2s_fd, I2S_RESET, 0);
		ioctl(i2s_fd, I2S_SRATE, strtoul(argv[2], NULL ,10));
		ioctl(i2s_fd, I2S_TX_VOL, strtoul(argv[3], NULL ,10));
#if defined(CONFIG_RALINK_MT7628)
		ioctl(i2s_fd, I2S_WORD_LEN, strtoul(argv[4], NULL ,10));
		ioctl(i2s_fd, I2S_ENDIAN_FMT, strtoul(argv[5], NULL ,10));
#endif
		fdm = mmap(0, i2s_stat.st_size, PROT_READ, MAP_SHARED, STDIN_FILENO, 0);
		if (fdm == MAP_FAILED)
			goto EXIT;

		//  Tiger
		ioctl(i2s_fd, I2S_MS_MODE_CTRL, 1);
#if defined(CONFIG_I2S_MMAP)
		i2s_txbuffer_mmap();
#endif
		//pBuf = (char*)calloc(I2S_PAGE_SIZE, sizeof(char));

		execute_mode = PLAYBACK_MODE;
		signal(SIGINT, i2s_ctrl_handler);
		signal(SIGUSR1, i2s_ctrl_handler);

		ioctl(i2s_fd, I2S_TX_ENABLE, 0);
		pos = 0;
		index = 0;

#if defined(CONFIG_I2S_MMAP)
		while ((pos+I2S_PAGE_SIZE)<=i2s_stat.st_size) {
			if (tx_pause_stat == 0) {
				ioctl(i2s_fd, I2S_PUT_AUDIO, &index);
				pBuf = shtxbuf[index];
				memcpy(pBuf, (char*)fdm+pos, I2S_PAGE_SIZE);
				//reverse_order(pBuf, (char *)fdm+pos, I2S_PAGE_SIZE);
				//swap_endian(pBuf, (char *)fdm+pos, I2S_PAGE_SIZE);
				//two_complement(pBuf, (char*)fdm+pos, I2S_PAGE_SIZE);
				pos+=I2S_PAGE_SIZE;
			}
		}
		if (tx_pause_stat == 0) {
			ioctl(i2s_fd, I2S_PUT_AUDIO, &index);
			pBuf = shtxbuf[index];
			memset(pBuf+(i2s_stat.st_size-pos), 0, I2S_PAGE_SIZE-(i2s_stat.st_size-pos));
			memcpy(pBuf, (char*)fdm+pos, (i2s_stat.st_size-pos));
			//reverse_order(pBuf, (char *)fdm+pos, (i2s_stat.st_size-pos));
			//swap_endian(pBuf, (char *)fdm+pos, (i2s_stat.st_size-pos));
			//two_complement(pBuf, (char*)fdm+pos, (i2s_stat.st_size-pos));
			pos+=(i2s_stat.st_size-pos);

			i = 0;
			while (1) {
				ioctl(i2s_fd, I2S_PUT_AUDIO, &i);
				pBuf = shtxbuf[i];
				memset(pBuf, 0, I2S_PAGE_SIZE);
				if (i==index)
					break;
			}
		}
#else
		while ((pos+I2S_PAGE_SIZE)<=i2s_stat.st_size) {
			pBuf = txbuffer;
			memcpy(pBuf, (char*)fdm+pos, I2S_PAGE_SIZE);
			ioctl(i2s_fd, I2S_PUT_AUDIO, pBuf);
			pos+=I2S_PAGE_SIZE;
		}
#endif
		ioctl(i2s_fd, I2S_DISABLE, 0);
		//free(pBuf);
		munmap(fdm, i2s_stat.st_size);
#if defined(CONFIG_I2S_MMAP)
		i2s_txbuffer_munmap();
#endif
		break;
	case 1:
#if defined(MIC_IN_PATH)
 #if defined(CONFIG_RALINK_MT7628) /*(CONFIG_I2S_WORD_LEN)*/
		if(argc < 9)
 #else
		if(argc < 7)
 #endif
#else
 #if defined(CONFIG_RALINK_MT7628) /*(CONFIG_I2S_WORD_LEN)*/
		if(argc < 7)
 #else
		if(argc < 5)
 #endif
#endif
		{
			usage();
			goto EXIT;
		}
		fd = open("record.snd",(O_CREAT| O_RDWR), (S_IRUSR | S_IRUSR));
		if(fd < 2)
		{
			printf("open fd error \n");
			goto EXIT;
		}
#if defined(MIC_IN_PATH)
		fsize = strtoul(argv[6], NULL ,10);
#else
		fsize = strtoul(argv[4], NULL ,10);
#endif
		if (lseek(fd,fsize -1 ,SEEK_SET) == -1)
		{
			printf("lseek failed\n");
			close(fd);
			goto EXIT;
		}
		write(fd,"\0",1);
		lseek(fd,0 ,SEEK_SET);
		printf("record size=%d\n",fsize);

		fdm = mmap(0, fsize, PROT_WRITE|PROT_READ, MAP_FILE|MAP_SHARED, fd, 0);
		close(fd);
		if (fdm == MAP_FAILED) {
			printf("mmap fdm failed\n");
			goto EXIT;
		}
		printf("fdm=0x%08X\n",fdm);

#if defined(MIC_IN_PATH)
		ioctl(i2s_fd, I2S_RESET, 0);
		ioctl(i2s_fd, I2S_SRATE, strtoul(argv[2], NULL ,10));
		ioctl(i2s_fd, I2S_RX_VOL, strtoul(argv[3], NULL ,10));
		ioctl(i2s_fd, I2S_CODEC_MIC_BOOST, strtoul(argv[4], NULL ,10));
		ioctl(i2s_fd, I2S_CODEC_MIC_IN, strtoul(argv[5], NULL ,10));
 #if defined(CONFIG_RALINK_MT7628) /*(CONFIG_I2S_WORD_LEN)*/
		ioctl(i2s_fd, I2S_WORD_LEN, strtoul(argv[7], NULL ,10));
		ioctl(i2s_fd, I2S_ENDIAN_FMT, strtoul(argv[8], NULL ,10));
 #endif
#else
		ioctl(i2s_fd, I2S_RESET, 0);
		ioctl(i2s_fd, I2S_SRATE, strtoul(argv[2], NULL ,10));
		ioctl(i2s_fd, I2S_RX_VOL, strtoul(argv[3], NULL ,10));
#if defined(CONFIG_RALINK_MT7628) /*(CONFIG_I2S_WORD_LEN)*/
		ioctl(i2s_fd, I2S_WORD_LEN, strtoul(argv[5], NULL ,10));
		ioctl(i2s_fd, I2S_ENDIAN_FMT, strtoul(argv[6], NULL ,10));
#endif
#endif
		//  Tiger set to slave
		ioctl(i2s_fd, I2S_MS_MODE_CTRL, 1);
#if defined(CONFIG_I2S_MMAP)
		i2s_txbuffer_mmap();
		i2s_rxbuffer_mmap();
#endif
		execute_mode = RECORD_MODE;
		printf("record mode\n");
		signal(SIGINT, i2s_ctrl_handler);
		signal(SIGUSR2, i2s_ctrl_handler);

		ioctl(i2s_fd, I2S_RX_ENABLE, 0);
		pos = 0;
		index = 0;

#if defined(CONFIG_I2S_MMAP)
		while ((pos+I2S_PAGE_SIZE)<=fsize) {
			int nRet = 0;
			if (rx_pause_stat == 0) {
				ioctl(i2s_fd, I2S_GET_AUDIO, &index);
				pBuf = shrxbuf[index];
				memcpy((char*)fdm+pos, pBuf, I2S_PAGE_SIZE);
				pos+=I2S_PAGE_SIZE;
			}
		}
#else
		/*while ((pos+I2S_PAGE_SIZE)<=fsize) {
			int nRet = 0;
			pBuf = rxbuffer;
			ioctl(i2s_fd, I2S_GET_AUDIO, pBuf);
			memcpy((char*)fdm+pos, pBuf, I2S_PAGE_SIZE);
			pos+=I2S_PAGE_SIZE;
		}*/
#endif
printf("%d\n", __LINE__);
		ioctl(i2s_fd, I2S_RX_DISABLE, 0);
#if defined(CONFIG_I2S_MMAP)
		i2s_txbuffer_munmap();
		i2s_rxbuffer_munmap();
#endif
		munmap(fdm, fsize);
		break;
	case 2:
#if defined(CONFIG_I2S_MMAP)
		i2s_txbuffer_mmap();
#endif
		execute_mode = PLAYBACK_NETWORK;
		ioctl(i2s_fd, I2S_RESET, 0);
		ioctl(i2s_fd, I2S_SRATE, strtoul(argv[2], NULL ,10));
		ioctl(i2s_fd, I2S_TX_VOL, strtoul(argv[3], NULL ,10));
#if defined(CONFIG_RALINK_MT7628) /*(CONFIG_I2S_WORD_LEN)*/
		ioctl(i2s_fd, I2S_WORD_LEN, strtoul(argv[4], NULL ,10));
		ioctl(i2s_fd, I2S_ENDIAN_FMT, strtoul(argv[5], NULL ,10));
#endif

		{
			struct sockaddr_in addr;
			int s = socket(AF_INET, SOCK_DGRAM, 0);

			signal(SIGINT, i2s_rtpplayback_handler);

			if(s > 0)
			{
				int index = 0;
				unsigned long g_seq = 0 ,last_lseq = 0;
				printf("socket %d opened\n",s);
				/* init addr */
				bzero(&addr, sizeof(addr));
				addr.sin_family = AF_INET;
				addr.sin_port = htons(5004);
				if(inet_pton(AF_INET, "10.10.10.254", &addr.sin_addr) <= 0)
				{
					printf("10.10.10.254 is not a valid IPaddress\n");
				}
				else
					printf("IP address is ok\n");

				bind(s, &addr, sizeof(addr));
				pos = 0;
#if defined(CONFIG_I2S_MMAP)
				ioctl(i2s_fd, I2S_PUT_AUDIO, &index);
				pBuf = shtxbuf[index];
#else
				pBuf = txbuffer;
#endif
				while (1) {
					int n;
					int payloadlen = 0;
					rtp_header_t* prtphdr;
					unsigned int rtphdr_size;

					n = recvfrom(s, packet, 1600, 0, NULL, NULL);
					rtphdr_size = 12;
					if ( n > 0) {
						prtphdr = (rtp_header_t*)packet;
						if (prtphdr->cc!=0) {
							printf("cc=%d\n",prtphdr->cc);
							rtphdr_size += prtphdr->cc;
						}
						if (g_seq==0) {
							ioctl(i2s_fd, I2S_TX_ENABLE, 0);
							g_seq = ntohs(prtphdr->seq_number);
							last_lseq = g_seq;
							printf("RTPV=%d PT=0x%02X [s]=%d\n",prtphdr->version,prtphdr->paytype, g_seq);
						} else {
							if (g_seq!=ntohs(prtphdr->seq_number)) {
								printf("RTPV=%d PT=%d, d=%d [ls]=%d\n", prtphdr->version,prtphdr->paytype,ntohs(prtphdr->seq_number)-g_seq,ntohs(prtphdr->seq_number)-last_lseq);
								last_lseq = ntohs(prtphdr->seq_number);
								g_seq = ntohs(prtphdr->seq_number);
							}
						}
						g_seq++;
						payloadlen = n - rtphdr_size;
#if defined(CONFIG_I2S_MMAP)
						if (pos+payloadlen>=I2S_PAGE_SIZE) {
							unsigned short *pData = pBuf;
							int ncopy = I2S_PAGE_SIZE - pos;
							memcpy(pBuf+pos, packet+rtphdr_size, ncopy);

							for ( i = 0 ; i < (I2S_PAGE_SIZE>>1) ; i ++ ) {
								*pData = ntohs(*pData);
								pData++;
							}
							ioctl(i2s_fd, I2S_PUT_AUDIO, &index);
							pBuf = shtxbuf[index];
							if(ncopy)
								memcpy(pBuf, packet+rtphdr_size+ncopy, payloadlen-ncopy);
							pos = payloadlen-ncopy;
						} else {
							memcpy(pBuf+pos, packet+rtphdr_size, payloadlen);
							pos += payloadlen;
						}
#else
						memcpy(txbuffer+pos, packet+rtphdr_size, payloadlen);
						pos+=payloadlen;

						if(pos >= I2S_PAGE_SIZE) {
							unsigned short *pData = txbuffer;

							for ( i = 0 ; i < (I2S_PAGE_SIZE>>1) ; i ++ ) {
								*pData = ntohs(*pData);
								pData++;
							}
							ioctl(i2s_fd, I2S_PUT_AUDIO, txbuffer);
							if(pos > I2S_PAGE_SIZE)
								memcpy(txbuffer, txbuffer+I2S_PAGE_SIZE, pos-I2S_PAGE_SIZE);
							pos = pos-I2S_PAGE_SIZE;
						}
#endif
					}
				}
				close(s);
			}
		}

		ioctl(i2s_fd, I2S_DISABLE, 0);
#if defined(CONFIG_I2S_MMAP)
		i2s_txbuffer_munmap();
#endif
		break;
	case 3:
#if defined(MIC_IN_PATH)
#if defined(CONFIG_RALINK_MT7628)
		if(argc < 9)
#else
		if(argc < 7)
#endif
#else
#if defined(CONFIG_RALINK_MT7628)
		if(argc < 6)
#else
		if(argc < 4)
#endif
#endif
		{
			usage();
			goto EXIT;
		}

#if defined(CONFIG_I2S_MMAP)
		i2s_txbuffer_mmap();
		i2s_rxbuffer_mmap();
#endif
		signal(SIGINT, i2s_udp_handler);

		execute_mode = UDP_SERVER;
#if defined(MIC_IN_PATH)
		ioctl(i2s_fd, I2S_RESET, 0);
		ioctl(i2s_fd, I2S_SRATE, strtoul(argv[2], NULL ,10));
		ioctl(i2s_fd, I2S_TX_VOL, strtoul(argv[3], NULL ,10));
		ioctl(i2s_fd, I2S_RX_VOL, strtoul(argv[4], NULL ,10));
		ioctl(i2s_fd, I2S_CODEC_MIC_BOOST, strtoul(argv[5], NULL ,10));
		ioctl(i2s_fd, I2S_CODEC_MIC_IN, strtoul(argv[6], NULL ,10));
#if defined(CONFIG_RALINK_MT7628) /*(CONFIG_I2S_WORD_LEN)*/
		ioctl(i2s_fd, I2S_WORD_LEN, strtoul(argv[7], NULL ,10));
		ioctl(i2s_fd, I2S_ENDIAN_FMT, strtoul(argv[8], NULL ,10));
#endif
#else
		ioctl(i2s_fd, I2S_RESET, 0);
		ioctl(i2s_fd, I2S_SRATE, strtoul(argv[2], NULL ,10));
		ioctl(i2s_fd, I2S_TX_VOL, strtoul(argv[3], NULL ,10));
		ioctl(i2s_fd, I2S_RX_VOL, strtoul(argv[3], NULL ,10));
#if defined(CONFIG_RALINK_MT7628) /*(CONFIG_I2S_WORD_LEN)*/
		ioctl(i2s_fd, I2S_WORD_LEN, strtoul(argv[4], NULL ,10));
		ioctl(i2s_fd, I2S_ENDIAN_FMT, strtoul(argv[5], NULL ,10));
#endif
#endif
		{
			struct sockaddr_in addr;
			int addr_len = sizeof(struct sockaddr_in);
			char buffer[256];
			int len;
			unsigned char transpkt[I2S_PAGE_SIZE];
			unsigned char recvpkt[I2S_PAGE_SIZE];
			int n, m;
			char* pGetBuf;
			char* pPutBuf;
			int i=0;
			int j=0;
			int putindex = 0;
			int getindex = 0;
			done = 0;
			char* pData;

			pData = (char*)calloc(I2S_PAGE_SIZE, sizeof(char));

			sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
			if (sockfd < 0) {
				printf("err:sockt\n");
				goto EXIT;
			}
			bzero(&addr,sizeof(addr));
			addr.sin_family = AF_INET;
			addr.sin_port = htons(PORT);
			addr.sin_addr.s_addr = htonl(INADDR_ANY);
			if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr))<0) {
				printf("err:bind\n");
				goto EXIT;
			}

			ioctl(i2s_fd, I2S_TXRX_COEXIST, 1);
			ioctl(i2s_fd, I2S_RX_ENABLE, 0);
			ioctl(i2s_fd, I2S_TX_ENABLE, 0);

			//if ( fcntl(sockfd, F_SETFL, O_NONBLOCK) != 0 )
			//	perror("Request nonblocking I/O");

			while (1) {
				if (done == 1) {
					break;
				}

				n = recvfrom(sockfd, recvpkt , I2S_PAGE_SIZE, 0, (struct sockaddr *)&addr, &addr_len);
				if (n > 0) {
					ioctl(i2s_fd, I2S_PUT_AUDIO, &putindex);
					pPutBuf = shtxbuf[putindex];
					memcpy(pPutBuf, recvpkt, I2S_PAGE_SIZE);
				} else {
					printf("recvfrom<=0, send 0\n");
					ioctl(i2s_fd, I2S_PUT_AUDIO, &putindex);
					pPutBuf = shtxbuf[putindex];
					memcpy(pPutBuf, pData, I2S_PAGE_SIZE);
				}

				i++;

				ioctl(i2s_fd, I2S_GET_AUDIO, &getindex);
				pGetBuf = shrxbuf[getindex];
				memcpy(transpkt, pGetBuf, I2S_PAGE_SIZE);
				if (transpkt) {
					m = sendto(sockfd, transpkt, I2S_PAGE_SIZE, 0, (struct sockaddr *)&addr, addr_len);
					if (m < 0) {
						printf("drop:sendto\n");
						goto EXIT;
					}
				}
			}
			close(sockfd);
		}
		ioctl(i2s_fd, I2S_TX_DISABLE, 0);
		ioctl(i2s_fd, I2S_RX_DISABLE, 0);
		ioctl(i2s_fd, I2S_TXRX_COEXIST, 0);
#if defined(CONFIG_I2S_MMAP)
		i2s_txbuffer_munmap();
		i2s_rxbuffer_munmap();
#endif
		break;
	case 4:
#if defined(MIC_IN_PATH)
 #if defined(CONFIG_RALINK_MT7628) /*(CONFIG_I2S_WORD_LEN)*/
		if(argc < 10)
 #else
		if(argc < 8)
 #endif
                {
                        usage();
                        goto EXIT;
                }

                ioctl(i2s_fd, I2S_RESET, 0);
                ioctl(i2s_fd, I2S_SRATE, strtoul(argv[2], NULL ,10));
                ioctl(i2s_fd, I2S_TX_VOL, strtoul(argv[3], NULL ,10));
                ioctl(i2s_fd, I2S_RX_VOL, strtoul(argv[4], NULL ,10));
                ioctl(i2s_fd, I2S_CODEC_MIC_BOOST, strtoul(argv[5], NULL ,10));
                ioctl(i2s_fd, I2S_CODEC_MIC_IN, strtoul(argv[6], NULL ,10));
 #if defined(CONFIG_RALINK_MT7628) /*(CONFIG_I2S_WORD_LEN)*/
                ioctl(i2s_fd, I2S_WORD_LEN, strtoul(argv[8], NULL ,10));
                ioctl(i2s_fd, I2S_ENDIAN_FMT, strtoul(argv[9], NULL ,10));
 #endif
#else
 #if defined(CONFIG_RALINK_MT7628) /*(CONFIG_I2S_WORD_LEN)*/
                if(argc < 7)
 #else
                if(argc < 5)
 #endif
		{
                        usage();
                        goto EXIT;
                }

                ioctl(i2s_fd, I2S_RESET, 0);
                ioctl(i2s_fd, I2S_SRATE, strtoul(argv[2], NULL ,10));
		ioctl(i2s_fd, I2S_TX_VOL, strtoul(argv[3], NULL ,10));
                ioctl(i2s_fd, I2S_RX_VOL, strtoul(argv[3], NULL ,10));
 #if defined(CONFIG_RALINK_MT7628) /*(CONFIG_I2S_WORD_LEN)*/
                ioctl(i2s_fd, I2S_WORD_LEN, strtoul(argv[5], NULL ,10));
                ioctl(i2s_fd, I2S_ENDIAN_FMT, strtoul(argv[6], NULL ,10));
 #endif
#endif

#if defined(CONFIG_I2S_MMAP)
		i2s_txbuffer_mmap();
		i2s_rxbuffer_mmap();
#endif
                execute_mode = UDP_CLINT;
                printf("udp clint\n");
		signal(SIGINT, i2s_udp_handler);

		{
			struct sockaddr_in addr;
			int addr_len=sizeof(struct sockaddr_in);
			int len;
			char buffer[256];
			int putindex = 0;
			int getindex = 0;
			int n, m;
			int i = 0;
			unsigned char transpkt[I2S_PAGE_SIZE];
			unsigned char recvpkt[I2S_PAGE_SIZE];
			char* pGetBuf;
			char* pPutBuf;
			done = 0;
			char* pData;

			pData = (char*)calloc(I2S_PAGE_SIZE, sizeof(char));

			sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
			if (sockfd < 0)
			{
				printf("err:socket");
				goto EXIT;
			}
			bzero(&addr,sizeof(addr));
			addr.sin_family = AF_INET;
			addr.sin_port = htons(PORT);
#if defined(MIC_IN_PATH)
			addr.sin_addr.s_addr = inet_addr(argv[7]);
#else
			addr.sin_addr.s_addr = inet_addr(argv[4]);
#endif
			ioctl(i2s_fd, I2S_TXRX_COEXIST, 1);
			ioctl(i2s_fd, I2S_RX_ENABLE, 0);
			ioctl(i2s_fd, I2S_TX_ENABLE, 0);

			if ( fcntl(sockfd, F_SETFL, O_NONBLOCK) != 0 )
				perror("Request nonblocking I/O");

			while (1) {
				if (done == 1) {
					break;
				}

				ioctl(i2s_fd, I2S_GET_AUDIO, &getindex);
				pGetBuf = shrxbuf[getindex];
				memcpy(transpkt, pGetBuf, I2S_PAGE_SIZE);
				if (transpkt) {
					m = sendto(sockfd, transpkt, I2S_PAGE_SIZE, 0, (struct sockaddr *)&addr, addr_len);
					if (m < 0) {
						printf("drop:sendto\n");
						goto EXIT;
					}
				}

				n = recvfrom(sockfd, recvpkt , I2S_PAGE_SIZE, 0, (struct sockaddr *)&addr, &addr_len);
				if (n > 0) {
					ioctl(i2s_fd, I2S_PUT_AUDIO, &putindex);
					pPutBuf = shtxbuf[putindex];
					memcpy(pPutBuf, recvpkt, I2S_PAGE_SIZE);
				} else  {
					printf("recvfrom<=0, send 0\n");
					ioctl(i2s_fd, I2S_PUT_AUDIO, &putindex);
					pPutBuf = shtxbuf[putindex];
					memcpy(pPutBuf, pData, I2S_PAGE_SIZE);
				}
				i++;
			}
			close(sockfd);
		}
		ioctl(i2s_fd, I2S_TX_DISABLE, 0);
		ioctl(i2s_fd, I2S_RX_DISABLE, 0);
#if defined(CONFIG_I2S_MMAP)
		i2s_txbuffer_munmap();
		i2s_rxbuffer_munmap();
#endif
		break;
	case 5:
#ifdef WRITE_TO_FILE
		if(argc < 5) {
#else
		if(argc < 4) {
#endif
			usage();
			goto EXIT;
		}

#ifdef WRITE_TO_FILE
		fd = open("record.snd",(O_CREAT| O_RDWR), (S_IRUSR | S_IRUSR));
		if(fd < 2) {
			printf("open fd error \n");
			goto EXIT;
		}
		fsize = strtoul(argv[4], NULL ,10);
		if (lseek(fd,fsize -1 ,SEEK_SET) == -1) {
			printf("lseek failed\n");
			close(fd);
			goto EXIT;
		}
		write(fd,"\0",1);
		lseek(fd,0 ,SEEK_SET);
		printf("record size=%d\n",fsize);
		fdm = mmap(0, fsize, PROT_WRITE|PROT_READ, MAP_FILE|MAP_SHARED, fd, 0);
		close(fd);
		if (fdm == MAP_FAILED) {
			printf("mmap fdm failed\n");
			goto EXIT;
		}
		printf("fdm=0x%08X\n",fdm);
#endif
		ioctl(i2s_fd, I2S_RESET, 0);
		ioctl(i2s_fd, I2S_SRATE, strtoul(argv[2], NULL ,10));
		ioctl(i2s_fd, I2S_TX_VOL, strtoul(argv[3], NULL ,10));
		ioctl(i2s_fd, I2S_RX_VOL, strtoul(argv[3], NULL ,10));
		//  Tiger set to slave
		ioctl(i2s_fd, I2S_MS_MODE_CTRL, 1);
#if defined(CONFIG_I2S_MMAP)
		i2s_txbuffer_mmap();
		i2s_rxbuffer_mmap();
#endif
		execute_mode = LOOPBACK_MODE;
		signal(SIGINT, i2s_ctrl_handler);

		ioctl(i2s_fd, I2S_TXRX_COEXIST, 1);
		ioctl(i2s_fd, I2S_RX_ENABLE, 0);
		ioctl(i2s_fd, I2S_TX_ENABLE, 0);

		printf("loopback mode\n");

		done = 0;
		pos = 0;
		index = 0;
#ifdef WRITE_TO_FILE
		while((pos+I2S_PAGE_SIZE)<=fsize) {
#else
		while (done == 0) {
#endif
 #if defined(CONFIG_I2S_MMAP)
			ioctl(i2s_fd, I2S_GET_AUDIO, &index);
			pBuf_in = shrxbuf[index];
 #else
			pBuf_in = rxbuffer;
			ioctl(i2s_fd, I2S_GET_AUDIO, pBuf_in);
 #endif

#ifdef WRITE_TO_FILE
			memcpy((char*)fdm+pos, pBuf_in, I2S_PAGE_SIZE);
			pos+=I2S_PAGE_SIZE;
#endif
 #if defined(CONFIG_I2S_MMAP)
			ioctl(i2s_fd, I2S_PUT_AUDIO, &index);
			pBuf = shtxbuf[index];
			memcpy(pBuf, pBuf_in, I2S_PAGE_SIZE);
 #else
			ioctl(i2s_fd, I2S_PUT_AUDIO, pBuf_in);
 #endif
			
		}
		ioctl(i2s_fd, I2S_TXRX_COEXIST, 0);
		ioctl(i2s_fd, I2S_RX_DISABLE, 0);
		ioctl(i2s_fd, I2S_TX_DISABLE, 0);
#if defined(CONFIG_I2S_MMAP)
		i2s_txbuffer_munmap();
		i2s_rxbuffer_munmap();
#endif
		munmap(fdm, fsize);
		break;
	case 10:
#if defined(CONFIG_RALINK_MT7628)
		if(argc < 6)
#else
		if(argc < 4)
#endif
		{
			usage();
	 		goto EXIT;
		}
		if (fstat(STDIN_FILENO, &i2s_stat) == -1 ||i2s_stat.st_size == 0)
			goto EXIT;
		ioctl(i2s_fd, I2S_RESET, 0);
		ioctl(i2s_fd, I2S_SRATE, strtoul(argv[2], NULL ,10));
		ioctl(i2s_fd, I2S_TX_VOL, strtoul(argv[3], NULL ,10));
		ioctl(i2s_fd, I2S_RX_VOL, strtoul(argv[3], NULL ,10));
#if defined(CONFIG_RALINK_MT7628)
		ioctl(i2s_fd, I2S_WORD_LEN, strtoul(argv[4], NULL ,10));
		ioctl(i2s_fd, I2S_ENDIAN_FMT, strtoul(argv[5], NULL ,10));
#endif
		printf("i2s_stat.st_size=%d\n", i2s_stat.st_size);
		fdm = mmap(0, i2s_stat.st_size, PROT_READ, MAP_SHARED, STDIN_FILENO, 0);
		if (fdm == MAP_FAILED)
			goto EXIT;
		printf("fdm=0x%08X\n",fdm);

#if defined(CONFIG_I2S_MMAP)
		i2s_txbuffer_mmap();
#endif
		fd_in = open("record.snd",(O_CREAT| O_RDWR), (S_IRUSR | S_IRUSR));
		if (fd_in < 2) {
			printf("open fd error \n");
			goto EXIT;
		}

		//fsize = strtoul(argv[4], NULL ,10);
		fsize = i2s_stat.st_size;
		if (lseek(fd_in,fsize -1 ,SEEK_SET) == -1) {
			printf("lseek failed\n");
			close(fd_in);
			goto EXIT;
		}
		write(fd_in,"\0",1);
		lseek(fd_in,0 ,SEEK_SET);
		printf("record size=%d\n",fsize);

		fdm_in = mmap(0, fsize, PROT_WRITE|PROT_READ, MAP_FILE|MAP_SHARED, fd_in, 0);
		close(fd_in);
		if (fdm_in == MAP_FAILED) {
			printf("mmap fdm_in failed\n");
			goto EXIT;
		}
		printf("fdm_in=0x%08X\n",fdm_in);

		//  Tiger set to slave
		ioctl(i2s_fd, I2S_MS_MODE_CTRL, 1);
#if defined(CONFIG_I2S_MMAP)
		i2s_rxbuffer_mmap();
#endif
		ioctl(i2s_fd,I2S_INTERNAL_LBK, 1);
		ioctl(i2s_fd, I2S_TXRX_COEXIST, 1);
		execute_mode = INTERNAL_LBK_DMA;
		printf("inlbk mode\n");
		signal(SIGINT, i2s_ctrl_handler);

		ioctl(i2s_fd, I2S_RX_ENABLE, 0);
		ioctl(i2s_fd, I2S_TX_ENABLE, 0);
		pos = 0;
		index = 0;
#if 1
		while ((pos+I2S_PAGE_SIZE)<=fsize) {
			//  write to SDO
			ioctl(i2s_fd, I2S_PUT_AUDIO, &index);
			pBuf = shtxbuf[index];
			memcpy(pBuf, (char*)fdm+pos, I2S_PAGE_SIZE);
			//two_complement(pBuf, (char*)fdm+pos, I2S_PAGE_SIZE);
			//reverse_order(pBuf, (char*)fdm+pos, I2S_PAGE_SIZE);
			//swap_endian(pBuf, (char*)fdm+pos, I2S_PAGE_SIZE);
			//  read from SDI
			ioctl(i2s_fd, I2S_GET_AUDIO, &index);
			pBuf = shrxbuf[index];
			memcpy((char*)fdm_in+pos, pBuf, I2S_PAGE_SIZE);
			pos+=I2S_PAGE_SIZE;
		}
#endif
		ioctl(i2s_fd, I2S_RX_DISABLE, 0);
		ioctl(i2s_fd, I2S_TX_DISABLE, 0);
		ioctl(i2s_fd, I2S_TXRX_COEXIST, 0);
#if defined(CONFIG_I2S_MMAP)
		i2s_txbuffer_munmap();
		i2s_rxbuffer_munmap();
#endif
		munmap(fdm_in, fsize);
		munmap(fdm, i2s_stat.st_size);

		break;
	case 11:
		{
			unsigned long param[4];
			param[0] = 192;
			param[1] = 0x5A5A5A5A;
			ioctl(i2s_fd, I2S_DEBUG_INLBK, param);
		}
		break;
	case 12:
		ioctl(i2s_fd, I2S_DEBUG_EXLBK, strtoul(argv[2], NULL ,10));
		break;
	case 13: /* External loopback */
#if defined(CONFIG_RALINK_MT7628) /*(CONFIG_I2S_WORD_LEN)*/
		if(argc < 7)
#else
		if(argc < 5)
#endif
		{
			usage();
			goto EXIT;
		}

		ioctl(i2s_fd, I2S_RESET, 0);
		ioctl(i2s_fd, I2S_SRATE, strtoul(argv[2], NULL ,10));
		ioctl(i2s_fd, I2S_RX_VOL, strtoul(argv[3], NULL ,10));
		ioctl(i2s_fd, I2S_TX_VOL, strtoul(argv[3], NULL ,10));
#if defined(CONFIG_RALINK_MT7628) /*(CONFIG_I2S_WORD_LEN)*/
		ioctl(i2s_fd, I2S_WORD_LEN, strtoul(argv[5], NULL ,10));
		ioctl(i2s_fd, I2S_ENDIAN_FMT, strtoul(argv[6], NULL ,10));
#endif
		fd_in = open("record.snd",(O_CREAT| O_RDWR), (S_IRUSR | S_IRUSR));
		if(fd_in < 2)
		{
			printf("open fd_in error \n");
			goto EXIT;
		}

		fsize = strtoul(argv[4], NULL ,10);
		if (lseek(fd_in,fsize -1 ,SEEK_SET) == -1)
		{
			printf("lseek failed\n");
			close(fd_in);
			goto EXIT;
		}
		write(fd_in,"\0",1);
		lseek(fd_in,0 ,SEEK_SET);
		printf("record size=%d\n",fsize);

		fdm_in = mmap(0, fsize, PROT_WRITE|PROT_READ, MAP_FILE|MAP_SHARED, fd_in, 0);
		close(fd_in);
		if (fdm_in == MAP_FAILED) {
			printf("mmap fdm_in failed\n");
			goto EXIT;
		}
		printf("fdm_in=0x%08X\n",fdm_in);

		//  Tiger set to slave
		ioctl(i2s_fd, I2S_MS_MODE_CTRL, 1);
#if defined(CONFIG_I2S_MMAP)
		i2s_txbuffer_mmap();
		i2s_rxbuffer_mmap();
#endif

		ioctl(i2s_fd,I2S_EXTERNAL_LBK, 1);
		ioctl(i2s_fd, I2S_TXRX_COEXIST, 1);
		execute_mode = EXTERNAL_LBK1 ;
		printf("exlbk mode\n");
		signal(SIGINT, i2s_ctrl_handler);

		ioctl(i2s_fd, I2S_RX_ENABLE, 0);
		pos = 0;
		index = 0;

#if 1
		while((pos+I2S_PAGE_SIZE)<=fsize)
		{
			char* pData;

			ioctl(i2s_fd, I2S_GET_AUDIO, &index);
			pBuf = shrxbuf[index];
			memcpy((char*)fdm_in+pos, pBuf, I2S_PAGE_SIZE);
			pos+=I2S_PAGE_SIZE;
		}
#endif

		ioctl(i2s_fd,I2S_EXTERNAL_LBK, 0);
		ioctl(i2s_fd, I2S_RX_DISABLE, 0);
		ioctl(i2s_fd, I2S_TXRX_COEXIST, 0);

#if defined(CONFIG_I2S_MMAP)
		i2s_txbuffer_munmap();
		i2s_rxbuffer_munmap();
#endif
		munmap(fdm_in, fsize);

		break;
	case 14:
#if defined(CONFIG_RALINK_MT7628) /*(CONFIG_I2S_WORD_LEN)*/
		if(argc < 7)
#else
		if(argc < 5)
#endif
		{
			usage();
			goto EXIT;
		}

		ioctl(i2s_fd, I2S_RESET, 0);
		ioctl(i2s_fd, I2S_SRATE, strtoul(argv[2], NULL ,10));
		ioctl(i2s_fd, I2S_RX_VOL, strtoul(argv[3], NULL ,10));
		ioctl(i2s_fd, I2S_TX_VOL, strtoul(argv[3], NULL ,10));
#if defined(CONFIG_RALINK_MT7628) /*(CONFIG_I2S_WORD_LEN)*/
		ioctl(i2s_fd, I2S_WORD_LEN, strtoul(argv[5], NULL ,10));
		ioctl(i2s_fd, I2S_ENDIAN_FMT, strtoul(argv[6], NULL ,10));
#endif
		//  Tiger set to slave
		ioctl(i2s_fd, I2S_MS_MODE_CTRL, 1);
#if defined(CONFIG_I2S_MMAP)
		i2s_txbuffer_mmap();
#endif
		fd_in = open("record.snd",(O_CREAT| O_RDWR), (S_IRUSR | S_IRUSR));
		if(fd_in < 2)
		{
			printf("open fd_in error \n");
			goto EXIT;
		}

		fsize = strtoul(argv[4], NULL ,10);
		if (lseek(fd_in,fsize -1 ,SEEK_SET) == -1)
		{
			printf("lseek failed\n");
			close(fd_in);
			goto EXIT;
		}
		write(fd_in,"\0",1);
		lseek(fd_in,0 ,SEEK_SET);
		printf("record size=%d\n",fsize);

		fdm_in = mmap(0, fsize, PROT_WRITE|PROT_READ, MAP_FILE|MAP_SHARED, fd_in, 0);
		close(fd_in);
		if (fdm_in == MAP_FAILED) {
			printf("mmap fdm_in failed\n");
			goto EXIT;
		}
		printf("fdm_in=0x%08X\n",fdm_in);

#if defined(CONFIG_I2S_MMAP)
		i2s_rxbuffer_mmap();
#endif
		execute_mode = EXTERNAL_LBK2;
		printf("exlbk2 mode\n");
		signal(SIGINT, i2s_ctrl_handler);
		signal(SIGUSR1, i2s_ctrl_handler);
		signal(SIGUSR2, i2s_ctrl_handler);

		ioctl(i2s_fd, I2S_TXRX_COEXIST, 1);
		ioctl(i2s_fd, I2S_RX_ENABLE, 0);
		ioctl(i2s_fd, I2S_TX_ENABLE, 0);
		pos = 0;
		index = 0;

#if 1
		char* pData;
		pData = (char*)calloc(I2S_PAGE_SIZE, sizeof(char));

		while((pos+I2S_PAGE_SIZE)<=fsize)
		{
			ioctl(i2s_fd, I2S_GET_AUDIO, &index);
			pBuf = shrxbuf[index];
			if (rx_pause_stat == 0)
				memcpy((char*)fdm_in+pos, pBuf, I2S_PAGE_SIZE);
			else
				memcpy((char*)fdm_in+pos, pData, I2S_PAGE_SIZE);
			ioctl(i2s_fd, I2S_PUT_AUDIO, &index);
			memcpy(shtxbuf[index], (char*)fdm_in+pos, I2S_PAGE_SIZE);
			pos+=I2S_PAGE_SIZE;
		}
#endif

		ioctl(i2s_fd, I2S_RX_DISABLE, 0);
		ioctl(i2s_fd, I2S_TX_DISABLE, 0);
		ioctl(i2s_fd, I2S_TXRX_COEXIST, 0);

#if defined(CONFIG_I2S_MMAP)
		i2s_txbuffer_munmap();
		i2s_rxbuffer_munmap();
#endif
		munmap(fdm_in, fsize);

		break;
	case 15:
		{
			unsigned long param[4];
			execute_mode = CODEC_BYPASS;
			ioctl(i2s_fd, I2S_DEBUG_CODECBYPASS, 0);
		}
		break;
	case 16:
		for(j=0;j<1000;j++)
		{
			close(i2s_fd);
			i2s_fd = open("dev/i2s0", O_RDWR|O_SYNC);
#if defined(CONFIG_I2S_MMAP)
			i2s_txbuffer_mmap();
#endif
			//icctl(i2s_fd, I2S_RESET, 0);
			//ioctl(i2s_fd, I2S_SRATE, 44100);
			//ioctl(i2s_fd, I2S_TX_VOL, 80);
			ioctl(i2s_fd, I2S_TX_ENABLE, 0);
			ioctl(i2s_fd, I2S_TX_DISABLE, 0);
#if defined(CONFIG_I2S_MMAP)
			i2s_txbuffer_munmap();
#endif
			printf("loop=%d\n",j);
		}
		break;
	case 17:
		if(argc < 3)
		ioctl(i2s_fd, I2S_RESET, 0);
		ioctl(i2s_fd, I2S_SRATE, strtoul(argv[2], NULL ,10));
		ioctl(i2s_fd, I2S_CLOCK_ENABLE, 0);
		break;
#if 0
#if defined(CONFIG_I2S_WM8960)
	case 18:
		{
			execute_mode = CODEC_EXLBK;
			ioctl(i2s_fd, I2S_DEBUG_CODEC_EXLBK, 0);
		}
		break;
#endif
#endif
	default:
		break;
	}

EXIT:
	close(i2s_fd);
	unlink(i2s_opt_pid_filename);

	printf("i2scmd ...quit\n");
	return 0;
}
