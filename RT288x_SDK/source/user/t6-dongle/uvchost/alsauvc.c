#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>

#include <alsa/asoundlib.h>

#include "alsauvc.h"
#include "uvchost.h"
#include "libavcodec/avcodec.h"


static int open_stream(snd_pcm_t **handle, const char *name, int dir,struct audio_para *par)
{
	snd_pcm_hw_params_t *hw_params;
	snd_pcm_sw_params_t *sw_params;
	const char *dirname = (dir == SND_PCM_STREAM_PLAYBACK) ? "PLAYBACK" : "CAPTURE";
	int err;
    unsigned int rate = par->rate[0];
	unsigned int ch   = par->ch;
    unsigned int format = SND_PCM_FORMAT_S16_LE;

	if ((err = snd_pcm_open(handle, name, dir, 0)) < 0) {
		printf("snd_pcm_open failed \n");
		return -1;
	}
	   
	if ((err = snd_pcm_hw_params_malloc(&hw_params)) < 0) {
		printf("snd_pcm_hw_params_malloc failed \n");
		return -1;
	}
			 
	if ((err = snd_pcm_hw_params_any(*handle, hw_params)) < 0) {
		printf("snd_pcm_hw_params_any failed \n");
		return -1;
	}

	if ((err = snd_pcm_hw_params_set_access(*handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
		printf("snd_pcm_hw_params_set_access failed \n");
		return -1;
	}

	if ((err = snd_pcm_hw_params_set_format(*handle, hw_params, format)) < 0) {
		printf("snd_pcm_hw_params_set_format failed \n");
		return -1;
	}

	if ((err = snd_pcm_hw_params_set_rate_near(*handle, hw_params, &rate, NULL)) < 0) {
		printf("snd_pcm_hw_params_set_rate_near failed \n");
		return -1;
	}

	if ((err = snd_pcm_hw_params_set_channels(*handle, hw_params, ch)) < 0) {
		printf("snd_pcm_hw_params_set_channels failed \n");
		return -1;
	}

	if ((err = snd_pcm_hw_params(*handle, hw_params)) < 0) {
		printf("snd_pcm_hw_params failed \n");
		return -1;
	}
   
	snd_pcm_hw_params_free(hw_params);

	

	return 0;
}

void stereoize(short *outbuf, short *inbuf, int num_samples)
{
    int i;
    for (i = 0; i < num_samples; i++) {
        outbuf[i * 2]  = inbuf[i];
		outbuf[i * 2+1] = inbuf[i];
    }
	
}

void PlaybackAudio(struct audio_para *par )
{
	
	int ret = 0 ;
	int r = 0 ;
	int len = 0 ;
   

	snd_pcm_t *playback_handle = NULL;
	char buf[3072]; 
	char outbuf[4096]; 
	int buffe_size = par->page_size ;
    
     printf("open_stream start PlaybackAudio %s \n",par->dev); 
	if ((ret = open_stream(&playback_handle,par->dev,SND_PCM_STREAM_PLAYBACK,par) < 0))
			goto END;
	
	
	while(*par->run){
		
		

		ret = TcpRead(par->socket,buf,3072);
		if(ret != 3072){
			printf("playback socket failed = %d \n",ret);
            break;
		}
        //printf("tcp audio speak data = %d \n",ret);
       // ret = 3072;
      
		if(par->resampleEngine != NULL){
			r = ret /4;
			r = audio_resample((ReSampleContext*)par->resampleEngine,(short *)outbuf,(short *)buf, r);
			//len = r * 4;
			if (ret = snd_pcm_writei(playback_handle, outbuf, r) == -EPIPE) {
				printf("XRUN.\n");
				snd_pcm_prepare(playback_handle);
			} else if (ret < 0) {
				printf("ERROR. Can't write to PCM device. %s\n", snd_strerror(ret));
				break;
			}
	
		
		}else{
			r = ret /4;
			if (ret = snd_pcm_writei(playback_handle, buf, r) == -EPIPE) {
				printf("XRUN.\n");
				snd_pcm_prepare(playback_handle);
			} else if (ret < 0) {
				printf("ERROR. Can't write to PCM device. %s\n", snd_strerror(ret));
				break;
			}
		}
		
		
		
		
  
	}
END:	
	if(par->resampleEngine != NULL)
		audio_resample_close((ReSampleContext*)par->resampleEngine);
	
	if(playback_handle!= NULL){
		snd_pcm_drain(playback_handle);
		snd_pcm_close(playback_handle);
	}


}

void PlayAudio(struct audio_para *par ,int card)
{
	
	int ret = 0 ;
	int r = 0 ;
	int len = 0 ;
   

	snd_pcm_t *playback_handle = NULL;
	char buf[4096]; 
	char outbuf[4096]; 
	int buffe_size = par->page_size ;
    

	if ((ret = open_stream(&playback_handle,par->dev,SND_PCM_STREAM_PLAYBACK,par) < 0))
			goto END;
	
	
	while(*par->run){
		
		if(*par->run2 == 1){
			break;
		}

		ret = UdpRead(par->socket,"10.10.10.254",GADGET_SPK_PORT,buf,1920);
		
		if(par->resampleEngine != NULL){
			r = audio_resample((ReSampleContext*)par->resampleEngine,(short *)outbuf,(short *)buf, r);
			//len = r * 4;
			if (ret = snd_pcm_writei(playback_handle, outbuf, r) == -EPIPE) {
				printf("XRUN.\n");
				snd_pcm_prepare(playback_handle);
			} else if (ret < 0) {
				printf("ERROR. Can't write to PCM device. %s\n", snd_strerror(ret));
				break;
			}
	
		
		}else{
			//len = r / 4 ;
			if (ret = snd_pcm_writei(playback_handle, buf, len) == -EPIPE) {
				printf("XRUN.\n");
				snd_pcm_prepare(playback_handle);
			} else if (ret < 0) {
				printf("ERROR. Can't write to PCM device. %s\n", snd_strerror(ret));
				break;
			}
		}
		
		printf("card = %d audio socket read r = %d   \n",card,len);
		
  
	}
END:	
	if(par->resampleEngine != NULL)
		audio_resample_close((ReSampleContext*)par->resampleEngine);
	
	if(playback_handle!= NULL){
		snd_pcm_drain(playback_handle);
		snd_pcm_close(playback_handle);
	}


}

void SendAudioCapture(struct audio_para *par ,int card)
{
	int ret = 0 ;
	int r = 0 ;
	int len = 0 ;
   

	snd_pcm_t *capture_handle = NULL;
    char buf[4096]; 
    char outbuf[4096]; 
	int buffe_size = par->page_size ;


	if ((ret = open_stream(&capture_handle,par->dev,SND_PCM_STREAM_CAPTURE,par) < 0))
			goto END;
	
	
	while(*par->run){
		
		if(*par->run2 == 1){
			break;
		}
		
		r = snd_pcm_readi(capture_handle,buf,buffe_size);
		if(r == -EAGAIN ){
			printf(" capture failed ret = -EAGAIN  \n");
			snd_pcm_wait(capture_handle, 100);
			continue;
		}else if(r == -EPIPE){
		    printf(" capture failed ret = -EPIPE \n");
			r = snd_pcm_prepare(capture_handle);
			if(r < 0){
				printf(" failed snd_pcm_prepare ret = %d \n",r);
			}
	        continue;
		   
		}else if (r == -ESTRPIPE){ 
		    printf(" capture failed ret = -ESTRPIPE \n");
			r = snd_pcm_resume(capture_handle);
			if(r < 0)
				snd_pcm_prepare(capture_handle);
			continue;
		}else if(r == -EIO ){
			printf(" sound i/o error r  =%d\n",r);
			continue;
		}else if(r <= 0){
			printf(" sound error r  =%d\n",r);
			break;
		}
		
		if(par->resampleEngine != NULL){

			r = audio_resample((ReSampleContext*)par->resampleEngine,(short *)outbuf,(short *)buf, r);
			len = r * 4;
		
		}else{
      
			len = r * 4 ;
			
		}
		
	    printf("card = %d audio socket write r = %d   \n",card,len);
	
	
	
  
	}
END:	
	if(par->resampleEngine != NULL)
    	audio_resample_close((ReSampleContext*)par->resampleEngine);
	if(capture_handle!= NULL)
		snd_pcm_close(capture_handle);



}



void RunAudioCapture(struct audio_para *par)
{
	int ret = 0 ;
	int r = 0 ;
	int len = 0 ;


	snd_pcm_t *capture_handle = NULL;

    char buf[4096]; 
    char outbuf[4096]; 
	char* udpptr; 
	char* udpptrout;
	int buffe_size = par->page_size ;

    char udpbuf[2][3072]; 
    int  udpindex = 0;
	int  udpoffset = 0;
	int  udpremain = 3072;
    printf("open_stream start %s \n",par->dev); 
	if ((ret = open_stream(&capture_handle,par->dev,SND_PCM_STREAM_CAPTURE,par) < 0)){
			goto END;
	}
	
	printf("open_stream sucessful \n"); 
	while(*par->run){
		
		r = snd_pcm_readi(capture_handle,buf,buffe_size);
		if(r == -EAGAIN ){
			printf(" capture failed ret = -EAGAIN  \n");
			snd_pcm_wait(capture_handle, 100);
			continue;
		}else if(r == -EPIPE){
		
		    printf(" capture failed ret = -EPIPE \n");
		    break;
		  
		}else if (r == -ESTRPIPE){ 
		    printf(" capture failed ret = -ESTRPIPE \n");
			r = snd_pcm_resume(capture_handle);
			if(r < 0)
				snd_pcm_prepare(capture_handle);
			continue;
		}else if(r == -EIO ){
			printf(" sound i/o error r  =%d\n",r);
			continue;
		}else if(r <= 0){
			printf(" sound error r  =%d\n",r);
			break;
		}
		
		if(par->resampleEngine != NULL){
		
			r = audio_resample((ReSampleContext*)par->resampleEngine,(short *)outbuf,(short *)buf, r);
			len = r * 4;		
			udpptr = outbuf;

		}else{
       
			len = r * 4 ;
			udpptr = buf;
		
		}
		ret = 0;
		ret = TcpWrite(par->socket,udpptr,len);
		//ret = UdpWrite(par->socket,"10.10.10.254",GADGET_MIC_PORT,udpptr,len);
/*
		if(len >= udpremain ){
		    int buflen = 0;
		    memcpy(udpbuf[udpindex]+ udpoffset,udpptr ,udpremain);
			//udp write
            ret = UdpWrite(par->socket,"10.10.10.254",GADGET_MIC_PORT,udpbuf[udpindex],3072);
			
			if(udpindex == 0)
				udpindex =1;
			else
				udpindex =0;
			buflen = len - udpremain ;
			memcpy(udpbuf[udpindex],udpptr+udpremain,buflen);
			udpoffset = buflen;
			udpremain = 3072 - buflen;

		}else{
			memcpy(udpbuf[udpindex]+udpoffset,udpptr,len);
			udpoffset += len;
            udpremain = udpremain - len;
		}
     
*/
		
	  //  printf("audio socket write ret len = %d  \n",ret,len);
		if(ret < 0){
			printf("audio socket write failed \n");
			break;
		}
	
  
	}
END:	
	if(par->resampleEngine != NULL)
    	audio_resample_close((ReSampleContext*)par->resampleEngine);
	if(capture_handle!= NULL)
		snd_pcm_close(capture_handle);
	

}

static char** str_split(char* a_str, const char a_delim)
{
    char** result    = 0;
    size_t count     = 0;
    char* tmp        = a_str;
    char* last_comma = 0;
    char delim[2];
    delim[0] = a_delim;
    delim[1] = 0;

    /* Count how many elements will be extracted. */
    while (*tmp)
    {
        if (a_delim == *tmp)
        {
            count++;
            last_comma = tmp;
        }
        tmp++;
    }

    /* Add space for trailing token. */
    count += last_comma < (a_str + strlen(a_str) - 1);

    /* Add space for terminating null string so caller
       knows where the list of returned strings ends. */
    count++;

    result = malloc(sizeof(char*) * count);

    if (result)
    {
        size_t idx  = 0;
        char* token = strtok(a_str, delim);

        while (token)
        {
            assert(idx < count);
            *(result + idx++) = strdup(token);
            token = strtok(0, delim);
        }
        assert(idx == count - 1);
        *(result + idx) = 0;
    }

    return result;
}

static void split_data(char *s ,int mode ,struct audio_para *par)
{
	char** tokens;
	tokens = str_split(s, ',');
	if (tokens)
	{
	    int i;
	    for (i = 0; *(tokens + i); i++)
	    {
	        if(mode == 2){
				//printf("%d\n", atoi(*(tokens + i)));
                par->ch = atoi(*(tokens + i));
	        }else if(mode == 3){
                //printf("%d\n", atoi(*(tokens + i)));
			    par->rateindex = i + 1;
				par->rate[i] = atoi(*(tokens + i));
	        }else if(mode == 1){
	        	  //printf("%s", *(tokens + i));
                  char * pch = NULL;
				  pch = strstr(*(tokens + i),"S16_LE");
				  if(pch != NULL){
                      par->format = SND_PCM_FORMAT_S16_LE;
				  }
				  
	        }
			free(*(tokens + i));
	    }
	   // printf("\n");
	    free(tokens);
	}
}


void DumpAudioParameter(struct audio_para *par)
{
    int i = 0;
	printf("dev = %s \n", par->dev);
	printf("mode = %d \n", par->mode);
	printf("channel = %d \n", par->ch);
	printf("format = %d \n", par->format);
    printf("rate index = %d \n", par->rateindex);
	for(i = 0; i < par->rateindex ; i++)
		printf("rate[%d] = %d \n",i,par->rate[i]);


}

void JudgeAudioResample2(struct audio_para *par)
{
  int i = 0;
  int setrate ;
  
  par->resampleEngine = NULL;
  for(i = 0; i < par->rateindex ; i++){
  	setrate = par->rate[i];
	if(setrate == 48000)
		break;
  }
  par->rate[0]  =  setrate;
  if(par->ch == 0)
  	par->ch =1;
  par->page_size = par->rate[0] / 100;
  printf("par->ch = %d ,par->rate[0] = %d par->page_size =%d \n",par->ch,par->rate[0],par->page_size );
  if(par->ch != 2 &&  par->rate[0] != 48000){
  	  printf("resample par->ch = %d ,par->rate[0] = %d \n",par->ch,par->rate[0]);
	  par->resampleEngine =(void*) av_audio_resample_init(par->ch,2,par->rate[0], 48000, SAMPLE_FMT_S16, SAMPLE_FMT_S16, 16, 10, 0, 0.8);

  }

}

void JudgeAudioResample(struct audio_para *par)
{
  int i = 0;
  int setrate ;
  
  par->resampleEngine = NULL;
  for(i = 0; i < par->rateindex ; i++){
  	setrate = par->rate[i];
	if(setrate == 48000)
		break;
  }
  par->rate[0]  =  setrate;
  if(par->ch == 0)
  	par->ch =1;
  par->page_size = par->rate[0] / 100;
  printf("par->ch = %d ,par->rate[0] = %d par->page_size =%d \n",par->ch,par->rate[0],par->page_size );
  if(par->ch != 2 ||  par->rate[0] != 48000){
  	  printf("resample par->ch = %d ,par->rate[0] = %d \n",par->ch,par->rate[0]);
	  par->resampleEngine =(void*) av_audio_resample_init(2, par->ch, 48000, par->rate[0], SAMPLE_FMT_S16, SAMPLE_FMT_S16, 16, 10, 0, 0.8);

  }
  
	



}
uint8_t GetAudioDevices()
{
 
	uint8_t count = 0;     
	if(0 == access("/proc/asound/card1/usbid", 0)){
		count | 0x01;
	}
    if(0 == access("/proc/asound/card2/usbid", 0)){
		count | 0x02;
	}
	return count;
}


int GetAudioUsbid(char* filename,uint16_t *vid,uint16_t *pid)
{
	char str[256];
	FILE * file=NULL;
	char * pch = NULL;
	file = fopen( filename, "r");
	if(file == NULL)
		return -1;
	if(fgets(str, sizeof(str), file)){
		char** tokens;
		tokens = str_split(str, ':');
		if (tokens)
		{
		    int i;
		    for (i = 0; *(tokens + i); i++)
		    {
		        int num = (int)strtol(*(tokens + i), NULL, 16);  
				if(i == 0)
					*vid = num;
				if(i == 1)
					*pid = num;
		    }
		}
	}
	fclose(file);
	return 0;	

}


int GetAudioStream(char* filename, struct audio_para *par ,int mode)
{
	char str[256];
	FILE * file = NULL;
	char * pch = NULL;
	int  index = -1;
	file = fopen(filename , "r");
	if (file == NULL)
		return -1;
   
	while (fgets(str, sizeof(str), file)) {
        pch = strstr(str,"Playback");
		if(pch != NULL){
			//printf("%s", str); 
			index = PLK;
		}else{
			pch = strstr(str,"Capture");
			if(pch != NULL){
				//printf("%s", str); 
				index = CAP;
			}
		}

		if(index == mode){
		    par->mode = mode;
			pch = strstr(str,"Format");
			if(pch != NULL)	{
				char *s = strtok(str, ":");
				s = strtok(NULL, ":");
        		if(s != NULL){
					split_data(s,1,par);
        		}
				continue;	
			}
			pch = strstr(str,"Channels");
			if(pch != NULL)	{
        		char *s = strtok(str, ":");
				s = strtok(NULL, ":");
        		if(s != NULL){
					split_data(s,2,par);
        		}
				continue;	
			}
			pch = strstr(str,"Rates");
			if(pch != NULL)	{
        		char *s = strtok(str, ":");
				s = strtok(NULL, ":");
        		if(s != NULL){
					split_data(s,3,par);
        		}
				continue;	
			}
		}

	}	
   	fclose(file);
	return 0;
   
}


