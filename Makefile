#
# Makefile for nk_message

#ifdef SYSTEMROOT
ifeq ($(OS),Windows_NT)
   RM = del /Q
   FixPath = $(subst /,\,$1)
else
   ifeq ($(shell uname), Linux)
      RM = rm -f
      FixPath = $1
   endif
endif


all:
	gcc -Wall nk_message.c -o nk_message.exe

clean:
	$(RM) nk_message.exe
