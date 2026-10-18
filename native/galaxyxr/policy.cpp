// SPDX-License-Identifier: GPL-3.0-or-later
// App-local generic-loader adapter. No system properties, root, files or services are modified.
#define XR_EXTENSION_PROTOTYPES
#include <openxr/openxr.h>
#ifndef GXR_POLICY_TEST
#include <android/log.h>
#endif
#include <dlfcn.h>
#include <algorithm>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <chrono>

#ifdef GXR_POLICY_TEST
#include <cstdio>
#define LOG(...) do { printf(__VA_ARGS__); printf("\n"); } while (0)
static void* testSymbol(const char* name);
static std::string testConfig = "performance=max\n";
#else
#define LOG(...) __android_log_print(ANDROID_LOG_INFO, "OverportGXR", __VA_ARGS__)

static void* backend() {
    static void* handle = [] {
        Dl_info self{};
        if (!dladdr(reinterpret_cast<void*>(&backend), &self) || !self.dli_fname) return static_cast<void*>(nullptr);
        std::string path(self.dli_fname);
        path.resize(path.find_last_of('/') + 1);
        path += "libopenxr_loader_gxrbase.so";
        void* result = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (!result) LOG("Cannot load original generic loader: %s", dlerror());
        return result;
    }();
    return handle;
}
#endif

template<typename T> static T symbol(const char* name) {
#ifdef GXR_POLICY_TEST
    return reinterpret_cast<T>(testSymbol(name));
#else
    void* handle = backend();
    return handle ? reinterpret_cast<T>(dlsym(handle, name)) : nullptr;
#endif
}
template<typename T> static T proc(XrInstance instance, const char* name) {
    auto get = symbol<PFN_xrGetInstanceProcAddr>("xrGetInstanceProcAddr");
    PFN_xrVoidFunction fn = nullptr;
    return get && XR_SUCCEEDED(get(instance, name, &fn)) ? reinterpret_cast<T>(fn) : nullptr;
}

struct Policy {
    PFN_xrCreateFoveationProfileFB createFoveation = nullptr;
    PFN_xrDestroyFoveationProfileFB destroyFoveation = nullptr;
    PFN_xrUpdateSwapchainFB updateSwapchain = nullptr;
    PFN_xrGetFoveationEyeTrackedStateMETA eyeState = nullptr;
    bool eye = false;
    bool gles = false;
    bool qcom = false;
    PFN_xrPerfSettingsSetPerformanceLevelEXT perf = nullptr;
    PFN_xrEnumerateDisplayRefreshRatesFB rates = nullptr;
    PFN_xrRequestDisplayRefreshRateFB request = nullptr;
    bool supports90 = false;
};
static std::mutex stateMutex;
static std::unordered_map<XrInstance, Policy> instances;
struct Session { XrInstance instance; Policy policy; uint32_t eyeFrames = 0; bool runtimeEyeAttempted = false; };
static std::unordered_map<XrSession, Session> sessions;

static std::string config() {
#ifdef GXR_POLICY_TEST
    return testConfig;
#else
    static const std::string text = [] {
        Dl_info self{};
        if (!dladdr(reinterpret_cast<void*>(&backend), &self) || !self.dli_fname) return std::string{};
        std::string path(self.dli_fname);
        path.resize(path.find_last_of('/') + 1);
        FILE* file = fopen((path + "liboverport.galaxyxr.config.so").c_str(), "rb");
        if (!file) return std::string{};
        char buffer[1024]{};
        size_t length = fread(buffer, 1, sizeof(buffer), file);
        fclose(file);
        return std::string(buffer, length);
    }();
    return text;
#endif
}
static bool maxPerformance() { static bool enabled = config().find("performance=max\n") != std::string::npos; return enabled; }
static bool runtimeEyeFixed() { return config().find("foveation=runtime-eye-fixed\n") != std::string::npos; }
static bool runtimeEyeDiagnostic() { return runtimeEyeFixed() || config().find("foveation=runtime-eye-debug\n") != std::string::npos; }
static auto runtimeDiagnosticStarted=std::chrono::steady_clock::now();
static int runtimeDiagnosticPhase() { return runtimeEyeFixed()?1:(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now()-runtimeDiagnosticStarted).count()/10)%3; }
static bool runtimeDiagnosticCoarse() { return runtimeDiagnosticPhase()!=0; }
// Fixed-center stress test: shrink the ideal full-density radius 4x versus gain 8.
static float runtimeDiagnosticGain() { return runtimeEyeFixed()?32.f:8.f; }
static bool runtimeDiagnosticCenter(const XrFovf& fov,float& x,float& y) {
    float l=std::tan(fov.angleLeft),r=std::tan(fov.angleRight),b=std::tan(fov.angleDown),t=std::tan(fov.angleUp);
    if(!(r>l) || !(t>b)) return false;
    float tangent=0;
    if(runtimeEyeDiagnostic() && runtimeDiagnosticPhase()==2) {
        float seconds=std::chrono::duration<float>(std::chrono::steady_clock::now()-runtimeDiagnosticStarted).count();
        tangent=.25f*std::sin(seconds*1.25663706f);
    }
    x=(2*tangent-r-l)/(r-l); y=-(t+b)/(t-b);
    return std::isfinite(x) && std::isfinite(y) && std::abs(x)<=1 && std::abs(y)<=1;
}
static bool runtimeEyeFoveation() { return runtimeEyeDiagnostic() || config().find("foveation=runtime-eye\n") != std::string::npos; }
static bool eyeFoveation() { return runtimeEyeFoveation() || config().find("foveation=eye\n") != std::string::npos; }
static float qcomSetting(const char* key, float fallback, float low, float high) {
    std::string text="\n"+config(), token="\n"+std::string(key)+"=";
    auto start=text.find(token); if(start==std::string::npos) return fallback;
    start+=token.size(); auto end=text.find('\n',start);
    std::string value=text.substr(start,end-start); char* tail=nullptr;
    float number=std::strtof(value.c_str(),&tail);
    return tail!=value.c_str() && *tail=='\0' && std::isfinite(number) && number>=low && number<=high ? number : fallback;
}
static bool qcomDiagnostic() { return config().find("foveation=qcom-debug\n") != std::string::npos; }
static bool qcomFoveation() { return qcomDiagnostic() || config().find("foveation=qcom\n") != std::string::npos; }
static Policy sessionPolicy(XrSession session) {
    std::lock_guard<std::mutex> lock(stateMutex);
    auto it = sessions.find(session);
    return it == sessions.end() ? Policy{} : it->second.policy;
}

#include "qcom_foveation.inc"
#include "runtime_eye.inc"

extern "C" XRAPI_ATTR XrResult XRAPI_CALL xrCreateInstance(const XrInstanceCreateInfo* info, XrInstance* instance) {
    auto create = symbol<PFN_xrCreateInstance>("xrCreateInstance");
    if (!create) return XR_ERROR_INITIALIZATION_FAILED;
    if (!info) return create(info, instance);
    auto enumerate = symbol<PFN_xrEnumerateInstanceExtensionProperties>("xrEnumerateInstanceExtensionProperties");
    uint32_t count = 0;
    std::vector<XrExtensionProperties> available;
    if (enumerate && XR_SUCCEEDED(enumerate(nullptr, 0, &count, nullptr)) && count <= 4096) {
        available.resize(count, {XR_TYPE_EXTENSION_PROPERTIES});
        if (XR_FAILED(enumerate(nullptr, count, &count, available.data()))) available.clear();
        else available.resize(count);
    }
    std::vector<const char*> names;
    for (uint32_t i = 0; i < info->enabledExtensionCount; ++i) names.push_back(info->enabledExtensionNames[i]);
    auto enable = [&](const char* name) {
        bool supported = std::any_of(available.begin(), available.end(), [&](const auto& p) { return !strcmp(p.extensionName, name); });
        bool enabled = std::any_of(names.begin(), names.end(), [&](const char* p) { return !strcmp(p, name); });
        if (supported && !enabled) names.push_back(name);
        return supported;
    };
    if (maxPerformance()) { enable(XR_EXT_PERFORMANCE_SETTINGS_EXTENSION_NAME); enable(XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME); }
    const char* eyeExtensions[] = {XR_FB_SWAPCHAIN_UPDATE_STATE_EXTENSION_NAME,
        XR_FB_FOVEATION_EXTENSION_NAME, XR_FB_FOVEATION_CONFIGURATION_EXTENSION_NAME,
        XR_META_FOVEATION_EYE_TRACKED_EXTENSION_NAME};
    bool eye = eyeFoveation() && std::all_of(std::begin(eyeExtensions), std::end(eyeExtensions), [&](const char* name) {
        return std::any_of(available.begin(), available.end(), [&](const auto& p) { return !strcmp(p.extensionName, name); });
    });
    auto namesWithoutEye = names;
    bool qcom = (qcomFoveation() || runtimeEyeFoveation()) && enable("XR_ANDROID_eye_tracking");
    if (eye) for (auto name : eyeExtensions) enable(name);
    auto enabled = [&](const char* name) { return std::any_of(names.begin(), names.end(), [&](const char* p) { return !strcmp(p, name); }); };
    bool perf = enabled(XR_EXT_PERFORMANCE_SETTINGS_EXTENSION_NAME);
    bool refresh = enabled(XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME);
    auto copy = *info;
    copy.enabledExtensionCount = static_cast<uint32_t>(names.size());
    copy.enabledExtensionNames = names.data();
    XrResult result = create(&copy, instance);
    if (XR_FAILED(result) && (eye || qcom) && names != namesWithoutEye) {
        LOG("Eye foveation extension request refused=%d; retrying original extension set", result);
        names = namesWithoutEye;
        copy.enabledExtensionCount = static_cast<uint32_t>(names.size());
        copy.enabledExtensionNames = names.data();
        eye = false; qcom = false;
        result = create(&copy, instance);
    }
    if (XR_SUCCEEDED(result) && instance) {
        Policy policy;
        policy.eye = eye;
        policy.qcom = qcom;
        if (enabled(XR_FB_FOVEATION_EXTENSION_NAME)) {
            policy.createFoveation = proc<PFN_xrCreateFoveationProfileFB>(*instance, "xrCreateFoveationProfileFB");
            policy.destroyFoveation = proc<PFN_xrDestroyFoveationProfileFB>(*instance, "xrDestroyFoveationProfileFB");
        }
        if (enabled(XR_FB_SWAPCHAIN_UPDATE_STATE_EXTENSION_NAME)) policy.updateSwapchain = proc<PFN_xrUpdateSwapchainFB>(*instance, "xrUpdateSwapchainFB");
        if (eye) policy.eyeState = proc<PFN_xrGetFoveationEyeTrackedStateMETA>(*instance, "xrGetFoveationEyeTrackedStateMETA");
        if (perf) policy.perf = proc<PFN_xrPerfSettingsSetPerformanceLevelEXT>(*instance, "xrPerfSettingsSetPerformanceLevelEXT");
        if (refresh) {
            policy.rates = proc<PFN_xrEnumerateDisplayRefreshRatesFB>(*instance, "xrEnumerateDisplayRefreshRatesFB");
            policy.request = proc<PFN_xrRequestDisplayRefreshRateFB>(*instance, "xrRequestDisplayRefreshRateFB");
        }
        std::lock_guard<std::mutex> lock(stateMutex);
        instances[*instance] = policy;
        LOG("Policy: maxPerformance=%d; extensions perf=%d refresh=%d", maxPerformance(), perf, refresh);
        LOG("Eye foveation requested=%d extensions=%d", eyeFoveation(), eye);
    }
    return result;
}

extern "C" XRAPI_ATTR XrResult XRAPI_CALL xrCreateSession(XrInstance instance, const XrSessionCreateInfo* info, XrSession* session) {
    auto create = symbol<PFN_xrCreateSession>("xrCreateSession");
    if (!create) return XR_ERROR_FUNCTION_UNSUPPORTED;
    XrResult result = create(instance, info, session);
    if (XR_SUCCEEDED(result) && session) {
        std::lock_guard<std::mutex> lock(stateMutex);
        auto it = instances.find(instance);
        sessions[*session] = {instance, it == instances.end() ? Policy{} : it->second};
        auto& policy = sessions[*session].policy;
        for (auto node = info ? static_cast<const XrBaseInStructure*>(info->next) : nullptr; node; node = node->next)
            if (node->type == XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR) policy.gles = true;
        if (policy.eye) {
            XrSystemFoveationEyeTrackedPropertiesMETA eye{XR_TYPE_SYSTEM_FOVEATION_EYE_TRACKED_PROPERTIES_META};
            XrSystemProperties properties{XR_TYPE_SYSTEM_PROPERTIES, &eye};
            auto getProperties = proc<PFN_xrGetSystemProperties>(instance, "xrGetSystemProperties");
            XrResult support = info && getProperties ? getProperties(instance, info->systemId, &properties) : XR_ERROR_FUNCTION_UNSUPPORTED;
            policy.eye = XR_SUCCEEDED(support) && eye.supportsFoveationEyeTracked && policy.eyeState && policy.createFoveation && policy.destroyFoveation && policy.updateSwapchain;
            LOG("Eye foveation system support=%d result=%d", policy.eye, support);
        }
    }
    if (result == XR_SUCCESS && session) qcomSessionCreated(*session, info);
    return result;
}

static void applyDefaults(XrSession session) {
    auto policy = sessionPolicy(session);
    if (policy.perf) {
        auto cpu = policy.perf(session, XR_PERF_SETTINGS_DOMAIN_CPU_EXT, XR_PERF_SETTINGS_LEVEL_BOOST_EXT);
        auto gpu = policy.perf(session, XR_PERF_SETTINGS_DOMAIN_GPU_EXT, XR_PERF_SETTINGS_LEVEL_BOOST_EXT);
        LOG("BOOST requests: CPU result=%d GPU result=%d (runtime controls clocks/thermal limits)", cpu, gpu);
    } else LOG("BOOST skipped: performance extension unavailable");
    bool supported = false;
    uint32_t count = 0;
    if (policy.rates && policy.request && XR_SUCCEEDED(policy.rates(session, 0, &count, nullptr)) && count > 0 && count <= 64) {
        std::vector<float> rates(count);
        if (XR_SUCCEEDED(policy.rates(session, count, &count, rates.data())) && count <= rates.size()) {
            for (uint32_t i = 0; i < count; ++i) {
                LOG("Supported refresh rate: %.2f Hz", rates[i]);
                if (rates[i] > 89.9f && rates[i] < 90.1f) supported = true;
            }
        }
    }
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        auto it = sessions.find(session);
        if (it != sessions.end()) it->second.policy.supports90 = supported;
    }
    if (supported) LOG("90 Hz request result=%d", policy.request(session, 90.0f));
    else LOG("90 Hz skipped: unavailable; preserving runtime refresh rate");
}

extern "C" XRAPI_ATTR XrResult XRAPI_CALL xrBeginSession(XrSession session, const XrSessionBeginInfo* info) {
    auto begin = symbol<PFN_xrBeginSession>("xrBeginSession");
    if (!begin) return XR_ERROR_FUNCTION_UNSUPPORTED;
    XrResult result = begin(session, info);
    if (result == XR_SUCCESS) { qcomRunning(session, true); enableRuntimeEye(session); }
    if (XR_SUCCEEDED(result) && maxPerformance()) applyDefaults(session);
    return result; // A refused performance/rate hint must never turn successful begin into failure.
}

extern "C" XRAPI_ATTR XrResult XRAPI_CALL xrPerfSettingsSetPerformanceLevelEXT(XrSession session, XrPerfSettingsDomainEXT domain, XrPerfSettingsLevelEXT level) {
    auto policy = sessionPolicy(session);
    if (!policy.perf) return XR_ERROR_FUNCTION_UNSUPPORTED;
    if (maxPerformance() && (domain == XR_PERF_SETTINGS_DOMAIN_CPU_EXT || domain == XR_PERF_SETTINGS_DOMAIN_GPU_EXT)) level = XR_PERF_SETTINGS_LEVEL_BOOST_EXT;
    return policy.perf(session, domain, level);
}

extern "C" XRAPI_ATTR XrResult XRAPI_CALL xrRequestDisplayRefreshRateFB(XrSession session, float rate) {
    auto policy = sessionPolicy(session);
    if (!policy.request) return XR_ERROR_FUNCTION_UNSUPPORTED;
    return policy.request(session, policy.supports90 ? 90.0f : rate);
}

#include "eye_foveation.inc"
#include "hand_tracking.inc"
#include "haptics.inc"

extern "C" XRAPI_ATTR XrResult XRAPI_CALL xrDestroySession(XrSession session) {
    auto destroy = symbol<PFN_xrDestroySession>("xrDestroySession");
    if (!destroy) return XR_ERROR_FUNCTION_UNSUPPORTED;
    haptics.forgetSession(session);
    XrResult result = destroy(session);
    if (XR_SUCCEEDED(result)) { std::lock_guard<std::mutex> lock(stateMutex); forgetEyeSession(session); qcomForget(session); sessions.erase(session); }
    return result;
}

extern "C" XRAPI_ATTR XrResult XRAPI_CALL xrDestroyInstance(XrInstance instance) {
    auto destroy = symbol<PFN_xrDestroyInstance>("xrDestroyInstance");
    if (!destroy) return XR_ERROR_FUNCTION_UNSUPPORTED;
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        for (const auto& entry : sessions)
            if (entry.second.instance == instance) haptics.forgetSession(entry.first);
    }
    XrResult result = destroy(instance);
    if (XR_SUCCEEDED(result)) {
        std::lock_guard<std::mutex> lock(stateMutex);
        instances.erase(instance);
        for (auto it = sessions.begin(); it != sessions.end();) {
            if (it->second.instance == instance) { forgetEyeSession(it->first); qcomForget(it->first); it = sessions.erase(it); } else ++it;
        }
    }
    return result;
}

extern "C" XRAPI_ATTR XrResult XRAPI_CALL xrGetInstanceProcAddr(XrInstance instance, const char* name, PFN_xrVoidFunction* function) {
    auto get = symbol<PFN_xrGetInstanceProcAddr>("xrGetInstanceProcAddr");
    if (!get) return XR_ERROR_INITIALIZATION_FAILED;
    XrResult result = get(instance, name, function);
    // Preserve validation and extension availability errors returned by the original loader.
    if (XR_FAILED(result) || !name || !function || !*function) return result;
#define HOOK(fn) if (!strcmp(name, #fn)) *function = reinterpret_cast<PFN_xrVoidFunction>(&fn)
    HOOK(xrGetInstanceProcAddr);
    HOOK(xrCreateInstance); HOOK(xrDestroyInstance);
    HOOK(xrCreateSession); HOOK(xrDestroySession); HOOK(xrBeginSession); HOOK(xrEndSession);
    HOOK(xrApplyHapticFeedback); HOOK(xrStopHapticFeedback);
    HOOK(xrCreateAction); HOOK(xrDestroyAction); HOOK(xrDestroyActionSet);
    HOOK(xrPerfSettingsSetPerformanceLevelEXT); HOOK(xrRequestDisplayRefreshRateFB);
    HOOK(xrCreateFoveationProfileFB); HOOK(xrDestroyFoveationProfileFB);
    HOOK(xrAcquireSwapchainImage); HOOK(xrWaitSwapchainImage); HOOK(xrReleaseSwapchainImage); HOOK(xrEndFrame);
    HOOK(xrCreateSwapchain); HOOK(xrDestroySwapchain); HOOK(xrUpdateSwapchainFB); HOOK(xrWaitFrame);
#ifdef GXR_INPUT_DIAGNOSTICS
    HOOK(xrSyncActions); HOOK(xrSuggestInteractionProfileBindings);
    HOOK(xrGetActionStateBoolean); HOOK(xrGetCurrentInteractionProfile);
#endif
    HAND_HOOK(xrCreateHandTrackerEXT, origCreateHandTracker); HAND_HOOK(xrDestroyHandTrackerEXT, origDestroyHandTracker);
    HAND_HOOK(xrLocateHandJointsEXT, origLocateHandJoints); HAND_HOOK(xrGetHandMeshFB, origGetHandMesh);
#undef HOOK
    return result;
}

#include "forwarders.inc"
#ifdef GXR_INPUT_DIAGNOSTICS
#include "input_diagnostics.inc"
#endif
