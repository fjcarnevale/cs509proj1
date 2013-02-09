// Frank Carnevale
// CS 529 Project 1

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <math.h>
#include <stdbool.h>
#include <unistd.h>
#include <alsa/asoundlib.h>
#include <sys/time.h>
#include <sys/resource.h>

#define BUF_SIZE 8000
#define SAMPLE_SIZE 80

FILE* sound_raw;
FILE* sound_data;
FILE* speech_raw;
FILE* energy_data;
FILE* zero_data;

snd_pcm_t* handle;

int IMX,IMN,I2;
double I1,ITL,ITU,ZCT;

typedef struct frame{
	double energy;
	int zcr;
	unsigned char buffer[SAMPLE_SIZE];
	struct frame* prev;
	struct frame* next;
}frame;

// computes standard deviation
// int elements - array of ints to compute std dev of
// int count - number of elements
// double mean - mean of the array
double stddev(int elements[], int count, double mean){
	int i;
	double sum = 0;

	for(i=0;i<count;i++){
		sum += pow(elements[i]-mean,2);
	}
	
	return pow(sum/count,.5);
}

// computes energy of a sample
// char* sample - the beginning of the sample
double energy(unsigned char* sample){

	int i;
	double sum=0;

	sample = &(sample[SAMPLE_SIZE/2]);
	for(i=-(SAMPLE_SIZE/2);i<(SAMPLE_SIZE/2);i++){
		sum += abs(sample[i]-127);
	}

	return sum;

}

// computes zero cross rate of a sample
// char* sample - the beginning of the sample
int zcr(unsigned char *sample){
	bool below;
	int crosses = 0;
	int i;

	if(sample[0] < 127)
		below = true;
	else
		below = false;

	
	for(i=1;i<SAMPLE_SIZE;i++){
		if(sample[i] > 127 && below){
			crosses++;
			below = false;
		}else if(sample[i] < 127 && !below){
			crosses++;
			below = true;
		}
	}

	return crosses;
}

void startup(snd_pcm_t** device_handle){
	
	unsigned char buffer[SAMPLE_SIZE*10];
	int energy_sum = 0;
	double energy_peak = -999;
	double energy_min = 100000;
	int zcr_sum;
	int zcr_interval[10];
	int i;
	double energy_avg;
	double zcr_avg;
	double zcr_stddev;

	snd_pcm_readi(*device_handle,buffer,SAMPLE_SIZE*10);

	double tmp_energy;

	for(i=0;i<10;i++){
		tmp_energy = energy(&buffer[i*SAMPLE_SIZE]);
		//printf("Energy: %f\n",tmp_energy);
		energy_sum += tmp_energy;
		if(tmp_energy > energy_peak)
			energy_peak = tmp_energy;
		else if(tmp_energy < energy_min)
			energy_min = tmp_energy;

		zcr_interval[i] = zcr(&buffer[i*SAMPLE_SIZE]);
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
	IMN = energy_avg;
	I1 = .03 * (IMX - IMN) + IMN;
	I2 = 4 * IMN;
	if(I1<I2){
		ITL = I1;
	}else{
		ITL = I2;
	}

	ITU = 5 * ITL;

/*
	printf("E peak: %f\n", energy_peak);
	printf("E min : %f\n", energy_min);
	printf("E avg: %f\n", energy_avg);

	printf("ZCR AVG: %f\n", zcr_avg);
	printf("ZCR STDDEV: %f\n",zcr_stddev);

	printf("I1: %f\n",I1);
	printf("I2: %d\n",I2);

	printf("IMX: %d\n",IMX);
	printf("IMN: %d\n",IMN);
	printf("ITL: %f\n",ITL);
	printf("ITU: %f\n",ITU);
*/

}

void handle_sigint(int s){
	rewind(sound_raw);
	unsigned char buffer[8000];
	char out_buffer[40001];
	char tmp[5]; 
	int i;
	size_t read = fread(buffer, sizeof(char), 8000, sound_raw);
	while(read > 0){
		out_buffer[0] = '\0';
		for(i=0;i<read;i++){
			sprintf(tmp, "%d", buffer[i]);
			strcat(tmp,",\n");
			strcat(out_buffer,tmp);
		}
		fwrite(out_buffer,sizeof(char), strlen(out_buffer), sound_data);
		read = fread(buffer,sizeof(char),8000,sound_raw);
	}
	fclose(sound_raw);
	fclose(sound_data);
	fclose(speech_raw);
	fclose(energy_data);
	fclose(zero_data);

	snd_pcm_close(handle);	
	exit(0);
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

frame* zcr_start_search(frame* current_frame){
	frame* tmp_frame = current_frame;
	int frame_count = 0;

	while(frame_count < 25 && tmp_frame->zcr > ZCT && tmp_frame->prev != NULL){
		tmp_frame = tmp_frame->prev;
		frame_count++;
	}

	if(frame_count<3)
		return current_frame;
	return tmp_frame;

}

frame* zcr_end_search(snd_pcm_t** device_handle, frame** old_frame){
	frame* current_frame = *old_frame;	
	frame* tmp_frame;
	int frame_count = 0;

	while(frame_count < 25 && current_frame->zcr > ZCT){
		tmp_frame = (frame*)malloc(sizeof(frame));
		snd_pcm_readi(*device_handle,tmp_frame->buffer,sizeof(char)*80);
		tmp_frame->energy = energy(tmp_frame->buffer);
		tmp_frame->zcr    = zcr(tmp_frame->buffer);
		current_frame->next = tmp_frame;
		current_frame = tmp_frame;
		frame_count++;
	}

	if(frame_count<3)
		return *old_frame;
	return tmp_frame;
	

}

void write_speech(frame* start, frame* end){

	while(start != end){
		fwrite(start->buffer, sizeof(char), 80, speech_raw);
		start = start->next;
	}

}

void record(snd_pcm_t** device_handle){
	bool speech = false;
	char tmp[16];


	frame* root_frame = (frame*)malloc(sizeof(frame));
	snd_pcm_readi(*device_handle,root_frame->buffer,sizeof(char)*80);
	root_frame->energy = energy(root_frame->buffer);
	root_frame->zcr = zcr(root_frame->buffer);
	root_frame->prev = NULL;
	root_frame->next = NULL;

	frame* current_frame = root_frame;
	frame* start_frame = NULL;
	frame* end_frame = NULL; 

	int frame_num = 0;

	while(1){
		frame* tmp_frame = (frame*)malloc(sizeof(frame));
		snd_pcm_readi(*device_handle,tmp_frame->buffer,sizeof(char)*80);
		fwrite(tmp_frame->buffer, sizeof(char), 80, sound_raw);
		tmp_frame->energy = energy(tmp_frame->buffer);
		tmp_frame->zcr    = zcr(tmp_frame->buffer);
		current_frame->next = tmp_frame;
		current_frame = tmp_frame;

		if(current_frame->energy > ITL && start_frame==NULL){
			start_frame = current_frame;
		}
		if(current_frame->energy > ITU && !speech){
			printf("Speech!\n");
			speech = true;
			start_frame = (frame*)zcr_start_search(start_frame);
		}
		if(current_frame->energy < ITL){
			if(speech){
				printf("No more speech!\n");
				end_frame = zcr_end_search(device_handle, &current_frame);
				speech=false;

				write_speech(start_frame,end_frame);
					
				start_frame = NULL;
				end_frame = NULL;		
	
			}else{
				start_frame = NULL;
			}	
		}


		sprintf(tmp,"%g",current_frame->energy);
		strcat(tmp,",\n");
		fwrite(tmp, sizeof(char), strlen(tmp), energy_data);
	
		sprintf(tmp,"%d",current_frame->zcr);
		strcat(tmp,",\n");
		fwrite(tmp,sizeof(char),strlen(tmp),zero_data);
	
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

	sound_raw = fopen("sound.raw","w+");
	if(sound_raw==NULL){
		printf("Failed to open sound.raw\n");
		return -1;
	}

	sound_data = fopen("sound.data","w+");
	if(sound_data == NULL){
		printf("Failed to open sound.data\n");
		return -1;
	}

	speech_raw = fopen("speech.raw","w+");
	if(speech_raw == NULL){
		printf("Failed to open speech.raw\n");
		return -1;
	}

	energy_data = fopen("energy.data","w+");
	if(speech_raw == NULL){
		printf("Failed to open energy.data\n");
		return -1;
	}

	zero_data = fopen("zero.data","w+");
	if(speech_raw == NULL){
		printf("Failed to open zero.data\n");
		return -1;
	}

	setup_device(&handle);

	startup(&handle);

	record(&handle);

	FILE* status = fopen("/proc/self/status","r");
	char buffer[128];
	while(fread(buffer,sizeof(char), 128, status)){
		printf("%s",buffer);
	}
}

