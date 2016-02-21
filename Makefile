all: lsvpd.c util.c smbios.c smbios.h ipmi.c
	gcc -Wall -g -o lsvpd lsvpd.c util.c smbios.c ipmi.c
