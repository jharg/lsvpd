/*
 *  Copyright (c) 2006-2016 Dell, Inc.
 *  by Jordan Hargrave <jordan_hargrave@dell.com>
 *  Licensed under the Lesser GNU General Public license, version 3.
 */
#define _XOPEN_SOURCE 600
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <inttypes.h>
#include <time.h>

#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/statfs.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <assert.h>

#include <linux/ipmi.h>
#include "ipmi.h"
#include "util.h"
static int maxsdrlen = 0x10;

int
ipmicmd(int sa, int lun, int netfn, int cmd, 
	int datalen, void *data,
	int resplen, int *rlen, void *resp)
{
	static int				msgid;
	struct ipmi_system_interface_addr	saddr;
	struct ipmi_ipmb_addr			iaddr;
	struct ipmi_addr			raddr;
	struct ipmi_req				req;
	struct ipmi_recv			rcv;
	fd_set					rfd;
	int					fd, rc;
	uint8_t					tresp[resplen+1];

	memset(resp, 0, resplen);
	fd = open("/dev/ipmi0", O_RDONLY);
	if (fd < 0)
		return -1;
#if 0
	printf("--ipmicmd: %.2x %.2x len=%d,%d\n", netfn, cmd,
	       datalen, resplen);
	dump(data, datalen);
#endif
	if (sa == BMC_SA) {
		saddr.addr_type = IPMI_SYSTEM_INTERFACE_ADDR_TYPE;
		saddr.channel = IPMI_BMC_CHANNEL;
		saddr.lun = 0;
		req.addr = (void *)&saddr;
		req.addr_len = sizeof(saddr);
	} else {
		iaddr.addr_type = IPMI_IPMB_ADDR_TYPE;
		iaddr.channel = 0;
		iaddr.slave_addr = sa;
		iaddr.lun = lun;
		req.addr = (void *)&iaddr;
		req.addr_len = sizeof(iaddr);
	}

	/* Issue command */
	req.msgid = ++msgid;
	req.msg.netfn = netfn;
	req.msg.cmd = cmd;
	req.msg.data_len = datalen;
	req.msg.data = data;
	rc = ioctl(fd, IPMICTL_SEND_COMMAND, (void *)&req);
	if (rc != 0) {
		perror("send");
		goto end;
	}

	/* Wait for Response */
	FD_ZERO(&rfd);
	FD_SET(fd, &rfd);
	rc = select(fd+1, &rfd, NULL, NULL, NULL);
	if (rc < 0) {
		perror("select");
		goto end;
	}

	/* Get response */
	rcv.msg.data = tresp;
	rcv.msg.data_len = resplen+1;
	rcv.addr = (void *)&raddr;
	rcv.addr_len = sizeof(raddr);
	rc = ioctl(fd, IPMICTL_RECEIVE_MSG_TRUNC, (void *)&rcv);
	if (rc != 0 && errno != EMSGSIZE) {
		perror("recv");
		goto end;
	}
	rc = rcv.msg.data[0];
	if (rc != 0) {
		rc = rcv.msg.data[0];
		//printf("IPMI Error: %.2x\n", rcv.msg.data[0]);
	} else {
		*rlen = rcv.msg.data_len - 1;
		memcpy(resp, rcv.msg.data + 1, *rlen);
	}
 end:
	close(fd);
	return rc;
}

/* Fru chassis:
   u8 version
   u8 length/8
   u8 type
   u8 partnum_length
   u8 partnum[n]
   u8 sernum_length
   u8 sernum[n]
   u8 fields[n]
   u8 0xC1
   u8 pad[n]
   u8 checksum
*/
char *_ipmi_sensor_name(uint8_t tc, void *n)
{
	char	 ascii6[] = " !\"#$%&\'{}*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_";
	char	 bcdplus[] = "0123456789 -.:,_";
	int	 slen;
	char	 *t;
	uint64_t xn;
	uint8_t	 *pn = n;
	int i;

	slen = tc & 0x1F;
	switch (tc >> 6) {
	case 0x00: /* unicode */
		t = strdup("unicodez");
		break;
	case 0x01: /* bcdplus */
		t = calloc(1, slen*2+4 + 10);
		strcpy(t, "bcd:");
		for (i = 0; i < slen; i++) {
			t[i*2+4] = bcdplus[(pn[i] >> 0) & 0xF];
			t[i*2+5] = bcdplus[(pn[i] >> 4) & 0xF];
		}
		break;
	case 0x02: /* ascii6 */
		t = calloc(1, slen*2 + 10);
		for (i = 0; i < slen; i += 3) {
			xn = pn[i] + (pn[i+1] << 8) + (pn[i+2] << 16);
			t[i*4+0] = ascii6[(xn >> 0) & 0x3F];
			t[i*4+1] = ascii6[(xn >> 6) & 0x3F];
			t[i*4+2] = ascii6[(xn >> 12) & 0x3F];
			t[i*4+3] = ascii6[(xn >> 18) & 0x3F];
		}
		break;
	case 0x03: /* ascii8 */
		t = calloc(1, slen + 10);
		memcpy(t, n, slen);
		break;
	}
	return t;
}

char *fru_string(uint8_t *src, uint8_t **end)
{
	uint8_t tlen = *src++;

	*end = src + (tlen & 0x3F);
	return _ipmi_sensor_name(tlen, src);
}

#if 0
void fru_product(uint8_t *ptr)
{
	uint8_t *pos;

	if (checksum(ptr, ptr[1]*8) != 0) {
		printf("bad checksum\n");
		return;
	}
	printf(" product ver:	%d\n", ptr[0] & 0xf);
	printf(" product lang:	%d\n", ptr[2]);
  
	pos = ptr+3;
	printf(" product mfctr: %s\n", fru_string(pos, &pos));
	printf(" product name : %s\n", fru_string(pos, &pos));
	printf(" product part : %s\n", fru_string(pos, &pos));
	printf(" product ver  : %s\n", fru_string(pos, &pos));
	printf(" product serial:%s\n", fru_string(pos, &pos));
	printf(" asset tag    : %s\n", fru_string(pos, &pos));
	printf(" file id      : %s\n", fru_string(pos, &pos));
}

void fru_chassis(uint8_t *ptr)
{
	uint8_t *pos;

	if (checksum(ptr, ptr[1] * 8) != 0) {
		printf("bad checksum\n");
		return;
	}
	printf(" chassis ver:	%d\n", ptr[0] & 0xF);
	printf(" chassis len:	%d\n", ptr[1] * 8);
	printf(" chassis type:	%d\n", ptr[2]);

	pos = ptr + 3;
	printf(" chassis part:	%s\n", fru_string(pos, &pos));
	printf(" chassis serial:%s\n", fru_string(pos, &pos));
}

void fru_board(uint8_t *ptr)
{
	int date;
	uint8_t *pos;

	if (checksum(ptr, ptr[1] * 8) != 0) {
		printf("bad checksum\n");
		return;
	}
	printf(" board ver : %d\n", ptr[0] & 0xF);
	printf(" board len : %d\n", ptr[1] * 8);
	printf(" board lang: %d\n", ptr[2]);

	date = ptr[3] + (ptr[4] << 8) + (ptr[5] << 16);
	printf(" board date:  %x\n", date);

	pos = ptr + 6;
	printf(" board mfcr:  %s\n", fru_string(pos, &pos));
	printf(" board prod:  %s\n", fru_string(pos, &pos));
	printf(" board serial:%s\n", fru_string(pos, &pos));
	printf(" board part:  %s\n", fru_string(pos, &pos));
	printf(" board file:  %s\n", fru_string(pos, &pos));
}

void showfru(void *data)
{
	struct fru_common *fcu = data;

	if (checksum(fcu, sizeof(*fcu)) != 0) {
		printf("bad fru checksum\n");
		return;
	}
	printf("version	   : %d\n", fcu->version & 0xF);
	printf("internal   : %d\n", fcu->iua_offset);
	printf("chassis	   : %d\n", fcu->cia_offset);
	printf("board	   : %d\n", fcu->ba_offset);
	printf("product	   : %d\n", fcu->pia_offset);
	printf("mra	   : %d\n", fcu->mra_offset);
	if (fcu->cia_offset)
		fru_chassis(data + fcu->cia_offset * 8);
	if (fcu->ba_offset)
		fru_board(data + fcu->ba_offset * 8);
	if (fcu->pia_offset)
		fru_product(data + fcu->pia_offset * 8);
}
#endif

void *
ipmi_read_fru(int sa, int id, int lun)
{
	uint8_t cmd[6];
	uint8_t data[64];
	uint8_t *fru;
	int frulen, i, rlen, off;

	memset(cmd, 0, sizeof(cmd));
	memset(data, 0, sizeof(data));
	cmd[0] = id;
	if (ipmicmd(sa, 0x00, STORAGE_NETFN, STORAGE_GET_FRU_INFO, 1,
		    cmd, sizeof(data), &rlen, data)) {
		return NULL;
	}
	frulen = (data[1] << 8) + data[0];

	off = 0;
	fru = malloc(frulen);
	for (i = 0; i < frulen; i += 16) {
		memset(cmd, 0, sizeof(cmd));
		cmd[0] = id;
		cmd[1] = i & 0xFF;
		cmd[2] = i >> 8;
		cmd[3] = 16;
		if (!ipmicmd(sa, 0x00, STORAGE_NETFN, STORAGE_READ_FRU, 4,
			     cmd, sizeof(data), &rlen, data)) {
			assert(off+data[0] <= frulen);
			memcpy(fru+off, data+1, data[0]);
			off += data[0];
		}
	}
	return fru;
}


int
get_sdr_partial(uint16_t recordId, uint16_t reserveId, uint8_t offset,
		uint8_t length, void *buffer, uint16_t *nxtRecordId)
{
	struct getsdr gsdr;
	uint8_t cmd[8 + length];
	int	len;

	gsdr.resid = reserveId;
	gsdr.recid = recordId;
	gsdr.offset = offset;
	gsdr.length = length;

	memset(cmd, 0, sizeof(cmd));
	if (ipmicmd(BMC_SA, 0, STORAGE_NETFN, STORAGE_GET_SDR, sizeof(gsdr),
		    &gsdr, 8 + length, &len, cmd)) {
		return -1;
	}
	if (nxtRecordId)
		*nxtRecordId = ((uint16_t *)cmd)[0];
	memcpy(buffer, cmd + 2, len - 2);
	return 0;
}

union sdr_type *
get_sdr(uint16_t recid, uint16_t *nxtrec)
{
	union sdr_type *psdr;
	uint16_t resid = 0;
	int len, sdrlen, offset;
	struct sdrhdr shdr;

	if (recid == 0xFFFF)
		return NULL;
	if (ipmicmd(BMC_SA, 0, STORAGE_NETFN, STORAGE_RESERVE_SDR,
		    0, NULL, sizeof(resid), &len, &resid)) {
		return NULL;
	}
	if (get_sdr_partial(recid, resid, 0, sizeof(shdr), &shdr, nxtrec)) {
		return NULL;
	}

	/* Calculate length of SDR */
	sdrlen = sizeof(shdr) + shdr.record_length;
	psdr = malloc(sdrlen);
	if (!psdr)
		return NULL;
	memcpy(&psdr->hdr, &shdr, sizeof(shdr));

	/* Get SDR objects */
	for (offset = sizeof(shdr); offset < sdrlen; offset += maxsdrlen) {
		len = sdrlen - offset;
		if (len > maxsdrlen)
			len = maxsdrlen;
		if (get_sdr_partial(recid, resid, offset, len,
				    (void *)psdr + offset, NULL)) {
			free(psdr);
			return NULL;
		}
	}
	return psdr;
}

void ipmi_init(void (*fn)(union sdr_type *))
{
	uint16_t rec;
	union sdr_type *psdr;

	rec = 0;
	while ((psdr = get_sdr(rec, &rec)) != NULL) {
		fn(psdr);
		free(psdr);
	}
}
