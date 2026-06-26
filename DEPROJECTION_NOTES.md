# Depth deprojection notes (Femto Bolt) — for maintainer discussion

Hardware-verified on an Orbbec **Femto Bolt** (depth 640x576 NFOV @30, color 1920x1080 MJPG),
Orbbec SDK v2.8.6, Windows / D3D11.

Current state: the backend fills **no `world[]`** and lets the tcxDepthCamera base
deproject with a **plain pinhole** (`getIntrinsic()` fx/fy/cx/cy, **distortion disabled**).
This is correct and dense. Two unresolved issues below are why we are NOT using the SDK
XY-table path nor the reported depth distortion.

---

## Issue 1 — SDK XY-table point cloud is broken vertically (UNRESOLVED)

Using the SDK path exactly like ofxOrbbec:

```cpp
auto calib = pipeline->getCalibrationParam(config);
CoordinateTransformHelper::transformationInitXYTables(calib, OB_SENSOR_DEPTH, buf, &bytes, &xyTables);
CoordinateTransformHelper::transformationDepthToPointCloud(&xyTables, depth->getData(), pointBuf);
```

…produces a cloud whose **vertical point spacing is ~30x the horizontal** (horizontal rows
are dense, rows are ~1 m apart in 3D → strong horizontal banding). Measured on the same frame:

```
spacing diag (XY table): mean H gap  33 mm   mean V gap 1070 mm   ratio V/H ≈ 32
spacing diag (pinhole):  mean H gap 5.3 mm   mean V gap  5.0 mm   ratio V/H ≈ 0.95
duplicate rows: 0/299   (rows are NOT duplicated/dropped — all present, just mis-placed)
```

Pinhole on the **same** depth + intrinsics is isotropic and correct, so the depth data and
intrinsics are fine — the XY-table transform output is wrong vertically.

### Ruled out
- **Resolution mismatch.** `getCalibrationParam(config)` returns depth intrinsic
  `640x576 fx=504.8 fy=504.79 cy=323.68`, identical to `depthProfile->getIntrinsic()` and
  matching the stream. (The OrbbecViewer-exported `.ini` shows `640x360`, but that is just a
  different export reference; the runtime API is 640x576 and correct.)
- **XY-table buffer size.** ofxOrbbec sizes its `vector<float>` to `w*h*2*sizeof(float)`
  elements (a 4x over-allocation); matching that exactly did **not** change the result
  (still ~32x). So buffer headroom is not the cause.

### Mystery
ofxOrbbec (`design-io/ofxOrbbec`, `src/ofxOrbbecCamera.cpp` `pointCloudToMesh`) uses the
**identical** SDK calls, same calibration, and renders a correct dense cloud — ours does not.
Untested differences worth checking with the maintainer:
1. ofxOrbbec calls **`pointCloud->process(frameSet)`** (an `ob::PointCloudFilter`) right
   before `transformationDepthToPointCloud` (line ~377). We never call it. Does `process()`
   pre-process / rescale the depth frame the static helper then relies on?
2. **`OBPoint` stride / `OBXYTables` struct ABI** — possible header/lib version skew between
   our build and ofxOrbbec's (older SDK).
3. Depth **value-scale** application timing (ofxOrbbec calls `setPositionDataScaled(scale)`
   for the RGBD path).

---

## Issue 2 — base distortion is 3-term Brown-Conrady; Femto Bolt depth needs the rational (K6) model

`depthProfile->getDistortion()` returns **large** coefficients:

```
depth getDistortion: k1=17.87  k2=8.89  k3=0.326  p1≈0  p2≈0
color getDistortion: k1=0.087  k2=-0.114 k3=0.048  ...   (normal Brown-Conrady)
```

`k1=17.87` is not a 3-term Brown-Conrady value — it is the **rational / Brown-Conrady-K6**
model the Azure-Kinect-class depth sensor uses, where the `k4..k6` denominator balances the
`k1..k3` numerator:

```
radial = (1 + k1 r² + k2 r⁴ + k3 r⁶) / (1 + k4 r² + k5 r⁴ + k6 r⁶)
```

The tcxDepthCamera base (`getWorldCoordinateAt`) and our `copyDistortion()` implement only the
**3-term** form and explicitly **drop k4..k6** ("fine for typically low depth distortion").
With `k1=17.87` and no denominator the radial term explodes → the cloud warps / edges fan out.

So we currently **do not copy depth distortion** (pinhole only). ofxOrbbec never applies
distortion by hand either — it hands the whole `OBCalibrationParam` to the SDK transform, which
applies the correct model internally (this is tied to Issue 1).

### Candidate fixes (base, tcxDepthCamera)
- Implement the rational K6 model in `getWorldCoordinateAt` (use k4..k6 when present), or
- Detect a distortion-model field / out-of-range coefficients and skip undistort, or
- Provide a backend hook to supply precomputed `world[]` from the SDK (preferred — Issue 1).

---

## Where the investigation / WIP code lives

The clean branch ships only the pinhole fix + `example-basic`. The experimental
work from this investigation is preserved on the **`debug/deprojection-wip`** branch
(reference-only, not part of the PR):

- **`example-debug/`** — the example with the in-development toggles (GPU point
  renderer `G`, splat size `[`/`]`, round splats `R`, preview-thumbnail toggle `T`),
  kept next to the clean `example-basic` for side-by-side comparison.
- The GPU renderer it uses graduated into **TrussC core** as `tc::PointCloudRenderer`
  (`core/include/tc/3d/tcPointCloudRenderer.h` + `core/shaders/pointCloud.glsl`, branch
  `feat/gpu-point-renderer`, with `examples/3d/pointCloudRendererExample`). It targets
  the *separate* fps problem (immediate-mode `Mesh::draw()` re-streams every vertex per
  frame; points/unlit have no retained-VBO path), not the geometry issues here.

## TL;DR
- **Working now:** pinhole via `getIntrinsic()`, distortion disabled. Correct + dense.
- **Issue 1:** our SDK XY-table cloud is vertically broken (~32x) for reasons we have not
  pinned; ofxOrbbec's identical calls work. Needs maintainer eyes (likely `pointCloud->process()`
  or an SDK-version/ABI difference).
- **Issue 2:** the base only does 3-term Brown-Conrady; Femto Bolt depth ships rational-K6
  coefficients that explode that formula. Either implement K6 in the base or rely on the SDK
  transform (Issue 1).
