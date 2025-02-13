#ifndef _PYGMY_PACKET_H_
#define _PYGMY_PACKET_H_

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdint.h>
#include <string.h>

#include "../common/configuration.h"

/****************************************************************************
 * Pre-processor definitions
 ****************************************************************************/

#define PACKED __attribute__((packed))

/* Maximum packet length in bytes */

#ifndef CONFIG_PYGMY_PACKET_MAXLEN
#define CONFIG_PYGMY_PACKET_MAXLEN 255
#endif

/****************************************************************************
 * Public Types
 ****************************************************************************/

/* Header sent with all packets */

struct packet_hdr_s {
  char callsign[CONFIG_PYGMY_CALLSIGN_LEN]; /* Call sign */
  uint8_t num;                              /* Rolling counter */
} PACKED;

/* Packet representation. */

struct packet_s {
  uint8_t *contents; /* Packet raw contents buffer */
  size_t len;        /* Packet length in bytes */
};

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

void packet_init(struct packet_s *pkt, void *buf);
void packet_header_init(struct packet_hdr_s *hdr, char *callsign, uint8_t num);
int packet_append(struct packet_s *pkt, const void *buf, size_t nbytes);

#endif /* _PYGMY_PACKET_H_ */
