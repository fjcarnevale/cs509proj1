#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <alsa/asoundlib.h>

#define BUF_SIZE 8000

FILE* sound_raw;
FILE* sound_data;
snd_pcm_t* handle;

void handle_sigint(int s){
	rewind(sound_raw);
	char buffer[8000];
	char tmp[6];
	int i;
	size_t read = fread(buffer, sizeof(char), 8000, sound_raw);
	while(read == 8000){
		for(i=0;i<read;i++){
			sprintf(tmp, "%d", buffer[i]+128);
			strcat(tmp,",\n");
			fwrite(tmp, sizeof(char), strlen(tmp), sound_data);	
		}
		read = fread(buffer,sizeof(char),8000,sound_raw);
	}
	
	for(i=0;i<read;i++){
		sprintf(tmp, "%d", buffer[i]);
		strcat(tmp,",\n");
		fwrite(tmp, sizeof(char), strlen(tmp), sound_data);	
	}


	fclose(sound_raw);
	fclose(sound_data);
	snd_pcm_close(handle);	
	exit(1);
}

int main(int argc, char** argv){

	// Sigint handler code adapted from
	// stackoverflow.com/questions/1641182/how-can-i-catch-a-ctrl-c-event-c
	struct sigaction sigIntHandler;
	sigIntHandler.sa_handler = handle_sigint;
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;
	sigaction(SIGINT, &sigIntHandler, NULL);

	sound_raw = fopen("sound.raw","rw");
	if(sound_raw==NULL){
		printf("Failed to open sound.raw\n");
		return -1;
	}

	sound_data = fopen("sound.data","w");
	if(sound_data == NULL){
		printf("Failed to open sound.data\n");
		return -1;
	}

	if(snd_pcm_open(&handle, "default", SND_PCM_STREAM_CAPTURE, 0) < 0){
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
			500000 // .5 seconds
		) < 0 ){
		printf("snd_pcm_set_params -- failed to set parameters\n");
		return -1;
	}

	char* buffer = (char*)malloc(sizeof(char) * BUF_SIZE);

	printf("Press cntl-c to stop recording\n");
	while(1){
		snd_pcm_readi(handle,buffer,BUF_SIZE);
		fwrite(buffer, sizeof(char), BUF_SIZE, sound_raw);
	}
}

