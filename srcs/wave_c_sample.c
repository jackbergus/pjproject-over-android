#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <assert.h>
#include <string.h>

struct WAVE {
	char ChunkID[4];
	char ChunkSize[4];
	char Format[4];
	char SubChunk1ID[4];
	char SubChunk1Size[4];
	char AudioFormat[2];
	char NumChannels[2];
	char SampleRate[4];
	char ByteRate[4];
	char BlockAlign[2];
	char BitsPerSample[2];
	char SubChunk2ID[4];
	char SubChunk2Size[4];
	char* DATA;
};

struct Wave {
	char ChunkID[4];
	unsigned int ChunkSize;
	char  Format[4];
	unsigned int SubChunk1ID;
	unsigned int SubChunk1Size;
	unsigned short AudioFormat;
	unsigned short NumChannels;
	unsigned int SampleRate;
	unsigned int ByteRate;
	unsigned short BlockAlign;
	unsigned short BitsPerSample;
	unsigned int SubChunk2ID;
	unsigned int SubChunk2Size;
	char* DATA;
};

unsigned short int char2(char* w) {
   unsigned short int word;
   assert(w);
   strncpy((char*)&word, w, 2);
   return word;
}

unsigned int char4(char* w) {
   unsigned int word;
   assert(w);
   strncpy((char*)&word, w, 4);
   return word;
}

#define mstrncpy(where,from,field,size) strncpy((where)->field,(from)->field,size)
#define cpy2(where,from,field) (where)->field = char2((from)->field);
#define cpy4(where,from,field) (where)->field = char4((from)->field);

void convertType(struct Wave* dest, struct WAVE* from) {
	assert(dest);
	assert(from);
	
	mstrncpy(dest,from,ChunkID,4);
	mstrncpy(dest,from,Format,4);
	
	cpy2(dest,from,AudioFormat);
	cpy2(dest,from,NumChannels);
	cpy2(dest,from,BlockAlign);
	cpy2(dest,from,BitsPerSample);
	
	cpy4(dest,from,ChunkSize);
	cpy4(dest,from,SubChunk1ID);
	cpy4(dest,from,SubChunk1Size);
	cpy4(dest,from,SampleRate);
	cpy4(dest,from,ByteRate);
	cpy4(dest,from,SubChunk2ID);
	cpy4(dest,from,SubChunk2Size);
	
}

#define printL(len,chars) printf("%.*s", len, chars);


void printW(struct Wave* w) {
	printf("ChunkID: "); printL(4,w->ChunkID); printf("\n");
	printf("ChunkSize: "); printf("%u\n",w->ChunkSize);
	printf("Format: "); printL(4,w->Format); printf("\n");
	printf("SubChunk1ID: "); printf("%u\n",w->SubChunk1ID);
	printf("SubChunk1Size: "); printf("%u\n",w->SubChunk1Size);
	printf("AudioFormat: ");  printf("%i\n",w->AudioFormat);
	printf("NumChannels: "); printf("%i\n",w->NumChannels);
	printf("SampleRate: "); printf("%u\n",w->SampleRate);
	printf("ByteRate: "); printf("%u\n",w->ByteRate);
	printf("BlockAlign: "); printf("%i\n",w->BlockAlign);
	printf("BitsPerSample: "); printf("%i\n",w->BitsPerSample);
	printf("SubChunk2ID: "); printf("%u\n",w->SubChunk2ID);
	printf("SubChunk2Size: "); printf("%u\n",w->SubChunk2Size);
}

#define EXAMPLE_WAV "au.wav"
#define FAIL 0
#define GAIN 1

int getWavSettings(char* filename, struct Wave* wav) {
    int fd = 0;
    struct WAVE file;
	
    if ((fd = open(filename,O_RDONLY))==0) { 
            perror ("open");
            return FAIL;
    }
    if (read(fd,(void*)&file,sizeof(file))<=0) {
			perror ("read");
            return FAIL;
    }
    if (close(fd)<0) {
		    perror ("close");
            return FAIL;
    }
	
	convertType(wav, &file);
	return GAIN;
}

int main(int argc, char* argv[]) {

	int fd = 0;
	struct Wave wav;
	
	if (argc<=1) {
		printf("Usage: %s file.wav\n", argv[0]);
		return 1;	
	}
	
	if (getWavSettings(argv[1],&wav)) printW(&wav);

          
    	return 0;

}
