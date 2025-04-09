/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <errno.h>
#include <string.h>

#include "../packets/packets.h"

/****************************************************************************
 * Pre-processor definitions
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: packet_header_init
 *
 * Description:
 *   Initialize a packet header with a call sign and sequence number.
 *   If `callsign` is too long, it will be truncated. If `callsign` is too
 *   short, it will be 0 post-padded.
 *
 ****************************************************************************/

void packet_header_init(struct packet_hdr_s *hdr, char *callsign, uint8_t num)
{
  int len;

  hdr->num = num;
  len = strlen(callsign);

  /* If user callsign is less than maximum, 0 pad the rest */

  if (len < CONFIG_PYGMY_CALLSIGN_LEN)
    {
      memcpy(hdr->callsign, callsign, len);
      memset(&hdr->callsign[len], 0, CONFIG_PYGMY_CALLSIGN_LEN - len);
    }

  /* Otherwise, truncate the call sign to the maximum */

  else
    {
      memcpy(hdr->callsign, callsign, CONFIG_PYGMY_CALLSIGN_LEN);
    }
}

/****************************************************************************
 * Name: packet_init
 *
 * Description:
 *   Initialize a radio packet.
 *
 * Arguments:
 *   pkt - The packet to initialize
 *   buf - A buffer of at least `CONFIG_PYGMY_PACKET_MAXLEN` bytes.
 *
 ****************************************************************************/

void packet_init(struct packet_s *pkt, void *buf)
{
  pkt->contents = buf;
  pkt->len = 0;
}

/****************************************************************************
 * Name: packet_reset
 *
 * Description:
 *   Resets a radio packet for more writing.
 *
 * Arguments:
 *   pkt - The packet to reset
 *
 ****************************************************************************/

void packet_reset(struct packet_s *pkt) { pkt->len = 0; }

/****************************************************************************
 * Name: packet_push
 *
 * Description:
 *   Append data to the radio packet.
 *
 * Arguments:
 *  pkt - The packet to append to
 *  buf - The data to append
 *  nbytes - The length of the data to append in bytes
 *
 * Returns:
 *  0 on success, ENOMEM if insufficient space is available in the
 *  packet.
 *
 ****************************************************************************/

int packet_push(struct packet_s *pkt, const void *buf, size_t nbytes)
{
  if (pkt->len + nbytes > CONFIG_PYGMY_PACKET_MAXLEN)
    {
      return ENOMEM;
    }

  memcpy(&pkt->contents[pkt->len], buf, nbytes);
  pkt->len += nbytes;
  return 0;
}

/****************************************************************************
 * Name: packet_push_block
 *
 * Description:
 *   Append a block to the radio packet.
 *
 * Arguments:
 *  pkt - The packet to append to
 *  kind - The block type
 *  block - The block to append
 *  nbytes - The length of the block to append in bytes
 *
 * Returns:
 *  0 on success, ENOMEM if insufficient space is available in the
 *  packet.
 *
 ****************************************************************************/

int packet_push_block(struct packet_s *pkt, const uint8_t kind,
                      const void *block, size_t nbytes)
{
  if (pkt->len + nbytes + sizeof(kind) > CONFIG_PYGMY_PACKET_MAXLEN)
    {
      return ENOMEM;
    }

  /* NOTE: cannot get to this point if there isn't enough room left, so errors
   * can be ignored */

  packet_push(pkt, &kind, sizeof(kind));
  packet_push(pkt, block, nbytes);
  return 0;
}
