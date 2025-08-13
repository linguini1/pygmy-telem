/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

#include "../packets/packets.h"
#include "arguments.h"
#include "syncro.h"
#include "syslogging.h"

/****************************************************************************
 * Private Data
 ****************************************************************************/

struct packet_s *pkt; /* Shared packet pointer */

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: close_fd
 *
 * Description:
 *   Closes the currently open file descriptor as a *`pthread_cleanup`
 *   routine.
 ****************************************************************************/

static void close_fd(void *arg) { close(*((int *)(arg))); }

/****************************************************************************
 * Name: logfile_next
 *
 * Description:
 *   Closes the currently open file descriptor and re-populates it with a new
 *   one of the next log file. The next log file will have a file-name with
 *   the next sequence number.
 *
 *   NOTE: Does not close file descriptor `fd` if it is a negative number.
 ****************************************************************************/

static int logfile_next(int *fd, unsigned *seqnum)
{
  int err;
  char filename[sizeof(CONFIG_PYGMY_TELEM_PWRFS "/") + 25];

  /* Close current file if valid */

  if (*fd > 0)
    {
      err = close(*fd);
      if (err < 0)
        {
          return errno;
        }
    }

  /* Create new file with file name of the next sequence number */

  snprintf(filename, sizeof(filename), CONFIG_PYGMY_TELEM_PWRFS "/log%d.bin",
           *seqnum);

  /* Create and open this file in write mode */

  err = open(filename, O_WRONLY | O_CREAT);
  if (err < 0)
    {
      pyerr("Couldn't open log file '%s': %d\n", filename, errno);
      return errno;
    }

  *fd = err;    /* File descriptor was returned */
  *seqnum += 1; /* Safe to increment sequence number */
  return 0;
}

/****************************************************************************
 * Name: parse_seqnum
 *
 * Description:
 *   Parses a number out of a log filename.
 ****************************************************************************/

static unsigned parse_seqnum(const char *str)
{
  const char *start = str;

  /* Find start digit */

  while (!isdigit(*start))
    {
      start++;
    }

  return strtoul(start, NULL, 10);
}

/****************************************************************************
 * Name: logfile_cur_seqnum
 *
 * Description:
 *   Reads through the current log files to find the greatest sequence number.
 ****************************************************************************/

static int logfile_cur_seqnum(unsigned *seqnum)
{
  unsigned maxseq = 0;
  unsigned seq = 0;
  struct dirent *de;

  /* Get power safe directory */

  DIR *dir = opendir(CONFIG_PYGMY_TELEM_PWRFS);
  if (dir == NULL)
    {
      return errno;
    }

  /* Go through all files in the directory and find out their name to parse
   * out the sequence number.
   *
   * NOTE: manpages for `readdir` say to set `errno` to zero prior.
   */

  errno = 0;
  while ((de = readdir(dir)) != NULL)
    {
      /* Skip non-files just in case */

      if (de->d_type != DT_REG)
        {
          continue;
        }

      /* WARNING: This parsing code makes the assumption that all log file
       * names have the format "log<n>.bin", and that no other file names will
       * be present in the power safe directory. */

      seq = parse_seqnum(de->d_name);
      if (seq > maxseq)
        {
          maxseq = seq;
        }
    }

  /* `readdir` returned NULL because of error, not because of end of entries
   */

  if (errno != 0)
    {
      closedir(dir);
      return errno;
    }

  *seqnum = maxseq + 1; /* Use next highest */
  closedir(dir);
  return 0;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: log_thread
 *
 * Description:
 *   Performs logging operations to onboard storage.
 ****************************************************************************/

void *log_thread(void *arg)
{
  syncro_t *syncro = args_syncro(arg);
  int err;
  int pwrfs = -1;
  unsigned seqnum = 0;
  ssize_t b_written;
  pkt = NULL;

  pyinfo("Log thread started.\n");

  /* Get the next available sequence number */

  err = logfile_cur_seqnum(&seqnum);
  if (err)
    {
      /* Should use `seqnum` of 0 from above var init in this case */

      pywarn("Couldn't get the next available sequence number.\n");
    }

  /* Open power safe file system */

  err = logfile_next(&pwrfs, &seqnum);
  if (err)
    {
      err = errno;
      pyerr("Couldn't open power safe log file: %d\n", err);
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
          pyerr("Error getting shared packet: %d\n", err);
          continue; /* Try again */
        }

      /* If packet was NULL, mark it as logged and try again */

      if (pkt == NULL)
        {
          pywarn("Shared packet was NULL.\n");
          syncro_mark_logged(syncro);
          continue;
        }

      /* Log packet */

      b_written = write(pwrfs, pkt->contents, pkt->len);
      if (b_written <= 0)
        {
          err = errno;

          /* Some unexpected error */

          if (err != EFBIG)
            {
              pyerr("Couldn't write data to logfile: %d\n", err);
              continue;
            }

          /* File exceeded maximum size, so we need to swap files and
           * continue. */

          err = logfile_next(&pwrfs, &seqnum);
          if (err)
            {
              pyerr("Couldn't create logfile %d: %d\n", seqnum, err);
              continue;
            }
        }
      pydebug("Logged %d!\n", ((struct packet_hdr_s *)(pkt->contents))->num);

      /* Mark packet as logged */

      syncro_mark_logged(syncro);

      /* Sync after `n` packets logged */

      if (count % CONFIG_PYGMY_NLOGSAVE == 0)
        {
          pydebug("Syncing log file...\n");
          err = fsync(pwrfs);
          if (err < 0)
            {
              err = errno;
              pyerr("Couldn't sync logfile: %d\n", err);
            }
          else
            {
              pydebug("Log file synced!\n");
            }
        }

      count++;
    }

  pthread_cleanup_pop(1); /* Close pwrfs */

  return (void *)(long)(err);
}
