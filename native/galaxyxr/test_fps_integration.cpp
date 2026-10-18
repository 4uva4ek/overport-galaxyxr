// SPDX-License-Identifier: GPL-3.0-or-later
// Exercise the public adapter hooks, not only the isolated HUD helpers.
#define GXR_POLICY_TEST
#include "policy.cpp"
#include <cassert>

static bool surfaceSupported = true, rejectSurface = false;
static bool exportInit = true;
static int creations = 0, initCalls = 0, beginCalls = 0;
static const char* eyeNames[] = {XR_FB_SWAPCHAIN_UPDATE_STATE_EXTENSION_NAME,
    XR_FB_FOVEATION_EXTENSION_NAME, XR_FB_FOVEATION_CONFIGURATION_EXTENSION_NAME,
    XR_META_FOVEATION_EYE_TRACKED_EXTENSION_NAME, "XR_ANDROID_eye_tracking"};
static XrResult enumerate(const char*, uint32_t capacity, uint32_t* count, XrExtensionProperties* out) {
    std::vector<const char*> names(std::begin(eyeNames), std::end(eyeNames));
    names.push_back(XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME);
    if (surfaceSupported) names.push_back(GXR_SURFACE_EXTENSION);
    *count = static_cast<uint32_t>(names.size());
    if (capacity >= *count)
        for (uint32_t i = 0; i < *count; ++i) strcpy(out[i].extensionName, names[i]);
    return XR_SUCCESS;
}
static XrResult create(const XrInstanceCreateInfo* info, XrInstance* instance) {
    ++creations;
    auto has = [&](const char* name) {
        for (uint32_t i = 0; i < info->enabledExtensionCount; ++i)
            if (!strcmp(info->enabledExtensionNames[i], name)) return true;
        return false;
    };
    // Removing an optional HUD must preserve the independently selected eye policy.
    for (auto name : eyeNames) assert(has(name));
    if (has(GXR_SURFACE_EXTENSION) && rejectSurface) return XR_ERROR_EXTENSION_NOT_PRESENT;
    *instance = reinterpret_cast<XrInstance>(1);
    return XR_SUCCESS;
}
static XrResult initialize(const XrLoaderInitInfoBaseHeaderKHR*) { ++initCalls; return XR_SUCCESS; }
static XrResult createSession(XrInstance, const XrSessionCreateInfo*, XrSession* session) {
    *session = reinterpret_cast<XrSession>(2); return XR_SUCCESS;
}
static XrResult properties(XrInstance, XrSystemId, XrSystemProperties* info) {
    info->graphicsProperties.maxLayerCount = 16;
    return XR_SUCCESS;
}
static XrResult poll(XrInstance, XrEventDataBuffer* event) {
    auto state = reinterpret_cast<XrEventDataSessionStateChanged*>(event);
    *state = {XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED};
    state->session = reinterpret_cast<XrSession>(2); state->state = XR_SESSION_STATE_FOCUSED;
    return XR_SUCCESS;
}
static XrResult wait(XrSession, const XrFrameWaitInfo*, XrFrameState* info) {
    info->shouldRender = XR_TRUE; return XR_SUCCESS;
}
static XrResult begin(XrSession, const XrFrameBeginInfo*) { ++beginCalls; return XR_FRAME_DISCARDED; }
static XrResult end(XrSession, const XrFrameEndInfo*) { return XR_SUCCESS; }
static XrResult destroyInstance(XrInstance) { return XR_SUCCESS; }
static XrResult destroySession(XrSession) { return XR_SUCCESS; }
static XrResult get(XrInstance, const char* name, PFN_xrVoidFunction* fn) {
    if (!strcmp(name, "xrInitializeLoaderKHR")) {
        *fn = reinterpret_cast<PFN_xrVoidFunction>(&initialize);
        return XR_SUCCESS;
    }
    *fn = reinterpret_cast<PFN_xrVoidFunction>(testSymbol(name));
    return *fn ? XR_SUCCESS : XR_ERROR_FUNCTION_UNSUPPORTED;
}
static void* testSymbol(const char* name) {
#define MOCK(api, fn) if (!strcmp(name, #api)) return reinterpret_cast<void*>(&fn)
    MOCK(xrEnumerateInstanceExtensionProperties, enumerate); MOCK(xrCreateInstance, create);
    if (exportInit) { MOCK(xrInitializeLoaderKHR, initialize); }
    MOCK(xrCreateSession, createSession);
    MOCK(xrGetSystemProperties, properties); MOCK(xrGetInstanceProcAddr, get);
    MOCK(xrPollEvent, poll); MOCK(xrWaitFrame, wait); MOCK(xrBeginFrame, begin); MOCK(xrEndFrame, end);
    MOCK(xrDestroySession, destroySession); MOCK(xrDestroyInstance, destroyInstance);
    return nullptr;
#undef MOCK
}
int main() {
    exportInit = false;
    assert(xrInitializeLoaderKHR(nullptr) == XR_SUCCESS && initCalls == 1);
    exportInit = true;
    initCalls = 0;
    testConfig = "performance=app\nfoveation=runtime-eye\nfps_hud=on\n";
    struct AndroidInit { XrStructureType type; const void* next; void* vm; void* context; } android{
        XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR, nullptr, reinterpret_cast<void*>(3), nullptr};
    assert(xrInitializeLoaderKHR(reinterpret_cast<XrLoaderInitInfoBaseHeaderKHR*>(&android)) == XR_SUCCESS);
    assert(initCalls == 1);
    XrInstance instance;
    XrInstanceCreateInfo info{XR_TYPE_INSTANCE_CREATE_INFO};
    assert(xrCreateInstance(&info, &instance) == XR_SUCCESS && creations == 1);
    assert(GxrFpsHud::instances.size() == 1);
    XrSession session;
    XrSessionCreateInfo sessionInfo{XR_TYPE_SESSION_CREATE_INFO};
    assert(xrCreateSession(instance, &sessionInfo, &session) == XR_SUCCESS);
    XrEventDataBuffer event{XR_TYPE_EVENT_DATA_BUFFER};
    assert(xrPollEvent(instance, &event) == XR_SUCCESS);
    XrFrameState frame{XR_TYPE_FRAME_STATE};
    assert(xrWaitFrame(session, nullptr, &frame) == XR_SUCCESS);
    // XR_FRAME_DISCARDED is a successful begin for the current frame.
    assert(xrBeginFrame(session, nullptr) == XR_FRAME_DISCARDED && beginCalls == 1);
    assert(xrEndFrame(session, nullptr) == XR_SUCCESS);
    PFN_xrVoidFunction fn = nullptr;
    assert(xrGetInstanceProcAddr(instance, "xrEndFrame", &fn) == XR_SUCCESS);
    assert(fn == reinterpret_cast<PFN_xrVoidFunction>(&xrEndFrame));
    assert(xrDestroySession(session) == XR_SUCCESS);
    assert(xrDestroyInstance(instance) == XR_SUCCESS && GxrFpsHud::instances.empty());
    rejectSurface = true; creations = 0;
    assert(xrCreateInstance(&info, &instance) == XR_SUCCESS && creations == 2);
    assert(GxrFpsHud::instances.empty()); // unsupported diagnostics were removed, not eye policy
    assert(xrDestroyInstance(instance) == XR_SUCCESS);
    surfaceSupported = false; creations = 0;
    assert(xrCreateInstance(&info, &instance) == XR_SUCCESS && creations == 1);
    assert(GxrFpsHud::instances.empty());
    assert(xrDestroyInstance(instance) == XR_SUCCESS);
    testConfig = "performance=app\nfoveation=runtime-eye\nfps_hud=off\n";
    surfaceSupported = true; rejectSurface = false; creations = 0;
    assert(xrCreateInstance(&info, &instance) == XR_SUCCESS && creations == 1);
    assert(GxrFpsHud::instances.empty());
    assert(xrDestroyInstance(instance) == XR_SUCCESS);
    puts("PASS: HUD extension fallback, eye policy preservation, loader init, public frame hooks and GIPA");
}
