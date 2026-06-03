# tcxOrbbec — Known issues & roadmap

Status: **scaffold, not yet hardware-verified.** The addon structure, CMake
wiring, example, and a first-pass Orbbec SDK v2 implementation exist, written
ahead of the hardware (Femto Bolt + Gemini 355L). The implementation has **not**
been compiled against the real SDK headers or run on a device. The items below
are the work to do once hardware + SDK are in hand.

## Verify against the real SDK (first thing when hardware arrives)

1. **Confirm the SDK v2 C++ API names used in `tcxOrbbec.cpp`.** Written from the
   docs, not compiled. Check against the installed `libobsensor` headers:
   - `ob::Context::queryDeviceList()` / `DeviceList::getCount()` / `getDevice(i)`
   - `Device::getDeviceInfo()->getName()`
   - `Pipeline(device)`, `Pipeline::getStreamProfileList(OB_SENSOR_*)`,
     `StreamProfileList::getProfile(OB_PROFILE_DEFAULT)`,
     `Config::enableStream(profile)`, `Pipeline::start(config)` / `stop()`
   - `FrameSet::depthFrame()/colorFrame()/irFrame()`
   - `Frame::getWidth()/getHeight()/getData()/getFormat()/getTimeStampUs()`
   - `DepthFrame::getValueScale()`
   - `VideoStreamProfile::getIntrinsic()` → `OBCameraIntrinsic{fx,fy,cx,cy,width,height}`
   - `VideoStreamProfile::getDistortion()` → `OBCameraDistortion{k1..k6,p1,p2}`
   - `StreamProfile::getExtrinsicTo(other)` → `OBExtrinsic{rot[9],trans[3]}`
   Fix any that differ (v2 had some renames vs v1).

## Functional gaps

2. **Color format handling is partial.** `captureInto()` converts RGB/BGR/RGBA/
   BGRA only. Femto Bolt / Gemini color often arrives as **MJPG or YUYV**, which
   is currently skipped with a one-time warning. Add an `ob::FormatConvertFilter`
   (or decode MJPG) to produce RGBA for those, then drop the warning.

3. **No SDK point cloud.** World coordinates come from the base's intrinsics
   deprojection. For an exact, distortion-aware cloud use the SDK's
   `ob::PointCloudFilter` and fill `frame.world` (like tcxAzureKinect does), then
   the base will prefer it.

4. **Hardcoded default profiles.** Streams use `OB_PROFILE_DEFAULT`. Expose
   resolution / fps / format selection through a config struct / setters before
   `setup()`.

5. **IR assumed uint16.** If a device delivers 8-bit IR, the F32 conversion needs
   to branch on the IR frame format.

6. **Single timestamp source.** `dst.timestamp` uses the depth frame timestamp;
   color/IR timestamps are ignored.

## Build / runtime

7. **`extensions/` folder not auto-bundled.** Only the SDK shared library is
   copied next to the app binary. The SDK's `extensions/` directory (filters /
   depth engine) must also sit next to the binary at runtime — copying it
   automatically (it is a directory) is not wired up yet. Copy by hand for now.

8. **Verify on all three platforms.** SDK v2 supports Windows / Linux / macOS;
   none built yet here. macOS is the easy local target (the dev machine), so
   start there.

## Devices

9. **Multi-device untested.** The `deviceIndex` arg exists; only conceptually.
   No enumeration / serial helper yet.

## Out of scope (for now)

10. IMU, multi-camera sync, and body/skeleton tracking are separate features, not
    bugs. Body tracking, if added, belongs in its own addon that extends the
    recorder/playback via the `tcxDepthRecord` block hooks (the official layer
    stays out of skeletons).
