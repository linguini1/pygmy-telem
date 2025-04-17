/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <nuttx/usb/cdcacm.h>
#include <sys/boardctl.h>

#include "../common/configuration.h"
#include "helptext.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#if !defined(CONFIG_CDCACM)
#error                                                                       \
    "CONFIG_CDCACM must be enabled, as it is required for USB configuration"
#endif !defined(CONFIG_CDCACM)

#if !defined(CONFIG_CDCACM_CONSOLE)
#error                                                                       \
    "CONFIG_CDCACM_CONSOLE must be enabled, as it is required for USB configuration"
#endif

#if !defined(CONFIG_BOARDCTL_RESET)
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
 * Name: usb_init
 *
 * Description:
 *   Initialize USB device driver for console debug output.
 ****************************************************************************/

static int usb_init(void)
{
  struct boardioc_usbdev_ctrl_s ctrl;
  FAR void *handle;
  int ret;
  int usb_fd;

  /* Initialize architecture */

  ret = boardctl(BOARDIOC_INIT, 0);
  if (ret != 0)
    {
      return ret;
    }

  /* Initialize the USB serial driver */

  ctrl.usbdev = BOARDIOC_USBDEV_CDCACM;
  ctrl.action = BOARDIOC_USBDEV_CONNECT;
  ctrl.instance = 0;
  ctrl.handle = &handle;

  ret = boardctl(BOARDIOC_USBDEV_CONTROL, (uintptr_t)&ctrl);
  if (ret < 0)
    {
      return ret;
    }

  /* Redirect standard streams to USB console */

  do
    {
      usb_fd = open("/dev/ttyACM0", O_RDWR);

      /* ENOTCONN means that the USB device is not yet connected, so sleep.
       * Anything else is bad.
       */

      assert(errno == ENOTCONN);
      usleep(100);
    }
  while (usb_fd < 0);

  usb_fd = open("/dev/ttyACM0", O_RDWR);

  dup2(usb_fd, 0); /* stdout */
  dup2(usb_fd, 1); /* stdin */
  dup2(usb_fd, 2); /* stderr */

  if (usb_fd > 2)
    {
      close(usb_fd);
    }
  sleep(1); /* Seems to help ensure first few prints get captured */

  return ret;
}

/****************************************************************************
 * Name: print_config
 *
 * Description:
 *   Prints the contents of a configuration object to stdout.
 ****************************************************************************/

void print_config(struct configuration_s *config)
{
  printf("Radio {");
  printf("\tCallsign: %s\n", config->radio.callsign);
  printf("\tFrequency: %u Hz\n", config->radio.frequency);
  printf("\tBandwidth: %u kHz\n", config->radio.bandwidth);
  printf("\tPreamble length: %u bytes\n", config->radio.prlen);
  printf("\tSpread factor: %u\n", config->radio.spread);
  printf("\tModulation type: %s\n", config->radio.mod == 0 ? "lora" : "fsk");
  printf("\tTransmit power: %f dBm\n", config->radio.txpower);
  printf("}");
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

  /* Enable the USB interface for configuring */

  err = usb_init();
  if (err < 0)
    {
      fprintf(stderr, "Failed to initialize USB console: %d\n", err);
    }

  /* Read existing configuration data */

  configfile = open(CONFIG_PYGMY_TELEM_CONFIGFILE, O_RDONLY);
  if (configfile < 0)
    {
      err = errno;
      fprintf(stderr, "Couldn't open configuration file: %d\n", err);
      return EXIT_FAILURE;
    }

  b_read = read(configfile, &config, sizeof(config));
  if (b_read < 0)
    {
      err = errno;
      fprintf(stderr, "Couldn't read configuration file: %d\n", err);
      close(configfile);
      return EXIT_FAILURE;
    }
  else if (b_read < sizeof(config))
    {
      fprintf(stderr, "Couldn't read complete configuration file: %d\n", err);
      close(configfile);
      return EXIT_FAILURE;
    }

  close(configfile);

  memcpy(&usrconfig, &config,
         sizeof(config)); /* Files are the same before modifications start */

  /* Infinitely perform blocking reads on the USB. As commands come in,
   * process them. */

  for (;;)
    {
      /* Minus one to leave space for null terminator */

      b_read = read(stdin, incoming_command, sizeof(incoming_command) - 1);
      if (b_read < 0)
        {
          err = errno;
          // TODO syslog error
        }

      /* Guarantee null terminator for safety */

      incoming_command[b_read] = '\0';

      /* Parse commands */

      if (!strcmp(incoming_command, "reboot"))
        {
          /* Reboot the board, causing changes to come into effect */

          boardctl(BOARDIOC_RESET, 0);
        }
      else if (!strcmp(incoming_command, "help"))
        {
          printf(HELP_TEXT);
        }
      else if (!strcmp(incoming_command, "current"))
        {
          print_config(&config);
        }
      else if (!strcmp(incoming_command, "modified"))
        {
          print_config(&usrconfig);
        }
      else if (!strcmp(incoming_command, "test"))
        {
          printf("Test passed.\n");
        }
    }

  return 0;
}
