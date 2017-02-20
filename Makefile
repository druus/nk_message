#
# Makefile for nk_message

all:
	gcc nk_message.c -o nk_message.exe

clean:
	del nk_message.exe
