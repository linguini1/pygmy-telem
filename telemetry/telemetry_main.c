/****************************************************************************
 * pygmy-telem/telemetry/telemetry_main.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <nuttx/usb/cdcacm.h>
#include <sys/boardctl.h>

#include "../common/configuration.h"
#include "arguments.h"
#include "syncro.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#ifndef PYGMY_LOG_THREAD_PRIORITY
#define PYGMY_LOG_THREAD_PRIORITY 130
#endif

#ifndef PYGMY_RADIO_THREAD_PRIORITY
#define PYGMY_RADIO_THREAD_PRIORITY 90
#endif

#ifndef PYGMY_PACKET_THREAD_PRIORITY
#define PYGMY_PACKET_THREAD_PRIORITY 100
#endif

#ifndef PYGMY_CONFIGURE_THREAD_PRIORITY
#define PYGMY_CONFIGURE_THREAD_PRIORITY 200
#endif

/****************************************************************************
 * Private Data
 ****************************************************************************/

static pthread_t radio_pid;
static pthread_t log_pid;
static pthread_t packet_pid;
static pthread_t configure_pid;

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

      DEBUGASSERT(errno == ENOTCONN);
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
 * Public Function Prototypes
 ****************************************************************************/

void *log_thread(void *arg);
void *radio_thread(void *arg);
void *packet_thread(void *arg);
void *configure_thread(void *arg);

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * telemetry_main
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  int err;
  int configfile;
  ssize_t b_read;
  struct configuration_s config;
  syncro_t syncro;
  const struct thread_args_t args = {
      .syncro = &syncro,
      .config = &config,
  };

  /* Enable the USB interface for configuring */

  err = usb_init();
  if (err < 0)
    {
      fprintf(stderr, "Failed to initialize USB console: %d\n", errno);
    }

  /* Read configuration data */

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

  /* Start up the configuration thread */

  err = pthread_create(&configure_pid, NULL, configure_thread, NULL);
  if (err < 0)
    {
      fprintf(stderr, "Failed to start configuration thread %d\n", err);
    }

  err = pthread_setschedprio(configure_pid, PYGMY_CONFIGURE_THREAD_PRIORITY);
  if (err < 0)
    {
      fprintf(stderr, "Failed to set priority of configuration thread: %d\n",
              err);
    }

  /* Initialize synchronization object */

  err = syncro_init(&syncro);
  if (err)
    {
      fprintf(stderr, "Could not initialize synchronization object: %d\n",
              err);
      return EXIT_FAILURE;
    }

  /* Start packet thread */

  err = pthread_create(&packet_pid, NULL, packet_thread, (void *)&args);
  if (err < 0)
    {
      fprintf(stderr, "Failed to start radio thread: %d\n", err);
    }

  err = pthread_setschedprio(packet_pid, PYGMY_PACKET_THREAD_PRIORITY);
  if (err < 0)
    {
      fprintf(stderr, "Failed to set priority of packet thread: %d\n", err);
    }

  /* Start logging thread. */

  err = pthread_create(&log_pid, NULL, log_thread, (void *)&args);
  if (err < 0)
    {
      fprintf(stderr, "Failed to start logging thread %d\n", err);
    }

  err = pthread_setschedprio(log_pid, PYGMY_LOG_THREAD_PRIORITY);
  if (err < 0)
    {
      fprintf(stderr, "Failed to set priority of logging thread: %d\n", err);
    }

  /* Start radio broadcast thread */

  err = pthread_create(&radio_pid, NULL, radio_thread, (void *)&args);
  if (err < 0)
    {
      fprintf(stderr, "Failed to start radio thread: %d\n", err);
    }

  err = pthread_setschedprio(radio_pid, PYGMY_RADIO_THREAD_PRIORITY);
  if (err < 0)
    {
      fprintf(stderr, "Failed to set priority of radio thread: %d\n", err);
    }

  pthread_join(log_pid, (void *)&err);
  if (err)
    {
      fprintf(stderr, "Logging thread exited with error: %d\n", err);
    }

  pthread_join(radio_pid, (void *)&err);
  if (err)
    {
      fprintf(stderr, "Radio thread exited with error: %d\n", err);
    }

  pthread_join(packet_pid, (void *)&err);
  if (err)
    {
      fprintf(stderr, "Packet thread exited with error: %d\n", err);
    }

  return EXIT_SUCCESS;
}
