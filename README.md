# Pygmy Telemetry Application

This repository hosts a NuttX application for telemetry logging and transmission on the [Pygmy flight computer][pygmy], as well
as some additional programs for calibration, testing and configuration of the Pygmy flight computer.

[The NuttX board support for Pygmy can be found here.][pygmy-nx]

In order to use this telemetry application, just clone this repository in your `nuttx-apps` directory. It will be
detected by Kconfig.

You can use the `telemetry` configuration to have the telemetry application run at startup, with all features enabled.

```console
$ cd nuttx-apps
$ git clone https://github.com/linguini1/pygmy-nx.git
$ cd ../nuttx
$ ./tools/configure.sh ../pygmy-nx/configs/telemetry
$ export PICO_SDK_PATH=$(realpath path/to/pico-sdk)
$ make -j
```

[pygmy]: https://github.com/linguini1/pygmy
[pygmy-nx]: https://github.com/linguini1/pygmy-nx
