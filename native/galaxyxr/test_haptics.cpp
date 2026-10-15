// Regression: Arizona Sunshine requests 2 seconds, then amplitude=0 after ~33 ms.
// The mock deliberately ignores Stop, as the affected device chain did.
#define GXR_POLICY_TEST
#include "policy.cpp"
#include <cassert>
#include <condition_variable>
#include <limits>

static std::mutex logMutex;
static std::condition_variable changed;
struct Call { XrSession session; XrPath path; XrDuration duration; float amplitude; };
static std::vector<Call> calls;
static XrResult nextResult = XR_SUCCESS;
static XrResult mockApply(XrSession session, const XrHapticActionInfo* info, const XrHapticBaseHeader* h) {
    std::lock_guard<std::mutex> lock(logMutex);
    if (!info || !h) return XR_ERROR_VALIDATION_FAILURE;
    auto v = reinterpret_cast<const XrHapticVibration*>(h);
    calls.push_back({session, info->subactionPath, v->duration, v->amplitude});
    changed.notify_all();
    return nextResult;
}
static XrResult mockStop(XrSession, const XrHapticActionInfo*) { return XR_SUCCESS; }
static XrResult mockEnd(XrSession) { return XR_SUCCESS; }
static XrResult mockDestroyAction(XrAction) { return XR_SUCCESS; }
static XrResult mockDestroySet(XrActionSet) { return XR_SUCCESS; }
static XrResult mockGet(XrInstance, const char* name, PFN_xrVoidFunction* fn) {
    *fn = reinterpret_cast<PFN_xrVoidFunction>(testSymbol(name));
    return *fn ? XR_SUCCESS : XR_ERROR_FUNCTION_UNSUPPORTED;
}
static void* testSymbol(const char* name) {
#define MOCK(n, f) if (!strcmp(name, #n)) return reinterpret_cast<void*>(&f)
    MOCK(xrApplyHapticFeedback, mockApply); MOCK(xrStopHapticFeedback, mockStop);
    MOCK(xrEndSession, mockEnd); MOCK(xrDestroySession, mockEnd);
    MOCK(xrDestroyAction, mockDestroyAction); MOCK(xrDestroyActionSet, mockDestroySet);
    MOCK(xrGetInstanceProcAddr, mockGet);
#undef MOCK
    return nullptr;
}
static size_t count() { std::lock_guard<std::mutex> lock(logMutex); return calls.size(); }
static void waitCalls(size_t n) {
    std::unique_lock<std::mutex> lock(logMutex);
    assert(changed.wait_for(lock, std::chrono::seconds(2), [&] { return calls.size() >= n; }));
}
static void quiet() {
    auto n = count();
    std::this_thread::sleep_for(std::chrono::milliseconds(45));
    assert(count() == n);
}
int main() {
    auto session = reinterpret_cast<XrSession>(1);
    auto action = reinterpret_cast<XrAction>(2);
    auto set = reinterpret_cast<XrActionSet>(3);
    XrHapticActionInfo info{XR_TYPE_HAPTIC_ACTION_INFO, nullptr, action, 11};
    XrHapticVibration v{XR_TYPE_HAPTIC_VIBRATION, nullptr, 2000000000, 0, 0.12f};
    auto apply = [&] { return xrApplyHapticFeedback(session, &info, reinterpret_cast<const XrHapticBaseHeader*>(&v)); };
    PFN_xrVoidFunction fn = nullptr;
    assert(xrGetInstanceProcAddr(nullptr, "xrApplyHapticFeedback", &fn) == XR_SUCCESS);
    assert(fn == reinterpret_cast<PFN_xrVoidFunction>(&xrApplyHapticFeedback));
    assert(xrGetInstanceProcAddr(nullptr, "xrStopHapticFeedback", &fn) == XR_SUCCESS);
    assert(fn == reinterpret_cast<PFN_xrVoidFunction>(&xrStopHapticFeedback));
    assert(apply() == XR_SUCCESS);
    waitCalls(3); // Sustained haptics must continue beyond the first slice.
    v.amplitude = 0;
    assert(apply() == XR_SUCCESS);
    quiet(); // No new command after zero, even when the backend Stop is ineffective.
    for (auto& c : calls) assert(c.duration > 0 && c.duration <= 20000000);

    v.amplitude = 0.3f; v.duration = 60000000;
    auto n = count(); assert(apply() == XR_SUCCESS); waitCalls(n + 3);
    std::this_thread::sleep_for(std::chrono::milliseconds(80)); quiet(); // deadline

    v.duration = 2000000000; assert(apply() == XR_SUCCESS);
    assert(xrStopHapticFeedback(session, &info) == XR_SUCCESS); quiet();
    assert(apply() == XR_SUCCESS);
    assert(xrEndSession(session) == XR_SUCCESS); quiet();
    assert(apply() == XR_SUCCESS);
    assert(xrDestroySession(session) == XR_SUCCESS); quiet();
    assert(apply() == XR_SUCCESS);
    assert(xrDestroyAction(action) == XR_SUCCESS); quiet();

    XrPath paths[] = {11, 12};
    XrActionCreateInfo create{XR_TYPE_ACTION_CREATE_INFO};
    create.actionType = XR_ACTION_TYPE_VIBRATION_OUTPUT;
    create.countSubactionPaths = 2; create.subactionPaths = paths;
    haptics.addAction(set, create, action);
    info.subactionPath = XR_NULL_PATH;
    assert(apply() == XR_SUCCESS);
    info.subactionPath = 11;
    assert(xrStopHapticFeedback(session, &info) == XR_SUCCESS);
    n = count(); waitCalls(n + 2);
    assert(xrDestroyActionSet(set) == XR_SUCCESS); quiet();
    for (size_t i = n; i < calls.size(); ++i) assert(calls[i].path == 12);

    // Short commands retain their timing and replace an outstanding long one.
    assert(apply() == XR_SUCCESS); v.duration = 5000000;
    assert(apply() == XR_SUCCESS); quiet(); assert(calls.back().duration == 5000000);
    v.duration = XR_MIN_HAPTIC_DURATION; assert(apply() == XR_SUCCESS); quiet();
    assert(calls.back().duration == XR_MIN_HAPTIC_DURATION);
    v.duration = XR_INFINITE_DURATION; assert(apply() == XR_SUCCESS);
    assert(xrStopHapticFeedback(session, &info) == XR_SUCCESS); quiet();
    // Non-focused success codes must not keep a background effect alive.
    nextResult = XR_SESSION_NOT_FOCUSED; v.duration = 2000000000;
    assert(apply() == XR_SESSION_NOT_FOCUSED); quiet();
    nextResult = XR_SUCCESS;
    assert(apply() == XR_SUCCESS);
    { std::lock_guard<std::mutex> lock(logMutex); nextResult = XR_SESSION_NOT_FOCUSED; }
    n = count(); waitCalls(n + 1); quiet();
    puts("PASS: finite slices, zero/Stop, sustained effects, deadlines, focus loss, lifecycle, independent hands, GIPA");
}
