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
  int i, off, nstr, slen;
  union smbios_type *tt;
  uint32_t ep;
  
  ep = smscan(0xF0000, 0xFFFFFL, 4, "_SM_", 16);
  if (ep) {
    printf("got SMBIOS EP: %" PRIx32 "\n", ep);
    physmemcpy(&sm, ep, sizeof(sm));

    smstrs[0] = "";
    ep = sm.dmi_tbladdr;
    tt = alloca(sm.ep_maxsz);
    for (i=0; i<sm.dmi_numtbl; i++) {
      physmemcpy(&tt->hdr, ep, sm.ep_maxsz);
      if (tt->hdr.type == 0x7F)
	break;
      nstr = 1;
      off = tt->hdr.length;
      /* Scan Strings */
      if (*(uint16_t *)((void *)tt + off) == 0) {
	off += 2;
	goto dump;
      }
      for(;;) {
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
}

