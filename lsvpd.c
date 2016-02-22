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
  
	if (!strlen(line))
		return;
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

	fp = fopen(file, "r");
	if (!fp)
		return;
	fseek(fp, off, SEEK_SET);
	if (fgets(line, sizeof(line), fp) != NULL) {
		if ((r = strrchr(line, '\n')) != NULL)
			*r = 0;
		vpd_attr(attr, line);
	}
	fclose(fp);
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
	fd = open(path, O_RDONLY);
	if (fd < 0)
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
	for (off = 0; off < rlen; off += vr->len+3) {
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

void sm_attr(char *attr, union smbios_type *tt, char *smstrs[], 
	     int id, int idpos)
{
	if (id && idpos < tt->hdr.length)
		vpd_attr(attr, smstrs[id]);
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
		sm_attr("MF", tt, smstrs, tt->type0.mf, 0x4);
		sm_attr("FW", tt, smstrs, tt->type0.ver, 0x5);
		sm_attr("FD", tt, smstrs, tt->type0.reldate, 0x8);
	}
	/* System/Baseboard */
	if (tt->hdr.type == 1) {
		sm_attr("MF", tt, smstrs, tt->type1.mf, 0x4);
		sm_attr("PN", tt, smstrs, tt->type1.pn, 0x5);
		sm_attr("VR", tt, smstrs, tt->type1.ver,0x6);
		sm_attr("SN", tt, smstrs, tt->type1.sn, 0x7);
		sm_attr("SK", tt, smstrs, tt->type1.skunum, 0x19);
		sm_attr("FM", tt, smstrs, tt->type1.family, 0x1A);
	}
	/* Baseboard */
	if (tt->hdr.type == 2) {
		sm_attr("MF", tt, smstrs, tt->type2.mf, 0x4);
		sm_attr("PN", tt, smstrs, tt->type2.pn, 0x5);
		sm_attr("VR", tt, smstrs, tt->type2.ver,0x6);
		sm_attr("SN", tt, smstrs, tt->type2.sn, 0x7);
		sm_attr("AT", tt, smstrs, tt->type2.at, 0x8);
		sm_attr("YL", tt, smstrs, tt->type2.loc, 0xA);
	}
	/* Type 3: Chassis */
	if (tt->hdr.type == 3) {
		sm_attr("MF", tt, smstrs, tt->type3.mf, 0x4);
		sm_attr("VR", tt, smstrs, tt->type3.ver,0x6);
		sm_attr("SN", tt, smstrs, tt->type3.sn, 0x7);
		sm_attr("AT", tt, smstrs, tt->type3.at, 0x8);
	}
	/* Type 4: Processor */
	if (tt->hdr.type == 4) {
		sm_attr("MF", tt, smstrs, tt->type4.mf, 0x7);
		sm_attr("PV", tt, smstrs, tt->type4.vr, 0x10);
		sm_attr("SN", tt, smstrs, tt->type4.sn, 0x20);
		sm_attr("AT", tt, smstrs, tt->type4.at, 0x21);
	}
	/* Memory DIMM */
	if (tt->hdr.type == 17 && tt->type17.sz) {
		vpd_attr("TY", "DIMM");
		sm_attr("MF", tt, smstrs, tt->type17.mf, 0x17);
		sm_attr("PN", tt, smstrs, tt->type17.pn, 0x1A);
		sm_attr("SN", tt, smstrs, tt->type17.sn, 0x18);
		sm_attr("YA", tt, smstrs, tt->type17.loc, 0x10);
		sm_attr("AT", tt, smstrs, tt->type17.at, 0x19);
	}
}

void fru_product(uint8_t *ptr)
{
	uint8_t *pos;

	if (checksum(ptr, ptr[1]*8) != 0) {
		return;
	}
	_vpdpath = "Product";

	pos = ptr+3;
	vpd_attr("MF", fru_string(pos, &pos));
	vpd_attr("NM", fru_string(pos, &pos));
	vpd_attr("PN", fru_string(pos, &pos));
	vpd_attr("VR", fru_string(pos, &pos));
	vpd_attr("AT", fru_string(pos, &pos));
}

void fru_chassis(uint8_t *ptr)
{
	uint8_t *pos;

	if (checksum(ptr, ptr[1] * 8) != 0) {
		return;
	}
	_vpdpath = "Chassis";
	pos = ptr + 3;
	vpd_attr("PN", fru_string(pos, &pos));
	vpd_attr("SN", fru_string(pos, &pos));
}

void fru_board(uint8_t *ptr)
{
	uint8_t *pos;
	
	if (checksum(ptr, ptr[1] * 8) != 0) {
		return;
	}
	_vpdpath = "Board";
	pos = ptr + 6;
	vpd_attr("MF", fru_string(pos, &pos));
	vpd_attr("PD", fru_string(pos, &pos));
	vpd_attr("SN", fru_string(pos, &pos));
	vpd_attr("PN", fru_string(pos, &pos));
}

void showfru(void *data)
{
	struct fru_common *fcu = data;

	if (checksum(fcu, sizeof(*fcu)) != 0) {
		return;
	}
	if (fcu->cia_offset)
		fru_chassis(data + fcu->cia_offset * 8);
	if (fcu->ba_offset)
		fru_board(data + fcu->ba_offset * 8);
	if (fcu->pia_offset)
		fru_product(data + fcu->pia_offset * 8);
}

void scan_ipmi(union sdr_type *psdr)
{
	void *pfru;

	if (psdr->hdr.record_type == 0x11) {
		pfru = ipmi_read_fru(psdr->type11.addr,
				     psdr->type11.slave_addr,
				     psdr->type11.lun);
		if (pfru) {
			showfru(pfru);
			free(pfru);
		}
	}
}

int main(int argc, char *argv[])
{
	smbios_init(scan_smbios);
	_scandir("/sys/block", scanblock, 0);
	_scandir("/sys/class/net", scannet, 0);	
	ipmi_init(scan_ipmi);
	return 0;
}

