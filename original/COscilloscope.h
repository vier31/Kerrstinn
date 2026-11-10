/*---------
header:
---------*/
#pragma once
#include "AEffEditor.h"
#include "vstgui.h"

#include "cfileselector.h"

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
class COscilloscope : public CControl
{
public:
	//CControl (CRect &size, CControlListener *listener, int tag, CBitmap *pBackground = 0); );
	COscilloscope (CRect &size, CControlListener *listener, int tag, float *inW,float* inWR,CBitmap *pBackground = 0); 

	/*COscilloscope (const CRect &size, CControlListener *listener, long tag, 
           CBitmap *background,
           CBitmap *handle, const CPoint &offset, float resolution);*/
	virtual ~COscilloscope ();

	virtual void draw (CDrawContext *pContext);
 	virtual void mouse (CDrawContext *pContext, CPoint &where, long button = -1);
	virtual bool onDrop (CDrawContext* context, CDragContainer* drag, const CPoint& where);
 
	virtual void setWavePointers(float *inW,float* inWR)
	{
		Wave = inW;
		WaveR = inWR;
	}
	
	virtual void setNumSlices(int in)
	{
		numslices = in;
	}

	char* getFileName( )
	{return thefilename;}
	
	virtual void setStuff(int inLe,int innumslices)
	{	

		length = inLe;
		numslices = innumslices;
	 
 	}
 
 
CLASS_METHODS(COscilloscope, CControl)
bool isDrawn;
protected:
 
char* thefilename;

//float Wave[(44100*10)];
//float WaveR[(44100*10)];
	 float* Wave;
 
	float* WaveR;
  int length;
  int numslices;
 
};