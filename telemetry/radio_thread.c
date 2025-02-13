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

#ifdef CONFIG_LPWAN_RN2XX3
#include <nuttx/wireless/lpwan/rn2xx3.h>
#endif

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#ifndef CONFIG_PYGMY_TELEM_RADIOPATH
#define CONFIG_PYGMY_TELEM_RADIOPATH "/dev/rn2903"
#endif

/* Handle ioctl errors by storing the value of errno, printing the error
 * information and exiting the thread. This is a cancellation point.
 */

#define ioctl_err_cancel(msg, err)                                           \
  do                                                                         \
    {                                                                        \
      if (err < 0)                                                           \
        {                                                                    \
          err = errno;                                                       \
          fprintf(stderr, msg ": %d\n", err);                                \
          pthread_exit((void *)(long)err);                                   \
        }                                                                    \
    }                                                                        \
  while (0)

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
 * Private Functions
 ****************************************************************************/

static void close_fd(void *arg) { close(*(int *)(arg)); }

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: radio_thread
 *
 * Description:
 *   Performs radio transmission of telemetry data.
 *
 ****************************************************************************/

void *radio_thread(void *arg)
{

  struct radio_config_s *config = (struct radio_config_s *)(arg);
  int radio;
  int err;
  ssize_t b_sent;

  /* Open radio file descriptor */

  radio = open(CONFIG_PYGMY_TELEM_RADIOPATH, O_RDWR);
  if (radio < 0)
    {
      err = errno;
      fprintf(stderr, "Couldn't open radio: %d\n", err);
      pthread_exit((void *)(long)err);
    }

  pthread_cleanup_push(close_fd, &radio);

  /* Configure radio parameters. This configuration is only performed if the
   * radio transceiver driver is enabled, because this enables mocking using
   * another character driver like a file in place of the radio during tests.
   */

#ifdef CONFIG_LPWAN_RN2XX3
  /* Set operating frequency */

  err = ioctl(radio, WLIOC_SETRADIOFREQ, config->frequency);
  ioctl_err_cancel("Couldn't set radio frequency", err);
  printf("Radio frequency set to %lu Hz\n", config->frequency);

  /* Set operating bandwidth */

  err = ioctl(radio, WLIOC_SETBANDWIDTH, config->bandwidth);
  ioctl_err_cancel("Couldn't set radio bandwidth", err);
  printf("Radio bandwidth set to %lu kHz\n", config->bandwidth);

  /* Set operating preamble length */

  err = ioctl(radio, WLIOC_SETPRLEN, config->prlen);
  ioctl_err_cancel("Couldn't set radio preamble length", err);
  printf("Radio preamble length set to %u\n", config->prlen);

  /* Set operating spread factor */

  err = ioctl(radio, WLIOC_SETSPREAD, config->spread);
  ioctl_err_cancel("Couldn't set radio spread factor", err);
  printf("Radio spread factor set to sf%u\n", config->spread);

  /* Set operating modulation */

  err = ioctl(radio, WLIOC_SETMOD, config->mod);
  ioctl_err_cancel("Couldn't set radio modulation", err);
  printf("Radio modulation set to %u\n", config->mod);

  /* Set operating transmission power */

  err = ioctl(radio, WLIOC_SETTXPOWER, &config->txpower);
  ioctl_err_cancel("Couldn't set radio transmit power", err);
  printf("Radio transmit power set to %.2f\n", config->txpower);

  printf("Radio configured.\n");
#endif

  /* Prepare packet header for constructing packets */

  packet_header_init(&pkt_hdr, config->callsign, 0);
  packet_init(&pkt, pkt_buf);

  /* Infinitely read sensors and send packets out */

  for (;;)
    {

      /* Reset packet for fresh construction */

      packet_init(&pkt, pkt_buf);

      /* Add header to packet */

      err = packet_append(&pkt, &pkt_hdr, sizeof(pkt_hdr));

      if (err)
        {
          fprintf(stderr, "Out of packet space!\n");
        }

      /* Read sensors until there's no more space TODO */

      /* Send packet over radio */

      printf("Transmitting...\n");
      b_sent = write(radio, pkt.contents, pkt.len);
      if (b_sent < 0)
        {
          fprintf(stderr, "Packet failed to send: %d\n", errno);
          continue; /* Try a new packet */
        }
      printf("Transmitted\n");

      /* Packet is complete, increment sequence number */

      pkt_hdr.num++;
    }

  pthread_cleanup_pop(1); /* Close radio */
  return (void *)(long)err;
}
