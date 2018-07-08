/*****************************************************************************

   NAME:          DEF.C

   DESCRIPTION:   This is the header file for the def application.

*****************************************************************************/

/*****************************************************************************
                           T Y P E D E F S
*****************************************************************************/

typedef struct _OLD_EXE
{
   unsigned short magic;               /* magic number */
   unsigned short cblp;                /* bytes on last page of file */
   unsigned short cp;                  /* pages in file  */
   unsigned short crlc;                /* relocations */
   unsigned short cparhdr;             /* size of header in paragraphs */
   unsigned short minalloc;            /* minimum extra paragraphs needed */
   unsigned short maxalloc;            /* maximum extra paragraphs needed */
   unsigned short ss;                  /* initial SS value */
   unsigned short sp;                  /* initial SP value */
   unsigned short csum;                /* checksum */
   unsigned short cs;                  /* initial CS value */
   unsigned short ip;                  /* initial IP value */
   unsigned short lfarlc;              /* file address of relocation table */
   unsigned short ovno;                /* overlay number */
   unsigned short res[4];      
   unsigned short oemid;               /* OEM identifier */
   unsigned short oeminfo;             /* OEM information */
   unsigned short res2[10];     
   unsigned long  lfanew;              /* file address of new exe header */
} OLD_EXE;

typedef struct _NEW_EXE
{
   unsigned short magic;               /* magic number */
   unsigned char  ver;                 /* version number */
   unsigned char  rev;                 /* revision number */
   unsigned short enttab;              /* offset of entry table */
   unsigned short cbenttab;            /* number o bytes in entry table */
   unsigned long  crc;                 /* checksum of whole file */
   unsigned short flags;               /* flag word */
   unsigned short autodata;            /* automatic data segment number */
   unsigned short heap;                /* initial heap allocation */
   unsigned short stack;               /* initial stack allocation */
   unsigned short ip;                  /* initial IP setting */
   unsigned short cs;                  /* initial CS setting */
   unsigned short sp;                  /* initial SP setting */
   unsigned short ss;                  /* initial SS setting */
   unsigned short cseg;                /* count of file segments */
   unsigned short cmod;                /* entries in module reference table */
   unsigned short cbnrestab;           /* size of non-resident name table */
   unsigned short segtab;              /* offset of segment table */
   unsigned short rsrctab;             /* offset of resource table */
   unsigned short restab;              /* offset of resident name table */
   unsigned short modtab;              /* offset of module reference table */
   unsigned short imptab;              /* offset of imported names table */
   unsigned long  nrestab;             /* offset of non-resident names table */
   unsigned short cmovent;             /* count of moveable entries */
   unsigned short calign;              /* segment alignment shift count */
   unsigned short cres;                /* count of resource entries */
   unsigned char  exetyp;              /* target operating system */
} NEW_EXE;

typedef struct _NEW_SEG
{
   unsigned short sector;              /* file sector of start of segment */
   unsigned short cbseg;               /* number of bytes in segment */
   unsigned short flags;               /* attribute flags */
   unsigned short minalloc;            /* minimum allocation in bytes */
} NEW_SEG, *PNEW_SEG;

typedef struct _IMP_ORD                /* import ordinal reloc entry */
{
   unsigned char type;                 /* type of relocation */
   unsigned char id;                   /* type of entry */
   unsigned short offset;              /* offset to item within segment */
   unsigned short index;               /* index to module reference table */
   unsigned short ordinal;             /* ordinal number */
} IMP_ORD, *PIMP_ORD;

typedef struct _INFO_TBL
{
   ATOM     atomName;
   SEL      selRestab;
   SEL      selNRestab;
   CHAR     *restab;
   CHAR     *nrestab;
   USHORT   restab_size;
   USHORT   nrestab_size;
} INFO_TBL, *PINFO_TBL;

/*****************************************************************************
                         P R O T O T Y P E S 
*****************************************************************************/

void main(USHORT argc, CHAR *argv[]);
void getstring(CHAR *table, USHORT num, CHAR *buf);
void error_exit(USHORT err, CHAR *msg, USHORT line, CHAR *file);
VOID CreateTable(HFILE hf, USHORT size, ULONG offset, CHAR **table, PSEL sel);
PNEW_SEG getsegment(PNEW_SEG table, USHORT num);
VOID BuildImportedInfoTable(VOID);
VOID GetDLLTables(PINFO_TBL pInfo, CHAR *szTmp);
VOID FreeImportedInfoTable(VOID);
CHAR *TableLookup(CHAR *szName, USHORT ordinal);


