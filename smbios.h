#ifndef __smbios_h__
#define __smbios_h__

#define __PACKED __attribute__((packed))

struct smbios_ep {
	uint8_t  ep_anchor[4];
	uint8_t  ep_cksum;
	uint8_t  ep_len;
	uint8_t  ep_mjr;
	uint8_t  ep_mnr;
	uint16_t ep_maxsz;
	uint8_t  ep_rev;
	uint8_t  ep_fmt[5];
	uint8_t  dmi_anchor[5];
	uint8_t  dmi_cksum;
	uint16_t dmi_tbllen;
	uint32_t dmi_tbladdr;
	uint16_t dmi_numtbl;
	uint8_t  dmi_bcdrev;
} __PACKED;

struct smbios_hdr {
	uint8_t  type;
	uint8_t  length;
	uint16_t handle;
} __PACKED;

union smbios_type {
	struct smbios_hdr hdr;
	/* Type 0: BIOS Information */
	struct {
		struct smbios_hdr hdr;
		uint8_t mf;
		uint8_t ver;
		uint16_t sseg;
		uint8_t reldate;
		uint8_t romsz;
		uint64_t flags;
		uint8_t  res[2];
		uint8_t  mjr;
		uint8_t  mnr;
		uint8_t  ecmjr;
		uint8_t  ecmnr;
	} __PACKED type0;
	/* Type 1: System Information */
	struct {
		struct smbios_hdr hdr;
		uint8_t mf;
		uint8_t pn;
		uint8_t ver;
		uint8_t sn;
	} __PACKED type1;
	/* Type 17: Memory Device */
	struct {
		struct smbios_hdr hdr;
		uint16_t pmah;
		uint16_t meih;
		uint16_t tw;
		uint16_t dw;
		uint16_t sz;
		uint8_t  ff;
		uint8_t  dset;
		uint8_t  loc;
		uint8_t  bank;
		uint8_t  mt;
		uint16_t td;
		uint16_t spd;
		uint8_t  mf;
		uint8_t  sn;
		uint8_t  at;
		uint8_t  pn;
	} __PACKED type17;
} __PACKED;

void smbios_init(void (*fn)(union smbios_type *, char *[]));
#endif
