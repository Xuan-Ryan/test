#include <stdio.h>
#include <stdlib.h>
#include <alsa/asoundlib.h>
	      
int main (int argc, char *argv[])
{
	int i;
	int err;
	int rate = 44100;
	short buf[6144];
	snd_pcm_t *capture_handle;
	snd_pcm_hw_params_t *hw_params;
	FILE * filp;

	if(argc > 2){
		filp = fopen(argv[2],"wb+");
		if(!filp){
			fprintf (stderr, "file open fail\n");
			exit (1);
		}
	}
	if ((err = snd_pcm_open (&capture_handle, argv[1], SND_PCM_STREAM_CAPTURE, 0)) < 0) {
		fprintf (stderr, "cannot open audio device %s (%s)\n",
			 argv[1],
			 snd_strerror (err));
		exit (1);
	}

	if ((err = snd_pcm_hw_params_malloc (&hw_params)) < 0) {
		fprintf (stderr, "cannot allocate hardware parameter structure (%s)\n",
			 snd_strerror (err));
		exit (1);
	}

	if ((err = snd_pcm_hw_params_any (capture_handle, hw_params)) < 0) {
		fprintf (stderr, "cannot initialize hardware parameter structure (%s)\n",
			 snd_strerror (err));
		exit (1);
	}

	if ((err = snd_pcm_hw_params_set_access (capture_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
		fprintf (stderr, "cannot set access type (%s)\n",
			 snd_strerror (err));
		exit (1);
	}
	if ((err = snd_pcm_hw_params_set_format (capture_handle, hw_params, SND_PCM_FORMAT_S16_LE)) < 0) {
		fprintf (stderr, "cannot set sample format (%s)\n",
			 snd_strerror (err));
		exit (1);
	}
	if ((err = snd_pcm_hw_params_set_rate_near (capture_handle, hw_params, &rate, 0)) < 0) {
		fprintf (stderr, "cannot set sample rate (%s)\n",
			 snd_strerror (err));
		exit (1);
	}
	if ((err = snd_pcm_hw_params_set_channels (capture_handle, hw_params, 1)) < 0) {
		fprintf (stderr, "cannot set channel count (%s)\n",
			 snd_strerror (err));
		exit (1);
	}

	if ((err = snd_pcm_hw_params (capture_handle, hw_params)) < 0) {
		fprintf (stderr, "cannot set parameters (%s)\n",
			 snd_strerror (err));
		exit (1);
	}
	snd_pcm_hw_params_free (hw_params);

	if ((err = snd_pcm_prepare (capture_handle)) < 0) {
		fprintf (stderr, "cannot prepare audio interface for use (%s)\n",
			 snd_strerror (err));
		exit (1);
	}
#if 1

	for (i = 0; i < 100; ++i) {
		if ((err = snd_pcm_readi (capture_handle, buf,6144)) != 6144) {
			fprintf (stderr, "read from audio interface failed (%s)\n",
				 snd_strerror (err));
			exit (1);
		}
		fwrite(&buf,sizeof(buf),1,filp);
	}
#endif
	fclose(filp);
	snd_pcm_close (capture_handle);
	exit (0);
}
