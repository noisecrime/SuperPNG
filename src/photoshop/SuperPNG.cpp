
#include "SuperPNG.h"

#include "SuperPNG_UI.h"

// glbals needed by a bunch of Photoshop SDK routines
#ifdef __PIWin__
HINSTANCE hDllInstance = NULL;
#endif

SPBasicSuite * sSPBasic = NULL;
SPPluginRef gPlugInRef = NULL;


static void DoAbout(AboutRecordPtr aboutP)
{
#ifdef __PIMac__
	const char * const plugHndl = "com.fnordware.Photoshop.SuperPNG";
	const void *hwnd = aboutP;	
#else
	const char * const plugHndl = NULL;
	HWND hwnd = (HWND)((PlatformData *)aboutP->platformData)->hwnd;
#endif

	SuperPNG_About(plugHndl, hwnd);
}

#pragma mark-

static void InitGlobals(Ptr globalPtr)
{	
	// create "globals" as a our struct global pointer so that any
	// macros work:
	GPtr globals = (GPtr)globalPtr;
		
	gPixelData = NULL;
	gRowBytes = 0;
	
	gInOptions.alpha			= PNG_ALPHA_TRANSPARENCY;
	gInOptions.mult				= FALSE;
	
	gOptions.compression 		= Z_BEST_COMPRESSION;
	gOptions.filter 			= PNG_ALL_FILTERS;
	gOptions.strategy			= Z_DEFAULT_STRATEGY;
	gOptions.interlace  		= PNG_INTERLACE_NONE;
	gOptions.metadata			= TRUE;
	gOptions.alpha				= PNG_ALPHA_TRANSPARENCY;
}

Handle myNewHandle(GPtr globals, const int32 inSize)
{
	if(gStuff->handleProcs != NULL && gStuff->handleProcs->numHandleProcs >= 6 && gStuff->handleProcs->newProc != NULL)
	{
		return gStuff->handleProcs->newProc(inSize);
	}
	else
	{
		return PINewHandle(inSize);
	}
}

Ptr myLockHandle(GPtr globals, Handle h)
{
	if(gStuff->handleProcs != NULL && gStuff->handleProcs->numHandleProcs >= 6 && gStuff->handleProcs->lockProc)
	{
		return gStuff->handleProcs->lockProc(h, TRUE);
	}
	else
	{
		return PILockHandle(h, TRUE);
	}
}

void myUnlockHandle(GPtr globals, Handle h)
{
	if(gStuff->handleProcs != NULL && gStuff->handleProcs->numHandleProcs >= 6 && gStuff->handleProcs->unlockProc)
	{
		gStuff->handleProcs->unlockProc(h);
	}
	else
	{
		PIUnlockHandle(h);
	}
}

int32 myGetHandleSize(GPtr globals, Handle h)
{
	if(gStuff->handleProcs != NULL && gStuff->handleProcs->numHandleProcs >= 6 && gStuff->handleProcs->getSizeProc)
	{
		return gStuff->handleProcs->getSizeProc(h);
	}
	else
	{
		return PIGetHandleSize(h);
	}
}

void mySetHandleSize(GPtr globals, Handle h, const int32 inSize)
{
	if(gStuff->handleProcs != NULL && gStuff->handleProcs->numHandleProcs >= 6 && gStuff->handleProcs->setSizeProc)
	{
		gStuff->handleProcs->setSizeProc(h, inSize);
	}
	else
	{
		PISetHandleSize(h, inSize);
	}
}

void myDisposeHandle(GPtr globals, Handle h)
{
	if(gStuff->handleProcs != NULL && gStuff->handleProcs->numHandleProcs >= 6 && gStuff->handleProcs->newProc != NULL)
	{
		gStuff->handleProcs->disposeProc(h);
	}
	else
	{
		PIDisposeHandle(h);
	}
}

OSErr myAllocateBuffer(GPtr globals, const int32 inSize, BufferID *outBufferID)
{
	*outBufferID = 0;
	
	if(gStuff->bufferProcs != NULL && gStuff->bufferProcs->numBufferProcs >= 4 && gStuff->bufferProcs->allocateProc != NULL)
		gResult = gStuff->bufferProcs->allocateProc(inSize, outBufferID);
	else
		gResult = memFullErr;

	return gResult;
}

Ptr myLockBuffer(GPtr globals, const BufferID inBufferID, Boolean inMoveHigh)
{
	if(gStuff->bufferProcs != NULL && gStuff->bufferProcs->numBufferProcs >= 4 && gStuff->bufferProcs->lockProc != NULL)
		return gStuff->bufferProcs->lockProc(inBufferID, inMoveHigh);
	else
		return NULL;
}

void myFreeBuffer(GPtr globals, const BufferID inBufferID)
{
	if(gStuff->bufferProcs != NULL && gStuff->bufferProcs->numBufferProcs >= 4 && gStuff->bufferProcs->freeProc != NULL)
		gStuff->bufferProcs->freeProc(inBufferID);
}

#pragma mark-

static void DoFilterFile(GPtr globals)
{
	SuperPNG_VerifyFile(globals);
}


// Additional parameter functions
//   These transfer settings to and from gStuff->revertInfo

#define SWAP_LONG(a)		((a >> 24) | ((a >> 8) & 0xff00) | ((a << 8) & 0xff0000) | (a << 24))

static void TwiddleOptions(SuperPNG_inData *options)
{
	// nothing to do
}

static void TwiddleOptions(SuperPNG_outData *options)
{
#ifndef __PIMacPPC__
	options->compression= SWAP_LONG(options->compression);
	options->filter		= SWAP_LONG(options->filter);
	options->strategy	= SWAP_LONG(options->strategy);
#endif
}

template <typename T>
static A_Boolean ReadParams(GPtr globals, T *options)
{
	A_Boolean found_revert = FALSE;
	
	if( gStuff->revertInfo != NULL )
	{
		if( myGetHandleSize(globals, gStuff->revertInfo) == sizeof(T) )
		{
			T *flat_options = (T *)myLockHandle(globals, gStuff->revertInfo);
			
			// flatten and copy
			TwiddleOptions(flat_options);
			
			memcpy((char*)options, (char*)flat_options, sizeof(T) );
			
			TwiddleOptions(flat_options);
			
			myUnlockHandle(globals, gStuff->revertInfo);
			
			found_revert = TRUE;
		}
	}
	
	return found_revert;
}

template <typename T>
static void WriteParams(GPtr globals, T *options)
{
	T *flat_options = NULL;
	
	if(gStuff->hostNewHdl != NULL) // we have the handle function
	{
		if(gStuff->revertInfo == NULL)
		{
			gStuff->revertInfo = myNewHandle(globals, sizeof(T) );
		}
		else
		{
			if(myGetHandleSize(globals, gStuff->revertInfo) != sizeof(T)  )
				mySetHandleSize(globals, gStuff->revertInfo, sizeof(T) );
		}
		
		flat_options = (T *)myLockHandle(globals, gStuff->revertInfo);
		
		// flatten and copy
		TwiddleOptions(flat_options);
		
		memcpy((char*)flat_options, (char*)options, sizeof(T) );	
		
		TwiddleOptions(flat_options);
			
		
		myUnlockHandle(globals, gStuff->revertInfo);
	}
}


static void DoReadPrepare(GPtr globals)
{
	gStuff->maxData = 0;
}


static void DoReadStart(GPtr globals)
{
	A_Boolean reverting = ReadParams(globals, &gInOptions);
	
	if(!reverting)
	{
		SuperPNG_InUI_Data params;
		
	#ifdef __PIMac__
		const char * const plugHndl = "com.fnordware.Photoshop.SuperPNG";
		const void *hwnd = globals;	
	#else
		const char *const plugHndl = NULL;
		HWND hwnd = (HWND)((PlatformData *)gStuff->platformData)->hwnd;
	#endif

		// SuperPNG_InUI is responsible for not popping a dialog if the user
		// didn't request it.  It still has to set the read settings from preferences though.
		bool result = SuperPNG_InUI(&params, plugHndl, hwnd);
		
		if(result)
		{
			gInOptions.alpha = params.alpha;
			gInOptions.mult = params.mult;
			
			WriteParams(globals, &gInOptions);
		}
		else
			gResult = userCanceledErr;
	}
	
	if(gResult == noErr)
		SuperPNG_FileInfo(globals);
}


static void DoReadContinue(GPtr globals)
{
	SuperPNG_ReadFile(globals);
}


static void DoReadFinish(GPtr globals)
{

}

#pragma mark-

static void DoOptionsPrepare(GPtr globals)
{
	gStuff->maxData = 0;
}


static DialogCompression ParamsToDialog(A_long compression, A_long filter, A_long strategy)
{
	if(compression == Z_NO_COMPRESSION)
	{
		return DIALOG_COMPRESSION_NONE;
	}
	else if(filter == PNG_FILTER_SUB)
	{
		return DIALOG_COMPRESSION_LOW;
	}
	else if(compression == Z_BEST_COMPRESSION)
	{
		return DIALOG_COMPRESSION_HIGH;
	}
	else
		return DIALOG_COMPRESSION_NORMAL;
}


static void DialogToParams(DialogCompression d_comp, A_long *compression, A_long *filter, A_long *strategy)
{
	switch(d_comp)
	{
		case DIALOG_COMPRESSION_NONE:
			*compression = Z_NO_COMPRESSION;
			*filter = PNG_NO_FILTERS;
			*strategy = Z_HUFFMAN_ONLY;
			break;
		
		case DIALOG_COMPRESSION_LOW:
			*compression = Z_DEFAULT_COMPRESSION;
			*filter = PNG_FILTER_SUB;
			*strategy = Z_HUFFMAN_ONLY;
			break;

		case DIALOG_COMPRESSION_NORMAL:
			*compression = Z_DEFAULT_COMPRESSION;
			*filter = PNG_ALL_FILTERS;
			*strategy = Z_DEFAULT_STRATEGY;
			break;
			
		case DIALOG_COMPRESSION_HIGH:
			*compression = Z_BEST_COMPRESSION;
			*filter = PNG_ALL_FILTERS;
			*strategy = Z_DEFAULT_STRATEGY;
			break;
	}
}


static void DoOptionsStart(GPtr globals)
{
	ReadParams(globals, &gOptions);
	
	if( ReadScriptParamsOnWrite(globals) )
	{
		bool have_transparency = false;
		const char *alpha_name = NULL;
		
		if(gStuff->hostSig == '8BIM')
			have_transparency = (gStuff->documentInfo && gStuff->documentInfo->mergedTransparency);
		else
			have_transparency = (gStuff->planes == 2 || gStuff->planes == 4);

			
		if(gStuff->documentInfo && gStuff->documentInfo->alphaChannels)
			alpha_name = gStuff->documentInfo->alphaChannels->name;
	
	
		SuperPNG_OutUI_Data params;
		
		params.compression	= ParamsToDialog(gOptions.compression, gOptions.filter, gOptions.strategy);
		params.interlace	= gOptions.interlace;
		params.metadata		= gOptions.metadata;
		params.alpha		= (DialogAlpha)gOptions.alpha;
	
	
	#ifdef __PIMac__
		const char * const plugHndl = "com.fnordware.Photoshop.SuperPNG";
		const void *hwnd = globals;	
	#else
		const char *const plugHndl = NULL;
		HWND hwnd = (HWND)((PlatformData *)gStuff->platformData)->hwnd;
	#endif

		bool result = SuperPNG_OutUI(&params, have_transparency, alpha_name, plugHndl, hwnd);
		
		
		if(result)
		{
			DialogToParams(params.compression, &gOptions.compression, &gOptions.filter, &gOptions.strategy);
			gOptions.interlace	= params.interlace;
			gOptions.metadata	= params.metadata;
			gOptions.alpha		= params.alpha;
			
			WriteParams(globals, &gOptions);
			WriteScriptParamsOnWrite(globals);
		}
		else
			gResult = userCanceledErr;

	}
}


static void DoOptionsContinue(GPtr globals)
{
	#ifdef __PIMWCW__
		#pragma unused (globals) // remove this when you write this routine
	#endif
}


static void DoOptionsFinish(GPtr globals)
{
	#ifdef __PIMWCW__
		#pragma unused (globals) // remove this when you write this routine
	#endif
}

#pragma mark-

static void DoEstimatePrepare(GPtr globals)
{
	gStuff->maxData = 0;

}


static void DoEstimateStart(GPtr globals)
{
	if(gStuff->HostSupports32BitCoordinates && gStuff->imageSize32.h && gStuff->imageSize32.v)
		gStuff->PluginUsing32BitCoordinates = TRUE;
		
	int width = (gStuff->PluginUsing32BitCoordinates ? gStuff->imageSize32.h : gStuff->imageSize.h);
	int height = (gStuff->PluginUsing32BitCoordinates ? gStuff->imageSize32.v : gStuff->imageSize.v);
	
	int64 dataBytes = (int64)width * (int64)height * (int64)gStuff->planes * (int64)(gStuff->depth >> 3);
					  
		
#ifndef MIN
#define MIN(A,B)			( (A) < (B) ? (A) : (B))
#endif
		
	gStuff->minDataBytes = MIN(dataBytes / 2, INT_MAX);
	gStuff->maxDataBytes = MIN(dataBytes, INT_MAX);
	
	gStuff->data = NULL;
}


static void DoEstimateContinue(GPtr globals)
{

}


static void DoEstimateFinish(GPtr globals)
{

}

#pragma mark-

static void DoWritePrepare(GPtr globals)
{
	gStuff->maxData = 0;
}


static void DoWriteStart(GPtr globals)
{
	ReadParams(globals, &gOptions);
	ReadScriptParamsOnWrite(globals);

	SuperPNG_WriteFile(globals);
}


static void DoWriteContinue(GPtr globals)
{

}


static void DoWriteFinish(GPtr globals)
{
	if(gStuff->hostSig != 'FXTC')
		WriteScriptParamsOnWrite(globals);
}


#pragma mark-


DLLExport MACPASCAL void PluginMain(const short selector,
						             FormatRecord *formatParamBlock,
						             entryData *data,
						             short *result)
{
	if (selector == formatSelectorAbout)
	{
		sSPBasic = ((AboutRecordPtr)formatParamBlock)->sSPBasic;

	#ifdef __PIWin__
		if(hDllInstance == NULL)
			hDllInstance = GetDLLInstance((SPPluginRef)((AboutRecordPtr)formatParamBlock)->plugInRef);
	#endif

		DoAbout((AboutRecordPtr)formatParamBlock);
	}
	else
	{
		sSPBasic = formatParamBlock->sSPBasic;  //thanks Tom
		
		gPlugInRef = (SPPluginRef)formatParamBlock->plugInRef;
		
	#ifdef __PIWin__
		if(hDllInstance == NULL)
			hDllInstance = GetDLLInstance((SPPluginRef)formatParamBlock->plugInRef);
	#endif

		
	 	static const FProc routineForSelector [] =
		{
			/* formatSelectorAbout  				DoAbout, */
			
			/* formatSelectorReadPrepare */			DoReadPrepare,
			/* formatSelectorReadStart */			DoReadStart,
			/* formatSelectorReadContinue */		DoReadContinue,
			/* formatSelectorReadFinish */			DoReadFinish,
			
			/* formatSelectorOptionsPrepare */		DoOptionsPrepare,
			/* formatSelectorOptionsStart */		DoOptionsStart,
			/* formatSelectorOptionsContinue */		DoOptionsContinue,
			/* formatSelectorOptionsFinish */		DoOptionsFinish,
			
			/* formatSelectorEstimatePrepare */		DoEstimatePrepare,
			/* formatSelectorEstimateStart */		DoEstimateStart,
			/* formatSelectorEstimateContinue */	DoEstimateContinue,
			/* formatSelectorEstimateFinish */		DoEstimateFinish,
			
			/* formatSelectorWritePrepare */		DoWritePrepare,
			/* formatSelectorWriteStart */			DoWriteStart,
			/* formatSelectorWriteContinue */		DoWriteContinue,
			/* formatSelectorWriteFinish */			DoWriteFinish,
			
			/* formatSelectorFilterFile */			DoFilterFile
		};
		
		Ptr globalPtr = NULL;		// Pointer for global structure
		GPtr globals = NULL; 		// actual globals

		
		if(formatParamBlock->handleProcs)
		{
			bool must_init = false;
			
			if(*data == NULL)
			{
				*data = (entryData)formatParamBlock->handleProcs->newProc(sizeof(Globals));
				
				must_init = true;
			}

			if(*data != NULL)
			{
				globalPtr = formatParamBlock->handleProcs->lockProc((Handle)*data, TRUE);
				
				if(must_init)
					InitGlobals(globalPtr);
			}
			else
			{
				*result = memFullErr;
				return;
			}

			globals = (GPtr)globalPtr;

			globals->result = result;
			globals->formatParamBlock = formatParamBlock;
		}
		else
		{
			// old lame way
			globalPtr = AllocateGlobals((allocateGlobalsPointer)result,
										 (allocateGlobalsPointer)formatParamBlock,
										 formatParamBlock->handleProcs,
										 sizeof(Globals),
						 				 data,
						 				 InitGlobals);

			if(globalPtr == NULL)
			{ // Something bad happened if we couldn't allocate our pointer.
			  // Fortunately, everything's already been cleaned up,
			  // so all we have to do is report an error.
			  
			  *result = memFullErr;
			  return;
			}
			
			// Get our "globals" variable assigned as a Global Pointer struct with the
			// data we've returned:
			globals = (GPtr)globalPtr;
		}


		// Dispatch selector
		if (selector > formatSelectorAbout && selector <= formatSelectorFilterFile)
			(routineForSelector[selector-1])(globals); // dispatch using jump table
		else
			gResult = formatBadParameters;
		
		
		if((Handle)*data != NULL)
		{
			if(formatParamBlock->handleProcs)
			{
				formatParamBlock->handleProcs->unlockProc((Handle)*data);
			}
			else
			{
				PIUnlockHandle((Handle)*data);
			}
		}
		
	
	} // about selector special		
}