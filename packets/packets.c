/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <errno.h>
#include <string.h>

#include "../packets/packets.h"

/****************************************************************************
 * Pre-processor definitions
 ****************************************************************************/

#define us_to_ms(us) ((us) / 1000)

#define RADS_TO_DEG (180.0f / 3.141592653f)

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

/****************************************************************************
 * Name: block_init_pressure
 *
 * Description:
 *   Initialize a pressure block
 *
 * Arguments:
 *  blk - The pressure block to initialize
 *  data - The uORB pressure data to initialize with
 *
 ****************************************************************************/

void block_init_pressure(press_p *blk, struct sensor_baro *data)
{
  blk->time = us_to_ms(data->timestamp);
  blk->press = (int32_t)(data->pressure * 100.0f);
}

/****************************************************************************
 * Name: block_init_temp
 *
 * Description:
 *   Initialize a temperature block
 *
 * Arguments:
 *  blk - The temperature block to initialize
 *  data - The uORB temperature data to initialize with
 *
 ****************************************************************************/

void block_init_temp(temp_p *blk, struct sensor_baro *data)
{
  blk->time = us_to_ms(data->timestamp);
  blk->temp = (int32_t)(data->temperature * 1000.0f);
}

/****************************************************************************
 * Name: block_init_accel
 *
 * Description:
 *   Initialize an acceleration block
 *
 * Arguments:
 *  blk - The acceleration block to initialize
 *  data - The uORB acceleration data to initialize with
 *
 ****************************************************************************/

void block_init_accel(accel_p *blk, struct sensor_accel *data)
{
  blk->time = us_to_ms(data->timestamp);
  blk->x = (int16_t)(data->x * 100.0f);
  blk->y = (int16_t)(data->y * 100.0f);
  blk->z = (int16_t)(data->z * 100.0f);
}

/****************************************************************************
 * Name: block_init_gyro
 *
 * Description:
 *   Initialize a gyro block
 *
 * Arguments:
 *  blk - The gyro block to initialize
 *  data - The uORB gyro data to initialize with
 *
 ****************************************************************************/

void block_init_gyro(gyro_p *blk, struct sensor_gyro *data)
{
  blk->time = us_to_ms(data->timestamp);
  blk->x = (int16_t)(data->x * RADS_TO_DEG * 10.0f);
  blk->y = (int16_t)(data->y * RADS_TO_DEG * 10.0f);
  blk->z = (int16_t)(data->z * RADS_TO_DEG * 10.0f);
}

/****************************************************************************
 * Name: block_init_mag
 *
 * Description:
 *   Initialize a magnetometer block
 *
 * Arguments:
 *  blk - The magnetometer block to initialize
 *  data - The uORB magnetometer data to initialize with
 *
 ****************************************************************************/

void block_init_mag(mag_p *blk, struct sensor_mag *data)
{
  blk->time = us_to_ms(data->timestamp);
  blk->x = (int16_t)(data->x * RADS_TO_DEG * 10.0f);
  blk->y = (int16_t)(data->y * RADS_TO_DEG * 10.0f);
  blk->z = (int16_t)(data->z * RADS_TO_DEG * 10.0f);
}
