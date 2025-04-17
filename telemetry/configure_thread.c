/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <assert.h>
#include <ctype.h>
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

  printf("Radio {\n");
  printf("\tCallsign: ");

  /* Only print the number of characters permitted to be in the callsign, or
   * up to the null terminator, whichever comes first. Prevents printing
   * garbage when EEPROM is uninitialized */

  for (int i = 0; i < sizeof(config->radio.callsign); i++)
    {
      if (config->radio.callsign[i] == '\0')
        {
          break;
        }
      putchar(config->radio.callsign[i]);
    }

  printf("\tFrequency: %lu Hz\n", config->radio.frequency);
  printf("\tBandwidth: %lu kHz\n", config->radio.bandwidth);
  printf("\tPreamble length: %u bytes\n", config->radio.prlen);
  printf("\tSpread factor: %u\n", config->radio.spread);
  printf("\tModulation type: %s\n", config->radio.mod == 0 ? "lora" : "fsk");
  printf("\tTransmit power: %f dBm\n", config->radio.txpower);
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

  memcpy(&usrconfig, &config,
         sizeof(config)); /* Files are the same before modifications start */

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
      printf("Read: %s\n", incoming_command);

      /* No-argument commands */

      if (starts_with(incoming_command, "reboot"))
        {
          /* Reboot the board, causing changes to come into effect */

          boardctl(BOARDIOC_RESET, 0);
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

      /* Configuration setting commands */

      else if (starts_with(incoming_command, "callsign"))
        {
          if (!set_callsign(incoming_command, &usrconfig))
            {
              fprintf(stderr, "Failed to set callsign.\n");
            }
        }

      /* Unrecognized command */

      else
        {
          printf("Unknown command: %s\n", incoming_command);
        }
    }

  return 0;
}
