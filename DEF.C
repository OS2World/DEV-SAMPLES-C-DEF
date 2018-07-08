/*****************************************************************************

   NAME:          DEF.C

   DESCRIPTION:   This module will re-create a module definition file (DEF)
                  for the indicated EXE file.

                  Output is sent to STDOUT and may be redirected.

                  Still To Do:

                     - go out to the entry table for STACKPARAMS on
                       IOPL segments

                     - better error checking

                     - cleanup

   ASSUMPTIONS:   1. Program doesn't import more than 1000 items
                  2. The program is in a valid format
                  3. The program was built with at least 1.1 SDK
                  4. The internal name for a given import is the same
                     as that used by the exporter.

   WARNINGS:      This program is fresh out of the oven. I have not done
                  exhaustive testing with it...

                  If you find any bugs or know of a better way to do
                  something, I'd appreciate hearing from you.
                  
                  My testing consisted of...

                     FOR %1 in (\os2\*.exe)     DO DEF %1
                     FOR %1 in (\os2\dll*.dll)  DO DEF %1
                     FOR %1 in (\os2\*.sys)     DO DEF %1

                  ...none of which generated GP faults anyway.

                  This program *definitely* needs to be upgraded to 1.2
                  but this is a four day weekend and the building is locked.

   REFERENCES:    Advanced OS/2 Programming - Ray Duncan
                  PCMagnet - Charles Petzold

   AUTHOR:        Charlie Schmitt
                  11/24/89
                  PCMagnet ID 70022, 773

*****************************************************************************/

/*****************************************************************************
                      I N C L U D E   F I L E S
*****************************************************************************/

#define  INCL_DOS
#define  INCL_WIN

#include <os2.h>
#include <stdio.h>
#include <string.h>
#include "def.h"
#include "dosexprt.h"

/*****************************************************************************
                        D E F I N I T I O N S
*****************************************************************************/

#define  LIBRARY  0x8000

#define  IsLibrary()    (new_exe.flags & LIBRARY)

/*****************************************************************************
                        G L O B A L   D A T A
*****************************************************************************/

HAB            hab;
NEW_EXE        new_exe;                /* new exe header */
OLD_EXE        old_exe;                /* old exe header */
ULONG          ulFilePtr;              /* current file position */
USHORT         cbBytes;                /* number of bytes moved */
USHORT         rc;                     /* return code */
HFILE          hf;                     /* file handle */
USHORT         usAction;               /* UNUSED, required for DosOpen */
CHAR           *file=__FILE__;         /* for use with error_exit() */

SEL            selImptab;              /* imported names table selector */
CHAR           *imptab;                /* imported names table ptr */
USHORT         imptab_size;            /* size of imported names table */
ULONG          imptab_offset;          /* location of imported names table */

SEL            selRestab;              /* resident names table selector */
CHAR           *restab;                /* resident names table ptr */
USHORT         restab_size;            /* size of resident names table */
ULONG          restab_offset;          /* location of resident names table */

SEL            selSegtab;              /* segment table selector */
CHAR           *segtab;                /* segment table ptr */
USHORT         segtab_size;            /* size of segment table */
ULONG          segtab_offset;          /* location of segment table */

SEL            selNRestab;             /* non-resident names table selector */
CHAR           *nrestab;               /* non-resident names table ptr */
USHORT         nrestab_size;           /* size of non-resident names table */
ULONG          nrestab_offset;         /* loc. of non-resident names table */

SEL            selModtab;              /* module reference table selector */
USHORT         *modtab;                /* module reference table ptr */
USHORT         modtab_size;            /* size of module reference table */
ULONG          modtab_offset;          /* loc. of module reference table */

SEL            selReloctab;            /* relocation table selector */
PIMP_ORD       reloctab;               /* relocation table pointer */
USHORT         reloctab_size;          /* size of relocation table */
ULONG          reloctab_offset;        /* loc. of relocation table */

CHAR           szName[256];            /* generic name */
CHAR           szComment[256];         /* import table comment */
CHAR           *p;                     /* work pointer */
CHAR           szTmp[256];             /* work buffer */
CHAR           szAtomName[256];        /* atom name string */
USHORT         i,j,k,l;                /* work var */
PNEW_SEG       pseg_entry;             /* ptr to a segment table entry */
USHORT         flag;                   /* another work var. */
PUSHORT        iptr;                   /* pointer to an int */
HATOMTBL       hAtomTbl;               /* atom table handle */
ATOM           atom, atomDOS;

/*****************************************************************************
                                main()
*****************************************************************************/

void main(USHORT argc, CHAR *argv[])
{
   USHORT code_segs, data_segs;
   USHORT num_relocs;
   ULONG  ulLong;
   USHORT usAppType;

   if (argc != 2)
      {
      fprintf(stderr, "Usage: def <fspec>\n");
      DosExit(EXIT_PROCESS, 0);
      }

   /***** SETCOM40 brought us here !! *****/

   if (rc=DosQAppType(argv[1], &usAppType))
      error_exit(rc, "DosQAppType", __LINE__, file);

   if (usAppType == 32)
      {
      fprintf(stderr, "Specified file is not valid EXE\n");
      DosExit(EXIT_PROCESS, 0);
      }

   /***** Open the specified executable *****/

   if (rc=DosOpen(argv[1], &hf, &usAction, 0L, FILE_READONLY, FILE_OPEN,
         OPEN_SHARE_DENYWRITE, 0L))
      error_exit(rc, "DosOpen", __LINE__, file);

   /***** Read in the old exe header information *****/

   if (rc=DosRead(hf, &old_exe, sizeof(old_exe), &cbBytes))
      error_exit(rc, "DosRead", __LINE__, file);

   /***** Perform some crude validation *****/

   if ( (old_exe.magic != 0x5a4d) || (old_exe.lfanew == 0L) )
      {
      fprintf(stderr, "Specified file is not valid EXE\n");
      DosExit(EXIT_PROCESS, 0);
      }

   /***** Isolate the new exe header information *****/

   if (rc=DosChgFilePtr(hf, old_exe.lfanew, FILE_BEGIN, &ulFilePtr))
      error_exit(rc, "DosChgFilePtr", __LINE__, file);

   /***** Read in the new exe header *****/

   if (rc=DosRead(hf, &new_exe, sizeof(new_exe), &cbBytes))
      error_exit(rc, "DosRead", __LINE__, file);
 
   /***** Perform some crude validation *****

   if (new_exe.magic != 0x454e)
      {
      fprintf(stderr, "Specified file is not valid EXE\n");
      DosExit(EXIT_PROCESS, 0);
      }

   /***** Allocate and read in the Imported Names Table *****/

   CreateTable(hf, imptab_size = new_exe.enttab - new_exe.imptab,
               imptab_offset = old_exe.lfanew + (long)new_exe.imptab,
               &imptab, &selImptab);

   /***** Allocate and read in the Resident Name Table *****/

   CreateTable(hf, restab_size = new_exe.modtab - new_exe.restab,
               restab_offset = old_exe.lfanew + (long)new_exe.restab,
               &restab, &selRestab);

   /***** Allocate and read in the Non-Resident Names Table *****/

   CreateTable(hf, nrestab_size = new_exe.cbnrestab,
               nrestab_offset = new_exe.nrestab,
               &nrestab, &selNRestab);

   /***** Allocate and read in the Segment Table *****/

   CreateTable(hf, segtab_size = new_exe.rsrctab - new_exe.segtab,
               segtab_offset = old_exe.lfanew + (long)new_exe.segtab,
               &segtab, &selSegtab);

   /***** Allocate and read in the Module Reference Table *****/

   CreateTable(hf, modtab_size = new_exe.imptab - new_exe.modtab,
               modtab_offset = old_exe.lfanew + (long)new_exe.modtab,
               (CHAR **)&modtab, &selModtab);

   /***** Get the module name *****/

   /***** SETCOM40.EXE reports 0 here (?) *****/

   if (restab_size)
      getstring(restab, 0, szName);
   else
      strcpy(szName, "NONAME");

   printf("; %s.DEF\n\n", szName);

   /***** Output appropriate first line *****/

   if (IsLibrary())
      {
      p = (new_exe.flags & 0x0004) ? "INITINSTANCE" : "INITGLOBAL";
      printf("%-14s %-14s %s\n", "LIBRARY", szName, p);
      }
   else
      {
      p = "UKNOWN";
      if (new_exe.flags & 0x0700)
         {
         i = (new_exe.flags >> 8) & 0x03;
         if (i == 3)
            p = "WINDOWAPI";
         else if (i == 2)
            p = "WINDOWCOMPAT";
         else
            p = "NOTWINDOWCOMPAT";
         }
      printf("%-14s %-14s %s\n", "NAME", szName, p);
      }

   /***** Now let's get the Description *****/

   if (nrestab_size)
      getstring(nrestab, 0, szName);
   else
      strcpy(szName, "NO DESCRIPTION");
   printf("%-14s '%s'\n", "DESCRIPTION", szName);

   /***** Output appropriate mode *****/
   /***** NOTE: REALMODE really means both... *****/

   p = (new_exe.flags & 0x0008) ? "PROTMODE" : "REALMODE";
   printf("%s\n", p);

   /***** Output DATA *****/

   flag = new_exe.flags & 0x0003;
   if (flag == 0)
      p = "NONE";
   else if (flag == 1)
      p = "SINGLE";
   else if (flag == 2)
      p = "MULTIPLE";
   printf("%-14s %s\n", "DATA", p);

   /***** Output STACKSIZE *****/
   
   printf("%-14s %u\n", "STACKSIZE", new_exe.stack);

   /***** Output HEAPSIZE *****/
   
   printf("%-14s %u\n", "HEAPSIZE", new_exe.heap);

   /***** Output Operating System Type *****/

   if (new_exe.exetyp == 1)
      p = "OS2";
   else if (new_exe.exetyp == 2)
      p = "WINDOWS";
   else
      p = "MSDOS4";
   printf("%-14s %s\n", "EXETYPE", p);

   /***** It's time for the Segments *****/

   printf("\nSEGMENTS\n");

   code_segs = data_segs = 0;          /* init counters */
   for (i=0; i < (segtab_size / sizeof(NEW_SEG)); i++)
      {
      pseg_entry = getsegment((PNEW_SEG)segtab, i);
      flag = pseg_entry->flags;
      if (flag & 0x0001)               /* DATA */
         {
         if (data_segs == 0)
            strcpy(szName, "_DATA");
         else
            sprintf(szName, "_DATA%u", data_segs);
         printf("\t%-8s CLASS 'DATA' ", szName);
         strcpy(szName, (flag & 0x0010) ? " MOVEABLE" : " FIXED");
         strcat(szName, (flag & 0x0020) ? " SHARED" : " NONSHARED");
         strcat(szName, (flag & 0x0040) ?  " PRELOAD" : " LOADONCALL");
         strcat(szName, (flag & 0x0080) ? " READONLY" : " READWRITE");
         strcat(szName, !(flag & 0x0800) ? " IOPL" : " NOIOPL");
         strcat(szName, (flag & 0x1000) ? " DISCARDABLE" : " NONDISCARDABLE");
         data_segs++;
         }
      else                             /* CODE */
         {
         if (code_segs == 0)
            strcpy(szName, "_TEXT");
         else
            sprintf(szName, "_TEXT%u", code_segs);
         printf("\t%-8s CLASS 'CODE' ", szName);
         strcpy(szName, (flag & 0x0010) ? " MOVEABLE" : " FIXED");
         strcat(szName, (flag & 0x0020) ? " PURE" : " IMPURE");
         strcat(szName, (flag & 0x0040) ?  " PRELOAD" : " LOADONCALL");
         strcat(szName, (flag & 0x0080) ? " EXECUTEONLY" : " EXECUTEREAD");
         strcat(szName, (flag & 0x0200) ? " CONFORMING" : " NONCONFORMING");
         strcat(szName, !(flag & 0x0800) ? " IOPL" : " NOIOPL");
         strcat(szName, (flag & 0x1000) ? " DISCARDABLE" : " NONDISCARDABLE");
         code_segs++;
         }

      /***** Kind of a Kludge *****/

      if (strlen(szName) > 49)
         {
         for (j=49; j; j--)
            if (szName[j] == ' ')
               break;
         szName[j] = '\0';
         printf("%s\n", szName);
         printf("%30s %s\n", " ", &szName[j+1]);
         }
      else
         printf("%s\n", szName);
      }

   /***** It's time for the Exports *****/

   printf("\nEXPORTS\n");

   /***** First let's do the Resident Names Table *****/

   p = restab;
   p += *p + 3;                        /* bypass the first entry */
   while (p < restab + restab_size - 1)
      {
      getstring(p, 0, szName);         /* grab entry name */
      p += *p + 1;                     /* bypass the length */
      iptr = (USHORT *)p;              /* grab the ordinal */
      printf("\t%-30s @%-3u    RESIDENTNAME\n", szName, *iptr);      
      p += 2;                          /* move on to next table entry */
      }

   /***** Now the Non-Resident Names Table *****/

   p = nrestab;
   p += *p + 3;                        /* bypass the first entry */
   while (p < nrestab + nrestab_size - 1)
      {
      getstring(p, 0, szName);         /* grab entry name */
      p += *p + 1;                     /* bypass the length */
      iptr = (USHORT *)p;              /* grab the ordinal */
      printf("\t%-30s @%u\n", szName, *iptr);      
      p += 2;                          /* move on to next table entry */
      }

  /////////////////// NEED STACK PARAMS ///////////////////////////

   /***** It's time for the Imports *****/

   /***** The atom table will prevent dups *****/

   hAtomTbl = WinCreateAtomTable(0, 1000);
   if (hAtomTbl == NULL)
      error_exit(0, "WinCreateAtomTable", __LINE__, file);

   /***** Build string lookup tables *****/

   BuildImportedInfoTable();

   /***** Add DOSCALLS for DosCallsTbl[] lookup *****/

   if (!(atomDOS = WinAddAtom(hAtomTbl, "DOSCALLS")))
      error_exit(0, "WinAddAtom", __LINE__, file);

   printf("\nIMPORTS\n");

   for (i=0; i < (segtab_size / sizeof(NEW_SEG)); i++)
      {
      pseg_entry = getsegment((PNEW_SEG)segtab, i);
      if (pseg_entry->cbseg == 0)
         continue;                     /* nothing to parse */

      if (pseg_entry->flags & 0x0001)
         continue;                     /* data */

      if (!(pseg_entry->flags & 0x0100))
         continue;                     /* no reloc information */

     /***** Isolate the segment's reloc information *****/

      ulLong = pseg_entry->sector * (ULONG)(1 << new_exe.calign);
      if (rc=DosChgFilePtr(hf, ulLong + pseg_entry->cbseg,
            FILE_BEGIN, &ulFilePtr))
         error_exit(rc, "DosChgFilePtr", __LINE__, file);

      /***** Read in the number of entries in the table *****/

      if (rc=DosRead(hf, &num_relocs, sizeof(num_relocs), &cbBytes))
         error_exit(rc, "DosRead", __LINE__, file);

      if (num_relocs == 0)
         continue;                     /* double check */

      if (rc=DosAllocSeg(cbBytes = num_relocs * sizeof(IMP_ORD),
               &selReloctab, 0))
         error_exit(rc, "DosAllocSeg", __LINE__, file);

      reloctab = MAKEP(selReloctab, 0);

      if (rc=DosRead(hf, reloctab, cbBytes, &cbBytes))
         error_exit(rc, "DosRead", __LINE__, file);

      /***** Step down the relocation table *****/

      for (j=0; j<num_relocs; j++, reloctab++)
         {
         if (reloctab->id == 0 || reloctab->id == 4)
            continue;
         l = reloctab->index;
         k = modtab[l-1];
         getstring(imptab + k, 0, szName);

         /***** Is the DLL DOSCALLS ? *****/

         p = "UNKNOWN";                       
         if (atomDOS == WinFindAtom(hAtomTbl, szName))
            {
            if (reloctab->ordinal <= DOS_TABLE_ENTRIES)
               p = DosCallsTbl[reloctab->ordinal].name;
            }
         else
            {
            p = TableLookup(szName, reloctab->ordinal);
            }

         /***** Create the import string *****/

         if (reloctab->id == 1 || reloctab->id == 5)
            {
            sprintf(szAtomName, "%-29s= %s.%u", p, szName, reloctab->ordinal);
            }
         else
            {
            getstring(imptab + reloctab->ordinal, 0, szTmp);
            sprintf(szAtomName, "%s.%s", szName, szTmp);
            }

         /***** Only output import if original *****/

         if (!WinFindAtom(hAtomTbl, szAtomName))
            {
            printf("\t%s\n", szAtomName);
            if (!WinAddAtom(hAtomTbl, szAtomName))
               error_exit(0, "WinAddAtom", __LINE__, file);
            }
         }

      /***** DOne with this buffer, go get another *****/

      if (rc=DosFreeSeg(selReloctab))
         error_exit(rc, "DosFreeSeg", __LINE__, file);
      }

   /***** Having a problem with this call *****/

//   if (WinDestroyAtomTable(hAtomTbl))
//      error_exit(0, "WinDestroyAtomTable", __LINE__, file);

   /***** Cleanup *****/

   /***** Free string lookup tables *****/

   FreeImportedInfoTable();

   if (selModtab)
      if (rc=DosFreeSeg(selModtab))
         error_exit(rc, "DosFreeSeg", __LINE__, file);

   if (selSegtab)
      if (rc=DosFreeSeg(selSegtab))
         error_exit(rc, "DosFreeSeg", __LINE__, file);

   if (selNRestab)
      if (rc=DosFreeSeg(selNRestab))
         error_exit(rc, "DosFreeSeg", __LINE__, file);

   if (selRestab)
      if (rc=DosFreeSeg(selRestab))
         error_exit(rc, "DosFreeSeg", __LINE__, file);

   if (selImptab)
      if (rc=DosFreeSeg(selImptab))
         error_exit(rc, "DosFreeSeg", __LINE__, file);

   if (rc=DosClose(hf))
      error_exit(rc, "DosClose", __LINE__, file);

   /***** Make sure OS/2 1.2 flushs STDOUT properly when redirected ! *****/

   fclose(stdout);

   DosExit(EXIT_PROCESS, 0);
}

/*****************************************************************************
                             CreateTable()
*****************************************************************************/

VOID CreateTable(HFILE hf, USHORT size, ULONG offset, CHAR **table, PSEL sel)
{
   if (size != 0)
      {
      if (rc=DosChgFilePtr(hf, offset, FILE_BEGIN, &ulFilePtr))
         error_exit(rc, "DosChgFilePtr", __LINE__, file);

      if (rc=DosAllocSeg(size, sel, 0))
         error_exit(rc, "DosAllocSeg", __LINE__, file);

      *table = MAKEP(*sel, 0);

      if (rc=DosRead(hf, *table, size, &cbBytes))
         error_exit(rc, "DosRead", __LINE__, file);
      }
   return;
}

/*****************************************************************************
                             getstring()
*****************************************************************************/

void getstring(CHAR *table, USHORT num, CHAR *buf)
{
   USHORT i, len;

   /***** Scan thru table of counted strings and return 'num' entry *****/

   for (i=0; ; i++)
      {
      len = *table++;                  /* get the string's size */
      if (i==num)                      /* out if this is the one we want */
         break;
      table += len;                    /* move on to next table entry */
      }
   strncpy(buf, table, len);           /* copy entry to supplied buffer */
   *(buf + len) = '\0';                /* make it ASCIIZ */
   return;
}

/*****************************************************************************
                             getsegment()
*****************************************************************************/

PNEW_SEG getsegment(PNEW_SEG table, USHORT num)
{
   USHORT i;

   /***** Loops thru the segment table till num entry *****/

   for (i=0; i < num; i++)
      table++;
   return(table);
}

/*****************************************************************************
                             error_exit()
*****************************************************************************/

void error_exit(USHORT err, CHAR *msg, USHORT line, CHAR *file)
{
   fprintf(stderr, "FILE: %s LINE: %u - Error %u returned from %s\n",
      file, line, err, msg);
   DosExit(EXIT_PROCESS, 0);
}

/*****************************************************************************
                                E N D
*****************************************************************************/


