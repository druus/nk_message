#
# Makefile for nk_message

#ifdef SYSTEMROOT
ifeq ($(OS),Windows_NT)
   RM = del /Q
   FixPath = $(subst /,\,$1)
   EXE = nk_message.exe
else
   ifeq ($(shell uname), Linux)
      RM = rm -f
      FixPath = $1
	  EXE = nk_message
   endif
endif


all:
	gcc -Wall -g nk_message.c -o $(EXE)

clean:
	$(RM) $(EXE)
