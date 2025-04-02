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

static struct packet_s *pkt; /* Shared packet pointer */

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
  syncro_t *syncro = args_syncro(arg);
  struct radio_config_s config = args_config(arg)->radio;

  int radio;
  int err;
  ssize_t b_sent;
  pkt = NULL;

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

  err = ioctl(radio, WLIOC_SETRADIOFREQ, config.frequency);
  ioctl_err_cancel("Couldn't set radio frequency", err);
  printf("Radio frequency set to %lu Hz\n", config.frequency);

  /* Set operating bandwidth */

  err = ioctl(radio, WLIOC_SETBANDWIDTH, config.bandwidth);
  ioctl_err_cancel("Couldn't set radio bandwidth", err);
  printf("Radio bandwidth set to %lu kHz\n", config.bandwidth);

  /* Set operating preamble length */

  err = ioctl(radio, WLIOC_SETPRLEN, config.prlen);
  ioctl_err_cancel("Couldn't set radio preamble length", err);
  printf("Radio preamble length set to %u\n", config.prlen);

  /* Set operating spread factor */

  err = ioctl(radio, WLIOC_SETSPREAD, config.spread);
  ioctl_err_cancel("Couldn't set radio spread factor", err);
  printf("Radio spread factor set to sf%u\n", config.spread);

  /* Set operating modulation */

  err = ioctl(radio, WLIOC_SETMOD, config.mod);
  ioctl_err_cancel("Couldn't set radio modulation", err);
  printf("Radio modulation set to %u\n", config.mod);

  /* Set operating transmission power */

  err = ioctl(radio, WLIOC_SETTXPOWER, &config.txpower);
  ioctl_err_cancel("Couldn't set radio transmit power", err);
  printf("Radio transmit power set to %.2f\n", config.txpower);

  printf("Radio configured.\n");
#endif

  /* Infinitely read sensors and send packets out */

  for (;;)
    {

      /* Wait for packet */

      err = syncro_get_untransmitted(syncro, &pkt);
      if (err)
        {
          fprintf(stderr, "Error getting packet: %d\n", err);
        }

      /* If packet is NULL, release it and try again */

      if (pkt == NULL)
        {
          fprintf(stderr, "Shared packet is NULL\n");
          syncro_mark_transmitted(syncro);
          continue;
        }

      /* Send packet over radio */

      printf("Transmitting...\n");
      // TODO: copy packet to our own intermediate buffer so it's not locked
      // for a long time
      b_sent = write(radio, pkt->contents, pkt->len);
      if (b_sent < 0)
        {
          fprintf(stderr, "Packet failed to send: %d\n", errno);
          syncro_mark_transmitted(syncro);
          continue; /* Try a new packet */
        }

      syncro_mark_transmitted(syncro);
      printf("Transmitted\n");
    }

  pthread_cleanup_pop(1); /* Close radio */
  return (void *)(long)err;
}
