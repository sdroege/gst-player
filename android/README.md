GST Player Android port
=======================

Prerequisites
-------------

1. Install Android SDK from https://developer.android.com/sdk/ & set `sdk.dir` in **local.properties** to the installation path
2. Install Android NDK from https://developer.android.com/tools/sdk/ndk/index.html & set `ndk.dir` in **local.properties** to the installation path
3. Install GStreamer Android SDK from http://gstreamer.freedesktop.org/data/pkg/android/ and set `gstreamerSdk.dir` in **local.properties** to the installation path
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