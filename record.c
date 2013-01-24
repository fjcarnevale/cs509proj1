#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <math.h>
#include <stdbool.h>
#include <alsa/asoundlib.h>

#define BUF_SIZE 8000
#define SAMPLE_SIZE 80

FILE* sound_raw;
FILE* sound_data;
snd_pcm_t* handle;

int IMX,IMN,I2;
double I1,ITL,ITU,ZCT;

// computes standard deviation
// int elements - array of ints to compute std dev of
// int count - number of elements
// double mean - mean of the array
double stddev(int elements[], int count, double mean){
	int i;
	int sum = 0;

	for(i=0;i<count;i++){
		sum += pow(elements[i]-mean,2);
	}

	return pow(sum/count,.5);
}

// computes energy of a sample
// char* sample - the beginning of the sample
int energy(char* sample){

	int i;
	int sum=0;

	sample = &(sample[SAMPLE_SIZE/2]);
	for(i=-40;i<=40;i++){
		sum += sample[i];
	}

	return sum;

}

// computes zero cross rate of a sample
// char* sample - the beginning of the sample
int zcr(char *sample){
	bool below;
	int crosses = 0;
	int i;

	if(sample[0] < 0)
		below = true;
	else
		below = false;

	
	for(i=1;i<SAMPLE_SIZE;i++){
		if(sample[i] > 0 && below){
			crosses++;
			below = false;
		}else if(sample[i] < 0 && !below){
			crosses++;
			below = true;
		}
	}

	return crosses;
}

void startup(snd_pcm_t** device_handle){
	
	char buffer[800];
	int energy_sum = 0;
	int energy_peak = -999;
	int energy_min = 1000;
	int zcr_sum;
	int zcr_interval[10];
	int i;
	double energy_avg;
	double zcr_avg;
	double zcr_stddev;

	snd_pcm_readi(*device_handle,buffer,800);

	int tmp_energy;
	for(i=0;i<10;i++){
		tmp_energy = energy(&buffer[i*80]);
		energy_sum += tmp_energy;
		if(tmp_energy > energy_peak)
			energy_peak = tmp_energy;
		else if(tmp_energy < energy_min)
			energy_min = tmp_energy;

		zcr_interval[i] = zcr(&buffer[i*80]);
		zcr_sum += zcr_interval[i];
	}

	energy_avg = energy_sum / 10.0;
	zcr_avg = zcr_sum / 10.0;

	zcr_stddev = stddev(zcr_interval, 10, zcr_avg);

	if(zcr_avg + 2 * zcr_stddev < 25)
		ZCT = zcr_avg + 2 * zcr_stddev;
	else
		ZCT = 25;

	IMX = energy_peak;
	IMN = energy_min;
	I1 = .03 * (IMX - IMN) + IMN;
	I2 = 4 * IMN;
	if(I1<I2)
		ITL = I1;
	else
		ITL = I2;
	ITU = 5 * ITL;

}

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

// Opens and sets the parameters for recording
int setup_device(snd_pcm_t** device_handle){
	
	if(snd_pcm_open(device_handle, "default", SND_PCM_STREAM_CAPTURE, 0) < 0){
		printf("snd_pcm_open -- failed to open device\n");
		return -1;
	}

	if(snd_pcm_set_params(
			*device_handle,
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

	return 0;
}

void record(snd_pcm_t** device_handle){

	char* buffer = (char*)malloc(sizeof(char) * BUF_SIZE);

	printf("Press cntl-c to stop recording\n");
	while(1){
		snd_pcm_readi(*device_handle,buffer,BUF_SIZE);
		fwrite(buffer, sizeof(char), BUF_SIZE, sound_raw);
	}

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

	setup_device(&handle);

	//startup(&handle)

	record(&handle);
}

