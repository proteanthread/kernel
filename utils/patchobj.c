/*
 * LibreDOS Kernel Utilities - Object File LNAMES Patch Utility (patchobj)
 *
 * Architectural Role:
 *   Modifies the compiler-generated object files (.obj) to replace segment and segment group
 *   names (LNAMES record) before linking. This allows customization of segment destinations,
 *   such as forcing Watcom libraries into designated HMA or INIT code blocks.
 *
 * Changeability & Constraints:
 *   - CAN BE CHANGED: Replacement array capacity, error logging, and standard terminal display options.
 *   - CANNOT BE CHANGED: OMF record signature matching (LNAMES record type 0x96 / 0x8C) and CRC/checksum
 *     calculations. Chesksum logic must match Intel OMF specifications exactly.
 *
 * Expected Behavior:
 *   - Parses binary records, checks record types, performs string replacement in place, and computes
 *     the required checksum before rewriting the target file.
 *
 * Diagnostics & Recovery:
 *   - If linking fails with missing segment errors after running patchobj, verify that the segment
 *     replacements match the names expected by the linker map configuration files.
 */

/*****************************************************************************
** PATCHOBJ facility - change strings in the LNAMES record					 
**
** Copyright 2001 by tom ehlert
**
** GPL bla to be added, but intended as GPL
**       
**
** 09/06/2001 - initial revision
** not my biggest kind of software; anyone willing to add 
** comments, errormessages, usage info,...???
** 
*****************************************************************************/

#include <stdio.h>
#if defined(__GNUC__) || defined(__WATCOMC__)
#include <unistd.h>
#else
#include <io.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

#ifndef TRUE
#define TRUE (1==1)
#define FALSE (0==1)
#endif

struct {
  char *sin, *sout;
} repl[100];
int repl_count;

void go_records(FILE * infile, FILE * outfile);

void quit(char *s, ...)
{
  va_list args;
  va_start(args, s);
  vprintf(s, args);
  va_end(args);
  exit(1);
}

void define_replace(char *sin)
{
  char *s;

  if (repl_count >= 99)
    quit("too many replacements");

  if ((s = strchr(sin, '=')) == NULL)
    quit("illegal replacement <%s>, missing '='", sin);

  *s = 0;
  repl[repl_count].sin = sin;
  repl[repl_count].sout = s + 1;

  repl_count++;
}

int main(int argc, char *argv[])
{
  char *argptr;
  FILE *infile, *outfile;
  char *inname = 0, *outname = "~patchob.tmp";

  int use_temp_file = TRUE;

  argc--, argv++;

  for (; argc != 0; argc--, argv++)
  {
    argptr = *argv;

    if (*argptr != '-' && *argptr != '/')
    {
      if (argc == 1)
      {
        inname = argptr;
        continue;
      }
      define_replace(argptr);
      continue;
    }
    switch (toupper(argptr[1]))
    {
      case 'O':
        outname = argptr + 2;
        use_temp_file = FALSE;
        break;
      default:
        quit("illegal argument <%s>\n", argptr);
        break;
    }
  }

  if (inname == 0)
    quit("Inputfile must be specified\n");

  if (repl_count == 0)
    quit("no replacements defined");

  if ((infile = fopen(inname, "rb")) == NULL)       /* open for READ/WRITE                                                                                                                                          */
    quit("can't read %s\n", inname);

  if ((outfile = fopen(outname, "wb")) == NULL)     /* open for READ/WRITE                                                                                                                                          */
    quit("can't write %s\n", outname);

  go_records(infile, outfile);

  fclose(infile);
  fclose(outfile);
  if (use_temp_file)
  {
    unlink(inname);
    rename(outname, inname);
  }

  return 0;

}

#include "algnbyte.h"
struct record {
  unsigned char rectyp;
  unsigned short datalen;
  char buffer[0x2000];
} Record, Outrecord;
#include "algndflt.h"

struct verify_pack1 { char x[ sizeof(struct record) == 0x2003 ? 1 : -1];};

void go_records(FILE * infile, FILE * outfile)
{
  unsigned char stringlen;
  char *string, *s;
  int i, j;
  unsigned char chksum;

  do
  {
    if (fread(&Record, 1, 3, infile) != 3)
    {                           /* read type and reclen */
      /* printf("end of infile read\n"); */
      break;
    }
    if (Record.datalen > sizeof(Record.buffer))
      quit("record to large : length %u Bytes \n", Record.datalen);

    if (fread(Record.buffer, 1, Record.datalen, infile) != Record.datalen)
    {
      printf("invalid record format\n");
      quit("can't continue\n");
    }

    if (Record.rectyp != 0x96 && Record.rectyp != 0x8c)  /* we are only interested in LNAMES */
    {
      fwrite(&Record, 1, 3 + Record.datalen, outfile);
      continue;
    }

    /* printf("at %lx - record type %x len %x\n",ftell(infile)-3,Record.rectyp,
       Record.datalen);*/
    Outrecord.rectyp = Record.rectyp;
    Outrecord.datalen = 0;

    for (i = 0; i < Record.datalen - 1;)
    {
      stringlen = (unsigned char)Record.buffer[i];
      i++;

      string = &Record.buffer[i];
      i += stringlen;

      if (i > Record.datalen)
        quit("invalid lnames record");

      for (j = 0; j < repl_count; j++)
        if (memcmp(string, repl[j].sin, stringlen) == 0
            && strlen(repl[j].sin) == stringlen)
        {
          string = repl[j].sout;
          stringlen = strlen(repl[j].sout);
        }
      Outrecord.buffer[Outrecord.datalen] = stringlen;
      Outrecord.datalen++;
      memcpy(Outrecord.buffer + Outrecord.datalen, string, stringlen);
      Outrecord.datalen += stringlen;
    }

    chksum = 0;
    for (s = (char *)&Outrecord;
         s < &Outrecord.buffer[Outrecord.datalen]; s++)
      chksum += (unsigned char)*s;

    Outrecord.buffer[Outrecord.datalen] = ~chksum;
    Outrecord.datalen++;

    /* printf("^sum = %02x - %02x\n",chksum,~chksum); */

    fwrite(&Outrecord, 1, 3 + Outrecord.datalen, outfile);

  }
  while (Record.rectyp != 0x00 /*ENDFIL*/);
}
