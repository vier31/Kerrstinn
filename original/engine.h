#ifndef engine_H
#define engine_H 
// #include <iostream>
//#include <stdlib.h>

#include <math.h>  
#include <stdio.h>
#include <stdlib.h>

#define DnumTypes 26
 
// types  
typedef long dword;  
typedef short int word;  
typedef unsigned char byte;


 
//------------------------------------------------------------------------
class engine  
{
public:
	engine ();
	~engine ();
 
unsigned long get_ulong(FILE *f);
unsigned get_ushort(FILE *f);
unsigned get_ubyte(FILE *f);

int getrand(int max);
int getrand2(int max);
int getrandb();
int getrandb2();
void smallWorkerAR(float* in, float* out,int slice);
void smallWorkerBackwards(float* in, float* out,int slice);
void smallWorkerDistortion(float* in, float* out,int slice);
void smallWorkerReallocate(float* in, float* out,int slice);
void smallWorkerPitcher(float* in, float* out,int slice);
void smallWorkerGapper(float* in, float* out,int slice);
void smallWorkerReallocateMixed(float* in, float* out,int slice);
void smallWorkerFFT(float* in, float* out,int slice);
void smallWorkerGranny(float* in, float* out,int slice);
void smallWorkerSwap(float* in, float* out,int slice);


void bigWorker(int slice);
void setParams(int inStartSeed, int inNumSlices, bool inKeepFirst, int inBisyness,int inrenderType );
void Init(int size);  
int getSlice(int position);
int loadFile(char* file);

void GetWavInfo(char *fname) ;
void convertToFloat();
void InfoDraw(bool showdata);

int getLastError()
{
return lastError;
}

bool isAllocated()
{
return allocated;
}

void setWaveSize(long inSize)
{
	wavesize = inSize;
}

float getData(int side); 
float* getPointerToData(int side);
void setData(int side,int position,float data);
float getData(int side,float position);
void movedata();
bool checkIfSLice(float position);

char* getrendertypename()
{
return rendertypename;

}

int getLength()
{
	return wavesize;
}

 
 float* getLeft()
{
return OutWave;
}
float* getRight()
{
return OutWaveR;
}
 
float* InWave;
float* InWaveR;


float* OutWave;
float* OutWaveR;  	 

//int wavesize;
protected:
char rendertypename[52];
bool allocated;// = false;
 

 
short mono1_stereo2;
long samplingfreq;
short bitlength;
short bitlength_1_2;
long totalbytes;
long bytesinfile ;
short chbytes_per_sample;
short bytes_per_sample;
long bytes_per_sec;
long wavesize;
int	dotn;





int OutCounter;
int OutCounterR;
int lastError;

int startSeed;
int numSlices;
bool keepFirst;
int bisyness;
int renderType;

#define pHalf  1
#define pDoub  2
#define ptrip  3

struct slice{
bool isBackward;
bool changedPosition;
int position;
bool changedPositionMIX;
int positionMIX;
bool changedPositionSilence;
bool isPitched;
int pitchType;
bool isDistorted;
int distortionTime;
bool isGapped;
int gappedType;
bool isFFT;
char note;
bool slide;
int slideamount;
bool isAttack;
bool isRelease;
int attack;
int release;
bool IsGranny ;
float grannySize ;
float grannyMove ;
float grannySpeed ;
float DistortionAmount;
float distortionThreshold;
bool isSwapped;
int swappedWith;

};
slice mySlice[512] ;

 } ;
#endif