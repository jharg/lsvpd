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

#endif
