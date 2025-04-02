/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "../common/configuration.h"
#include "../packets/packets.h"
#include "arguments.h"
#include "syncro.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* Buffer for packet header construction */

static struct packet_hdr_s pkt_hdr;

/* Packet under construction */

static struct packet_s pkt;

/* Buffer for constructing packets */

static uint8_t pkt_buf[CONFIG_PYGMY_PACKET_MAXLEN];

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
  packet_init(&pkt, pkt_buf);

  for (;;)
    {
      /* Construct a packet from sensor data */

      for (;;)
        {
          /* Reset packet for fresh construction */

          packet_init(&pkt, pkt_buf);

          /* Add header to packet */

          err = packet_push(&pkt, &pkt_hdr, sizeof(pkt_hdr));

          if (err)
            {
              fprintf(stderr, "Out of packet space!\n");
            }

          /* Read sensors until there's no more space TODO */

          /* Following packet_push() ... */

          if (err == ENOMEM)
            {
              break;
            }
        }

      /* Share this packet with other threads using syncro monitor */

      err = syncro_publish(syncro, &pkt);
      if (err)
        {
          fprintf(stderr, "Couldn't publish new packet: %d\n", err);
        }
    }

  return 0;
}
