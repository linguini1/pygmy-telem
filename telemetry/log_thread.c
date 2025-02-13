/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

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
 * Private Functions
 ****************************************************************************/

static void close_fd(void *arg) { close(*(int *)(arg)); }

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

void *log_thread(void *arg) {

  int err;
  int pwrfs;

  /* Open power safe file system */

  pwrfs = open(CONFIG_PYGMY_TELEM_PWRFS "/somefile.bin", O_RDWR);
  if (pwrfs < 0) {
    err = errno;
    fprintf(stderr, "Couldn't open power safe log file: %d\n", err);
    pthread_exit((void *)(long)err);
  }

  pthread_cleanup_push(close_fd, &pwrfs);

  /* Log sensor data continuously */

  for (;;) {
    printf("Logged\n");
  }

  pthread_cleanup_pop(1); /* Close pwrfs */

  return (void *)(long)(err);
}
