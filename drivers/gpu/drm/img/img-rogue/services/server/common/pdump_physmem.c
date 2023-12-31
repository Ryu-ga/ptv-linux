/*************************************************************************/ /*!
@File
@Title          Physmem PDump functions
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Common PDump (PMR specific) functions
@License        Dual MIT/GPLv2

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Alternatively, the contents of this file may be used under the terms of
the GNU General Public License Version 2 ("GPL") in which case the provisions
of GPL are applicable instead of those above.

If you wish to allow use of your version of this file only under the terms of
GPL, and not to allow others to use your version of this file under the terms
of the MIT license, indicate your decision by deleting the provisions above
and replace them with the notice and other provisions required by GPL as set
out in the file called "GPL-COPYING" included in this distribution. If you do
not delete the provisions above, a recipient may use your version of this file
under the terms of either the MIT license or GPL.

This License is also included in this distribution in the file called
"MIT-COPYING".

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */ /***************************************************************************/

#if defined(PDUMP)

#if defined(__linux__)
#include <linux/ctype.h>
#else
#include <ctype.h>
#endif

#include "img_types.h"
#include "img_defs.h"
#include "pvr_debug.h"
#include "pvrsrv_error.h"

#include "pdump_physmem.h"
#include "pdump_km.h"

#include "allocmem.h"
#include "osfunc.h"

#include "pvrsrv.h"

/* #define MAX_PDUMP_MMU_CONTEXTS	(10) */
/* static IMG_UINT32 guiPDumpMMUContextAvailabilityMask = (1<<MAX_PDUMP_MMU_CONTEXTS)-1; */


struct _PDUMP_PHYSMEM_INFO_T_
{
	IMG_CHAR aszSymbolicAddress[PHYSMEM_PDUMP_MEMSPNAME_SYMB_ADDR_MAX_LENGTH];
	IMG_UINT64 ui64Size;
	IMG_UINT32 ui32Align;
	IMG_UINT32 ui32SerialNum;
};

static IMG_BOOL _IsAllowedSym(IMG_CHAR sym)
{
	/* Numbers, Characters or '_' are allowed */
	if (isalnum(sym) || sym == '_')
		return IMG_TRUE;
	else
		return IMG_FALSE;
}

static IMG_BOOL _IsLowerCaseSym(IMG_CHAR sym)
{
	if (sym >= 'a' && sym <= 'z')
		return IMG_TRUE;
	else
		return IMG_FALSE;
}

void PDumpMakeStringValid(IMG_CHAR *pszString,
                          IMG_UINT32 ui32StrLen)
{
	IMG_UINT32 i;

	if (pszString)
	{
		for (i = 0; i < ui32StrLen; i++)
		{
			if (_IsAllowedSym(pszString[i]))
			{
				if (_IsLowerCaseSym(pszString[i]))
					pszString[i] = pszString[i]-32;
				else
					pszString[i] = pszString[i];
			}
			else
			{
				pszString[i] = '_';
			}
		}
	}
}

/**************************************************************************
 * Function Name  : PDumpGetSymbolicAddr
 * Inputs         :
 * Outputs        :
 * Returns        : PVRSRV_ERROR
 * Description    :
 **************************************************************************/
PVRSRV_ERROR PDumpGetSymbolicAddr(const IMG_HANDLE hPhysmemPDumpHandle,
                                  IMG_CHAR **ppszSymbolicAddress)
{
	PDUMP_PHYSMEM_INFO_T *psPDumpAllocationInfo;

	PVR_RETURN_IF_INVALID_PARAM(hPhysmemPDumpHandle);

	psPDumpAllocationInfo = (PDUMP_PHYSMEM_INFO_T *)hPhysmemPDumpHandle;
	*ppszSymbolicAddress = psPDumpAllocationInfo->aszSymbolicAddress;

	return PVRSRV_OK;
}

/*************************************************************************/ /*!
@Function       PDumpMalloc
@Description    Builds and writes an allocation command to pdump output. Whilst
                writing the thread is locked.

@Input          psDeviceNode        A pointer to a device node.
@Input          pszDevSpace         Device space string.
@Input          pszSymbolicAddress  Name of the allocation.
@Input          ui64Size            String size.
@Input          uiAlign             Command alignment.
@Input          bInitialise         Should the command initialise the allocation.
@Input          ui8InitValue        The value memory is initialised to.
@Input          phHandlePtr         PDump allocation handle.
@Input          ui32PDumpFlags      PDump allocation flags.

@Return         This function returns a PVRSRV_ERROR. PVRSRV_OK on success.
*/ /**************************************************************************/
PVRSRV_ERROR PDumpMalloc(PVRSRV_DEVICE_NODE *psDeviceNode,
                         const IMG_CHAR *pszDevSpace,
                         const IMG_CHAR *pszSymbolicAddress,
                         IMG_UINT64 ui64Size,
                         IMG_DEVMEM_ALIGN_T uiAlign,
                         IMG_BOOL bInitialise,
                         IMG_UINT8 ui8InitValue,
                         IMG_HANDLE *phHandlePtr,
                         IMG_UINT32 ui32PDumpFlags)
{
	PVRSRV_ERROR eError = PVRSRV_OK;

	PDUMP_LOCK(ui32PDumpFlags);
	eError = PDumpMallocUnlocked(psDeviceNode,
	                             pszDevSpace,
	                             pszSymbolicAddress,
	                             ui64Size,
	                             uiAlign,
	                             bInitialise,
	                             ui8InitValue,
	                             phHandlePtr,
	                             ui32PDumpFlags);
	PDUMP_UNLOCK(ui32PDumpFlags);
	return eError;
}

/*************************************************************************/ /*!
@Function       PDumpMallocUnlocked
@Description    Builds and writes an allocation command to pdump output. Whilst
                writing the thread remains unlocked.

@Input          psDeviceNode        A pointer to a device node.
@Input          pszDevSpace         Device space string.
@Input          pszSymbolicAddress  Name of the allocation.
@Input          ui64Size            String size.
@Input          uiAlign             Command alignment.
@Input          bInitialise         Should the command initialise the allocation.
@Input          ui8InitValue        The value memory is initialised to.
@Input          phHandlePtr         PDump allocation handle.
@Input          ui32PDumpFlags      PDump allocation flags.

@Return         This function returns a PVRSRV_ERROR. PVRSRV_OK on success.
*/ /**************************************************************************/
PVRSRV_ERROR PDumpMallocUnlocked(PVRSRV_DEVICE_NODE *psDeviceNode,
                                 const IMG_CHAR *pszDevSpace,
                                 const IMG_CHAR *pszSymbolicAddress,
                                 IMG_UINT64 ui64Size,
                                 IMG_DEVMEM_ALIGN_T uiAlign,
                                 IMG_BOOL bInitialise,
                                 IMG_UINT8 ui8InitValue,
                                 IMG_HANDLE *phHandlePtr,
                                 IMG_UINT32 ui32PDumpFlags)
{
	PVRSRV_ERROR eError = PVRSRV_OK;
	PDUMP_PHYSMEM_INFO_T *psPDumpAllocationInfo;

	PDUMP_GET_SCRIPT_STRING()

	psPDumpAllocationInfo = OSAllocMem(sizeof*psPDumpAllocationInfo);
	PVR_ASSERT(psPDumpAllocationInfo != NULL);

	/*
	 * PDUMP_CONT and PDUMP_PERSIST flag can't set together.
	 */
	if (ui32PDumpFlags == PDUMP_NONE)
	{
		/*
			Set continuous flag because there is no way of knowing beforehand which
			allocation is needed for playback of the captured range.
		*/
		ui32PDumpFlags |= PDUMP_FLAGS_CONTINUOUS;
	}

	ui32PDumpFlags |= PDUMP_FLAGS_BLKDATA;

	/*
		construct the symbolic address
	 */

	OSSNPrintf(psPDumpAllocationInfo->aszSymbolicAddress,
	           sizeof(psPDumpAllocationInfo->aszSymbolicAddress),
	           ":%s:%s",
	           pszDevSpace,
	           pszSymbolicAddress);

	/*
		Write to the MMU script stream indicating the memory allocation
	*/
	if (bInitialise)
	{
		eError = PDumpSNPrintf(hScript, ui32MaxLen, "CALLOC %s 0x%"IMG_UINT64_FMTSPECX" 0x%"IMG_UINT64_FMTSPECX" 0x%X\n",
		                          psPDumpAllocationInfo->aszSymbolicAddress,
		                          ui64Size,
		                          uiAlign,
		                          ui8InitValue);
	}
	else
	{
		eError = PDumpSNPrintf(hScript, ui32MaxLen, "MALLOC %s 0x%"IMG_UINT64_FMTSPECX" 0x%"IMG_UINT64_FMTSPECX"\n",
		                          psPDumpAllocationInfo->aszSymbolicAddress,
		                          ui64Size,
		                          uiAlign);
	}

	if (eError != PVRSRV_OK)
	{
		OSFreeMem(psPDumpAllocationInfo);
		goto _return;
	}

	PDumpWriteScript(psDeviceNode, hScript, ui32PDumpFlags);

	psPDumpAllocationInfo->ui64Size = ui64Size;
	psPDumpAllocationInfo->ui32Align = TRUNCATE_64BITS_TO_32BITS(uiAlign);

	*phHandlePtr = (IMG_HANDLE)psPDumpAllocationInfo;

_return:
	PDUMP_RELEASE_SCRIPT_STRING();
	return eError;
}

/*************************************************************************/ /*!
@Function       PDumpFree
@Description    Writes a FREE command for an allocation handle to the pdump out2
                stream. When writing to the output stream the thread is locked.

@Input          psDeviceNode                A pointer to a device node.
@Input          hPDumpAllocationInfoHandle  A PDump allocation handle.

@Return         This function returns a PVRSRV_ERROR. PVRSRV_OK on success.
*/ /**************************************************************************/
PVRSRV_ERROR PDumpFree(PVRSRV_DEVICE_NODE *psDeviceNode,
                       IMG_HANDLE hPDumpAllocationInfoHandle)
{
	PVRSRV_ERROR eError = PVRSRV_OK;

	PDUMP_LOCK(PDUMP_FLAGS_NONE);
	eError = PDumpFreeUnlocked(psDeviceNode, hPDumpAllocationInfoHandle);
	PDUMP_UNLOCK(PDUMP_FLAGS_NONE);

	return eError;
}

/*************************************************************************/ /*!
@Function       PDumpFreeUnlocked
@Description    Writes a FREE command for an allocation handle to the pdump
                out2 stream. When writing to the output stream the thread
                remains unlocked.

@Input          psDeviceNode                A pointer to a device node.
@Input          hPDumpAllocationInfoHandle  A PDump allocation handle.

@Return         This function returns a PVRSRV_ERROR. PVRSRV_OK on success.
*/ /**************************************************************************/
PVRSRV_ERROR PDumpFreeUnlocked(PVRSRV_DEVICE_NODE *psDeviceNode,
                               IMG_HANDLE hPDumpAllocationInfoHandle)
{
	PVRSRV_ERROR eError = PVRSRV_OK;
	PDUMP_PHYSMEM_INFO_T *psPDumpAllocationInfo;
	IMG_UINT32 ui32Flags = PDUMP_FLAGS_CONTINUOUS | PDUMP_FLAGS_BLKDATA;

	PDUMP_GET_SCRIPT_STRING()

	psPDumpAllocationInfo = (PDUMP_PHYSMEM_INFO_T *)hPDumpAllocationInfoHandle;

	/*
		Write to the MMU script stream indicating the memory free
	 */
	eError = PDumpSNPrintf(hScript, ui32MaxLen, "FREE %s\n",
	                          psPDumpAllocationInfo->aszSymbolicAddress);
	PVR_GOTO_IF_ERROR(eError, _return);

	PDumpWriteScript(psDeviceNode, hScript, ui32Flags);
	OSFreeMem(psPDumpAllocationInfo);

_return:
	PDUMP_RELEASE_SCRIPT_STRING();
	return eError;
}


/* Checking that the request is for the PDump-bound device
 * should be done before the following function is called
 */
PVRSRV_ERROR
PDumpPMRWRW32(PVRSRV_DEVICE_NODE *psDeviceNode,
              const IMG_CHAR *pszDevSpace,
              const IMG_CHAR *pszSymbolicName,
              IMG_DEVMEM_OFFSET_T uiOffset,
              IMG_UINT32 ui32Value,
              PDUMP_FLAGS_T uiPDumpFlags)
{
	PVRSRV_ERROR eError = PVRSRV_OK;

	PDUMP_GET_SCRIPT_STRING()

	eError = PDumpSNPrintf(hScript,
	                       ui32MaxLen,
	                       "WRW :%s:%s:" IMG_DEVMEM_OFFSET_FMTSPEC " "
	                       PMR_VALUE32_FMTSPEC,
	                       pszDevSpace,
	                       pszSymbolicName,
	                       uiOffset,
	                       ui32Value);
	PVR_GOTO_IF_ERROR(eError, _return);

	PDUMP_LOCK(uiPDumpFlags);
	PDumpWriteScript(psDeviceNode, hScript, uiPDumpFlags);
	PDUMP_UNLOCK(uiPDumpFlags);

_return:
	PDUMP_RELEASE_SCRIPT_STRING();
	return eError;
}

/* Checking that the request is for the PDump-bound device
 * should be done before the following function is called
 */
PVRSRV_ERROR
PDumpPMRWRW32InternalVarToMem(PVRSRV_DEVICE_NODE *psDeviceNode,
                              const IMG_CHAR *pszDevSpace,
                              const IMG_CHAR *pszSymbolicName,
                              IMG_DEVMEM_OFFSET_T uiOffset,
                              const IMG_CHAR *pszInternalVar,
                              PDUMP_FLAGS_T uiPDumpFlags)
{
	PVRSRV_ERROR eError = PVRSRV_OK;

	PDUMP_GET_SCRIPT_STRING()

	eError = PDumpSNPrintf(hScript,
	                          ui32MaxLen,
	                          "WRW :%s:%s:" IMG_DEVMEM_OFFSET_FMTSPEC " %s",
	                          pszDevSpace,
	                          pszSymbolicName,
	                          uiOffset,
	                          pszInternalVar);
	PVR_GOTO_IF_ERROR(eError, _return);

	PDUMP_LOCK(uiPDumpFlags);
	PDumpWriteScript(psDeviceNode, hScript, uiPDumpFlags);
	PDUMP_UNLOCK(uiPDumpFlags);

_return:
	PDUMP_RELEASE_SCRIPT_STRING();
	return eError;
}

/* Checking that the request is for the PDump-bound device
 * should be done before the following function is called
 */
PVRSRV_ERROR
PDumpPMRRDW32MemToInternalVar(PVRSRV_DEVICE_NODE *psDeviceNode,
                              const IMG_CHAR *pszInternalVar,
                              const IMG_CHAR *pszDevSpace,
                              const IMG_CHAR *pszSymbolicName,
                              IMG_DEVMEM_OFFSET_T uiOffset,
                              PDUMP_FLAGS_T uiPDumpFlags)
{
	PVRSRV_ERROR eError = PVRSRV_OK;

	PDUMP_GET_SCRIPT_STRING()

	eError = PDumpSNPrintf(hScript,
	                          ui32MaxLen,
	                          "RDW %s :%s:%s:" IMG_DEVMEM_OFFSET_FMTSPEC,
	                          pszInternalVar,
	                          pszDevSpace,
	                          pszSymbolicName,
	                          uiOffset);
	PVR_GOTO_IF_ERROR(eError, _return);

	PDUMP_LOCK(uiPDumpFlags);
	PDumpWriteScript(psDeviceNode, hScript, uiPDumpFlags);
	PDUMP_UNLOCK(uiPDumpFlags);

_return:
	PDUMP_RELEASE_SCRIPT_STRING();
	return eError;
}

/* Checking that the request is for the PDump-bound device
 * should be done before the following function is called
 */
PVRSRV_ERROR
PDumpPMRWRW64(PVRSRV_DEVICE_NODE *psDeviceNode,
              const IMG_CHAR *pszDevSpace,
              const IMG_CHAR *pszSymbolicName,
              IMG_DEVMEM_OFFSET_T uiOffset,
              IMG_UINT64 ui64Value,
              PDUMP_FLAGS_T uiPDumpFlags)
{
	PVRSRV_ERROR eError = PVRSRV_OK;

	PDUMP_GET_SCRIPT_STRING()

	eError = PDumpSNPrintf(hScript,
	                       ui32MaxLen,
	                       "WRW64 :%s:%s:" IMG_DEVMEM_OFFSET_FMTSPEC " "
	                       PMR_VALUE64_FMTSPEC,
	                       pszDevSpace,
	                       pszSymbolicName,
	                       uiOffset,
	                       ui64Value);
	PVR_GOTO_IF_ERROR(eError, _return);

	PDUMP_LOCK(uiPDumpFlags);
	PDumpWriteScript(psDeviceNode, hScript, uiPDumpFlags);
	PDUMP_UNLOCK(uiPDumpFlags);

_return:
	PDUMP_RELEASE_SCRIPT_STRING();
	return eError;
}

/* Checking that the request is for the PDump-bound device
 * should be done before the following function is called
 */
PVRSRV_ERROR
PDumpPMRWRW64InternalVarToMem(PVRSRV_DEVICE_NODE *psDeviceNode,
                              const IMG_CHAR *pszDevSpace,
                              const IMG_CHAR *pszSymbolicName,
                              IMG_DEVMEM_OFFSET_T uiOffset,
                              const IMG_CHAR *pszInternalVar,
                              PDUMP_FLAGS_T uiPDumpFlags)
{
	PVRSRV_ERROR eError = PVRSRV_OK;

	PDUMP_GET_SCRIPT_STRING()

	eError = PDumpSNPrintf(hScript,
	                          ui32MaxLen,
	                          "WRW64 :%s:%s:" IMG_DEVMEM_OFFSET_FMTSPEC " %s",
	                          pszDevSpace,
	                          pszSymbolicName,
	                          uiOffset,
	                          pszInternalVar);
	PVR_GOTO_IF_ERROR(eError, _return);

	PDUMP_LOCK(uiPDumpFlags);
	PDumpWriteScript(psDeviceNode, hScript, uiPDumpFlags);
	PDUMP_UNLOCK(uiPDumpFlags);

_return:
	PDUMP_RELEASE_SCRIPT_STRING();
	return eError;
}

/* Checking that the request is for the PDump-bound device
 * should be done before the following function is called
 */
PVRSRV_ERROR
PDumpPMRRDW64MemToInternalVar(PVRSRV_DEVICE_NODE *psDeviceNode,
                              const IMG_CHAR *pszInternalVar,
                              const IMG_CHAR *pszDevSpace,
                              const IMG_CHAR *pszSymbolicName,
                              IMG_DEVMEM_OFFSET_T uiOffset,
                              PDUMP_FLAGS_T uiPDumpFlags)
{
	PVRSRV_ERROR eError = PVRSRV_OK;

	PDUMP_GET_SCRIPT_STRING()

	eError = PDumpSNPrintf(hScript,
	                          ui32MaxLen,
	                          "RDW64 %s :%s:%s:" IMG_DEVMEM_OFFSET_FMTSPEC,
	                          pszInternalVar,
	                          pszDevSpace,
	                          pszSymbolicName,
	                          uiOffset);
	PVR_GOTO_IF_ERROR(eError, _return);

	PDUMP_LOCK(uiPDumpFlags);
	PDumpWriteScript(psDeviceNode, hScript, uiPDumpFlags);
	PDUMP_UNLOCK(uiPDumpFlags);

_return:
	PDUMP_RELEASE_SCRIPT_STRING();
	return eError;
}

/* Checking that the request is for the PDump-bound device
 * should be done before the following function is called
 */
PVRSRV_ERROR
PDumpPMRLDB(PVRSRV_DEVICE_NODE *psDeviceNode,
            const IMG_CHAR *pszDevSpace,
            const IMG_CHAR *pszSymbolicName,
            IMG_DEVMEM_OFFSET_T uiOffset,
            IMG_DEVMEM_SIZE_T uiSize,
            const IMG_CHAR *pszFilename,
            IMG_UINT32 uiFileOffset,
            PDUMP_FLAGS_T uiPDumpFlags)
{
	PVRSRV_ERROR eError = PVRSRV_OK;

	PDUMP_GET_SCRIPT_STRING()

	eError = PDumpSNPrintf(hScript,
	                          ui32MaxLen,
	                          "LDB :%s:%s:" IMG_DEVMEM_OFFSET_FMTSPEC " "
	                          IMG_DEVMEM_SIZE_FMTSPEC " "
	                          PDUMP_FILEOFFSET_FMTSPEC " %s\n",
	                          pszDevSpace,
	                          pszSymbolicName,
	                          uiOffset,
	                          uiSize,
	                          uiFileOffset,
	                          pszFilename);
	PVR_GOTO_IF_ERROR(eError, _return);

	PDUMP_LOCK(uiPDumpFlags);
	PDumpWriteScript(psDeviceNode, hScript, uiPDumpFlags);
	PDUMP_UNLOCK(uiPDumpFlags);

_return:
	PDUMP_RELEASE_SCRIPT_STRING();
	return eError;
}

/* Checking that the request is for the PDump-bound device
 * should be done before the following function is called
 */
PVRSRV_ERROR PDumpPMRSAB(PVRSRV_DEVICE_NODE *psDeviceNode,
                         const IMG_CHAR *pszDevSpace,
                         const IMG_CHAR *pszSymbolicName,
                         IMG_DEVMEM_OFFSET_T uiOffset,
                         IMG_DEVMEM_SIZE_T uiSize,
                         const IMG_CHAR *pszFileName,
                         IMG_UINT32 uiFileOffset)
{
	PVRSRV_ERROR eError = PVRSRV_OK;
	IMG_UINT32 uiPDumpFlags;

	PDUMP_GET_SCRIPT_STRING()

	uiPDumpFlags = 0;

	eError = PDumpSNPrintf(hScript,
	                          ui32MaxLen,
	                          "SAB :%s:%s:" IMG_DEVMEM_OFFSET_FMTSPEC " "
	                          IMG_DEVMEM_SIZE_FMTSPEC " "
	                          "0x%08X %s.bin\n",
	                          pszDevSpace,
	                          pszSymbolicName,
	                          uiOffset,
	                          uiSize,
	                          uiFileOffset,
	                          pszFileName);
	PVR_GOTO_IF_ERROR(eError, _return);

	PDUMP_LOCK(uiPDumpFlags);
	PDumpWriteScript(psDeviceNode, hScript, uiPDumpFlags);
	PDUMP_UNLOCK(uiPDumpFlags);

_return:
	PDUMP_RELEASE_SCRIPT_STRING();
	return eError;
}

/* Checking that the request is for the PDump-bound device
 * should be done before the following function is called
 */
PVRSRV_ERROR
PDumpPMRPOL(PVRSRV_DEVICE_NODE *psDeviceNode,
            const IMG_CHAR *pszMemspaceName,
            const IMG_CHAR *pszSymbolicName,
            IMG_DEVMEM_OFFSET_T uiOffset,
            IMG_UINT32 ui32Value,
            IMG_UINT32 ui32Mask,
            PDUMP_POLL_OPERATOR eOperator,
            IMG_UINT32 uiCount,
            IMG_UINT32 uiDelay,
            PDUMP_FLAGS_T uiPDumpFlags)
{
	PVRSRV_ERROR eError = PVRSRV_OK;

	PDUMP_GET_SCRIPT_STRING()

	eError = PDumpSNPrintf(hScript,
	                          ui32MaxLen,
	                          "POL :%s:%s:" IMG_DEVMEM_OFFSET_FMTSPEC " "
	                          "0x%08X 0x%08X %d %d %d\n",
	                          pszMemspaceName,
	                          pszSymbolicName,
	                          uiOffset,
	                          ui32Value,
	                          ui32Mask,
	                          eOperator,
	                          uiCount,
	                          uiDelay);
	PVR_GOTO_IF_ERROR(eError, _return);

	PDUMP_LOCK(uiPDumpFlags);
	PDumpWriteScript(psDeviceNode, hScript, uiPDumpFlags);
	PDUMP_UNLOCK(uiPDumpFlags);

_return:
	PDUMP_RELEASE_SCRIPT_STRING();
	return eError;
}

/* Checking that the request is for the PDump-bound device
 * should be done before the following function is called
 */
PVRSRV_ERROR
PDumpPMRCBP(PVRSRV_DEVICE_NODE *psDeviceNode,
            const IMG_CHAR *pszMemspaceName,
            const IMG_CHAR *pszSymbolicName,
            IMG_DEVMEM_OFFSET_T uiReadOffset,
            IMG_DEVMEM_OFFSET_T uiWriteOffset,
            IMG_DEVMEM_SIZE_T uiPacketSize,
            IMG_DEVMEM_SIZE_T uiBufferSize)
{
	PVRSRV_ERROR eError = PVRSRV_OK;
	PDUMP_FLAGS_T uiPDumpFlags = 0;

	PDUMP_GET_SCRIPT_STRING()

	eError = PDumpSNPrintf(hScript,
	                          ui32MaxLen,
	                          "CBP :%s:%s:" IMG_DEVMEM_OFFSET_FMTSPEC " "
	                          IMG_DEVMEM_OFFSET_FMTSPEC " " IMG_DEVMEM_SIZE_FMTSPEC " " IMG_DEVMEM_SIZE_FMTSPEC "\n",
	                          pszMemspaceName,
	                          pszSymbolicName,
	                          uiReadOffset,
	                          uiWriteOffset,
	                          uiPacketSize,
	                          uiBufferSize);

	PVR_GOTO_IF_ERROR(eError, _return);

	PDUMP_LOCK(uiPDumpFlags);
	PDumpWriteScript(psDeviceNode, hScript, uiPDumpFlags);
	PDUMP_UNLOCK(uiPDumpFlags);

_return:
	PDUMP_RELEASE_SCRIPT_STRING();
	return eError;
}

/* Checking that the request is for the PDump-bound device
 * should be done before the following function is called
 */
PVRSRV_ERROR
PDumpWriteParameterBlob(PVRSRV_DEVICE_NODE *psDeviceNode,
                        IMG_UINT8 *pcBuffer,
                        size_t uiNumBytes,
                        PDUMP_FLAGS_T uiPDumpFlags,
                        IMG_CHAR *pszFilenameOut,
                        size_t uiFilenameBufSz,
                        PDUMP_FILEOFFSET_T *puiOffsetOut)
{
	PVRSRV_ERROR eError = PVRSRV_ERROR_INVALID_PARAMS;
	PVR_UNREFERENCED_PARAMETER(uiFilenameBufSz);

	PVR_ASSERT(uiNumBytes > 0);

	eError = PDumpReady();
	if (eError != PVRSRV_OK)
	{
		/* Mask suspension from caller as this is terminal & logged */
		eError = (eError == PVRSRV_ERROR_PDUMP_NOT_ACTIVE) ?
				PVRSRV_ERROR_PDUMP_NOT_ALLOWED :
				eError;
		return eError;
	}

	PVR_ASSERT(uiFilenameBufSz <= PDUMP_PARAM_MAX_FILE_NAME);

	PDUMP_LOCK(uiPDumpFlags);

	eError = PDumpWriteParameter(psDeviceNode, pcBuffer, uiNumBytes,
	                             uiPDumpFlags, puiOffsetOut, pszFilenameOut);
	PDUMP_UNLOCK(uiPDumpFlags);

	if ((eError != PVRSRV_OK) && (eError != PVRSRV_ERROR_PDUMP_NOT_ALLOWED))
	{
		PVR_LOG_RETURN_IF_ERROR(eError, "PDumpWriteParameter");
	}
	/* else Write to parameter file Ok or Prevented under the flags or
	 * current state of the driver so skip further writes and let caller know.
	 */
	return eError;
}

#endif /* PDUMP */
