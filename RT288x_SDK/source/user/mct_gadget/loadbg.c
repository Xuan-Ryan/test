#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/time.h>
#include <syslog.h>
#include <pthread.h>
#include <sys/sem.h>
#include <semaphore.h>

#include "nvram.h"
#include "i2s_ctrl.h"
#include "linux/autoconf.h"

#include "mct_gadget.h"
#include "uvcdev.h"

#define LOADBG_PID_FILE                                "/var/run/loadbg.pid"

static int exited = 0;

static char video_buffer[JUVC_MAX_VIDEO_PACKET_SIZE];
static int total_size;

static void signal_handler(int sig)
{
	if (sig == SIGINT || sig == SIGTERM) {
		if (sig == SIGINT)
			PRINT_MSG("loadbg: SIGINT!\n");
		else
			PRINT_MSG("loadbg: SIGTERM!\n");
		exited = 1;
	}
}

static void load_bg(void)
{
	const char * lang =  nvram_get(RT2860_NVRAM, "lang");
	char full_name[256];
	FILE * fp = NULL;

	if (strlen(lang) > 0) {
		sprintf(full_name, "/home/bg/%s/%s", lang, BG_FILENAME1080P);
	} else {
		sprintf(full_name, "/home/bg/en-us/%s", BG_FILENAME1080P);
	}

	fp = fopen(full_name, "r");
	if (fp) {
		total_size = fread(video_buffer, 1, JUVC_MAX_VIDEO_PACKET_SIZE, fp);
		fclose(fp);
	} else {
		total_size = 0;
	}
}

static void save_pid(void)
{
	FILE * fp = NULL;
	fp = fopen(LOADBG_PID_FILE, "w");
	if (fp) {
		fprintf(fp, "%d", getpid());
		fclose(fp);
	}
}

static int initialize_procedure(int argc, char **argv)
{
	load_bg();

	if (init_uvc_dev() < 0)
		return -1;

	/*  save our pid  */
	save_pid();

	return 0;
}

static void close_procedure(void)
{
	unlink(LOADBG_PID_FILE);

	close_uvc();
}

int main(int argc, char ** argv)
{
	pid_t child = 0, sid = 0;

	int bgmode = 1;

	if(argc > 1) {
		if(strcasecmp(argv[1], "-f") == 0)  /*  run as foreground  */
			bgmode = 0;
	}

	if(bgmode == 1) {
		/*  run in background  */
		child = fork();
		if(child < 0) {
			PRINT_MSG("loadbg: Execute %s failed\n", argv[0]);
			exit(-1);
		}

		if(child > 0) {
			/*  parent process  */
			exit(0);
		}
	}

	PRINT_MSG("loadbg: here we go\n");

	(void)signal(SIGINT, signal_handler);
	(void)signal(SIGTERM, signal_handler);

	if (initialize_procedure(argc, argv) < 0) {
		exit(EXIT_FAILURE);
	}

	while(exited == 0) {
		write_uvc(video_buffer, total_size);
	}

	close_procedure();

	PRINT_MSG("loadbg: terminated\n");

	return 0;
}
