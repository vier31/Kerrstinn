 
#ifndef __vstgui__
#include "vstgui.h"
#endif
#include "cfileselector.h"


#ifndef __CDropFile__
#include "DropFile.h"
#endif

//#ifndef __AEffect__
//#include "AEffect.h"
//#endif

#ifdef __APPLE_CC__
#include <Carbon/Carbon.h>
#else
//#include <Carbon.h>
#endif

#include <math.h>

//------------------------------------------------------------------------
// CXYPad Surface
//------------------------------------------------------------------------
/*
CDropFile::CDropFile (const CRect &size, CControlListener *listener, long tag, CBitmap *background)
:	CControl (size, listener, tag, background) 
*/ 
CDropFile::CDropFile(CRect &size, CControlListener *listener, int tag, CBitmap *pBackground)
:	CControl (size, listener, tag, pBackground)
{
 
	thefilename = new char[1024];
	//setDefaultValue (0.f);
	
 
}

//------------------------------------------------------------------------
CDropFile::~CDropFile ()
{
 
}

//------------------------------------------------------------------------
void CDropFile::draw (CDrawContext *pContext)
{
	if (pBackground)
	{
		if (bTransparencyEnabled)
			pBackground->drawTransparent (pContext, size);
		else
			pBackground->draw (pContext, size);
	}
 
	setDirty (false);
}

bool CDropFile::onDrop(CDrawContext* context, CDragContainer* drag, const CPoint& where)
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
void CDropFile::mouse (CDrawContext *pContext, CPoint &where, long button)
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

