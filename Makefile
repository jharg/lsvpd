all: lsvpd.c util.c smbios.c smbios.h
	gcc -Wall -g -o lsvpd lsvpd.c util.c smbios.c
