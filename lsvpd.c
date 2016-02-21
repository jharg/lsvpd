#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <dirent.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <inttypes.h>
#include <sys/ioctl.h>

#include "lsvpd.h"
#include "smbios.h"
#include "util.h"
#include "ipmi.h"

#include <net/if.h>
#include <net/if_arp.h>
#include <linux/sockios.h>

#include <linux/ethtool.h>

const char *_vpdpath;

/*=======================*
 * VPD code
 *=======================*/
void vpd_attr(const char *attr, const char *line)
{
  const char *end;
  
  if (_vpdpath) {
    printf("=== %s\n", _vpdpath);
    _vpdpath = NULL;
  }
  line = _stripspc(line);
  end  = _rstripspc(line);
  printf("  %.2s  '%.*s'\n", attr, (int)(end-line), line);
}

void vpd_fileattr(const char *attr, const char *file, int off)
{
  char line[128], *r;
  FILE *fp;

  if ((fp = fopen(file, "r")) != NULL) {
    fseek(fp, off, SEEK_SET);
    if (fgets(line, sizeof(line), fp) != NULL) {
      if ((r = strrchr(line, '\n')) != NULL)
	*r = 0;
      vpd_attr(attr, line);
    }
    fclose(fp);
  }
}

int vpd_readtag(int fd, int off, int *len)
{
  uint8_t tag, tbuf[2];

  if (pread(fd, &tag, 1, off) != 1)
    return -1;
  if (tag == 0 || tag == 0xFF || tag == 0x7F)
    return -1;
  if (tag & 0x80) {
    /* Long resource */
    if (pread(fd, tbuf, 2, off+1) != 2)
      return -1;
    *len = tbuf[0] +  (tbuf[1] << 8);
    return tag;
  }
  /* Short resource */
  *len = tag & 0xF;
  return tag & ~0xF;
}

struct vpdr_tag
{
  char    sig[2];
  uint8_t len;
};

void readvpd(const char *path)
{
  int fd, tag, ilen, rlen, off, vlen;
  struct vpdr_tag *vr;
  char vrstr[258];
  void *buf;

  memset(vrstr, 0, sizeof(vrstr));
  if ((fd = open(path, O_RDONLY)) < 0)
    return;
  /* Read VPD-I */
  tag = vpd_readtag(fd, 0, &ilen);
  if (tag != 0x82)
    goto end;

  /* Read VPD-R */
  tag = vpd_readtag(fd, ilen+3, &rlen);
  if (tag != 0x90)
    goto end;
  buf = alloca(rlen);
  if (pread(fd, buf, rlen, ilen+3+3) != rlen)
    goto end;
  printf("hazvpd\n");
  dump(buf, rlen);
  for (off=0; off<rlen; off += vr->len+3) {
    vr = buf + off;
    vlen = vr->len;
    if (memcmp(vr->sig, "RV", 2)) {
      snprintf(vrstr, sizeof(vrstr), "%.*s",
	       vlen, (char *)&vr[1]);
      vpd_attr(vr->sig, vrstr);
    }
  }
 end:
  close(fd);
}

/* Dump VPD for block devices */
int scanblockdev(const char *path, void *arg)
{
  const char *vb;
  
  vb = _basename(path);
  if (!strcmp(vb, "vendor"))
    vpd_fileattr("MF", path, 0);
  if (!strcmp(vb, "model"))
    vpd_fileattr("PN", path, 0);
  if (!strcmp(vb, "vpd_pg80")) {
    vpd_fileattr("SN", path, 4);
  }
  return 0;
}

int scannetdev(const char *path, void *arg)
{
  const char *vb;

  vb = _basename(path);
  if (!strcmp(vb, "vendor"))
    vpd_fileattr("MF", path, 0);
  if (!strcmp(vb, "device"))
    vpd_fileattr("PN", path, 0);
  if (!strcmp(vb, "vpd"))
    readvpd(path);
  return 0;
}

int scanblock(const char *path, void *arg)
{
  char vpath[PATH_MAX];
  
  _vpdpath = path;
  snprintf(vpath, sizeof(vpath), "%s/device", path);
  _scandir(vpath, scanblockdev, (void *)path);
  return 0;
}

int scannet(const char *path, void *arg)
{
  struct ethtool_drvinfo dinfo;
  char vpath[PATH_MAX];
  const char *vb;
  struct ifreq ifr;
  int fd, err;

  _vpdpath = path;
  vb = _basename(path);
  snprintf(vpath, sizeof(vpath), "%s/device", path);
  _scandir(vpath, scannetdev, (void *)path);

  memset(&ifr, 0, sizeof(ifr));
  strncpy(ifr.ifr_name, vb, sizeof(ifr.ifr_name)-1);
  fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0)
    return 0;
  dinfo.cmd = ETHTOOL_GDRVINFO;
  ifr.ifr_data = (void *)&dinfo;
  err = ioctl(fd, SIOCETHTOOL, &ifr);
  close(fd);

  if (!err && strcmp(dinfo.fw_version, "N/A")) {
    vpd_attr("TY", "NET");
    vpd_attr("FW", dinfo.fw_version);
  }
  return 0;
}

void scan_smbios(union smbios_type *tt, char *smstrs[])
{
  char vpdkey[32];
  
  snprintf(vpdkey, sizeof(vpdkey), "%.2x-%.4x",
	   tt->hdr.type, tt->hdr.handle);
  _vpdpath = vpdkey;

  /* BIOS */
  if (tt->hdr.type == 0) {
    vpd_attr("TY", "BIOS");
    vpd_attr("MF", smstrs[tt->type0.mf]);
    vpd_attr("FW", smstrs[tt->type0.ver]);
    vpd_attr("FD", smstrs[tt->type0.reldate]);
  }
  /* System/Baseboard */
  if (tt->hdr.type == 1 || tt->hdr.type == 2) {
    vpd_attr("MF", smstrs[tt->type1.mf]);
    vpd_attr("PN", smstrs[tt->type1.pn]);
    vpd_attr("SN", smstrs[tt->type1.sn]);
  }
  /* Memory DIMM */
  if (tt->hdr.type == 17 && tt->type17.sz) {
    vpd_attr("TY", "DIMM");
    vpd_attr("MF", smstrs[tt->type17.mf]);
    vpd_attr("PN", smstrs[tt->type17.pn]);
    vpd_attr("SN", smstrs[tt->type17.sn]);
    vpd_attr("YA", smstrs[tt->type17.loc]);
    vpd_attr("AT", smstrs[tt->type17.at]);
  }
}

int main(int argc, char *argv[])
{
  ipmi_scan();
  smbios_init(scan_smbios);
  _scandir("/sys/block", scanblock, 0);
  _scandir("/sys/class/net", scannet, 0);
  return 0;
}
