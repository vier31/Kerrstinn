 
#ifndef __vstgui__
#include "vstgui.h"
#endif

#ifndef __COscilloscope__
#include "COscilloscope.h"
#endif

#include <math.h>

//------------------------------------------------------------------------
// CXYPad Surface
//------------------------------------------------------------------------
/*
COscilloscope::COscilloscope (const CRect &size, CControlListener *listener, long tag, CBitmap *background)
:	CControl (size, listener, tag, background) 
*/ 
COscilloscope::COscilloscope(CRect &size, CControlListener *listener, int tag, float *inW,float* inWR, CBitmap *pBackground)
:	CControl (size, listener, tag, pBackground)
{
 	//Wave = (float *)malloc((44100*10)* sizeof(float));
 	//Wave = float[(44100*10)];
	
//	WaveR = (float *)malloc((44100*10)* sizeof(float));
//	memset(Wave,0,(44100*10)*sizeof(float));
//	memset(WaveR,0,(44100*10)*sizeof(float));

	isDrawn = false;
	length=0;
	numslices = 0;
	Wave = inW;
	WaveR = inWR;
	 
	//setDefaultValue (0.f);
}

//------------------------------------------------------------------------
COscilloscope::~COscilloscope ()
{
 	
}

//------------------------------------------------------------------------
void COscilloscope::draw (CDrawContext *pContext)
{
CRect r;
CPoint c;
////#E2B028 och FF0101

//	CColor kMyLodLineColor= { 126, 76, 40, 127 };

	if (pBackground)
	{
		if (bTransparencyEnabled)
			pBackground->drawTransparent (pContext, size);
		else
			pBackground->draw (pContext, size);
	}
 
if (length<0)
return;
	float incstep = (float)length/(float)(pBackground->getWidth());
	 
//	pContext->setDrawMode(kXorMode);
	 
//pContext->setLineStyle(kLineOnOffDash);
//pContext->setLineWidth();
//pContext->setLineWidth(<#CCoord width#>);
#ifndef NOMINMAX
#ifndef max
#define max(a,b)            (((a) > (b)) ? (a) : (b))
#endif
#ifndef min
#define min(a,b)            (((a) < (b)) ? (a) : (b))
#endif
#endif  /* NOMINMAX */
 
pContext->setFrameColor(kBlackCColor); 

 
  pContext->moveTo  	(c(size.left+1 , size.bottom-(pBackground->getHeight()/2))	) ;
	for (int i=1 ;i<pBackground->getWidth()-1 ;i++)
	{
		//int pos ;
		float myMax =0;
		float myMin =0;
		for(int j = 0;j<incstep;j++)
		{
			myMax = max( (Wave[int(i*incstep)-j]), myMax);
			myMin = min( (Wave[int(i*incstep)-j]), myMin);
		}
		myMax*=0.90;
		myMin*=0.90;
	
		float center = size.bottom-(pBackground->getHeight()/2);
 	//dra linje frŒn ovan till under
		pContext->moveTo(  	c(size.left+i, (center+(myMax*center))) ) ;
		pContext->lineTo(	c(size.left+i, (center+(myMin*center))) ) ;	
	} 
 
 
 
	pContext->moveTo  	(c(size.left+1 , size.bottom-(pBackground->getHeight()/2))	) ;
	for (int i=1 ;i<pBackground->getWidth()-1 ;i++)
	{
		//int pos ;
		float myMax =0;
		float myMin =0;
		for(int j = 0;j<incstep;j++)
		{
			myMax = max( (WaveR[int(i*incstep)-j]), myMax);
			myMin = min( (WaveR[int(i*incstep)-j]), myMin);
		}
		myMax*=0.90;
		myMin*=0.90;
		float center = size.bottom-(pBackground->getHeight()/2);
 	//dra linje frŒn ovan till under
		pContext->moveTo(  	c(size.left+i, (center+(myMax*center))) ) ;
		pContext->lineTo(	c(size.left+i, (center+(myMin*center))) ) ;	
	} 
 
	pContext->setFrameColor(kWhiteCColor); 
	for (int i=1;i< numslices;i++)
	{
		float fposGreen = (float)((float)pBackground->getWidth()/(float)numslices);
		fposGreen = fposGreen* i;
		pContext->moveTo(  	c(size.left+fposGreen , size.top+10	)) ;
		pContext->lineTo(	c(size.left+fposGreen,  size.top+pBackground->getHeight()-10)) ;
	}
  isDrawn=true;
	setDirty (false);
}

bool COscilloscope::onDrop(CDrawContext* context, CDragContainer* drag, const CPoint& where)
{
	long size,type;
	char* mytext= 	(char*)drag->first(size,type);
			strcpy(thefilename,mytext);
	beginEdit ();
	
		listener->valueChanged (context, this);
	
		// end of edit parameter
		endEdit ();
	return false;

}

 
//------------------------------------------------------------------------
void COscilloscope::mouse (CDrawContext *pContext, CPoint &where, long button)
{
	if (!bMouseEnabled)
		return;
				thefilename = "/Users/larsby/Documents/Mp3-renders/fubb.wav";

//...fill up vstFileSelect structure first
    VstFileType waveType ("Wave File", ".WAV", "wav", "wav", "audio/wav", "audio/x-wav");

  

VstFileSelect myVstFileSelect;

memset (&myVstFileSelect, 0, sizeof (VstFileSelect));
myVstFileSelect.returnPath = new char[1024];
myVstFileSelect.sizeReturnPath = 1023;

      myVstFileSelect.command = kVstFileLoad;
      myVstFileSelect.type = kVstFileType;
      strcpy (myVstFileSelect.title, "Test for open file selector");
      myVstFileSelect.nbFileTypes = 1;
      myVstFileSelect.fileTypes = &waveType;
      myVstFileSelect.returnPath = new char[1024]; // Deallocate this somewhere
      myVstFileSelect.initialPath = 0; 


CFileSelector * myFileSelect = new CFileSelector(NULL); //create the file selector
myFileSelect->run(&myVstFileSelect); //get the info from it
if (myFileSelect) 
{ //if it wasn't cancelled
#if MAC
FSRef tmpRef;
FSpMakeFSRef((FSSpec*)myVstFileSelect.returnPath,&tmpRef); //get a FSRef from the FSSpec
FSRefMakePath (&tmpRef, (UInt8 *)myVstFileSelect.returnPath, 1024);
//make a path from the ref
#endif

thefilename = myVstFileSelect.returnPath; //generic function to load from a path

 		
		beginEdit ();
		listener->valueChanged (pContext, this);
		// end of edit parameter
		endEdit ();
 
}


 
}

  