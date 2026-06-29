#include "tcApp.h"

void tcApp::setup() {
    setWindowTitle("tcxOrbbec - Orbbec point cloud");

    camera = make_shared<Orbbec>();        // first device
    camera->setThreaded(true);             // grab on a background thread (before setup)
    camera->enableDepth();                 // streams are all off by default; enable what we use
    camera->enableColor();
    camera->enableInfrared();
    deviceOk = camera->setup();            // false if no device / SDK runtime missing
    if (!deviceOk) {
        logError("example-basic") << "Orbbec camera not available — is a device "
                                     "connected and the Orbbec SDK installed?";
    }

    // Orbit around the camera origin (0,0,0) — where the sensor sits in the
    // point cloud. distance frames the scene.
    view.setTarget(0.0f, 0.0f, 0.0f);
    view.setDistance(500.0f);
    view.enableMouseInput();
    // Orthographic projection: a depth grid rendered as 1px points aliases into
    // "interlaced" scanlines under perspective (row spacing varies with distance,
    // beating against the pixel grid). Ortho keeps row spacing uniform, so the
    // cloud reads smooth — this is what Orbbec-Sample-Viewer uses. Toggle with O.
    view.enableOrtho();
}

void tcApp::update() {
    if (!deviceOk) return;
    camera->update();
    if (camera->isFrameNew()) {
        // Camera frame rate from device-timestamp deltas (smoothed).
        const uint64_t ts = camera->getDeviceTimestampUs();
        if (lastDevTs_ != 0 && ts > lastDevTs_) {
            const float inst = 1.0e6f / float(ts - lastDevTs_);
            fps_ = (fps_ <= 0.0f) ? inst : fps_ * 0.9f + inst * 0.1f;
        }
        lastDevTs_ = ts;
        rebuild();
    }
}

// Rebuild the point-cloud mesh from the latest frame, in display units.
void tcApp::rebuild() {
    cloud = camera->toMesh({.colors = colored, .step = step});
    // tcxDepthCamera world coords are camera/CV space (+X right, +Y down, +Z
    // away). Convert to GL display: scale up and flip BOTH Y and Z. Flipping only
    // Y leaves a left-handed, depth-mirrored cloud (looks ok head-on but stretched
    // /wrong when you orbit). Negating Z too makes it right-handed; the camera
    // (sensor) stays at the origin as the orbit pivot. Matches ofxOrbbec's (x,-y,-z).
    cloud.scale(100.0f, -100.0f, -100.0f);
}

void tcApp::draw() {
    clear(0.08f, 0.09f, 0.11f);

    if (deviceOk) {
        view.begin();
        pushMatrix();
        if (!colored) setColor(0.55f, 0.75f, 1.0f);
        cloud.draw();
        popMatrix();
        view.end();
    }

    // 2D stream previews along the bottom: color, depth, IR thumbnails. Each is
    // the camera's cached preview Image (getXxxImage()) - uploaded once per new
    // frame, no per-app conversion code. Drawn one after another, advancing x.
    if (deviceOk) {
        const float h = 130.0f, pad = 10.0f;
        const float y = getWindowHeight() - h - pad;
        float x = pad;
        setColor(1.0f);

        if (camera->hasColor()) {
            Image& color = camera->getColorImage();
            if (color.isAllocated()) {
                float w = h * color.getWidth() / (float)color.getHeight();
                color.draw(x, y, w, h);
                drawBitmapString("color", x + 4, y - 6);
                x += w + pad;
            }
        }

        Image& depth = camera->getDepthImage({.nearM = 0.3f, .farM = 4.0f});
        if (depth.isAllocated()) {
            float w = h * depth.getWidth() / (float)depth.getHeight();
            depth.draw(x, y, w, h);
            drawBitmapString("depth", x + 4, y - 6);
            x += w + pad;
        }

        if (camera->hasInfrared()) {
            Image& ir = camera->getInfraredImage();
            if (ir.isAllocated()) {
                float w = h * ir.getWidth() / (float)ir.getHeight();
                ir.draw(x, y, w, h);
                drawBitmapString("ir", x + 4, y - 6);
                x += w + pad;
            }
        }
    }

    setColor(1.0f);
    string hud;
    if (deviceOk) {
        hud  = camera->getDeviceName().empty() ? "Orbbec\n" : (camera->getDeviceName() + "\n");
        hud += to_string(cloud.getNumVertices()) + " points";
        hud += colored ? "  [colored]" : "  [plain]";
        hud += view.getOrtho() ? "  [ortho]" : "  [persp]";
        hud += "\nC: color    O: ortho/persp    1-4: decimation    drag: orbit";
    } else {
        hud  = "No Orbbec device found.\n";
        hud += "Connect the camera, install the Orbbec SDK v2, and relaunch.";
    }
    drawBitmapString(hud, 20, 20);

    // OrbbecViewer-style frame telemetry (top-right). Useful for spotting
    // dropped/duplicated frames or an unstable rate.
    if (deviceOk) {
        const int f10 = (int)(fps_ * 10.0f + 0.5f);
        string t  = "Frame index:        " + to_string(camera->getFrameIndex()) + "\n";
        t        += "Device timestamp:   " + to_string(camera->getDeviceTimestampUs()) + " us\n";
        t        += "Global timestamp:   " + to_string(camera->getGlobalTimestampUs()) + " us\n";
        t        += "System timestamp:   " + to_string(camera->getSystemTimestampUs()) + " us\n";
        t        += "Average frame rate: " + to_string(f10 / 10) + "." + to_string(f10 % 10) + " fps";
        drawBitmapString(t, getWindowWidth() - 380.0f, 20.0f);
    }
}

void tcApp::keyPressed(int key) {
    if (key == 'c' || key == 'C') {
        colored = !colored;
    } else if (key == 'o' || key == 'O') {
        view.setOrtho(!view.getOrtho());   // compare ortho vs perspective
    } else if (key >= '1' && key <= '4') {
        step = key - '0';
    }
    if (deviceOk) rebuild();
}
