GST Player Android port
=======================

Prerequisites
-------------

1. Install Android SDK from https://developer.android.com/sdk/ & set `sdk.dir` in **local.properties** to the installation path
2. Install Android NDK from https://developer.android.com/tools/sdk/ndk/index.html & set `ndk.dir` in **local.properties** to the installation path
3. Install GStreamer Android port from http://gstreamer.freedesktop.org/data/pkg/android/ and set `gstreamer.dir` in **local.properties** to the installation path
4. If you have a different special directory for pkg-config or other tools (e.g. on OSX when using Homebrew), then also set this path using the `ndk.extraPath` variable in **local.properties**

Compiling the sample
--------------------

Use

    ./gradlew installDebug

to compile and install a debug version onto all connected devices.

Please note this component is using the new Android build system based on Gradle. More information about this is available on http://tools.android.com/tech-docs/new-build-system.

Android Studio
--------------

Android Studio builds will work out of the box. Simply open `build.gradle` in this folder to import the project.

Manual NDK build
----------------

It is still possible to build just the NDK portion. This will speed up the process a bit as you don't need to start gradle first and compile the complete App.
For this to work, you still need to set the `GSTREAMER_ROOT_ANDROID` and `NDK_PROJECT_PATH` environment variables.
Also, make sure all the SDK & NDK tools are available in `$PATH`.

Within this directory, invoke:

    export GSTREAMER_ROOT_ANDROID=/path/to/gstreamer
    export NDK_PROJECT_PATH=$PWD/app/src/main

    ndk-build