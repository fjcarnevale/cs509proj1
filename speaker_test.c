#include <stdio.h>
#include <string.h>
#include <alsa/asoundlib.h>

int main(int argc, char** argv){
	
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

	snd_pcm_close(handle);
}
