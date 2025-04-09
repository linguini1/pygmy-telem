/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
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

/* Array length helper */

#define array_len(arr) sizeof(arr) / sizeof((arr)[0])

/* Sensor indexes */

#define SENSOR_BARO 0

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

/* Buffer to store blocks under construction temporarily */

static uint8_t block_buf[32];

/* Buffer to store some sensor data temporarily */

static uint8_t uorb_data[32];

/* uORB sensor polling */

struct pollfd fds[] = {
    [SENSOR_BARO] = {.fd = -1, .events = POLLIN, .revents = 0},
};

/* uORB sensor metadata */

struct orb_metadata const *metas[] = {
    [SENSOR_BARO] = NULL,
};

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

  /* Get sensor metadata */

  metas[SENSOR_BARO] = orb_get_meta("sensor_baro");

  /* Subscribe to sensors */

  for (int i = 0; i < array_len(fds); i++)
    {
      fds[i].fd = orb_subscribe(metas[i]);
      if (fds[i].fd < 0)
        {
          fprintf(stderr, "Failed to open sensor device: %d\n", errno);
        }
    }

  /* Set sensor frequencies TODO */

  err = orb_set_frequency(fds[SENSOR_BARO].fd, 50);
  if (err < 0)
    {
      fprintf(stderr, "Couldn't set barometer frequency.");
    }

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

      printf("Packet constructing\n");
      for (;;)
        {
          /* Read sensors until there's no more space in the current packet.
           * Poll forever until some data is available */

          err = poll(fds, array_len(fds), -1);

          if (err < 0)
            {
              fprintf(stderr, "Error polling sensors: %d\n", errno);
              continue;
            }

          /* Polling worked and we have some data to package */

          for (int i = 0; i < array_len(fds); i++)
            {
              /* Some data available on this sensor */

              if (fds[i].revents & POLLIN)
                {
                  fds[i].revents = 0; /* Reset events */

                  err = orb_copy(metas[i], fds[i].fd, uorb_data);
                  if (err < 0)
                    {
                      fprintf(stderr, "Error copying uORB data: %d\n", err);
                      continue;
                    }

                  /* We got some data, package it accordingly */

                  // TODO: make packaging sensor dependent

                  block_init_pressure((void *)block_buf, (void *)uorb_data);
                  err = packet_push_block(pkt_cur, PACKET_PRESS, block_buf,
                                          sizeof(press_p));
                  if (err == ENOMEM) break; /* Exit collection loop */

                  /* Temperature data */

                  block_init_temp((void *)block_buf, (void *)uorb_data);
                  err = packet_push_block(pkt_cur, PACKET_TEMP, block_buf,
                                          sizeof(temp_p));
                  if (err == ENOMEM) break;

                  /* Accelerometer data TODO */

                  /* Gyro data TODO */

                  /* Magnetometer data TODO */

                  /* Altitude data TODO */

                  /* GPS data TODO */
                }
            }

          /* We exited the collection loop after polling, if out of memory
           * then break completely to finish this packet */

          if (err == ENOMEM) break;
        }

      /* Share this packet with other threads using syncro monitor */

      printf("Packet publishing\n");
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
