<p align="center"><img src="assets/overport.svg" alt="overport logo"></p>

# ovrport/app

overport (aka ovrport) app is used to patch Android games targeting Quest headsets. Functionality will be
expanded in the future.

Powered by [Compose Multiplatform](https://www.jetbrains.com/compose-multiplatform/)
and [ARSCLib](https://github.com/REAndroid/ARSCLib).

[Builds](https://github.com/ovrport/app/releases) available for
Windows, Linux, macOS and Android.

# Supported platforms

Tier 1 platforms:

* [x] Pico 4 Ultra

Tier 2 platforms:

* [x] Pico 4 / 4 Pro
* [x] Pico Neo 3
* [x] Oculus Quest 2

Tier 3 platforms:

* [x] Oculus Quest (1)
* [x] Meta Quest 3 / 3s
* [x] YVR/PFDMR headsets

Other OpenXR-compatible headsets may also be supported but they require additional testing.

# Reporting issues

Please, attach logs to your issue report if you encounter any problems. You can obtain them with `adb logcat`.

If you have any issues regarding game compatibility, please, enable verbose logging first with
`adb shell setprop debug.sys.ovrport.verboselogging 1`.

# About legality

Think of this app like an emulator: you need to legally obtain the game files to use it.

Obviously, to get a Quest game running on a different headset, the app has to strip away some entitlement checks.
However, the game still won't launch on a Quest unless you’ve actually bought it.

# License

overport app is licensed under the GPLv3 license. Please see the [
`LICENSE`](https://github.com/ovrport/app/blob/master/LICENSE) file for more information.