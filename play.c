#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <alsa/asoundlib.h>

#define BUF_SIZE 16000

int main(int argc, char** argv){
	if(argc<2){
		printf("Must specify file to play\n");
		return -1;
	}
	
	FILE* fp = fopen(argv[1],"r");
	if(fp == NULL){
		printf("Failed to open specified file\n");
		return -1;
	}

	snd_pcm_t *handle;
	
	if(snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0) < 0){
		printf("snd_pcm_open -- failed to open device\n");
		return -1;
	}

	if(snd_pcm_set_params(
			handle,
			SND_PCM_FORMAT_U8,	
			SND_PCM_ACCESS_RW_INTERLEAVED,
			1, // channels
			8000, // sample rate
			1, // allow resampling
			50000 // .5 seconds
		) < 0 ){
		printf("snd_pcm_set_params -- failed to set parameters\n");
		return -1;
	}

	// white noise buffer
	char* buffer = (char*)malloc(sizeof(char)*BUF_SIZE);
	

	size_t read = fread(buffer, sizeof(char), BUF_SIZE, fp);
	while(read == BUF_SIZE){ // while we haven't hit the end of the file
		snd_pcm_writei(handle, buffer, 16000);
		read = fread(buffer,sizeof(char), BUF_SIZE, fp);
	}
	snd_pcm_close(handle);
}
