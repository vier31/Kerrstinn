
//using namespace std;
#include "engine.h"
#include "string.h"



const double midiScaler = (1. / 127.);

unsigned long engine::get_ulong(FILE *f)
{
    unsigned char s[4];
	unsigned res;
	
    if (fread(s, 4, 1, f) != 1) {
		res = 0;
    } else {
		res = s[0] + 256LU * (s[1] + 256LU * (s[2] + 256LU * s[3]));
	}
    return res;
}

unsigned engine::get_ushort(FILE *f)
{
    unsigned char s[2];
	unsigned res;
	
    if (fread(s, 2, 1, f) != 1) {
		res = 0;
    } else {
		res = s[0] + 256U * s[1];
	}
    return res;
}

unsigned engine::get_ubyte(FILE *f)
{
    unsigned char s[1];
	unsigned res;
	
    if (fread(s, 1, 1, f) != 1) {
		res = 0;
    } else {
		res = s[0];
	}
    return res; 
}

engine::engine()
{	
	allocated = false;
	OutCounter = 0;
	OutCounterR = 0;
	lastError = 0;
	sprintf(rendertypename,"ununit");
	for(int i=0;i<512;i++)
	{
		//dont forget to use keepFirst
		mySlice[i].isBackward=false;
		mySlice[i].changedPosition=false;
		mySlice[i].position=i;
		mySlice[i].isPitched=false;
		mySlice[i].isDistorted=false;
		mySlice[i].pitchType=0;
		mySlice[i].isGapped = false;
		mySlice[i].gappedType =0;
		mySlice[i].changedPositionMIX = false;
		mySlice[i].changedPositionMIX=i;
		mySlice[i].isFFT = false;
		mySlice[i].note =0;;
		mySlice[i].isAttack=false;
		mySlice[i].isRelease=false;
		mySlice[i].attack = 2;
		mySlice[i].release =2;
		mySlice[i].DistortionAmount=0;
		mySlice[i].distortionThreshold=0;
		mySlice[i].isSwapped = false;
		mySlice[i].swappedWith =0;
		
		
	}	
	
InWave =NULL;
InWaveR =NULL;
OutWave =NULL;
OutWaveR =NULL;

	
}

void engine::Init(int inSize)  
{  
	lastError = 0;
	// 	_SIZE =(size) ;
	// 	printf("Init %d\n",_SIZE);
	
	// 	my_Smp = (byte *) malloc( _SIZE*_SAMPLE*mono1_stereo2 );  
	// 	my_Samples = my_Smp;//+sizeof( Wavhead );  
	//if (InWave)
	//	free(InWave);


if(InWave == NULL)
	InWave = (float *)malloc(inSize* sizeof(float));
else
 InWave = (float*)realloc(InWave,inSize* sizeof(float));

if(InWaveR == NULL)
	InWaveR = (float *)malloc(inSize* sizeof(float));
else
 InWaveR = (float*)realloc(InWaveR,inSize* sizeof(float));


if(OutWave == NULL)
	OutWave = (float *)malloc(inSize* sizeof(float));
else
 OutWave = (float*)realloc(OutWave,inSize* sizeof(float));


if(OutWaveR == NULL)
	OutWaveR = (float *)malloc(inSize* sizeof(float));
else
 OutWaveR = (float*)realloc(OutWaveR,inSize* sizeof(float));



/*

	InWaveR = (float *)malloc(inSize* sizeof(float));
 	OutWave = (float *)malloc(inSize* sizeof(float));
	OutWaveR = (float *)malloc(inSize* sizeof(float));
*/
 

	memset(InWave,0,inSize);
	memset(InWaveR,0,inSize);
	memset(OutWave,0,inSize);
	memset(OutWaveR,0,inSize);
	
	//allocated = true;
//	wavesize =0;
}

engine::~engine() 
{
	//	printf("FreeMemory()\n");
	if( InWave ) {
		free(InWave);
		
	}
	if( InWaveR ) {
		free(InWaveR);
		
	}
	if( OutWave ) {
		free(OutWave);
		
	} 
	if( OutWaveR ) {
		free(OutWaveR);
		
	}
	
}



int engine::getrand(int max)
{
	float fslump = ((float)bisyness/100.0);	
	int toReturn =(int)((float)(rand()%(max+1))*(float)fslump );			
	//printf("toReturn %d\n",toReturn);
	return toReturn;
}
int engine::getrand2(int max)
{
	int toReturn =(int)((float)(rand()%(max+1)));			
	//printf("toReturn %d\n",toReturn);
	return toReturn;
}


int engine::getrandb()
{
 	if ( (rand()%100) < bisyness)
		return 1;
	else
		return 0;
}
int engine::getrandb2()
{
 	if ( (rand()%100) > bisyness)
		return 1;
	else
		return 0;
}


void engine::bigWorker(int slice)
{
	//printf("firstGo()\n");
	if (!allocated)
		return;
	int mySlice= slice-1;
	if(mySlice==-1)
		mySlice = numSlices;
	   
	//TODO: PErhaps add order here?
	
	smallWorkerBackwards(InWave,OutWave,mySlice);
	smallWorkerBackwards(InWaveR,OutWaveR,mySlice); 
	smallWorkerDistortion(OutWave,OutWave,mySlice);
	smallWorkerDistortion(OutWaveR,OutWaveR,mySlice); 
	smallWorkerPitcher(OutWave,OutWave,mySlice);
	smallWorkerPitcher(OutWaveR,OutWaveR,mySlice);
	smallWorkerGapper(OutWave,OutWave,mySlice);
	smallWorkerGapper(OutWaveR,OutWaveR,mySlice);
	smallWorkerReallocate(OutWave,OutWave,mySlice);
	smallWorkerReallocate(OutWaveR,OutWaveR,mySlice);
	smallWorkerReallocateMixed(OutWave,OutWave,mySlice);
	smallWorkerReallocateMixed(OutWaveR,OutWaveR,mySlice);
	smallWorkerFFT(OutWave,OutWave,mySlice);
	smallWorkerFFT(OutWaveR,OutWaveR,mySlice);

smallWorkerSwap(OutWave,OutWave,mySlice);
smallWorkerSwap(OutWaveR,OutWaveR,mySlice);

	smallWorkerAR(OutWave,OutWave,mySlice);
	smallWorkerAR(OutWaveR,OutWaveR,mySlice);
	smallWorkerGranny(OutWave,OutWave,mySlice);
	smallWorkerGranny(OutWaveR,OutWaveR,mySlice);
			
			
	
		

}

void engine::setParams(int inStartSeed, int inNumSlices, bool inKeepFirst, int inBisyness,int inrenderType)
{
	//printf("setParams()\n");
	startSeed = inStartSeed;
	numSlices = inNumSlices;
	keepFirst = inKeepFirst;	
	bisyness = inBisyness;
    renderType = inrenderType;
 	
	renderType=inrenderType;
sprintf(rendertypename,"uninit");

//--------------------------- Null everything
for(int i=0;i<numSlices;i++)	{
			//dont forget to use keepFirst
			mySlice[i].isBackward=false;
			mySlice[i].changedPositionMIX=false;
			mySlice[i].positionMIX=i;
			mySlice[i].changedPosition=false;
			mySlice[i].position=i;
			mySlice[i].changedPositionSilence = true;
			mySlice[i].isPitched=false;
			mySlice[i].isDistorted=false;
			mySlice[i].distortionTime=i;
			mySlice[i].isGapped = false;
			mySlice[i].gappedType =0;
			mySlice[i].isFFT = false;
			mySlice[i].note =0;;
			mySlice[i].slide= false;
			mySlice[i].slideamount=0;
			mySlice[i].isAttack=false;
			mySlice[i].isRelease=false;
			
			mySlice[i].attack = 2;
			mySlice[i].release =2;
			mySlice[i].IsGranny = false;
			mySlice[i].DistortionAmount=0;
			mySlice[i].distortionThreshold=0;
			mySlice[i].isSwapped = false;
			mySlice[i].swappedWith =0;


	
			}	

//--------------------------- end Null everything

	srand(startSeed);
	 
	if (renderType==1) //botox
	{
		sprintf(rendertypename,"Botox");
	if (!allocated)
		return;
		
		for(int i=0;i<numSlices;i++)
		{
			//dont forget to use keepFirst
			if (keepFirst && i==0)
			{
				mySlice[i].position=0;
 			}	
			else
			{
				if ( getrandb() && i!=0) // copy previous 
				{
					mySlice[i] = mySlice[i-1];
				}	
				else
				{
					mySlice[i].isBackward=getrandb();;
					mySlice[i].changedPosition=getrandb();
					mySlice[i].position=getrand(numSlices-i);
					mySlice[i].isPitched = getrandb();
					mySlice[i].pitchType=0;
					mySlice[i].isGapped = getrandb();
					mySlice[i].gappedType =19;;
				}	
			}	
		}	
	} 	 //botox
	else if (renderType==2) //clustercunt
	{
		
		sprintf(rendertypename,"General Cluster");
		if (!allocated)
		return;
	
		for(int i=0;i<numSlices;i++)
		{
			//dont forget to use keepFirst
			mySlice[i].changedPosition=getrandb();
			mySlice[i].position=(i%(numSlices/4))*(getrand2(i)/(numSlices/4)) ;
		}	
	} 	
	else if (renderType==3)
	{
		
		sprintf(rendertypename,"Retardler");
		if (!allocated)
		return;
	
		
		for(int i=0;i<numSlices;i++)
		{
			//dont forget to use keepFirst
			mySlice[i].isBackward=getrandb();
			mySlice[i].changedPosition=getrandb();
		}	
	} 	
 	else if (renderType==4)
	{
		
		sprintf(rendertypename,"Frikadish A");
		if (!allocated)
		return;
		
		for(int i=0;i<numSlices;i++)
		{
			//dont forget to use keepFirst
	 
			mySlice[i].isFFT = getrandb();
			//mySlice[i].isFFT = true;
			mySlice[i].note =getrand(100)+1;

			mySlice[i].slide= getrandb();
			mySlice[i].slideamount=getrand(2);

		}	
	} 	
 else if (renderType==5)
	{
		
		sprintf(rendertypename,"Freaky 4");
		if (!allocated)
		return;
		
		for(int i=0;i<numSlices;i++)
		{
			//dont forget to use keepFirst
			if ((i%4)==0)
			{
				mySlice[i].isBackward=getrandb();
				mySlice[i].isFFT = getrandb();
				//mySlice[i].isFFT = true;
				mySlice[i].note =getrand(100)+1;
				mySlice[i].slide= getrandb();
				mySlice[i].slideamount=getrand(2);
			}
		}	
	} 	

	else if (renderType==6)
	{
		
		sprintf(rendertypename,"Silence Sometimes");
		if (!allocated)
		return;
		int tok= getrand(numSlices/2)+1;
		for(int i=0;i<numSlices;i++)
		{
			//dont forget to use keepFirst
			mySlice[i].changedPositionMIX = getrandb();
			mySlice[i].positionMIX = (i%tok)*(numSlices/tok);
			 
			mySlice[i].changedPositionSilence = true; 
 

		}	
	} 	

	else if (renderType==7)
	{
		
		sprintf(rendertypename,"Magaluf");
		if (!allocated)
		return;
 

		for(int i=0;i<numSlices;i++)
		{
		//dont forget to use keepFirst
			if (keepFirst && i==0)
			{
				mySlice[i].position=0;
 			}	
			else
			{
				if ( getrandb() && i!=0) // copy previous 
				{
					mySlice[i] = mySlice[i-1];
				}	
				else
				{
					mySlice[i].isBackward=getrandb();;
					mySlice[i].changedPosition=getrandb();
					mySlice[i].position=getrand(numSlices-i);
					mySlice[i].isPitched = getrandb();
					mySlice[i].pitchType=0;
					mySlice[i].isGapped = getrandb();
					mySlice[i].gappedType =19;;
					mySlice[i].isFFT = getrandb();
					//mySlice[i].isFFT = true;
					mySlice[i].note =getrand(100)+1;
					mySlice[i].slide= getrandb();
					mySlice[i].slideamount=getrand(2);
				}	
			}	
		} 
	} 	
	else if (renderType==8)
	{
		
		sprintf(rendertypename,"Wiiooowww (AR)");
		if (!allocated)
		return;
	 
			for(int i=0;i<numSlices;i++)
		{
			//dont forget to use keepFirst
			mySlice[i].isAttack =getrandb();
			mySlice[i].isRelease=getrandb();
			mySlice[i].attack   =getrand(100);
			mySlice[i].release  =getrand(100);
	

		}	
	} 	

	//Circle teller
	else if (renderType==9)
	{
		
		sprintf(rendertypename,"Circle teller");
		if (!allocated)
		return;
		
		int arne = getrand(numSlices);
		int barne = getrand(numSlices);
		int carne = getrand(numSlices);
		int darne = getrand(numSlices);
	 
	 
		for(int i=0;i<numSlices;i++)
		{
			//dont forget to use keepFirst
			int myNumber = getrand(3);
					mySlice[i].changedPosition=getrandb();
					
					if(myNumber==0)
					{
						mySlice[i].position=arne;
					}
					if(myNumber==1)
					{
						mySlice[i].position=barne;
						mySlice[i].isGapped = getrandb();
						mySlice[i].gappedType =getrand(19);;
					}
					if(myNumber==2)
					{
						mySlice[i].position=carne;
						mySlice[i].isBackward=getrandb();
					}
					if(myNumber==3)
					{
						mySlice[i].isFFT = getrandb();
						mySlice[i].position=darne;
						mySlice[i].note =getrand(100)+1;
						mySlice[i].slide= getrandb();
						mySlice[i].slideamount=getrand(2);
					}
					

		}	
	} 	
 
	else if (renderType==10)
	{
		
		sprintf(rendertypename,"Passfelt");
		if (!allocated)
		return;
		
		int forevery = getrand(numSlices-1)+1;
			 
	 
		for(int i=0;i<numSlices;i++)
		{

					mySlice[i].changedPosition=getrandb();
					
					if( i%forevery ==1)
					{
						mySlice[i].isBackward=getrandb();
						mySlice[i].isFFT = getrandb();
						mySlice[i].note =getrand(100)+1;
						mySlice[i].slide= getrandb();
						mySlice[i].slideamount=getrand(2);
					}
					

		}	
	} 	

	else if (renderType==11)
	{
		
		sprintf(rendertypename,"Granny's favorite");
		if (!allocated)
		return;
		#define FORMULA ((getrand(50)+1)/100.f);
	
		for(int i=0;i<numSlices;i++)
		{

			mySlice[i].IsGranny = getrandb();
			mySlice[i].grannySize = FORMULA;
			mySlice[i].grannyMove =FORMULA;
			mySlice[i].grannySpeed = FORMULA;

		}	
	} 	

	
	else if (renderType==12)
	{
		
		sprintf(rendertypename,"R.A.W.MEAT");
//		sprintf(rendertypename,"Tuzticle");
	
		if (!allocated)
		return;
	    bool lastwas = false;
		int numGranns =0;
		int numcount=0;
		for(int i=0;i<numSlices;i++)
		{

			if (!lastwas)
			{
				lastwas = getrandb();
				mySlice[i].IsGranny = lastwas;
				mySlice[i].grannySize = FORMULA;
				mySlice[i].grannyMove =FORMULA;
				mySlice[i].grannySpeed = FORMULA;
				numGranns = getrand(numSlices-i);
		
			}
			else
			{
				mySlice[i].IsGranny = lastwas;
				mySlice[i].grannySize = mySlice[i-1].grannySize;
				mySlice[i].grannyMove =mySlice[i-1].grannyMove;
				mySlice[i].grannySpeed = mySlice[i-1].grannySpeed;
				numcount++;
				if (numcount==numGranns)
					lastwas=false;
	 
					
			}

		}	
	} 	
	else if (renderType==13)
	{
		
		sprintf(rendertypename,"Tuzticle");
		if (!allocated)
			return;
	    
		bool lastwas = false;
		int startGranny = getrand2(numSlices);
		for(int i=0;i<numSlices;i++)
		{
			if(lastwas)
			{
				mySlice[i].changedPositionMIX = true;
				mySlice[i].positionMIX = getrand(numSlices);
				mySlice[i].changedPositionSilence = false; 
			}
			if (startGranny == i)
			{
				lastwas = true;
				mySlice[i].changedPositionMIX = true;
				mySlice[i].positionMIX = getrand(numSlices);
				mySlice[i].changedPositionSilence = false; 
			}
		}
	} 

	else if (renderType==14)
	{
		
		sprintf(rendertypename,"ElgKebab");
		if (!allocated)
		return;
	    for(int i=0;i<numSlices;i++)
		{
				int banan = getrand(numSlices)+1;
			
				if ((i%banan)==0)
				{
					mySlice[i].isFFT = getrandb();
					mySlice[i].note =100-((getrand(8)+1)*2);
			 		mySlice[i].isAttack =getrandb();
					mySlice[i].isRelease=getrandb();
					mySlice[i].attack   =getrand(100);
					mySlice[i].release  =getrand(100);
					mySlice[i].changedPosition=getrandb();
					mySlice[i].position=getrand(numSlices);
					//		mySlice[i].position=(i%(numSlices/4))*(getrand2(i)/(numSlices/4)) ;
				}
		}
	}
	
	else if (renderType==15)
	{
		sprintf(rendertypename,"PiiTiiVii");
		if (!allocated)
		return;
	    for(int i=0;i<numSlices;i++)
		{
					mySlice[i].isDistorted=getrandb();
					mySlice[i].DistortionAmount=getrand(100)/100.0;
					mySlice[i].distortionThreshold=getrand(30)/100.0;;

		}
	}
	
	else if (renderType==16)
	{
		sprintf(rendertypename,"RythmSickness");
		if (!allocated)
		return;
	    for(int i=0;i<numSlices;i++)
		{

					mySlice[i].isSwapped = getrandb();;
					mySlice[i].swappedWith =getrand(numSlices);

		}
	}




}
float* engine::getPointerToData(int side)
{

if (side ==0)
	return OutWave;
if (side ==1)
	return OutWaveR;
	
return NULL;

}


float engine::getData(int side)
{
	
	float toreturn;
	OutCounter++;
	OutCounterR++;
	
	if (OutCounter > wavesize)
		OutCounter =0;
	if (OutCounterR > wavesize)
		OutCounterR =0;
	
	if (side ==0)
		toreturn = OutWave[OutCounter];
	else if (side ==1)
		toreturn =  OutWaveR[OutCounterR];	 
	
	return toreturn;
}

void engine::setData(int side,int position,float data)
{
	//add sanity check?
	/*if (side == 0)
		InWave[position]=data;
	if (side == 1)
		InWaveR[position]=data; */
}
float engine::getData(int side,float position)
{
	float toreturn;
	
	//add sanity check?
	
	if (side ==0)
		toreturn = OutWave[(int)(wavesize*position)];
	else if (side ==1)
		toreturn =  OutWaveR[(int)(wavesize*position)];	 
	
	return toreturn;
	
}

void engine::movedata()
{
	for(int i=0;i<wavesize;i++)
	{
		OutWave[i] = InWave[i];
		OutWaveR[i] = InWaveR[i];
		
	}
}
bool engine::checkIfSLice(float position)
{
	int mambo =wavesize/numSlices;
	if ( fmod((float)((wavesize*position)),mambo) == 0)
		return true;
	//if (position == mambo)
	//	return true;
	return false;
}

int engine::getSlice(int position)
{
	int mambo =wavesize/numSlices;
	return  position/mambo;
}
int engine::loadFile(char* file)
{
	allocated = false;
	GetWavInfo(file);
	InfoDraw(false);
	for(int ikki = 1; ikki< (numSlices+1); ikki++)
	{
		bigWorker(ikki);
	}
//	convertToFloat();
return 1;
}
void engine::GetWavInfo(char *fname) 
{
	unsigned long len;
	unsigned char s[10];
//	loaded = false;
	int i;
	long fmtbytes;
	
	FILE *ifp;
	lastError = 666;
//	printf("GetWavInfo()\n");
	
	ifp = fopen( fname, "rb" );
	
	if (ifp == NULL) 
		lastError = 1;

	
	if (ifp != NULL) 
	{
		fread(s, 4, 1, ifp); // "RIFF" 
		if(!(s[0]=='R' && s[1]=='I' && s[2]=='F' && s[3]=='F'))
		{
			lastError =2;
			return;
		}		
		bytesinfile = get_ulong(ifp); // total bytes in the file
		fread(s, 8, 1, ifp); // "WAVEfmt" 
		if(!(s[0]=='W' && s[1]=='A' && s[2]=='V' && s[3]=='E'))
			{
			lastError =2;
			return;
		}
		fmtbytes = get_ulong(ifp); // header size
		len = get_ushort(ifp); // 1: pcm
		mono1_stereo2 = get_ushort(ifp); // 1: mono, 2; stereo
		samplingfreq = get_ulong(ifp); // sampling frequency
		bytes_per_sec = get_ulong(ifp); // bytes per second
		chbytes_per_sample = get_ushort(ifp); // bytes per sample
		bitlength = get_ushort(ifp); // bit length
	//	_SAMPLE = bitlength/8; 
		for (i = 0; i < (fmtbytes-16)/2; i++ ) {
			len = get_ushort(ifp); // skip redundant header information
		}
		do{ // look for "data" chunk
			fread(s, 4, 1, ifp); // expect "data"
			if(((s[0] == 'd')&(s[1] == 'a')&(s[2] == 't')&(s[3] == 'a')) == 0){ // step over chunks unless it's "data" chunk
				len = get_ulong(ifp);
				fseek(ifp, len, SEEK_CUR);
			}else{
				break ;
			}
		} while(1);
		totalbytes = get_ulong(ifp); // total bytes of wav data
		
		bytes_per_sample = bitlength / 8;
		if(totalbytes == 0)
		{
			lastError =2;
			totalbytes =2;
			
		}
		if(mono1_stereo2 ==0)
		{
			lastError =2;
			mono1_stereo2 =2;
			
		}
		if(bytes_per_sample ==0)
		{
			lastError =2;
			bytes_per_sample =16;
			
		}
		
		
	 	
		  
		wavesize = totalbytes / (mono1_stereo2 * bytes_per_sample);
		
	/*	if (wavesize > )
		{
   				lastError = 3;
				return;
		}*/
		int i, m;
		long x;
		
		fpos_t pos;
		fgetpos (ifp,&pos);
//		printf("filepos %d\n",ftell(ifp));
	//	waveheedsize = ftell(ifp);
	//	printf("waveheedsize %d\n",waveheedsize);
	//	waveheed = (unsigned char *) malloc( sizeof(unsigned char )*waveheedsize);
 	//	fseek (ifp,0,SEEK_SET);
 	//	fread(waveheed, sizeof(unsigned char),waveheedsize, ifp); // 
 	//	printf("chbytes_per_sample  %d\n",chbytes_per_sample);
		//fsetpos(ifp,&pos);  
		
		Init(wavesize);
		
		if (mono1_stereo2 == 1) {
			for (i = 0; i < wavesize; i++) { // read mono wav data
				if (fread(s, bytes_per_sample, 1, ifp) != 1) break;
				if (bytes_per_sample == 2) x = (signed short)(s[0] + (256 * s[1]));
				else x = (signed short)(s[0]) - 128;
				InWave[i] = (double)(x);
			}
		} else {
			for (i = 0; i < wavesize * 2; i++) { // read stereo wav data
				m = i / 2;
				if (fread(s, bytes_per_sample, 1, ifp) != 1) break;
				
				if (bytes_per_sample == 2) //16 bitar
					x = (( signed short)(s[0] + (256 * s[1])));
				else x = (signed short)(s[0]) - 128; //8 bitar
				
				if(i%2==0) InWave[m] = (double)(x); //left
				else InWaveR[m] = (double)(x); //right
				
				
			}
		}
		
		fclose( ifp );
//		loaded = true;
		convertToFloat();
	}
	else
		lastError = 2;
	
	//	printf("error loading file\n");
		
}

void engine::convertToFloat()
{
	int i,m;
//	printf("convertToFloat()\n");
	//			Init(wavesize);
				
				if (mono1_stereo2 == 1) 
				{
//					printf("mono\n");
					for (i = 0; i < wavesize; i++) 
					{ // read mono wav data
						
						InWave[i] = (double)(InWave[i])/32767.0;
						InWaveR[i] = InWave[i];
			
						//	printf("%d,",i);
					}
				}
				else {
//					printf("stereo\n");
					for (i = 0; i < wavesize * 2; i++) 
					{ // read stereo wav data
						m = i / 2;
						if(i%2==0) InWave[m] = (double)(InWave[m])/32767.0; //left
						else InWaveR[m] = (double)(InWaveR[m])/32767.0; //right
					}
				}
				allocated = true;
}

void engine::InfoDraw(bool showdata)
{
//	printf("InfoDraw()\n");
/*	int i;
	if( mono1_stereo2 == 1 ) 
		printf("Mono \n");
	else 
		printf("Stereo \n");
	
	printf("Sampling Freq %d \n", samplingfreq);
 	printf("bitlength %d \n", bitlength);
  	printf("wavesize %d \n", wavesize);
    printf("Time Length %f \n", (double)wavesize/(double)samplingfreq);
    printf("bmp %f \n", 60.0/((((double)wavesize/(double)samplingfreq))/4.0) ); //tempo dubbelt altså.
	printf("bmp double %f \n", 60.0/((((double)wavesize/(double)samplingfreq)/2)/4.0) ); //tempo dubbelt altså.
	
	if (showdata)
	{
		if (mono1_stereo2 == 1) 
		{
			for(i=0;i<wavesize;i++)
				printf("%f,",InWave[i]);
		} 
		else
		{
			for(i=0;i<wavesize*2;i++)
			{
				if(i%2==0) printf("%f,",InWave[i/2]);
				else printf("%f \n",InWaveR[i/2]);
			} 
		} 
	}*/
}
