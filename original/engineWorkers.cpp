
//using namespace std;
#include "engine.h"
#include "string.h"
 

void engine::smallWorkerAR(float* in, float* out,int slice)
{
	
	int i;	
	float mambo =((float)wavesize/(float)numSlices);
	
	
	int first=(slice)*mambo;
	int second=(slice+1)*mambo;

		for(i=0;i<mambo;i++)
		{
			out[i+first] = in[i+first] ;  
		}
	
	
	if (mySlice[slice].isAttack)
	{	
	//attack
		float attacklength = (mySlice[slice].attack / 100.0f);
		float myAttack	= 0.0 ;
		float myAttackChanger = 1.0 /(mambo*attacklength) ;

		for(i=0;i<mambo*attacklength;i++)
		{ 
			in[i+first] = in[i+first]*myAttack ;  
			myAttack += myAttackChanger;
		}	
	}
	//release
	if (mySlice[slice].isRelease)
	{	
		float releaselength = (mySlice[slice].release / 100.0f);
		float myRelease	= 0.0 ;
		float myReleaseChanger = 1.0/ (mambo*releaselength) ;
	
		for(i=0;i<mambo*releaselength;i++)
		{ 
			out[second-i] = out[second-i]*myRelease ;  
			myRelease += myReleaseChanger;
		}

	}	
	
	
	
			
}

void engine::smallWorkerBackwards(float* in, float* out,int slice)
{
	
	int i;	
	float mambo =((float)wavesize/(float)numSlices);
	
	
	int first=(slice)*mambo;
	int second=(slice+1)*mambo;
	
	if (mySlice[slice].isBackward)
	{	
		
		for(i=0;i<mambo;i++)
		{ 
			out[i+first] = in[second-i] ;  
		}
	}	
	else
		for(i=0;i<mambo;i++)
		{
			out[i+first] = in[i+first] ;  
		}
			
}


void engine::smallWorkerDistortion(float* in, float* out,int slice)
{
	if (!mySlice[slice].isDistorted )
		return;
	
	float inMY;
	
	int i;	
	float mambo =((float)wavesize/(float)numSlices);
	//float threshold = 0.2;
	//float amount = 0.6;
	
	float k = 2*mySlice[slice].DistortionAmount/(1-mySlice[slice].DistortionAmount);
	for(i=slice*mambo;i<(slice+1)*mambo;i++)
	{
	inMY=(in[i]);
		if (inMY>mySlice[slice].distortionThreshold || inMY<-mySlice[slice].distortionThreshold)
		{
		inMY= fabs(fabs(fmod(inMY - mySlice[slice].distortionThreshold, mySlice[slice].distortionThreshold*4)) - mySlice[slice].distortionThreshold*2) - mySlice[slice].distortionThreshold;
		}
		inMY = (1+k)*in[i]/(1+k*abs((int)(inMY)));
		out[i] = inMY;
		
	if(out[i]>1.0)
			out[i]=1.0;
	else if(out[i]<-1.0)
			out[i]=-1.0;
			
	}
	
	
}



void engine::smallWorkerReallocate(float* in, float* out,int slice)
{
	
	if (!mySlice[slice].changedPosition )
		return;
	if (mySlice[slice].position >= numSlices)
		return;
	 
	int i;	
	int mambo =wavesize/numSlices;
	int first=slice*mambo;
	int second=(mySlice[slice].position)*mambo;
	
	
	for(i=0;i<mambo;i++)
	{ 
		out[i+first] = in[i+second] ;  
	}
	
}
void engine::smallWorkerReallocateMixed(float* in, float* out,int slice)
{
	
	if (!mySlice[slice].changedPositionMIX )
		return;
	if (mySlice[slice].positionMIX >= numSlices)
		return;

	int i;	
	int mambo =wavesize/numSlices;
	int first=slice*mambo;
	int second=(mySlice[slice].positionMIX)*mambo;
	
	
	for(i=mambo;i>0;i--)
	{ 
		out[i+first] += in[i+second] ;  
		if (mySlice[slice].changedPositionSilence)
		{
			out[i+second] =0.0f;
		}
	}
	
	
}
void engine::smallWorkerPitcher(float* in, float* out,int slice)
{
  	int i;	
	int mambo =wavesize/numSlices;
	
	if (mySlice[slice].isPitched)
	{
		/*	if(mySlice[slice].pitchType==1)
	{
			int firsthalf=slice*mambo;
			for(i=0;i<mambo;i++)
			{
				out[i+firsthalf] = (in[((i/4)+firsthalf)]) ;
			}
	}
		if(mySlice[slice].pitchType==2)
	{
			int firsthalf=slice*mambo;
			for(i=0;i<mambo;i++)
			{
				out[i+firsthalf] =(in[((i/2)+firsthalf)]) ; 
			}
	}*/
		if(mySlice[slice].pitchType==1)
		{
			int first=slice*mambo;
			int second=first+(mambo/2);
			
			for(i=0;i<mambo/2;i++)
			{
				out[i+first] = (in[((i*2)+first)]) ;  
				out[(i)+second] = (in[((i*2)+first)]) ;  
			}
		}
		
		else if(mySlice[slice].pitchType==2)
		{
			int first=slice*mambo;
			int second =first+( mambo/3);
			int third = first+((mambo/3)*2);
			for(i=0;i<mambo/3;i++)
			{
				out[i+first] = (in[((i*3)+first)]) ;  
				out[(i)+second] = (in[((i*3)+first)]) ;  
				out[(i)+third] = (in[((i*3)+first)]) ;  
				
				
			}
		}		
		else if(mySlice[slice].pitchType==3)
		{
			int first=slice*mambo;
			int second =first+( mambo/4);
			int third = first+((mambo/4)*2);
			int forth = first+((mambo/4)*3);
			
			for(i=0;i<mambo/4;i++)
			{
				out[i+first] = (in[((i*4)+first)]) ;  
				out[(i)+second] = (in[((i*4)+first)]) ;  
				out[(i)+third] = (in[((i*4)+first)]) ;  
				out[(i)+forth] = (in[((i*4)+first)]) ;  
				
				
			}
		}			
	}
	
}

void engine::smallWorkerGapper(float* in, float* out,int slice)
{
	if (!mySlice[slice].isGapped)
		return;
  	int i;	
	int mambo =wavesize/numSlices;
	
 			
	if(mySlice[slice].gappedType==1)
	{
		int first=slice*mambo;
		
		for(i=0;i<mambo;i++)
		{
			out[i+first] = 0.0f;  
			
		}
	}			
	else if(mySlice[slice].gappedType==2)
	{
		int first=slice*mambo;
		for(i=0;i<mambo/2;i++)
		{
			out[i+first] = 0.0f;  
			
		}
	}
	
	else if(mySlice[slice].gappedType==3)
	{
		int first=slice*mambo;
		int second=first+(mambo/2);
		
		for(i=0;i<mambo/2;i++)
		{
			out[(i)+second] = 0.0f; 
		}
	}
	
	else if(mySlice[slice].gappedType==4)
	{
		int first=slice*mambo;
		for(i=0;i<mambo/3;i++)
		{
			out[i+first] = 0.0f ;  
			
			
		}
	}		
	else if(mySlice[slice].gappedType==5)
	{
		int first=slice*mambo;
		int second =first+( mambo/3);
		for(i=0;i<mambo/3;i++)
		{
			out[(i)+second] =0.0f ;  
		}
	}		
	else if(mySlice[slice].gappedType==6)
	{
		int first=slice*mambo;
		int third = first+((mambo/3)*2);
		for(i=0;i<mambo/3;i++)
		{
			out[(i)+third] =0.0f ;  
		}
	}
	else if(mySlice[slice].gappedType==7)
	{
		int first=slice*mambo;
		int second =first+( mambo/3);
		for(i=0;i<mambo/3;i++)
		{
			out[i+first] = 0.0f ;  
			out[(i)+second] = 0.0f;  
		}
	}		
	else if(mySlice[slice].gappedType==8)
	{
		int first=slice*mambo;
		int third = first+((mambo/3)*2);
		for(i=0;i<mambo/3;i++)
		{
			out[i+first] = 0.0f;
			out[(i)+third] = 0.0f;  
			
			
		}
	}		
	else if(mySlice[slice].gappedType==9)
	{
		int first=slice*mambo;
		int second =first+( mambo/3);
		for(i=0;i<mambo/3;i++)
		{
			out[(i)+second] = 0.0f  ;
		}
	}		
	
	
	else if(mySlice[slice].gappedType==10)
	{
		int first=slice*mambo;
		
		for(i=0;i<mambo/4;i++)
		{
			out[i+first] = 0.0f  ;
			
			
		}
	}
	
	else if(mySlice[slice].gappedType==11)
	{
		int first=slice*mambo;
		int second =first+( mambo/4);
		
		for(i=0;i<mambo/4;i++)
		{
			out[(i)+second] = 0.0f ;
			
			
		}
	}
	
	else if(mySlice[slice].gappedType==12)
	{
		int first=slice*mambo;
		int third = first+((mambo/4)*2);
		
		for(i=0;i<mambo/4;i++)
		{
			out[(i)+third] =0.0f;
			
			
		}				}
	
	else if(mySlice[slice].gappedType==13)
	{
		int first=slice*mambo;
		int forth = first+((mambo/4)*3);
		
		for(i=0;i<mambo/4;i++)
		{
			
			out[(i)+forth] =0.0f;
			
			
		}				}
	
	else if(mySlice[slice].gappedType==14)
	{
		int first=slice*mambo;
		int second =first+( mambo/4);
		
		for(i=0;i<mambo/4;i++)
		{
			out[i+first] = 0.0f ;
			out[(i)+second] = 0.0f;
			
			
		}				}
	
	else if(mySlice[slice].gappedType==15)
	{
		int first=slice*mambo;
		int third = first+((mambo/4)*2);
		
		for(i=0;i<mambo/4;i++)
		{
			out[i+first] = 0.0f;
			out[(i)+third] = 0.0f ;
			
			
		}				}
	
	
	else if(mySlice[slice].gappedType==16)
	{
		int first=slice*mambo;
		int forth = first+((mambo/4)*3);
		
		for(i=0;i<mambo/4;i++)
		{
			out[i+first] = 0.0f  ;
			out[(i)+forth] = 0.0f ; 
			
			
		}				}
	
	else if(mySlice[slice].gappedType==17)
	{
		int first=slice*mambo;
		int second =first+( mambo/4);
		int third = first+((mambo/4)*2);
		
		for(i=0;i<mambo/4;i++)
		{
			out[(i)+second] =0.0f;
			out[(i)+third] = 00.0f; 
			
			
		}				}
	
	else if(mySlice[slice].gappedType==18)
	{
		int first=slice*mambo;
		int second =first+( mambo/4);
		int forth = first+((mambo/4)*3);
		
		for(i=0;i<mambo/4;i++)
		{
			out[(i)+second] = 0.0f  ;
			out[(i)+forth] = 0.0f;
			
			
		}				}
	
	else if(mySlice[slice].gappedType==19)
	{
		int first=slice*mambo;
		int third = first+((mambo/4)*2);
		int forth = first+((mambo/4)*3);
		
		for(i=0;i<mambo/4;i++)
		{
			out[(i)+third] = 0.0f ;
			out[(i)+forth] = 0.0f;
			
			
		}				}
	
	else if(mySlice[slice].gappedType==20)
	{
		int first=slice*mambo;
		int second =first+( mambo/4);
		int third = first+((mambo/4)*2);
		int forth = first+((mambo/4)*3);
		
		for(i=0;i<mambo/4;i++)
		{
			out[(i)+second] =0.0f  ;
			out[(i)+third] = 0.0f;
			out[(i)+forth] =0.0f;
			
			
		}				}
	
	else if(mySlice[slice].gappedType==21)
	{
		int first=slice*mambo;
		int third = first+((mambo/4)*2);
		int forth = first+((mambo/4)*3);
		
		for(i=0;i<mambo/4;i++)
		{
			out[i+first] = 0.0f;
			out[(i)+third] = 0.0f;  
			out[(i)+forth] = 0.0f;					 
			
		}				}
	
	else if(mySlice[slice].gappedType==22)
	{
		int first=slice*mambo;
		int second =first+( mambo/4);
		int forth = first+((mambo/4)*3);
		
		for(i=0;i<mambo/4;i++)
		{
			out[i+first] = 0.0f ;  
			out[(i)+second] = 0.0f ;  
			out[(i)+forth] = 0.0f;  
			
			
		}				
	}
	
	else if(mySlice[slice].gappedType==23)
	{
		int first=slice*mambo;
		int second =first+( mambo/4);
		int third = first+((mambo/4)*2);
		
		for(i=0;i<mambo/4;i++)
		{
			out[i+first] = 0.0f ;  
			out[(i)+second] = 0.0f ;  
			out[(i)+third] = 0.0f ;  
			
			
		}				
		
		
		
		
    }					
}




void engine::smallWorkerFFT(float* in, float* out,int slice)
{
  	int i;	
	int mambo =wavesize/numSlices;

		if (mySlice[slice].isFFT)
		{
	
			int first=slice*mambo;
			for(i=0;i<mambo;i++)
			{
			 int destination = (first + (i% (int(mySlice[slice].note+1)*110))) + (mySlice[slice].slideamount+i);
          
			 if (destination > wavesize)
				 destination = wavesize;

			 if (destination < 1)
				 destination = 1;
			
			out[i+first] = in[destination] ;  
	 		}
	}
	
}

//-------------------------------------------------------------------
void engine::smallWorkerGranny(float* in, float* out,int slice)
{
	int i,ii;	
	int mambo =wavesize/numSlices;
		
		if (mySlice[slice].IsGranny )
		{	
			int first=slice*mambo;
			for(i=0;i<mambo;i++)
			{ 
				//vanlig men tuff
				//out[i +first]=in[( i% (int)(mySlice[ii].grannyMove*mambo)) +first ]; 
				//lite crazy men okelidokeli
				//out[i]=in[ i% ((int)(mySlice[ii].grannyMove*mambo)+(int)(mySlice[ii].grannySize*mambo)) ];
				//doktigt alien crazy
				out[i + first]=in[( (i% (int)(mySlice[slice].grannySize*mambo)) + (int)(mySlice[slice].grannySpeed*i) +(int)(mySlice[slice].grannyMove*mambo)) + first ];
			}
			
	}
}
void engine::smallWorkerSwap(float* in, float* out,int slice)
{
	int i,ii;	
	int mambo =wavesize/numSlices;
	 
		if (mySlice[slice].isSwapped )
		{	
			int first=slice*mambo;
			int second =mySlice[slice].swappedWith; 
			for(i=0;i<mambo;i++)
			{ 
			out[i + first] = in[i + first] ;
			out[i + second] =in[i + second] ;

			out[i + first] = out[i + first] + out[i + second];
			
			out[i + second] = out[i + first] - out[i + second];
			
		   out[i + first] = out[i + first] - out[i + second];
			
			
			
			
			
			//	out[i + first]=in[( (i% (int)(mySlice[slice].grannySize*mambo)) + (int)(mySlice[slice].grannySpeed*i) +(int)(mySlice[slice].grannyMove*mambo)) + first ];
			}
			
	}
}

//------------------------------------------------------------------ 