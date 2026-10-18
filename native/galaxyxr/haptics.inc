// Galaxy XR may acknowledge amplitude=0 / Stop without cancelling the motor.
// Bound each hardware command to 20 ms and renew long effects until their
// requested deadline. Cancelling drops renewals, rather than relying on Stop.
#include <chrono>
#include <condition_variable>
#include <cmath>
#include <thread>
#include <system_error>

class HapticScheduler {
    using Clock = std::chrono::steady_clock;
    static constexpr XrDuration slice = 20000000;
    static constexpr XrDuration interval = 10000000;
    struct Action { XrActionSet set; std::vector<XrPath> paths; };
    struct Job {
        XrSession session;
        XrHapticActionInfo info;
        XrHapticVibration vibration;
        Clock::time_point end, next;
        PFN_xrApplyHapticFeedback apply;
    };
    std::mutex mutex;
    std::condition_variable wake;
    std::vector<Job> jobs;
    std::unordered_map<XrAction, Action> actions;
    std::thread worker;
    bool quitting = false;

    template<class Predicate> void erase(Predicate predicate) {
        jobs.erase(std::remove_if(jobs.begin(), jobs.end(), predicate), jobs.end());
        wake.notify_all();
    }
    void cancel(XrSession session, const XrHapticActionInfo& info) {
        erase([&](const Job& j) {
            return j.session == session && j.info.action == info.action &&
                (info.subactionPath == XR_NULL_PATH || j.info.subactionPath == info.subactionPath);
        });
    }
    void run() {
        std::unique_lock<std::mutex> lock(mutex);
        while (!quitting) {
            if (jobs.empty()) { wake.wait(lock); continue; }
            auto now = Clock::now();
            auto next = Clock::time_point::max();
            for (auto it = jobs.begin(); it != jobs.end();) {
                auto& j = *it;
                if (now >= j.end) { it = jobs.erase(it); continue; }
                if (now >= j.next) {
                    j.vibration.duration = std::min<XrDuration>(slice,
                        std::chrono::duration_cast<std::chrono::nanoseconds>(j.end - now).count());
                    // Serialize sends with Stop/destruction, so nothing can be
                    // submitted after those functions have cancelled a job.
                    auto result = j.apply(j.session, &j.info,
                        reinterpret_cast<const XrHapticBaseHeader*>(&j.vibration));
                    if (result != XR_SUCCESS) { it = jobs.erase(it); continue; }
                    // Never replay missed ticks after a scheduling stall.
                    j.next = Clock::now() + std::chrono::nanoseconds(interval);
                }
                next = std::min(next, std::min(j.next, j.end));
                ++it;
            }
            if (!jobs.empty()) wake.wait_until(lock, next);
        }
    }
public:
    ~HapticScheduler() {
        { std::lock_guard<std::mutex> lock(mutex); quitting = true; jobs.clear(); wake.notify_all(); }
        if (worker.joinable()) worker.join();
    }
    void addAction(XrActionSet set, const XrActionCreateInfo& info, XrAction action) {
        if (info.actionType != XR_ACTION_TYPE_VIBRATION_OUTPUT) return;
        std::lock_guard<std::mutex> lock(mutex);
        auto& a = actions[action]; a.set = set; a.paths.clear();
        if (info.countSubactionPaths) a.paths.assign(info.subactionPaths, info.subactionPaths + info.countSubactionPaths);
    }
    void forgetAction(XrAction action) {
        std::lock_guard<std::mutex> lock(mutex);
        erase([&](const Job& j) { return j.info.action == action; });
        actions.erase(action);
    }
    void forgetSet(XrActionSet set) {
        std::lock_guard<std::mutex> lock(mutex);
        for (auto it = actions.begin(); it != actions.end();) {
            if (it->second.set != set) { ++it; continue; }
            auto action = it->first;
            erase([&](const Job& j) { return j.info.action == action; });
            it = actions.erase(it);
        }
    }
    void forgetSession(XrSession session) {
        std::lock_guard<std::mutex> lock(mutex);
        erase([&](const Job& j) { return j.session == session; });
    }
    XrResult stop(XrSession session, const XrHapticActionInfo* info, PFN_xrStopHapticFeedback fn) {
        std::lock_guard<std::mutex> lock(mutex);
        if (info) cancel(session, *info);
        return fn ? fn(session, info) : XR_ERROR_FUNCTION_UNSUPPORTED;
    }
    XrResult apply(XrSession session, const XrHapticActionInfo* info,
                   const XrHapticBaseHeader* h, PFN_xrApplyHapticFeedback fn, PFN_xrStopHapticFeedback stopFn) {
        if (!fn) return XR_ERROR_FUNCTION_UNSUPPORTED;
        std::lock_guard<std::mutex> lock(mutex);
        if (!info || !h) return fn(session, info, h);
        // Do not retain caller-owned extension chains or reinterpret other haptics types.
        if (h->type != XR_TYPE_HAPTIC_VIBRATION || h->next || info->next) {
            auto result = fn(session, info, h);
            if (result == XR_SUCCESS) cancel(session, *info);
            return result;
        }
        auto v = *reinterpret_cast<const XrHapticVibration*>(h);
        if (v.amplitude == 0.0f) {
            cancel(session, *info);
            return stopFn ? stopFn(session, info) : fn(session, info, h);
        }
        if (!(v.amplitude > 0.0f && v.amplitude <= 1.0f) || !std::isfinite(v.frequency) ||
            v.frequency < 0 || v.duration <= slice) {
            auto result = fn(session, info, h);
            if (result == XR_SUCCESS) cancel(session, *info);
            return result;
        }
        if (!worker.joinable()) {
            try { worker = std::thread([this] { run(); }); }
            catch (const std::system_error&) { return XR_ERROR_RUNTIME_FAILURE; }
        }
        const auto now = Clock::now();
        // Saturate the deadline (XR_INFINITE_DURATION must not overflow a time_point).
        const auto room = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::time_point::max() - now).count();
        const auto end = now + std::chrono::nanoseconds(std::min<XrDuration>(v.duration, room));
        v.duration = slice;
        auto result = fn(session, info, reinterpret_cast<const XrHapticBaseHeader*>(&v));
        if (result != XR_SUCCESS) return result;
        cancel(session, *info);
        auto add = [&](XrPath path) {
            auto copy = *info; copy.subactionPath = path;
            jobs.push_back({session, copy, v, end, now + std::chrono::nanoseconds(interval), fn});
        };
        auto a = actions.find(info->action);
        // Split a both-hands command so stopping one hand preserves the other.
        if (info->subactionPath == XR_NULL_PATH && a != actions.end() && !a->second.paths.empty())
            for (auto path : a->second.paths) add(path);
        else add(info->subactionPath);
        wake.notify_all();
        return result;
    }
};
static HapticScheduler haptics;

extern "C" XRAPI_ATTR XrResult XRAPI_CALL xrApplyHapticFeedback(XrSession session,
        const XrHapticActionInfo* info, const XrHapticBaseHeader* h) {
    return haptics.apply(session, info, h, symbol<PFN_xrApplyHapticFeedback>("xrApplyHapticFeedback"),
                         symbol<PFN_xrStopHapticFeedback>("xrStopHapticFeedback"));
}
extern "C" XRAPI_ATTR XrResult XRAPI_CALL xrStopHapticFeedback(XrSession session, const XrHapticActionInfo* info) {
    return haptics.stop(session, info, symbol<PFN_xrStopHapticFeedback>("xrStopHapticFeedback"));
}
extern "C" XRAPI_ATTR XrResult XRAPI_CALL xrEndSession(XrSession session) {
    haptics.forgetSession(session);
    auto fn = symbol<PFN_xrEndSession>("xrEndSession");
    // Android Surface writers must finish before the runtime ends the session.
    gxrFpsStopSession(session);
    auto result = fn ? fn(session) : XR_ERROR_FUNCTION_UNSUPPORTED;
    return result;
}
#ifdef GXR_INPUT_DIAGNOSTICS
static void logInputAction(const XrActionCreateInfo*, XrAction);
#endif
extern "C" XRAPI_ATTR XrResult XRAPI_CALL xrCreateAction(XrActionSet set, const XrActionCreateInfo* info, XrAction* action) {
    auto fn = symbol<PFN_xrCreateAction>("xrCreateAction");
    auto result = fn ? fn(set, info, action) : XR_ERROR_FUNCTION_UNSUPPORTED;
    if (XR_SUCCEEDED(result) && info && action) {
        haptics.addAction(set, *info, *action);
#ifdef GXR_INPUT_DIAGNOSTICS
        logInputAction(info, *action);
#endif
    }
    return result;
}
extern "C" XRAPI_ATTR XrResult XRAPI_CALL xrDestroyAction(XrAction action) {
    haptics.forgetAction(action);
    auto fn = symbol<PFN_xrDestroyAction>("xrDestroyAction");
    return fn ? fn(action) : XR_ERROR_FUNCTION_UNSUPPORTED;
}
extern "C" XRAPI_ATTR XrResult XRAPI_CALL xrDestroyActionSet(XrActionSet set) {
    haptics.forgetSet(set);
    auto fn = symbol<PFN_xrDestroyActionSet>("xrDestroyActionSet");
    return fn ? fn(set) : XR_ERROR_FUNCTION_UNSUPPORTED;
}
