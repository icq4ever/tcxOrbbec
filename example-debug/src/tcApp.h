#pragma once

#include <TrussC.h>
#include <tcxDepthCamera.h>
#include <tcxOrbbec.h>
using namespace std;
using namespace tc;
using namespace tcx;

// Point-cloud viewer driven by an Orbbec camera through the tcxDepthCamera
// interface: update() -> isFrameNew() -> toMesh(). Requires the hardware + the
// Orbbec SDK v2. If no device is found, the app still runs and shows a message
// rather than crashing.
class tcApp : public App {
public:
    void setup() override;
    void update() override;
    void draw() override;
    void keyPressed(int key) override;

private:
    void rebuild();

    shared_ptr<Orbbec> camera;
    bool deviceOk = false;
    EasyCam view;
    Mesh cloud;
    bool colored = true;    // Orbbec has color; show it by default
    int step = 1;           // full resolution (match ofxOrbbec: no decimation)

    PointCloudRenderer pcRenderer_;        // GPU instanced fat-point path (tc:: core)
    bool  useGpu_ = true;       // G: toggle GPU renderer vs immediate-mode draw
    float pointSize_ = 0.02f;   // [ / ]: splat size as screen fraction (NDC)
    bool  roundPts_ = false;    // R: square vs disc splats
    bool  showThumbs_ = true;   // T: bottom color/depth/ir previews (texture uploads)

    float    fps_ = 0.0f;     // smoothed camera frame rate (from device timestamps)
    uint64_t lastDevTs_ = 0;  // previous device timestamp (us)
};
