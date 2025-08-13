/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/boardctl.h>

#include "../common/configuration.h"
#include "helptext.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define thread_err(err) ((void *)(long)((err)))

#ifndef CONFIG_CDCACM
#error                                                                       \
    "CONFIG_CDCACM must be enabled, as it is required for USB configuration"
#endif

#ifndef CONFIG_CDCACM_CONSOLE
#error                                                                       \
    "CONFIG_CDCACM_CONSOLE must be enabled, as it is required for USB configuration"
#endif

#ifndef CONFIG_BOARDCTL_RESET
#error "CONFIG_BOARDCTL_RESET must be enabled"
#endif

/****************************************************************************
 * Private Data
 ****************************************************************************/

static char incoming_command[256];
static char copy_buf[1024];

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: starts_with
 *
 * Description:
 *   Detects if a string starts with a sub-string
 ****************************************************************************/

static bool starts_with(const char *haystack, const char *needle)
{
  size_t haylen = strlen(haystack);
  size_t needlen = strlen(needle);
  return haylen < needlen ? false : memcmp(needle, haystack, needlen) == 0;
}

/****************************************************************************
 * Name: trim_trailing
 *
 * Description:
 *   Trims the trailing white space off of a string.
 ****************************************************************************/

static void trim_trailing(char *str)
{
  if (str == NULL) return;

  char *end;
  end = str + strlen(str) - 1;
  while (end > str && isspace((unsigned char)*end))
    end--;
  end++;
  *end = '\0';
}

/****************************************************************************
 * Name: print_config
 *
 * Description:
 *   Prints the contents of a configuration object to stdout.
 ****************************************************************************/

static void print_config(struct configuration_s *config)
{
  printf("Radio {\n");
  printf("\tCallsign: ");

  /* Only print the number of characters permitted to be in the callsign, or
   * up to the null terminator, whichever comes first. Prevents printing
   * garbage when EEPROM is uninitialized */

  for (int i = 0; i < CONFIG_PYGMY_CALLSIGN_LEN; i++)
    {
      if (config->radio.callsign[i] == '\0')
        {
          break;
        }
      putchar(config->radio.callsign[i]);
    }
  putchar('\n');

  printf("\tFrequency: %lu Hz\n", config->radio.frequency);
  printf("\tBandwidth: %lu kHz\n", config->radio.bandwidth);
  printf("\tPreamble length: %u bytes\n", config->radio.prlen);
  printf("\tSpread factor: %u\n", config->radio.spread);
  printf("\tModulation type: %s\n", config->radio.mod == 0 ? "lora" : "fsk");
  printf("\tTransmit power: %f dBm\n", config->radio.txpower);
  printf("}\n");

  printf("IMU {\n");
  printf("\tAccelerometer full scale range: %u g\n", config->imu.xl_fsr);
  printf("\tGyroscope full scale range: %u dps\n", config->imu.gyro_fsr);
  printf("\tAccelerometer offsets: (x=%f, y=%f, z=%f) m/s^2\n",
         config->imu.xl_offsets[0], config->imu.xl_offsets[1],
         config->imu.xl_offsets[2]);
  printf("\tGyroscope offsets: (x=%f, y=%f, z=%f) dps\n",
         config->imu.xl_offsets[0], config->imu.xl_offsets[1],
         config->imu.xl_offsets[2]);
  printf("}\n");
}

/****************************************************************************
 * Name: set_callsign
 *
 * Description:
 *   Sets the call sign in the configuration settings from what is provided in
 *   the command.
 ****************************************************************************/

static bool set_callsign(char *command, struct configuration_s *config)
{
  char *callsign;
  callsign = strtok(command, " ");
  callsign = strtok(NULL, " ");

  trim_trailing(callsign);

  if (callsign == NULL) return false;

  size_t len = strlen(callsign);

  /* If user call sign is more than what's allowed, report failure instead of
   * just truncating */

  if (len > CONFIG_PYGMY_CALLSIGN_LEN)
    {
      return false;
    }

  /* If user call sign is less than maximum, 0 pad the rest */
  if (len < CONFIG_PYGMY_CALLSIGN_LEN)
    {
      memcpy(config->radio.callsign, callsign, len);
      memset(&config->radio.callsign[len], 0,
             CONFIG_PYGMY_CALLSIGN_LEN - len);
    }

  /* Otherwise, truncate the call sign to the maximum */

  else
    {
      memcpy(config->radio.callsign, callsign, CONFIG_PYGMY_CALLSIGN_LEN);
    }

  return true;
}

/****************************************************************************
 * Name: get_first_arg
 *
 * Description:
 *   Gets the first argument from the command.
 *
 * Returns: A pointer to the first argument
 ****************************************************************************/

static char *get_first_arg(char *command)
{
  strtok(command, " ");
  return strtok(NULL, " ");
}

/****************************************************************************
 * Name: save_settings
 *
 * Description:
 *   Saves the parts of the configuration that have changed.
 *
 * Returns: 0 on success, an errno code on failure.
 ****************************************************************************/

static int save_settings(const struct configuration_s *old,
                         const struct configuration_s *new)
{
  int configfile;
  int err = 0;
  ssize_t written;

  /* Open configuration file */

  configfile = open(CONFIG_PYGMY_TELEM_CONFIGFILE, O_WRONLY | O_CREAT);
  if (configfile < 0)
    {
      err = errno;
      fprintf(stderr, "Couldn't open configuration file: %d\n", err);
      return err;
    }

  /* Detect if there was a difference. TODO: more granularity */

  if (!memcmp(old, new, sizeof(struct configuration_s)))
    {
      return 0;
    }

  /* There was a difference, save the new configuration. */

  written = write(configfile, new, sizeof(struct configuration_s));
  if (written < 0)
    {
      err = errno;
      fprintf(stderr, "Couldn't save new configuration: %d\n", err);
      goto cleanup;
    }
  else if (written < sizeof(struct configuration_s))
    {
      fprintf(stderr,
              "Could only write %zu/%zu bytes of configuration fully.\n",
              written, sizeof(struct configuration_s));
      err = ENOMEM;
      goto cleanup;
    }

  /* Everything was written fully */

  err = 0;

cleanup:
  close(configfile);
  return err;
}

/****************************************************************************
 * Name: copy_file
 *
 * Description:
 *   Copies a file from the power safe log file system to the user file
 *   system.
 *
 * Returns: 0 on success, an errno code on failure.
 ****************************************************************************/

static int copy_file(const char *fname)
{
  int pwrfd;
  int usrfd;
  char pwrfname[sizeof(CONFIG_PYGMY_TELEM_PWRFS) + 34];
  char usrfname[sizeof(CONFIG_PYGMY_TELEM_USRFS) + 34];
  ssize_t bread;
  ssize_t bwrote;

  /* Create corresponding user and powerfs file names since `fname` does not
   * include the path, just the name */

  snprintf(pwrfname, sizeof(pwrfname), CONFIG_PYGMY_TELEM_PWRFS "/%s", fname);
  snprintf(usrfname, sizeof(usrfname), CONFIG_PYGMY_TELEM_USRFS "/%s", fname);

  /* Open original log */

  pwrfd = open(pwrfname, O_RDONLY);
  if (pwrfd < 0)
    {
      return errno;
    }

  /* Create new user log file (we can overwrite it because we're taking from
   * the source) */

  usrfd = open(usrfname, O_WRONLY | O_CREAT);
  if (usrfd < 0)
    {
      fprintf(stderr, "Couldn't create log file in user filesystem: %d\n",
              errno);
      close(pwrfd);
      return errno;
    }

  /* Copy files over */

  for (;;)
    {
      bread = read(pwrfd, copy_buf, sizeof(copy_buf));

      /* End of file, all done */

      if (bread == 0)
        {
          break;
        }

      /* Some error happened */

      if (bread < 0)
        {
          fprintf(stderr, "Error reading from log file: %d.\n", errno);
          close(pwrfd);
          close(usrfd);
          return errno;
        }

      /* Copy all information read to the new file */

      bwrote = write(usrfd, copy_buf, bread);

      /* Some error happened */

      if (bwrote < 0)
        {
          fprintf(stderr, "Error writing to user file: %d.\n", errno);
          close(pwrfd);
          close(usrfd);
          return errno;
        }
    }

  close(pwrfd);
  close(usrfd);
  return 0;
}

/****************************************************************************
 * Name: copy_files
 *
 * Description:
 *   Copies all the power safe file system logs to the user file system.
 *
 * Returns: 0 on success, an errno code on failure.
 ****************************************************************************/

static int copy_files(void)
{
  int err;
  DIR *pwrdir;
  struct dirent *de;

  pwrdir = opendir(CONFIG_PYGMY_TELEM_PWRFS);
  if (pwrdir == NULL)
    {
      return errno;
    }

  /* Go through all files in the directory and find out their name to parse
   * out the sequence number.
   *
   * NOTE: manpages for `readdir` say to set `errno` to zero prior.
   */

  errno = 0;
  while ((de = readdir(pwrdir)) != NULL)
    {
      /* Skip non-files just in case */

      if (de->d_type != DT_REG)
        {
          continue;
        }

      printf("Copying '%s'!\n", de->d_name);
      err = copy_file(de->d_name);
      if (err)
        {
          fprintf(stderr, "Failed to copy %s: %d\n", de->d_name, errno);
          continue;
        }

      printf("Copied %s successfully!\n", de->d_name);
    }

  if (errno != 0)
    {
      fprintf(stderr, "Error listing files in log directory: %d\n", errno);
      return errno;
    }

  return 0;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: configure_thread
 *
 * Description:
 *   Allows the user to interact with the device over USB to configure flight
 *   settings.
 ****************************************************************************/

void *configure_thread(void *arg)
{
  /*
   * Use-case: configure settings in EEPROM for a flight from an external
   * computer, then reboot for settings to take effect.
   *
   * When being configured, nothing should interrupt the use of the USB
   * console. This means that once we detect that configuration is happening,
   * a lock should be placed on the console until configuration is complete.
   *
   * The user should be able to configure *any and all* options from here,
   * which will be persisted in the EEPROM.
   *
   * Any configuration options being sent should be verified against what was
   * read from the EEPROM before configuration. This will minimize writes when
   * options have not changed.
   *
   * This thread should be given the highest priority of all threads. If there
   * is no information being exchanged on the USB, then it will never run.
   * This means it stays dormant during flight. There is no risk of taking CPU
   * time from more flight-critical threads.
   */

  int err;
  int configfile;
  ssize_t b_read;
  char *argument;
  struct configuration_s config;
  struct configuration_s usrconfig;

  /* Read existing configuration data */

  configfile = open(CONFIG_PYGMY_TELEM_CONFIGFILE, O_RDONLY);
  if (configfile < 0)
    {
      err = errno;
      fprintf(stderr, "Couldn't open configuration file: %d\n", err);
      return thread_err(EXIT_FAILURE);
    }

  b_read = read(configfile, &config, sizeof(config));
  if (b_read < 0)
    {
      err = errno;
      fprintf(stderr, "Couldn't read configuration file: %d\n", err);
      close(configfile);
      return thread_err(EXIT_FAILURE);
    }
  else if (b_read < sizeof(config))
    {
      fprintf(stderr, "Couldn't read complete configuration file.\n");
      close(configfile);
      return thread_err(EXIT_FAILURE);
    }

  close(configfile);

  /* Files are the same before modifications start */

  memcpy(&usrconfig, &config, sizeof(config));

  /* Infinitely perform blocking reads on the USB. As commands come in,
   * process them. */

  for (;;)
    {
      /* Minus one to leave space for null terminator */

      b_read = read(1, incoming_command, sizeof(incoming_command) - 1);
      if (b_read < 0)
        {
          err = errno;
          // TODO syslog error
        }

      /* Guarantee null terminator for safety */

      incoming_command[b_read] = '\0';

      /* No-argument commands */

      if (starts_with(incoming_command, "reboot"))
        {
          /* Reboot the board, causing changes to come into effect */

          boardctl(BOARDIOC_RESET, 0);
        }
      else if (starts_with(incoming_command, "copy"))
        {
          /* Copy files from log file system to user file system */
          err = copy_files();
          if (err)
            {
              fprintf(stderr, "Failed to copy all files: %d\n", errno);
            }
        }
      else if (starts_with(incoming_command, "help"))
        {
          printf(HELP_TEXT);
        }
      else if (starts_with(incoming_command, "current"))
        {
          print_config(&config);
        }
      else if (starts_with(incoming_command, "modified"))
        {
          print_config(&usrconfig);
        }
      else if (starts_with(incoming_command, "save"))
        {
          err = save_settings(&config, &usrconfig);
          if (err)
            {
              fprintf(stderr, "Couldn't save settings: %d\n", err);
            }
        }

      /* Configuration setting commands */

      else if (starts_with(incoming_command, "callsign"))
        {
          if (!set_callsign(incoming_command, &usrconfig))
            {
              fprintf(stderr,
                      "Failed to set callsign. Ensure it's less than %d "
                      "characters\n",
                      CONFIG_PYGMY_CALLSIGN_LEN);
            }
          printf("Saved successfully! Reboot for changes to take effect.\n");
        }
      else if (starts_with(incoming_command, "frequency"))
        {
          argument = get_first_arg(incoming_command);
          usrconfig.radio.frequency = strtoul(argument, NULL, 10);
        }
      else if (starts_with(incoming_command, "bandwidth"))
        {
          argument = get_first_arg(incoming_command);
          usrconfig.radio.bandwidth = strtoul(argument, NULL, 10);
        }
      else if (starts_with(incoming_command, "preamble"))
        {
          argument = get_first_arg(incoming_command);
          usrconfig.radio.prlen = strtoul(argument, NULL, 10);
        }
      else if (starts_with(incoming_command, "spread"))
        {
          argument = get_first_arg(incoming_command);
          usrconfig.radio.spread = strtoul(argument, NULL, 10);
        }
      else if (starts_with(incoming_command, "mod"))
        {
          argument = get_first_arg(incoming_command);
          trim_trailing(argument);
          if (!strcmp(argument, "lora"))
            {
              usrconfig.radio.mod = 0;
            }
          else if (!strcmp(argument, "fsk"))
            {
              usrconfig.radio.mod = 1;
            }
          else
            {
              fprintf(stderr, "Unrecognized modulation.\n");
            }
        }
      else if (starts_with(incoming_command, "txpower"))
        {
          argument = get_first_arg(incoming_command);
          usrconfig.radio.txpower = strtof(argument, NULL);
        }
      else if (starts_with(incoming_command, "xl_fsr"))
        {
          argument = get_first_arg(incoming_command);
          usrconfig.imu.xl_fsr = strtoul(argument, NULL, 10);
        }
      else if (starts_with(incoming_command, "gyro_fsr"))
        {
          argument = get_first_arg(incoming_command);
          usrconfig.imu.gyro_fsr = strtoul(argument, NULL, 10);
        }
      else if (starts_with(incoming_command, "xl_off"))
        {
          argument = get_first_arg(incoming_command);
          usrconfig.imu.xl_offsets[0] = strtof(argument, NULL);
          argument = strtok(NULL, " ");
          usrconfig.imu.xl_offsets[1] = strtof(argument, NULL);
          argument = strtok(NULL, " ");
          usrconfig.imu.xl_offsets[2] = strtof(argument, NULL);
        }
      else if (starts_with(incoming_command, "gyro_off"))
        {
          argument = get_first_arg(incoming_command);
          usrconfig.imu.gyro_offsets[0] = strtof(argument, NULL);
          argument = strtok(NULL, " ");
          usrconfig.imu.gyro_offsets[1] = strtof(argument, NULL);
          argument = strtok(NULL, " ");
          usrconfig.imu.gyro_offsets[2] = strtof(argument, NULL);
        }

      /* Unrecognized command */

      else
        {
          printf("Unknown command: %s\n", incoming_command);
        }
    }

  return 0;
}
