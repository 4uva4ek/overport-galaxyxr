// Focused host test: extension negotiation, lifecycle, failed hints and unsupported refresh.
#define GXR_POLICY_TEST
#include "policy.cpp"
#include <cassert>

static bool extensions = true, rate90 = true, failBegin = false, failHints = false;
static int cpuCalls = 0, gpuCalls = 0, rateCalls = 0;
static float requestedRate = 0;
static bool appMode = false;
static bool appRequestedExtensions = false;
static XrResult mockEnumerate(const char*, uint32_t capacity, uint32_t* count, XrExtensionProperties* out) {
    *count = extensions ? 2 : 0;
    if (extensions && capacity >= 2) {
        strcpy(out[0].extensionName, XR_EXT_PERFORMANCE_SETTINGS_EXTENSION_NAME);
        strcpy(out[1].extensionName, XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME);
    }
    return XR_SUCCESS;
}
static XrResult mockCreate(const XrInstanceCreateInfo* info, XrInstance* instance) {
    assert(info->enabledExtensionCount == (extensions && (!appMode || appRequestedExtensions) ? 2u : 0u));
    *instance = reinterpret_cast<XrInstance>(1);
    return XR_SUCCESS;
}
static XrResult mockCreateSession(XrInstance, const XrSessionCreateInfo*, XrSession* session) {
    *session = reinterpret_cast<XrSession>(2); return XR_SUCCESS;
}
static XrResult mockBegin(XrSession, const XrSessionBeginInfo*) { return failBegin ? XR_ERROR_RUNTIME_FAILURE : XR_SUCCESS; }
static XrResult mockPerf(XrSession, XrPerfSettingsDomainEXT domain, XrPerfSettingsLevelEXT level) {
    assert(level == (appMode ? XR_PERF_SETTINGS_LEVEL_POWER_SAVINGS_EXT : XR_PERF_SETTINGS_LEVEL_BOOST_EXT));
    if (domain == XR_PERF_SETTINGS_DOMAIN_CPU_EXT) ++cpuCalls; else ++gpuCalls;
    return failHints ? XR_ERROR_RUNTIME_FAILURE : XR_SUCCESS;
}
static XrResult mockRates(XrSession, uint32_t capacity, uint32_t* count, float* rates) {
    *count = rate90 ? 3 : 2;
    if (capacity >= *count) { rates[0] = 60; rates[1] = 72; if (rate90) rates[2] = 90; }
    return XR_SUCCESS;
}
static XrResult mockRate(XrSession, float rate) { ++rateCalls; requestedRate = rate; return failHints ? XR_ERROR_RUNTIME_FAILURE : XR_SUCCESS; }
static XrResult mockDestroySession(XrSession) { return XR_SUCCESS; }
static XrResult mockDestroyInstance(XrInstance) { return XR_SUCCESS; }
static XrResult mockGet(XrInstance, const char* name, PFN_xrVoidFunction* fn) {
    *fn = reinterpret_cast<PFN_xrVoidFunction>(testSymbol(name));
    return *fn ? XR_SUCCESS : XR_ERROR_FUNCTION_UNSUPPORTED;
}
static void* testSymbol(const char* name) {
#define MOCK(api, impl) if (!strcmp(name, #api)) return reinterpret_cast<void*>(&impl)
    MOCK(xrEnumerateInstanceExtensionProperties, mockEnumerate);
    MOCK(xrCreateInstance, mockCreate); MOCK(xrCreateSession, mockCreateSession);
    MOCK(xrBeginSession, mockBegin); MOCK(xrGetInstanceProcAddr, mockGet);
    MOCK(xrDestroyInstance, mockDestroyInstance); MOCK(xrDestroySession, mockDestroySession);
    if (extensions) {
        MOCK(xrPerfSettingsSetPerformanceLevelEXT, mockPerf);
        MOCK(xrEnumerateDisplayRefreshRatesFB, mockRates); MOCK(xrRequestDisplayRefreshRateFB, mockRate);
    }
    return nullptr;
#undef MOCK
}
int main(int argc, char**) {
    appMode = argc > 1;
    if (appMode) testConfig = "performance=app\n";
    XrInstance instance;
    XrSession session;
    XrInstanceCreateInfo info{XR_TYPE_INSTANCE_CREATE_INFO};
    auto start = [&] {
        assert(xrCreateInstance(&info, &instance) == XR_SUCCESS);
        assert(xrCreateSession(instance, nullptr, &session) == XR_SUCCESS);
    };
    if (appMode) {
        // No implicit extensions or hints; explicitly requested extensions still work.
        start();
        assert(xrBeginSession(session, nullptr) == XR_SUCCESS && cpuCalls == 0 && rateCalls == 0);
        assert(xrDestroyInstance(instance) == XR_SUCCESS);
        appRequestedExtensions = true;
        const char* names[] = {XR_EXT_PERFORMANCE_SETTINGS_EXTENSION_NAME, XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME};
        info.enabledExtensionCount = 2; info.enabledExtensionNames = names;
        start();
        assert(xrBeginSession(session, nullptr) == XR_SUCCESS && cpuCalls == 0 && rateCalls == 0);
        assert(xrPerfSettingsSetPerformanceLevelEXT(session, XR_PERF_SETTINGS_DOMAIN_CPU_EXT, XR_PERF_SETTINGS_LEVEL_POWER_SAVINGS_EXT) == XR_SUCCESS);
        assert(xrRequestDisplayRefreshRateFB(session, 72) == XR_SUCCESS && requestedRate == 72);
        assert(xrDestroyInstance(instance) == XR_SUCCESS && sessions.empty());
        puts("PASS: app mode preserves requested extensions, levels and refresh without automatic hints");
        return 0;
    }
    start();
    failBegin = true;
    assert(xrBeginSession(session, nullptr) == XR_ERROR_RUNTIME_FAILURE);
    assert(cpuCalls == 0 && rateCalls == 0);
    failBegin = false; failHints = true;
    assert(xrBeginSession(session, nullptr) == XR_SUCCESS); // hints cannot fail a session
    assert(cpuCalls == 1 && gpuCalls == 1 && requestedRate == 90);
    failHints = false;
    assert(xrPerfSettingsSetPerformanceLevelEXT(session, XR_PERF_SETTINGS_DOMAIN_CPU_EXT, XR_PERF_SETTINGS_LEVEL_POWER_SAVINGS_EXT) == XR_SUCCESS);
    assert(xrRequestDisplayRefreshRateFB(session, 60) == XR_SUCCESS && requestedRate == 90);
    PFN_xrVoidFunction fn = nullptr;
    assert(xrGetInstanceProcAddr(instance, "xrBeginSession", &fn) == XR_SUCCESS);
    assert(fn == reinterpret_cast<PFN_xrVoidFunction>(&xrBeginSession));
    assert(xrGetInstanceProcAddr(instance, "xrMissingEXT", &fn) == XR_ERROR_FUNCTION_UNSUPPORTED && fn == nullptr);
    assert(xrDestroySession(session) == XR_SUCCESS && sessions.empty());
    assert(xrDestroyInstance(instance) == XR_SUCCESS && instances.empty());
    rate90 = false; start(); rateCalls = 0;
    assert(xrBeginSession(session, nullptr) == XR_SUCCESS && rateCalls == 0);
    assert(xrRequestDisplayRefreshRateFB(session, 60) == XR_SUCCESS && requestedRate == 60);
    assert(xrDestroyInstance(instance) == XR_SUCCESS && sessions.empty());
    extensions = false; start(); cpuCalls = 0; rateCalls = 0;
    assert(xrBeginSession(session, nullptr) == XR_SUCCESS && cpuCalls == 0 && rateCalls == 0);
    assert(xrDestroyInstance(instance) == XR_SUCCESS);
    puts("PASS: extension negotiation, failed begin/hints, BOOST, 90/72/60 handling, GIPA, cleanup");
}
