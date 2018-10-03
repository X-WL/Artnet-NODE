#ifndef _artnet_node_defined
#define _artnet_node_defined

#include "common.h"

#define UNICAST               0
#define BROADCAST             1
#define ARNET_HEADER_SIZE     17

#define short_get_high_byte(x)((HIGH_BYTE & x) >> 8)
#define short_get_low_byte(x)(LOW_BYTE & x)
#define bytes_to_short(h,l)( ((h << 8) & 0xff00) | (l & 0x00FF) );

struct artnet_node_s {
  uint8_t  id           [8];
  uint16_t opCode;
  uint8_t  localIp      [4];
  uint16_t localPort;
  uint8_t  verH;
  uint8_t  ver;
  uint8_t  subH;
  uint8_t  sub;
  uint8_t  oemH;
  uint8_t  oem;
  uint8_t  ubea;
  uint8_t  status;
  uint8_t  etsaman      [2];
  uint8_t  shortname    [ARTNET_SHORT_NAME_LENGTH];
  uint8_t  longname     [ARTNET_LONG_NAME_LENGTH];
  uint8_t  nodereport   [ARTNET_REPORT_LENGTH];
  uint8_t  numbportsH;
  uint8_t  numbports;
  uint8_t  porttypes	[4];
  uint8_t  goodinput    [ARTNET_MAX_PORTS];
  uint8_t  goodoutput   [ARTNET_MAX_PORTS];
  uint8_t  swin         [ARTNET_MAX_PORTS];
  uint8_t  swout        [ARTNET_MAX_PORTS];
  uint8_t  swvideo;
  uint8_t  swmacro;
  uint8_t  swremote;
  uint8_t  style;
  uint8_t  remoteIp     [4];
  uint16_t remotePort;
  uint8_t  broadcastIp  [4];
  uint8_t  gateway      [4];
  uint8_t  subnetMask   [4];
  uint8_t  mac          [6];
  uint8_t  ProVerH;
  uint8_t  ProVer;
  uint8_t  ttm;
} __attribute__((packed));

typedef struct artnet_node_s artnet_node_t;

#endif
