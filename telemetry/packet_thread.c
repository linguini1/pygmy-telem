/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <uORB/uORB.h>

#include "../common/configuration.h"
#include "../packets/packets.h"
#include "arguments.h"
#include "syncro.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define deref(data, kind) (*((kind *)(data)))

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* Buffer for packet header construction */

static struct packet_hdr_s pkt_hdr;

/* Packet under construction */

static struct packet_s pkt_a;
static struct packet_s pkt_b;

/* Buffer for constructing packets */

static uint8_t pkt_bufa[CONFIG_PYGMY_PACKET_MAXLEN];
static uint8_t pkt_bufb[CONFIG_PYGMY_PACKET_MAXLEN];

static struct packet_s *pkt_cur = &pkt_a;
static struct packet_s *pkt_prev = &pkt_b;

/* Buffer to store some sensor data temporarily */

static uint8_t uorb_data[32];

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: packet_thread
 *
 * Description:
 *   Constructs packets of sensor data for consumption by radio and logging
 *   thread.
 *
 ****************************************************************************/

void *packet_thread(void *arg)
{
  int err;

  /* Unpack arguments */

  syncro_t *syncro = args_syncro(arg);
  struct configuration_s *config = args_config(arg);

  /* Prepare packet header for constructing packets */

  packet_header_init(&pkt_hdr, config->radio.callsign, 0);

  /* Prepare packets for construction */

  packet_init(&pkt_a, pkt_bufa);
  packet_init(&pkt_b, pkt_bufb);

  /* Subscribe to sensors */

  int baro_fd = orb_subscribe(orb_get_meta("sensor_baro"));
  if (baro_fd < 0)
    {
      fprintf(stderr, "Failed to open sensor_baro: %d\n", errno);
    }

  press_p pressure = {
      .time = 0,
      .press = 1012,
  };

  for (;;)
    {
      /* Reset current packet for fresh construction */

      packet_reset(pkt_cur);

      /* Add header to packet */

      err = packet_push(pkt_cur, &pkt_hdr, sizeof(pkt_hdr));
      if (err)
        {
          fprintf(stderr, "Out of packet space!\n");
        }

      /* Construct a packet from sensor data */

      for (;;)
        {
          /* Read sensors until there's no more space */

          err =
              orb_copy_multi(baro_fd, uorb_data, sizeof(struct sensor_baro));
          if (err < 0)
            {
              fprintf(stderr, "Couldn't get barometer data: %d\n", errno);
            }

          block_init_pressure(&pressure, (struct sensor_baro *)uorb_data);

          err = packet_push_block(pkt_cur, PACKET_PRESS, &pressure,
                                  sizeof(pressure));

          if (err == ENOMEM)
            {
              break;
            }
        }

      /* Share this packet with other threads using syncro monitor */

      err = syncro_publish(syncro, pkt_cur);
      if (err)
        {
          fprintf(stderr, "Couldn't publish new packet: %d\n", err);
        }

      /* Swap to already consumed packet buffer for next iteration */

      struct packet_s *tmp = pkt_cur;
      pkt_cur = pkt_prev;
      pkt_prev = tmp;

      /* Update packet sequence number */

      pkt_hdr.num++;
    }

  return 0;
}
