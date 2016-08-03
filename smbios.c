/*
 *  Copyright (c) 2006-2016 Dell, Inc.
 *  by Jordan Hargrave <jordan_hargrave@dell.com>
 *  Licensed under the Lesser GNU General Public license, version 3.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <dirent.h>
#include <sys/queue.h>
#include <inttypes.h>
#include "smbios.h"
#include "util.h"

/*=======================*
 * SMBIOS
 *=======================*/
void smbios_init(void (*fn)(union smbios_type *tt, char *smstrs[]))
{
	struct smbios_ep sm;
	char *smstrs[32], *smstr;
	int fd, i, off, nstr, slen;
	union smbios_type *tt;
	uint32_t ep;

	if ((fd = open("/sys/firmware/dmi/tables/smbios_entry_point", O_RDONLY)) >= 0) {
		read(fd, &sm, sizeof(sm));
		close(fd);
		ep = 0;
	} else {
		ep = smscan(0xF0000, 0xFFFFFL, 4, "_SM_", 16);
		if (!ep)
			return;
		physmemcpy(&sm, ep, sizeof(sm));
		ep = sm.dmi_tbladdr;
	}
	tt = alloca(sm.ep_maxsz);
	printf("got SMBIOS EP: %" PRIx32 " %d.%d\n", ep, sm.ep_mjr, sm.ep_mnr);

	smstrs[0] = "";
	for (i = 0; i < sm.dmi_numtbl; i++) {
		if ((fd = open("/sys/firmware/dmi/tables/DMI", O_RDONLY)) >= 0) {
			pread(fd, tt, sm.ep_maxsz, ep);
			close(fd);
		} else {
			physmemcpy(tt, ep, sm.ep_maxsz);
		}
		if (tt->hdr.type == 0x7F)
			break;
		nstr = 1;
		off = tt->hdr.length;
		/* Scan Strings */
		if (*(uint16_t *)((void *)tt + off) == 0) {
			off += 2;
			goto dump;
		}
		for (;;) {
			smstr = (char *)tt + off;
			slen = strlen(smstr);
			off += slen+1;
			if (!slen)
				break;
			smstrs[nstr++] = smstr;
		}
	dump:
		fn(tt, smstrs);
		ep += off;
	}
}
