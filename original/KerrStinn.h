#ifndef __AKerrStinn__
#define __AKerrStinn__

#include <vector>
#include <string>
#include <fstream>
#include "audioeffectx.h"
#include "engine.h"
#include "KerrStinnEditor.h"

enum
{
	// Global
	kNumPrograms = 16,

kRand=0,
kSlices,
kBisyness,
kGenerationMatrix,
kNumBars,
kOn,
kNumParams

};

#include <vector>
#include <string>
#include <fstream>
#define Use_GUI 1
//#define _DEBUG_LOG 1

typedef std::vector<VstMidiEvent> VstMidiEventVec;

#define MAX_EVENTS_PER_TIMESLICE 256
 

 

//audio i/o
#define PLUG_AUDIO_INPUTS   0
#define PLUG_AUDIO_OUTPUTS  2


//max 1!!! it cant have more than one midi stream in VST 2.x!!
#define PLUG_MIDI_INPUTS    1 //(only 1 or 0!)
#define PLUG_MIDI_OUTPUTS   1 //(only 1 or 0!)
//#define _DEBUG 0


#ifdef _DEBUG_LOG
#define DEBUG_OUT(x) (cout << x << endl)
#else
#define DEBUG_OUT(x)
#endif;


class AKerrStinn;

//------------------------------------------------------------------------
struct AKerrStinnProgram
{

	float pfRand;
float pfSlices;
float pfBisyness;
float pfGenerationMatrix;
float pfNumBars;
float pfOn;
char name[24];
char filename[2048];
};
struct KerrstinnProjectData
{
        long version;
        AKerrStinnProgram programs[kNumPrograms];
};
//static KerrstinnProjectData projectData = {1, 1, {0}};
//static KerrstinnProjectData projectData = {1, 1, {0}};


//------------------------------------------------------------------------
class AKerrStinn : public AudioEffectX
{
friend class KerrStinnEditor;
public:
	AKerrStinn (audioMasterCallback audioMaster);
	~AKerrStinn ();
	

	virtual void processReplacing (float **inputs, float **outputs, VstInt32 sampleFrames);
    

	virtual void setProgram (VstInt32 program);
	virtual void setProgramName (char *name);
	virtual void getProgramName (char *name);
	virtual bool getProgramNameIndexed (VstInt32 category, VstInt32 index, char* text);
	
	virtual void setParameter (VstInt32 index, float value);
	virtual float getParameter (VstInt32 index);
	virtual void getParameterLabel (VstInt32 index, char *label);
	virtual void getParameterDisplay (VstInt32 index, char *text);
	virtual void getParameterName (VstInt32 index, char *text);
    virtual VstInt32 canDo (char* text);
    //virtual long processEvents (VstEvents* ev);

	virtual void resume ();
	virtual void suspend();
	
	virtual VstInt32 fxIdle ();
	virtual void setSampleRate(float sampleRate);
	virtual void setBlockSize(VstInt32 blockSize);
	virtual void setBlockSizeAndSampleRate(VstInt32 blockSize, float sampleRate);

	virtual bool getEffectName (char* name);
	virtual bool getVendorString (char* text);
	virtual bool getProductString (char* text);
 
VstInt32 getChunk (void** data, bool isPreset);
VstInt32 setChunk (void* data, VstInt32 byteSize, bool isPreset);

int loadFile(char* file)
{ 
	int toreturn = 0;
	bParamsChanged = true;
	if (strlen(file)!=0)
	{
		 
		myEngine.loadFile(file);
		if(_pEditor->myOscilloscope != NULL && _pEditor->bOpened )
		strcpy (projectData.programs[curProgram].filename, file);
 
		}
		
	if(_pEditor->myOscilloscope && _pEditor->bOpened )
	if (myEngine.isAllocated() && bParamsChanged)
	{
		if(_pEditor->myOscilloscope  )
		{
		//	_pEditor->myOscilloscope->setStuff(myEngine.getLeft() ,myEngine.getRight() ,myEngine.getLength() , mySlices); 
		//	_pEditor->myOscilloscope->setStuff(myEngine.getLeft() ,myEngine.getRight() ,myEngine.getLength() , mySlices); 
		_pEditor->myOscilloscope->setWavePointers(myEngine.getPointerToData(0),myEngine.getPointerToData(1));
			_pEditor->myOscilloscope->setStuff(myEngine.getLength() , mySlices); 
			 
			
			_pEditor->myOscilloscope->setDirty(); 
			_pEditor->setFileChanged(false);
			bParamsChanged  = false;

		}
	//	myEditor->update();
	 }
	
	
	return toreturn;
}
	
	virtual VstPlugCategory getPlugCategory () { return kPlugCategEffect; }
bool init(void);

virtual void preProcess(void);
	engine myEngine;	 
// KerrstinnProjectData projectData = {1, 1, {0}};
 KerrstinnProjectData projectData;



protected:
 
	//AKerrStinnProgram programs[kNumPrograms];

long mySamplePos;
long myLocalWaveSize;

   bool _hostCanDoVstTimeInfo;
    bool _hostCanDoTempo;

 
//bool isTainted;

    float _bpm;
    int _myBar;
/*
float fRand;
float fSlices;
float fBisyness;
float fGenerationMatrix;
float fLoad;
float fNumBars;
float fOn;
*/
// KerrstinnProjectData* projectData;// = {1, 1, 0};
 
int currplace;

bool isPlaing;
int oldpos;
int mySlices;
int numbars;


#ifdef Use_GUI
		KerrStinnEditor* _pEditor;	// Reference to the Editor
#endif	

bool bParamsChanged;
  long _iter;
#ifdef _DEBUG_LOG
    std::streambuf *_outbuf;
    std::streambuf *_errbuf;
    std::ofstream _debugOut;
    bool          _debugActive;
#endif

};

#endif
