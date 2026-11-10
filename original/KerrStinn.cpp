/*
 *  aKerrStinn.cpp
 *  KerrStinn
 *
 *  Created by larsby on 5/27/05
 *  Copyright 2005 __MyCompanyName__. All rights reserved.
 *
 */
 
 //	#define numbars 1.0f


#include <iostream>
#include <string>
#include <cmath>
#include <exception>
#include <algorithm>
#include "Midi.h"
#include "engine.h"

#include "KerrStinnEditor.h"


using namespace std;



#ifndef __AKerrStinn__
#include "KerrStinn.h"
#endif


#include "AEffEditor.h"

using namespace std;
//-------------------------------------------------------------------------------------------------------

AudioEffect* createEffectInstance (audioMasterCallback audioMaster)
{
	DEBUG_OUT("createEffectInstance");
	return new AKerrStinn(audioMaster);
}

 

//-----------------------------------------------------------------------------
AKerrStinn::AKerrStinn (audioMasterCallback audioMaster)
: AudioEffectX (audioMaster, kNumPrograms, kNumParams)
{


	DEBUG_OUT("AKerrStinn AKerrStinn");
	
#ifdef Use_GUI
	_pEditor = new KerrStinnEditor(this);
	editor = _pEditor;
#endif
	//	setNumInputs(2);
	setNumInputs(2);
	setNumOutputs(2);
	setUniqueID ('frch');
	currplace=0;
//	isTainted=false;
 	isSynth(true);
	init ();		// flush buffer
					// check to see if the host supports sending tempo & time information to VST plugins
	_hostCanDoVstTimeInfo = canHostDo("sendVstTimeInfo")?true:false;
 //	needIdle();
	suspend();
		canProcessReplacing();
isPlaing=false;
oldpos =0;

 
//init programs
	for (int i=0;i < kNumPrograms;i++)
	{
		
		projectData.programs[i].pfRand =0.2;;
		projectData.programs[i].pfSlices=0.1212121212;
		projectData.programs[i].pfBisyness=0.5;;
		projectData.programs[i].pfGenerationMatrix=0.3;
		projectData.programs[i].pfNumBars=0.0;
		projectData.programs[i].pfOn=1.0;
		strcpy(projectData.programs[i].filename,"\0");
		strcpy (projectData.programs[i].name, "Init");
	}
//end init programs
	
	curProgram=0;

	if (projectData.programs)
		setProgram (curProgram);
	programsAreChunks();
	bParamsChanged = false;
	

	
}
VstInt32 AKerrStinn::getChunk (void** data, bool isPreset)
{
	DEBUG_OUT("AKerrStinn getChunk");

   if (isPreset)
    {
		*data = &projectData.programs[curProgram];
		return sizeof (AKerrStinnProgram);
    }
    *data = &projectData;
    return sizeof (KerrstinnProjectData);
}
VstInt32 AKerrStinn::setChunk (void* data, VstInt32 byteSize, bool isPreset)
{
	DEBUG_OUT("AKerrStinn setChunk");

  if (isPreset)
    {
        if (byteSize != sizeof (AKerrStinnProgram))
            return 0;
		projectData.programs[curProgram] = *(AKerrStinnProgram*)data;
		setProgram(curProgram);

    }
    else
    {
        if (byteSize != sizeof (KerrstinnProjectData))
            return 0;
       // projectData = *(KerrstinnProjectData*)data;
		memcpy((void*)(&projectData),data,byteSize);
	 	setProgram(curProgram);

	}
 
    return 1;
}
 
//-----------------------------------------------------------------------------------------

VstInt32 AKerrStinn::fxIdle()
{
	DEBUG_OUT("AKerrStinn fxIdle");

/* if (isTainted)
 {
	myEngine.movedata();
    myEngine.bigWorker();
    // isTainted=false;
}*/	
return 0;
}
bool AKerrStinn::init(void){
	DEBUG_OUT("AKerrStinn init");
	
#ifdef _DEBUG_LOG
    // quick and easy log file!
//    streambuf *psbuf;
  //  string log("KerrStinn");
 //   log.append(".log");
 //   _debugOut.open (log.c_str());
 //   psbuf = _debugOut.rdbuf();
 //   _outbuf = cout.rdbuf(psbuf);
//    _errbuf = cerr.rdbuf(psbuf);
 //   _debugActive = true;
 //   DEBUG_OUT("KerrStinn" << " log file");
#endif
    DEBUG_OUT("loading");
	DEBUG_OUT("AKerrStinn init 2");
	
    _iter =0;
	// todo : Remove this initialisation!!!
//		myEngine.GetWavInfo("fjord.wav") ;
	//	myEngine.InfoDraw(false);
	//myEngine.Init(44100*10);
mySamplePos =0;
	DEBUG_OUT("AKerrStinn init 3");

  	return true;
}



void
AKerrStinn::preProcess(void){
		DEBUG_OUT("AKerrStinn preProcess");

	// preparing Proccess
	VstTimeInfo * timeInfo = NULL;
	timeInfo = getTimeInfo(0xffff); //ALL
	
	if (_hostCanDoVstTimeInfo && timeInfo)
	{

		if (kVstTempoValid & timeInfo->flags)
		{
 
			myLocalWaveSize= (((timeInfo->sampleRate*60.0) /(timeInfo->tempo))*(double)(timeInfo->timeSigDenominator) );//66399
			mySamplePos = 	(((timeInfo->ppqPos-timeInfo->barStartPos))/(4.0))*((timeInfo->sampleRate*60.0) /timeInfo->tempo) *timeInfo->timeSigDenominator;

			_myBar = (int)(timeInfo->ppqPos/4.0)%(int)(numbars); //ahh finally this was a hard one to crack.			

			if (oldpos == 	mySamplePos)	
			isPlaing=false;
			else
			isPlaing=true;


			oldpos = mySamplePos;

			_bpm = (float)timeInfo->tempo;
			// you can get all other pertinent info from VstTimeInfo
		}

	}
    else
		if (_hostCanDoTempo){
			
 		}
    else
	{
		_bpm = 120.0f ;
	}
		if (mySamplePos < 0)
		mySamplePos=0;

	
}



VstInt32 AKerrStinn::canDo (char* text)
{
	DEBUG_OUT("AKerrStinn canDo");

	long returnCode = 0L;
	
    if (!strcmp (text, "receiveVstEvents"))
		returnCode = 1L;
    else if (!strcmp (text, "receiveVstMidiEvent"))
		returnCode = 1L;
    if (!strcmp(text, "receiveVstTimeInfo"))
		return 1;
	/*
	 if (!strcmp(text, "sendVstEvents"))
	 return 1;
	 if (!strcmp(text, "sendVstMidiEvent"))
	 return 1; 
	 */
	return (returnCode);
}

//------------------------------------------------------------------------
AKerrStinn::~AKerrStinn ()
{
//	if (programs)
//		delete[] programs;
	
 	DEBUG_OUT("AKerrStinn ~AKerrStinn");

		
#ifdef _DEBUG_LOG
    _debugOut.flush();
    _debugOut.close();
    cout.rdbuf(_outbuf);
    cerr.rdbuf(_errbuf);
#endif
}

//------------------------------------------------------------------------
void AKerrStinn::setProgram (VstInt32 program)
{
//programs[i].
//	AKerrStinnProgram* ap = &projectData.programs[program];
		DEBUG_OUT("AKerrStinn setProgram");

	curProgram = program;
	loadFile(projectData.programs[program].filename);
	setParameter(kRand ,projectData.programs[program].pfRand );
	setParameter(kSlices,projectData.programs[program].pfSlices);
	setParameter(kBisyness,projectData.programs[program].pfBisyness);
	setParameter(kGenerationMatrix,projectData.programs[program].pfGenerationMatrix);
	setParameter(kOn,projectData.programs[program].pfOn);
	setParameter(kNumBars,projectData.programs[program].pfNumBars);
}


//------------------------------------------------------------------------
void AKerrStinn::setProgramName (char *name)
{
	DEBUG_OUT("AKerrStinn setProgramName");

	strcpy (projectData.programs[curProgram].name, name);
}

//------------------------------------------------------------------------
void AKerrStinn::getProgramName (char *name)
{
	DEBUG_OUT("AKerrStinn getProgramName");

	if (!strcmp (projectData.programs[curProgram].name, "Init"))
		sprintf (name, "%s %d", projectData.programs[curProgram].name, (int)(curProgram + 1));
	else
		strcpy (name, projectData.programs[curProgram].name);
}

//-----------------------------------------------------------------------------------------
bool AKerrStinn::getProgramNameIndexed (VstInt32 category, VstInt32 index, char* text)
{
	DEBUG_OUT("AKerrStinn getProgramNameIndexed");

	if (index < kNumPrograms)
	{
		strcpy (text, projectData.programs[index].name);
		return true;
	}
	return false;
}

//------------------------------------------------------------------------
void AKerrStinn::suspend ()
{
	DEBUG_OUT("AKerrStinn suspend");

		AudioEffectX::suspend();
}
void AKerrStinn::resume ()
{
	DEBUG_OUT("AKerrStinn resume");

 	AudioEffectX::resume ();
}

//------------------------------------------------------------------------
void AKerrStinn::setParameter (VstInt32 index, float value)
{
	DEBUG_OUT("AKerrStinn setParameter");

//	AKerrStinnProgram* ap = &projectData.programs[curProgram];
	switch (index)
	{ 
		case kRand				:  projectData.programs[curProgram].pfRand= value; break;
		case kSlices			:  projectData.programs[curProgram].pfSlices= value; break;
		case kBisyness			:  projectData.programs[curProgram].pfBisyness= value; break;
		case kNumBars			:  projectData.programs[curProgram].pfNumBars= value; break;
		case kOn				:  projectData.programs[curProgram].pfOn= value; break;
		case kGenerationMatrix  :  projectData.programs[curProgram].pfGenerationMatrix= value; break;
 	}
	
	numbars = int(projectData.programs[curProgram].pfNumBars *7.99)+1;
	mySlices = (int)(((projectData.programs[curProgram].pfSlices*32)+1))*2;

	myEngine.setParams(projectData.programs[curProgram].pfRand*65535,mySlices,false,projectData.programs[curProgram].pfBisyness*100,(projectData.programs[curProgram].pfGenerationMatrix*(float)(DnumTypes-1)+0.99)+1);
	for (int i=1;i<((projectData.programs[curProgram].pfSlices*63)+1);i++)
	{
		myEngine.bigWorker(i);
	}
	bParamsChanged = true;

	_pEditor->setParameter(index, value);

	if(!isPlaing)
	if(_pEditor->bOpened )
	if(bParamsChanged )
	if (myEngine.isAllocated())
	{
	//		_pEditor->myOscilloscope->setStuff(myEngine.getLeft() ,myEngine.getRight() ,myEngine.getLength() , mySlices ); 
			_pEditor->myOscilloscope->setWavePointers(myEngine.getPointerToData(0),myEngine.getPointerToData(1));
			_pEditor->myOscilloscope->setStuff(myEngine.getLength() , mySlices); 
			_pEditor->myOscilloscope->setDirty(); 
			_pEditor->setFileChanged(false);
			bParamsChanged  = false;
	 }

}

//------------------------------------------------------------------------
float AKerrStinn::getParameter (VstInt32 index)
{
	DEBUG_OUT("AKerrStinn getParameter");

	float v = 0;
	
	switch (index)
	{
		case kRand				: v= projectData.programs[curProgram].pfRand;break;
		case kSlices			: v= projectData.programs[curProgram].pfSlices ;break;
		case kBisyness			: v= projectData.programs[curProgram].pfBisyness;break;
		case kNumBars			: v= projectData.programs[curProgram].pfNumBars;break;
		case kOn				: v= projectData.programs[curProgram].pfOn;break;
		case kGenerationMatrix	: v= projectData.programs[curProgram].pfGenerationMatrix;break;
  	}
	return v;
}

//------------------------------------------------------------------------
void AKerrStinn::getParameterName (VstInt32 index, char *label)
{
	DEBUG_OUT("AKerrStinn getParameterName");

	switch (index)
	{
		
		case kRand :  strcpy (label, "Rand Seed");break;
		case kSlices :  strcpy (label, "Slices");break;
		case kBisyness :  strcpy (label, "Bisyness");break;
		case kNumBars :  strcpy (label, "Number of bars");break;
		case kOn :  strcpy (label, "IsOn");break;
		case kGenerationMatrix :  strcpy (label, "GenMatr");break;
			
	}
	
}

//------------------------------------------------------------------------
void AKerrStinn::getParameterDisplay (VstInt32 index, char *text)
{
	DEBUG_OUT("AKerrStinn getParameterDisplay");

	switch (index)
	{
		case kRand : sprintf(text,"%d",int(projectData.programs[curProgram].pfRand *65535));break;
		case kSlices :sprintf(text,"%d",mySlices);break;
		case kBisyness : sprintf(text,"%d",int(projectData.programs[curProgram].pfBisyness *100));break;
		case kNumBars : sprintf(text,"%d",numbars);break;
		case kGenerationMatrix : 
			sprintf(text,"%d",int((projectData.programs[curProgram].pfGenerationMatrix*(float)(DnumTypes-1)+0.99)+1));break;
		case kOn : 
		{
		if (projectData.programs[curProgram].pfOn==0.0f)
		sprintf(text,"OFF");
		else
		sprintf(text,"ON");
		break;
		
		}
		default: sprintf(text,"%d",int(0.5 *65535));break;

	}
} 

//------------------------------------------------------------------------
void AKerrStinn::getParameterLabel (VstInt32 index, char *label)
{
	DEBUG_OUT("AKerrStinn getParameterLabel");

	switch (index)
	{
		
		case kRand : strcpy (label, "num");break;
		case kSlices : strcpy (label, "num");break;
		case kBisyness :  strcpy (label, "%");break;
		case kNumBars :  strcpy (label, "NR");break;
		case kOn :  strcpy (label, "");break;
		case kGenerationMatrix: strcpy	(label, myEngine.getrendertypename());break;

	}
	
	
}

//------------------------------------------------------------------------
bool AKerrStinn::getEffectName (char* name)
{
	DEBUG_OUT("AKerrStinn getEffectName");

	strcpy (name, "KerrStinn");
	return true;
}

//------------------------------------------------------------------------
bool AKerrStinn::getProductString (char* text)
{
	DEBUG_OUT("AKerrStinn getProductString");

	strcpy (text, "KerrStinn");
	return true;
}

//------------------------------------------------------------------------
bool AKerrStinn::getVendorString (char* text)
{
	DEBUG_OUT("AKerrStinn getVendorString");

	strcpy (text, "www.shuriken.se");
	return true;
}




//-----------------------------------------------------------------------------------------
void AKerrStinn::setSampleRate(float sampleRate) {
	DEBUG_OUT("AKerrStinn setSampleRate");

	this->sampleRate = sampleRate;

	// TODO: update time-depending parameters
}

//-----------------------------------------------------------------------------------------
void AKerrStinn::setBlockSize(VstInt32 blockSize) {
	DEBUG_OUT("AKerrStinn setBlockSize");

	this->blockSize = blockSize;

	// TODO: change blockSize depending objects
}

//-----------------------------------------------------------------------------------------
void AKerrStinn::setBlockSizeAndSampleRate(VstInt32 blockSize, float sampleRate) {
	DEBUG_OUT("AKerrStinn setBlockSizeAndSampleRate");

	this->sampleRate = sampleRate;
	this->blockSize = blockSize;

	// TODO: update time-depending parameters
	// TODO: change blockSize depending objects
}

//Only modify this if you want to do paralel Audio/Midi
//-----------------------------------------------------------------------------------------

void AKerrStinn::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames){
	DEBUG_OUT("AKerrStinn processReplacing");

	//takes care of VstTimeInfo and such
	preProcess();
	float* out1 = outputs[0];
	float* out2 = outputs[1];

	float* in1 = inputs[0];
	float* in2 = inputs[1];

float inpos;

	if(myEngine.isAllocated() ==false ) 
	{
		for(int i=0; i<sampleFrames; ++i)
		{
		out1[i] = in1[i];
		out2[i] = in2[i];
		}
 
	}
	else if (isPlaing == false)
	{
		for(int i=0; i<sampleFrames; ++i)
		{
		out1[i] = 0.0f;
		out2[i] = 0.0f;
		}	
	}
	else if (projectData.programs[curProgram].pfOn == false)
	{
		for(int i=0; i<sampleFrames; ++i)
		{
		out1[i] = 0.0f;
		out2[i] = 0.0f;
		}	
	}
	
	else
	{
		for(int i=0; i<sampleFrames; ++i)
		{
	
			inpos= float(((float)(mySamplePos)+(float)(i))/(float)((myLocalWaveSize+sampleFrames)));
			
			inpos = inpos/numbars;
			
			inpos+=(( 1.0/numbars)*(_myBar));
			//inpos+=0.5;
			if (inpos>1.0)
			{
				inpos=0;

			}
			
			out1[i]=myEngine.getData(0,inpos);
			out2[i]=myEngine.getData(1,inpos  );

		}
	} 
 	
    _iter++;

 
 

    if(myEngine.isAllocated() )
	{
	
	if(_pEditor->myOscilloscope != NULL && _pEditor->bOpened )
	{
		
	if (bParamsChanged)
	{
	//	_pEditor->myOscilloscope->setStuff(myEngine.getLeft() ,myEngine.getRight() ,myEngine.getLength() , mySlices);
		_pEditor->myOscilloscope->setWavePointers(myEngine.getPointerToData(0),myEngine.getPointerToData(1));
		_pEditor->myOscilloscope->setStuff(myEngine.getLength() , mySlices); 
		_pEditor->setFileChanged(false);
		bParamsChanged  = false;
		_pEditor->myOscilloscope->setDirty();
//		myEditor->update();
 
	 }
	   if((rand()%100)==2 && myEngine.getLength()!=0 )
	 	{
		
	//	_pEditor->myOscilloscope->setStuff(myEngine.getLeft() ,myEngine.getRight() ,myEngine.getLength() , mySlices);
			_pEditor->myOscilloscope->setWavePointers(myEngine.getPointerToData(0),myEngine.getPointerToData(1));
		_pEditor->myOscilloscope->setStuff(myEngine.getLength() , mySlices); 
		_pEditor->setFileChanged(false);
		bParamsChanged  = false;
		_pEditor->myOscilloscope->setDirty();
//		myEditor->update();
 
		}
	
	 }
	}
	
}


