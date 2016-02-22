#ifndef __ipmi_h__
#define __ipmi_h__

#define BMC_SA    0x20

#define APP_NETFN			0x06
#define APP_GET_DEVICE_ID		0x01
#define APP_SETSYSINFO                  0x58
#define APP_GETSYSINFO                  0x59

#define STORAGE_NETFN			0x0A
#define STORAGE_GET_FRU_INFO            0x10
#define STORAGE_READ_FRU                0x11
#define STORAGE_WRITE_FRU               0x12
#define STORAGE_RESERVE_SDR		0x22
#define STORAGE_GET_SDR			0x23
#define STORAGE_RESERVE_SEL		0x42
#define STORAGE_GET_SEL			0x43

#define SE_NETFN			0x04
#define SE_SET_PEF_CONFIG_PARAMS        0x12
#define SE_GET_PEF_CONFIG_PARAMS        0x13
#define SE_GET_SENSOR_THRESHOLD		0x27
#define SE_GET_SENSOR_READING		0x2D

#define TRANSPORT_NETFN                 0x0c
#define TR_GET_LAN_CONFIG_PARAMS        0x02

void ipmi_scan();

#define __PACKED __attribute__((packed))

struct getsdr {
	uint16_t      resid;
	uint16_t      recid;
	uint8_t	      offset;
	uint8_t	      length;
} __PACKED;

struct sdrhdr {
	uint16_t record_id;
	uint8_t	 sdr_version;
	uint8_t	 record_type;
	uint8_t	 record_length;
} __PACKED;

union sdr_type {
	struct sdrhdr		hdr;
	struct {
		struct sdrhdr	sdrhdr;
		uint8_t		addr;
		uint8_t		slave_addr;
		uint8_t		lun;
		uint8_t		channel;

		uint8_t		rsvd;
		uint8_t		type;
		uint8_t		modifier;
		uint8_t		entity_id;
		uint8_t		entity_instance;
		uint8_t		oem;
		uint8_t		typelen;
		uint8_t		name[1];
	} __PACKED type11;
} __PACKED;

struct fru_common {
	uint8_t version;
	uint8_t iua_offset;  /* internal use area */
	uint8_t cia_offset;  /* chassis info area */
	uint8_t ba_offset;   /* board area */
	uint8_t pia_offset;  /* product info area */
	uint8_t mra_offset;  /* multirecord area */
	uint8_t pad;
	uint8_t cksum;
};
#endif
