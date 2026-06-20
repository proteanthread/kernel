!include "../mkfiles/generic.mak"

CFLAGS = -I..$(DIRSEP)hdr

production: patchobj.exe exeflat.exe upxentry.bin upxdevic.bin

patchobj.exe: patchobj.c
	$(CL_HOST) $(CFLAGS) patchobj.c

upxentry.bin: upxentry.asm
	$(NASM) -f bin upxentry.asm -o upxentry.bin

upxdevic.bin: upxdevic.asm
	$(NASM) -f bin upxdevic.asm -o upxdevic.bin

exeflat.exe: exeflat.c ../hdr/exe.h
	$(CL_HOST) $(CFLAGS) exeflat.c


clobber: clean

clean:
	$(RM) *.obj *.bak *.crf *.xrf *.map *.lst *.las *.cod *.err status.me
	$(RM) exeflat.exe patchobj.exe upxentry.bin upxdevic.bin
