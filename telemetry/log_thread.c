/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

#include "../packets/packets.h"
#include "arguments.h"
#include "syncro.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#ifndef CONFIG_PYGMY_TELEM_PWRFS
#define CONFIG_PYGMY_TELEM_PWRFS "/pwrfs"
#endif

#ifndef CONFIG_PYGMY_TELEM_USRFS
#define CONFIG_PYGMY_TELEM_USRFS "/usrfs"
#endif

/****************************************************************************
 * Private Data
 ****************************************************************************/

struct packet_s *pkt; /* Shared packet pointer */

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void close_fd(void *arg) { close(*((int *)(arg))); }

/****************************************************************************
 * Name: log_thread
 *
 * Description:
 *   Performs logging operations to onboard storage.
 *
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/

void *log_thread(void *arg)
{
  syncro_t *syncro = args_syncro(arg);

  int err;
  int pwrfs;
  ssize_t b_written;
  pkt = NULL;

  /* Open power safe file system */

  pwrfs = open(CONFIG_PYGMY_TELEM_PWRFS "/somefile.bin", O_WRONLY | O_CREAT);
  if (pwrfs < 0)
    {
      err = errno;
      fprintf(stderr, "Couldn't open power safe log file: %d\n", err);
      pthread_exit((void *)(long)err);
    }

  pthread_cleanup_push(close_fd, pwrfs);

  /* Log sensor data continuously */

  unsigned count = 0;
  for (;;)
    {

      /* Wait for unlogged packet */

      err = syncro_get_unlogged(syncro, &pkt);
      if (err)
        {
          fprintf(stderr, "Error getting shared packet: %d\n", err);
          continue; /* Try again */
        }

      /* If packet was NULL, mark it as logged and try again */

      if (pkt == NULL)
        {
          fprintf(stderr, "Shared packet was NULL.\n");
          syncro_mark_logged(syncro);
          continue;
        }

      /* Log packet */

      printf("Logging #%u...\n", count);
      b_written = dprintf(pwrfs, "Some data #%u\n", count);
      if (b_written <= 0)
        {
          err = errno;
          fprintf(stderr, "Couldn't write data to logfile: %d\n", err);
          continue;
        }

      /* Mark packet as logged */

      syncro_mark_logged(syncro);

      /* Sync after 100 packets logged */

      if (count % 100 == 0)
        {
          err = fsync(pwrfs);
          if (err < 0)
            {
              err = errno;
              fprintf(stderr, "Couldn't sync logfile: %d\n", err);
            }
        }

      printf("Logged #%u.\n", count);
      count++;
    }

  pthread_cleanup_pop(1); /* Close pwrfs */

  return (void *)(long)(err);
}
