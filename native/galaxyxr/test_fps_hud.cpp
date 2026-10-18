// SPDX-License-Identifier: GPL-3.0-or-later
#define GXR_POLICY_TEST
#include <openxr/openxr.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <cassert>
static std::string settings;
static std::string config() { return settings; }
static void* mockProc(const char*);
template<class T> static T proc(XrInstance, const char* name) { return reinterpret_cast<T>(mockProc(name)); }
#include "fps_hud.inc"

static int calls = 0, created = 0, destroyed = 0, spacesCreated = 0, spacesDestroyed = 0;
static bool expectQuad = false, surfaceFailure = false;
static XrResult backendResult = XR_SUCCESS;
static const XrFrameEndInfo* original = nullptr;
static XrResult createSurface(XrSession, const XrSwapchainCreateInfo* info, XrSwapchain* chain, void** surface) {
    ++created;
    assert(info->width == 384 && info->height == 96);
    assert(!info->format && !info->sampleCount && !info->faceCount && !info->arraySize && !info->mipCount);
    if (surfaceFailure) return XR_ERROR_RUNTIME_FAILURE;
    *chain = reinterpret_cast<XrSwapchain>(4); *surface = reinterpret_cast<void*>(5);
    return XR_SUCCESS;
}
static XrResult createSpace(XrSession, const XrReferenceSpaceCreateInfo* info, XrSpace* space) {
    ++spacesCreated;
    assert(info->referenceSpaceType == XR_REFERENCE_SPACE_TYPE_VIEW && info->poseInReferenceSpace.orientation.w == 1);
    *space = reinterpret_cast<XrSpace>(6); return XR_SUCCESS;
}
static XrResult destroySpace(XrSpace) {
    assert(!GxrFpsHud::testPaintEntered.load() || GxrFpsHud::testPaintExited.load());
    ++spacesDestroyed; return XR_SUCCESS;
}
static XrResult destroySwapchain(XrSwapchain) {
    assert(!GxrFpsHud::testPaintEntered.load() || GxrFpsHud::testPaintExited.load());
    ++destroyed; return XR_SUCCESS;
}
static XrResult refresh(XrSession, float* hz) { *hz = 90; return XR_SUCCESS; }
static void* mockProc(const char* name) {
#define MOCK(api, fn) if (!strcmp(name, #api)) return reinterpret_cast<void*>(&fn)
    MOCK(xrCreateSwapchainAndroidSurfaceKHR, createSurface);
    MOCK(xrCreateReferenceSpace, createSpace); MOCK(xrDestroySpace, destroySpace);
    MOCK(xrDestroySwapchain, destroySwapchain); MOCK(xrGetDisplayRefreshRateFB, refresh);
    return nullptr;
#undef MOCK
}
static XrResult endFrame(XrSession, const XrFrameEndInfo* info) {
    ++calls;
    if (!expectQuad) assert(info == original);
    else {
        assert(info != original && info->layerCount == original->layerCount + 1);
        assert(info->next == original->next && info->displayTime == original->displayTime);
        assert(info->environmentBlendMode == original->environmentBlendMode);
        for (uint32_t i = 0; i < original->layerCount; ++i) assert(info->layers[i] == original->layers[i]);
        auto quad = reinterpret_cast<const XrCompositionLayerQuad*>(info->layers[info->layerCount - 1]);
        assert(quad->type == XR_TYPE_COMPOSITION_LAYER_QUAD && quad->eyeVisibility == XR_EYE_VISIBILITY_BOTH);
        assert(quad->space && quad->subImage.swapchain && quad->pose.orientation.w == 1);
    }
    return backendResult;
}
static void begin(XrSession session, bool render = true) {
    XrFrameState wait{XR_TYPE_FRAME_STATE}; wait.shouldRender = render;
    gxrFpsOnWaitFrame(session, &wait); gxrFpsOnBeginFrame(session);
}
static XrResult render(XrSession session, const XrFrameEndInfo* frame, bool shouldRender = true) {
    begin(session, shouldRender); return gxrFpsEndFrame(session, frame, endFrame);
}
static void waitEntered() {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!GxrFpsHud::testPaintEntered.load() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    assert(GxrFpsHud::testPaintEntered.load());
}
static void waitReady(XrSession session) {
    auto worker = GxrFpsHud::sessions.at(session)->worker;
    std::unique_lock<std::mutex> lock(worker->mutex);
    assert(worker->changed.wait_for(lock, std::chrono::seconds(2), [&] { return worker->ready.load() || worker->failed.load(); }));
    assert(worker->ready.load());
    assert(worker->text->display.find("DISPLAY HZ") == 0);
}
static void startBlocked(XrSession session, const XrFrameEndInfo* frame) {
    GxrFpsHud::testPaintEntered = false; GxrFpsHud::testPaintExited = false;
    GxrFpsHud::testBlockPaint = true;
    expectQuad = false;
    auto before = std::chrono::steady_clock::now();
    assert(render(session, frame) == XR_SUCCESS);
    assert(std::chrono::steady_clock::now() - before < std::chrono::milliseconds(200));
    waitEntered(); // a vendor CPU buffer call is still blocked on the worker
}
int main() {
    const auto instance = reinterpret_cast<XrInstance>(1);
    const auto session = reinterpret_cast<XrSession>(2);
    struct AndroidInfo { XrStructureType type; const void* next; void* vm; void* activity; } android{
        XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR, nullptr, reinterpret_cast<void*>(3), nullptr};
    XrInstanceCreateInfo instanceInfo{XR_TYPE_INSTANCE_CREATE_INFO}; instanceInfo.next = &android;
    XrCompositionLayerProjectionView view{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW};
    XrCompositionLayerProjection projectionLayer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
    projectionLayer.viewCount = 1; projectionLayer.views = &view;
    const XrCompositionLayerBaseHeader* layers[] = {reinterpret_cast<XrCompositionLayerBaseHeader*>(&projectionLayer)};
    XrFrameEndInfo frame{XR_TYPE_FRAME_END_INFO};
    frame.layerCount = 1; frame.layers = layers; frame.displayTime = 123;
    frame.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    original = &frame;
    settings = "performance=max\n";
    assert(!gxrFpsHudRequested());
    gxrFpsRememberInstance(instance, &instanceInfo, true);
    assert(GxrFpsHud::instances.empty());
    settings += "fps_hud=on\n";
    assert(gxrFpsHudRequested());
    gxrFpsRememberInstance(instance, &instanceInfo, false);
    assert(GxrFpsHud::instances.empty());
    gxrFpsRememberInstance(instance, &instanceInfo, true);
    gxrFpsRememberSession(instance, session, 16);
    GxrFpsHud::testSurfaceAvailable = true;
    // Until a real visible/focused event AND matching wait/begin pair,
    // preserve the original game frame exactly and allocate no HUD resources.
    assert(gxrFpsEndFrame(session, &frame, endFrame) == XR_SUCCESS && created == 0);
    gxrFpsOnSessionState(session, XR_SESSION_STATE_FOCUSED);
    startBlocked(session, &frame);
    assert(created == 1 && spacesCreated == 1);
    GxrFpsHud::testBlockPaint = false; waitReady(session);
    expectQuad = true;
    // Wait for N+1 may finish before N's end: it cannot hide/count frame N
    // using N+1's false shouldRender value.
    begin(session, true);
    XrFrameState next{XR_TYPE_FRAME_STATE}; next.shouldRender = XR_FALSE;
    gxrFpsOnWaitFrame(session, &next);
    assert(gxrFpsEndFrame(session, &frame, endFrame) == XR_SUCCESS);
    expectQuad = false; gxrFpsOnBeginFrame(session);
    assert(gxrFpsEndFrame(session, &frame, endFrame) == XR_SUCCESS);
    // Never create a HUD-only frame or count a zero-layer submission.
    expectQuad = false; frame.layerCount = 0;
    assert(render(session, &frame) == XR_SUCCESS && created == 1);
    frame.layerCount = 1;
    XrCompositionLayerQuad appQuad{XR_TYPE_COMPOSITION_LAYER_QUAD};
    const XrCompositionLayerBaseHeader* quadOnly[] = {reinterpret_cast<XrCompositionLayerBaseHeader*>(&appQuad)};
    frame.layers = quadOnly;
    assert(render(session, &frame) == XR_SUCCESS && created == 1);
    frame.layers = layers;
    assert(render(session, &frame, false) == XR_SUCCESS);
    GxrFpsHud::sessions.at(session)->maxLayers = 1;
    assert(render(session, &frame) == XR_SUCCESS);
    GxrFpsHud::sessions.at(session)->maxLayers = 16;
    // Error is returned unchanged, original backend called exactly once, HUD
    // disabled and resources freed; subsequent frames remain unmodified.
    backendResult = XR_ERROR_RUNTIME_FAILURE; expectQuad = true;
    int before = calls;
    assert(render(session, &frame) == backendResult && calls == before + 1);
    assert(destroyed == 1 && spacesDestroyed == 1);
    backendResult = XR_SUCCESS; expectQuad = false;
    assert(render(session, &frame) == XR_SUCCESS && created == 1);
    gxrFpsForgetSession(session); assert(GxrFpsHud::sessions.empty());
    // Failed optional surface creation cleans up its reference space and
    // submits the original game frame once without the HUD.
    surfaceFailure = true;
    gxrFpsRememberSession(instance, session, 16);
    gxrFpsOnSessionState(session, XR_SESSION_STATE_VISIBLE);
    assert(render(session, &frame) == XR_SUCCESS);
    assert(created == 2 && spacesCreated == 2 && spacesDestroyed == 2);
    gxrFpsForgetSession(session);
    surfaceFailure = false;
    gxrFpsRememberSession(instance, session, 16);
    gxrFpsOnSessionState(session, XR_SESSION_STATE_VISIBLE);
    startBlocked(session, &frame);
    GxrFpsHud::testBlockPaint = false; waitReady(session);
    expectQuad = true; assert(render(session, &frame) == XR_SUCCESS);
    gxrFpsOnSessionState(session, XR_SESSION_STATE_STOPPING);
    expectQuad = false; assert(gxrFpsEndFrame(session, &frame, endFrame) == XR_SUCCESS);
    gxrFpsForgetInstance(instance);
    assert(GxrFpsHud::instances.empty() && GxrFpsHud::sessions.empty());
    assert(destroyed == 2 && spacesCreated == 3 && spacesDestroyed == 3);
    // Modern loaders may receive their VM only through xrInitializeLoaderKHR,
    // without the legacy Android instance-create pNext structure.
    gxrFpsRememberLoaderInit(nullptr);
    AndroidInfo loader{XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR, nullptr, reinterpret_cast<void*>(7), nullptr};
    gxrFpsRememberLoaderInit(reinterpret_cast<XrLoaderInitInfoBaseHeaderKHR*>(&loader));
    instanceInfo.next = nullptr;
    gxrFpsRememberInstance(instance, &instanceInfo, true);
    assert(GxrFpsHud::instances.at(instance).vm == reinterpret_cast<void*>(7));
    gxrFpsRememberSession(instance, session, 16);
    gxrFpsOnSessionState(session, XR_SESSION_STATE_FOCUSED);
    startBlocked(session, &frame);
    auto blocked = GxrFpsHud::sessions.at(session)->worker;
    std::atomic<bool> backendEndSessionCalled{false};
    std::thread endSessionCaller([&] {
        gxrFpsStopSession(session); // parent wrapper must call this first
        backendEndSessionCalled = true; // mock underlying xrEndSession
    });
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!blocked->stop.load() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    assert(blocked->stop.load() && !backendEndSessionCalled.load());
    assert(destroyed == 2); // resources cannot be destroyed during a post
    GxrFpsHud::testBlockPaint = false;
    endSessionCaller.join();
    assert(backendEndSessionCalled.load());
    assert(GxrFpsHud::testPaintExited.load() && blocked->done);
    unsigned posts = GxrFpsHud::testPostCount.load();
    assert(destroyed == 3 && spacesCreated == 4 && spacesDestroyed == 4);
    // A nonvisible session-state event must also remain inside our hook until
    // the in-flight writer drains; no post can follow event delivery.
    gxrFpsOnSessionState(session, XR_SESSION_STATE_FOCUSED);
    startBlocked(session, &frame);
    blocked = GxrFpsHud::sessions.at(session)->worker;
    std::atomic<bool> eventDelivered{false};
    std::thread pollCaller([&] {
        gxrFpsOnSessionState(session, XR_SESSION_STATE_STOPPING);
        eventDelivered = true;
    });
    deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!blocked->stop.load() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    assert(blocked->stop.load() && !eventDelivered.load());
    GxrFpsHud::testBlockPaint = false;
    pollCaller.join();
    assert(eventDelivered.load() && blocked->done);
    assert(GxrFpsHud::testPostCount.load() == posts + 1);
    posts = GxrFpsHud::testPostCount.load();
    {
        std::unique_lock<std::mutex> lock(blocked->mutex);
        assert(blocked->done);
    }
    gxrFpsForgetInstance(instance);
    assert(GxrFpsHud::testPostCount.load() == posts); // no late producer posts
    assert(destroyed == 4 && spacesCreated == 5 && spacesDestroyed == 5);
    assert(gxrFpsEndFrame(session, &frame, nullptr) == XR_ERROR_FUNCTION_UNSUPPORTED);
    puts("PASS: blocked-paint isolation, drained EndSession/event teardown, no late posts, pipelined latch, layers and VM lifecycle");
}
