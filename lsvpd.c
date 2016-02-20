#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <dirent.h>
#include <sys/queue.h>
#include <inttypes.h>

#include "lsvpd.h"
#include "smbios.h"
#include "util.h"

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
  char vpath[PATH_MAX];
  
  _vpdpath = path;
  snprintf(vpath, sizeof(vpath), "%s/device", path);
  _scandir(vpath, scannetdev, (void *)path);
  return 0;
}

void scan_smbios(union smbios_type *tt, char *smstrs[])
{
  char vpdkey[32];
  
  snprintf(vpdkey, sizeof(vpdkey), "%.2x-%.4x",
	   tt->hdr.type, tt->hdr.handle);
  _vpdpath = vpdkey;

  /* System/Baseboard */
  if (tt->hdr.type == 1 || tt->hdr.type == 2) {
    vpd_attr("MF", smstrs[tt->type1.mf]);
    vpd_attr("PN", smstrs[tt->type1.pn]);
    vpd_attr("SN", smstrs[tt->type1.sn]);
  }
  /* Memory DIMM */
  if (tt->hdr.type == 17 && tt->type17.sz) {
    vpd_attr("MF", smstrs[tt->type17.mf]);
    vpd_attr("PN", smstrs[tt->type17.pn]);
    vpd_attr("SN", smstrs[tt->type17.sn]);
    vpd_attr("YA", smstrs[tt->type17.loc]);
  }
}

int main(int argc, char *argv[])
{
  smbios_init(scan_smbios);
  _scandir("/sys/block", scanblock, 0);

  _scandir("/sys/class/net", scannet, 0);
}
