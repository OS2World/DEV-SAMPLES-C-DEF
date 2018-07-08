/*****************************************************************************

   NAME:          DEF1.C

   DESCRIPTION:   This module contains the logic to create and perform
                  lookups into the resident and non-resident name tables
                  for the files mentioned in a given exe's imported
                  name table (excluding DOSCALLS).

*****************************************************************************/

/*****************************************************************************
                      I N C L U D E   F I L E S
*****************************************************************************/

#define  INCL_DOS
#define  INCL_WIN
#define  INCL_ERRORS

#include <os2.h>
#include <stdio.h>
#include <string.h>
#include "def.h"

/*****************************************************************************
                        D E F I N I T I O N S
*****************************************************************************/

/*****************************************************************************
                        G L O B A L   D A T A
*****************************************************************************/

SEL            selInfo;                /* selector for info table */
PINFO_TBL      pInfo;                  /* our table of resources */
USHORT         num_entries;            /* entries in info table */

static CHAR    *file=__FILE__;         /* for use with error_exit() */

extern USHORT  imptab_size;            /* size of imported names table */
extern CHAR    *imptab;                /* imported names table ptr */
extern HATOMTBL hAtomTbl;

/*****************************************************************************
                      BuildImportedInfoTable()
*****************************************************************************/

VOID BuildImportedInfoTable()
{
   USHORT   i, len, j, rc;
   CHAR     szTmp[256];

   /***** First determine the number of strings in imptab *****/

   for (i=0, num_entries = 0; i<imptab_size; )
      {
      len = imptab[i];
      if (len)
         num_entries++;
      i += len + 1;
      }

   /***** Allocate our information table storage *****/

   if (rc=DosAllocSeg(num_entries * sizeof(INFO_TBL), &selInfo, 0))
         error_exit(rc, "DosAllocSeg", __LINE__, file);

   pInfo = MAKEP(selInfo, 0);

   /***** Step down the imported names table *****/

   for (i=0; i<imptab_size; )
      {
      if (len = imptab[i])
         {
         for (j=0; j<len; j++)
            szTmp[j] = imptab[i+j+1];
         szTmp[j] = '\0';
         pInfo->selRestab = 0;
         pInfo->selNRestab = 0;
         if (!(pInfo->atomName = WinAddAtom(hAtomTbl, szTmp)))
            error_exit(0, "WinAddAtom", __LINE__, file);
         if (strcmp(szTmp, "DOSCALLS"))
            GetDLLTables(pInfo, szTmp);
         pInfo++;
         }
      i += len + 1;
      }
   return;
}

/*****************************************************************************
                            GetDLLTables()
*****************************************************************************/

VOID GetDLLTables(PINFO_TBL pInfo, CHAR *szTmp)
{
   CHAR        szName[256];
   HMODULE     hMod;
   HFILE       hf;
   NEW_EXE     new_exe;                /* new exe header */
   OLD_EXE     old_exe;                /* old exe header */
   USHORT      cbBytes;
   ULONG       ulFilePtr;
   USHORT      usAction;               /* UNUSED, required for DosOpen */
   USHORT      rc;

   if (rc=DosLoadModule(szName, sizeof(szName), szTmp, &hMod))
      /***** Not sure here, it may be an imported function *****/
      return;
//      error_exit(rc, "DosLoadModule", __LINE__, file);

   if (rc=DosGetModName(hMod, sizeof(szName), szName))
      error_exit(rc, "DosGetModName", __LINE__, file);

   if (rc=DosFreeModule(hMod))
      {
      /***** Another Kludge, E.EXE brought us here *****/

      if (rc != ERROR_INVALID_ACCESS)
         error_exit(rc, "DosFreeModule", __LINE__, file);
      }

   if (rc=DosOpen(szName, &hf, &usAction, 0L, FILE_READONLY, FILE_OPEN,
         OPEN_SHARE_DENYWRITE, 0L))
      error_exit(rc, "DosOpen", __LINE__, file);

   /***** Read in the old exe header information *****/

   if (rc=DosRead(hf, &old_exe, sizeof(old_exe), &cbBytes))
      error_exit(rc, "DosRead", __LINE__, file);

   /***** Isolate the new exe header information *****/

   if (rc=DosChgFilePtr(hf, old_exe.lfanew, FILE_BEGIN, &ulFilePtr))
      error_exit(rc, "DosChgFilePtr", __LINE__, file);

   /***** Read in the new exe header *****/

   if (rc=DosRead(hf, &new_exe, sizeof(new_exe), &cbBytes))
      error_exit(rc, "DosRead", __LINE__, file);

   /***** Allocate and read in the Resident Name Table *****/

   CreateTable(hf, pInfo->restab_size = new_exe.modtab - new_exe.restab,
               old_exe.lfanew + (long)new_exe.restab,
               &(pInfo->restab), &(pInfo->selRestab));

   /***** Allocate and read in the Non-Resident Names Table *****/

   CreateTable(hf, pInfo->nrestab_size = new_exe.cbnrestab,
               new_exe.nrestab,
               &(pInfo->nrestab), &(pInfo->selNRestab));

   if (rc=DosClose(hf))
      error_exit(rc, "DosClose", __LINE__, file);

   return; 
}
   
/*****************************************************************************
                      FreeImportedInfoTable()
*****************************************************************************/

VOID FreeImportedInfoTable()
{
   USHORT   i;
   USHORT   rc;

   pInfo = MAKEP(selInfo, 0);
   for (i=0; i < num_entries; i++, pInfo++)
      {
      if (pInfo->selRestab)
         if (rc=DosFreeSeg(pInfo->selRestab))
            error_exit(rc, "DosFreeSeg", __LINE__, file);

      if (pInfo->selNRestab)
         if (rc=DosFreeSeg(pInfo->selNRestab))
            error_exit(rc, "DosFreeSeg", __LINE__, file);
      }

   if (rc=DosFreeSeg(selInfo))
      error_exit(rc, "DosFreeSeg", __LINE__, file);
}

/*****************************************************************************
                            TableLookup()
*****************************************************************************/

CHAR *TableLookup(CHAR *szName, USHORT ordinal)
{
   ATOM           atom;
   CHAR           *p;
   USHORT         *iptr;
   USHORT         more = TRUE;
   USHORT         i;
   static CHAR    szBuf[256];
   static CHAR    *ptr = szBuf;

   pInfo = MAKEP(selInfo, 0);
   if (!(atom=WinFindAtom(hAtomTbl, szName)))
      error_exit(0, "WinFindAtom", __LINE__, file);

   for (i=0; i < num_entries; i++, pInfo++)
      if (pInfo->atomName == atom)
         break;
   
   if (i == num_entries)
      error_exit(0, "TableLookup", __LINE__, file);

   /***** First let's search the Resident Names Table *****/

   p = pInfo->restab;
   p += *p + 3;                        /* bypass the first entry */
   while (p < pInfo->restab + pInfo->restab_size - 1 && more)
      {
      getstring(p, 0, szBuf);          /* grab entry name */
      p += *p + 1;                     /* bypass the length */
      iptr = (USHORT *)p;              /* grab the ordinal */
      if (*iptr == ordinal)
         more = FALSE;
      p += 2;                          /* move on to next table entry */
      }

   if (!more)
      return(ptr);

   /***** Now the Non-Resident Names Table *****/

   p = pInfo->nrestab;
   p += *p + 3;                        /* bypass the first entry */
   while (p < pInfo->nrestab + pInfo->nrestab_size - 1 && more)
      {
      getstring(p, 0, szBuf);          /* grab entry name */
      p += *p + 1;                     /* bypass the length */
      iptr = (USHORT *)p;              /* grab the ordinal */
      if (*iptr == ordinal)
         more = FALSE;
      p += 2;                          /* move on to next table entry */
      }

   return(ptr);      
}
