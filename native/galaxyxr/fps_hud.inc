// SPDX-License-Identifier: GPL-3.0-or-later
// Included after policy.cpp's config()/proc() helpers. App-local CPU-painted
// Android Surface swapchains work independently of the game's GLES/Vulkan API.
#include "fps_metrics.h"
#include <chrono>
#include <memory>
#include <array>
#include <atomic>
#include <condition_variable>
#include <thread>
#ifdef LOG
#define GXR_FPS_LOG(...) LOG(__VA_ARGS__)
#else
#define GXR_FPS_LOG(...) do { std::printf(__VA_ARGS__); std::printf("\n"); } while (0)
#endif
#ifndef GXR_POLICY_TEST
#include <jni.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#endif

static constexpr const char* GXR_SURFACE_EXTENSION = "XR_KHR_android_surface_swapchain";
static bool gxrFpsHudRequested() { return config().find("fps_hud=on\n") != std::string::npos; }
static uint64_t gxrFpsNow() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

namespace GxrFpsHud {
constexpr int Width = 384, Height = 96;
#ifdef GXR_POLICY_TEST
using SurfaceObject = void*;
using Vm = void;
static bool testSurfaceAvailable = false;
static std::atomic<bool> testBlockPaint{false}, testPaintEntered{false}, testPaintExited{false};
static std::atomic<unsigned> testPostCount{0};
#else
using SurfaceObject = jobject;
using Vm = JavaVM;
#endif
using CreateSurface = XrResult (XRAPI_PTR *)(XrSession, const XrSwapchainCreateInfo*, XrSwapchain*, SurfaceObject*);
struct Instance {
    Vm* vm = nullptr;
    CreateSurface createSurface = nullptr;
    PFN_xrCreateReferenceSpace createSpace = nullptr;
    PFN_xrDestroySpace destroySpace = nullptr;
    PFN_xrDestroySwapchain destroySwapchain = nullptr;
    PFN_xrGetDisplayRefreshRateFB refresh = nullptr;
};
struct Text { std::string app, display; };
// The producer thread owns only this state and its retained NativeWindow. It
// never accesses Session, calls OpenXR, or uses a Java local reference.
struct Worker {
    std::mutex mutex;
    std::condition_variable changed;
    std::shared_ptr<const Text> text;
    std::atomic<bool> stop{false}, ready{false}, failed{false};
    bool done = false;
#ifndef GXR_POLICY_TEST
    ANativeWindow* window = nullptr;
    ~Worker() { if (window) ANativeWindow_release(window); }
#endif
};
struct Session {
    Instance instance;
    XrInstance owner = XR_NULL_HANDLE;
    XrSession session = XR_NULL_HANDLE;
    XrSpace view = XR_NULL_HANDLE;
    XrSwapchain swapchain = XR_NULL_HANDLE;
    uint32_t maxLayers = 0;
    XrSessionState state = XR_SESSION_STATE_UNKNOWN;
    bool failed = false;
    GxrFpsFrameLatch frames;
    GxrFpsMetrics metrics;
    uint64_t lastPublish = 0;
    float hz = 0;
    std::shared_ptr<Worker> worker;
    std::thread painter;
};
static std::mutex mutex;
static Vm* loaderVm = nullptr; // borrowed process-lifetime VM from loader init
static std::unordered_map<XrInstance, Instance> instances;
static std::unordered_map<XrSession, std::unique_ptr<Session>> sessions;

static bool visible(const Session& s) {
    return s.state == XR_SESSION_STATE_VISIBLE || s.state == XR_SESSION_STATE_FOCUSED;
}
static bool projection(const XrFrameEndInfo* info) {
    if (!info || !info->layerCount || info->layerCount > 64 || !info->layers) return false;
    for (uint32_t i = 0; i < info->layerCount; ++i)
        if (info->layers[i] && info->layers[i]->type == XR_TYPE_COMPOSITION_LAYER_PROJECTION) {
            auto p = reinterpret_cast<const XrCompositionLayerProjection*>(info->layers[i]);
            if (p->viewCount && p->views) return true;
        }
    return false;
}

#ifndef GXR_POLICY_TEST
class Env {
public:
    explicit Env(Vm* vm) : vm_(vm) {
        if (!vm_) return;
        jint result = vm_->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
        if (result == JNI_EDETACHED) attached_ = vm_->AttachCurrentThread(&env, nullptr) == JNI_OK;
        else if (result != JNI_OK) env = nullptr;
    }
    ~Env() { if (attached_) vm_->DetachCurrentThread(); }
    JNIEnv* env = nullptr;
private:
    Vm* vm_;
    bool attached_ = false;
};
class LocalFrame {
public:
    explicit LocalFrame(JNIEnv* env) : valid(env && env->PushLocalFrame(8) == 0), env_(env) {}
    ~LocalFrame() { if (valid) env_->PopLocalFrame(nullptr); }
    bool valid;
private:
    JNIEnv* env_;
};

// Compact 5x7 glyphs for the fixed diagnostic text. No font/Java/UI dependency.
static const uint8_t* glyph(char c) {
    static const uint8_t blank[7] = {};
    static const uint8_t digits[10][7] = {
        {14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},
        {30,1,1,14,1,1,30},{2,6,10,18,31,2,2},{31,16,16,30,1,1,30},
        {14,16,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},
        {14,17,17,15,1,1,14}};
    if (c >= '0' && c <= '9') return digits[c - '0'];
    static const uint8_t letters[][7] = {
        {14,17,17,31,17,17,17}, // A
        {30,17,17,30,16,16,16}, // P
        {31,16,16,30,16,16,16}, // F
        {15,16,16,14,1,1,30},   // S
        {17,17,17,31,17,17,17}, // H
        {31,1,2,4,8,16,31},    // Z
        {17,17,17,17,17,17,14}, // U
        {30,17,17,30,17,17,30}, // B
        {0,0,0,0,0,6,6},       // .
        {0,0,0,31,0,0,0},      // -
        {30,17,17,17,17,17,30}, // D
        {14,4,4,4,4,4,14},     // I
        {16,16,16,16,16,16,31}, // L
        {17,17,10,4,4,4,4}};   // Y
    const char* supported = "APFSHZUB.-DILY";
    const char* found = strchr(supported, c);
    return found ? letters[found - supported] : blank;
}
static bool paint(Worker& worker, const Text& text) {
    if (worker.stop.load()) return true;
    ANativeWindow_Buffer buffer{};
    // This potentially blocking call runs ONLY on the producer worker.
    if (!worker.window) return false;
    int locked = ANativeWindow_lock(worker.window, &buffer, nullptr);
    if (locked != 0) { GXR_FPS_LOG("FPS HUD disabled: CPU Surface lock failed=%d", locked); return false; }
    bool valid = buffer.bits && buffer.width >= Width && buffer.height >= Height &&
                 buffer.stride >= buffer.width && buffer.format == WINDOW_FORMAT_RGBA_8888;
    if (valid && !worker.stop.load()) {
        auto pixels = static_cast<uint32_t*>(buffer.bits);
        for (int y = 0; y < buffer.height; ++y)
            for (int x = 0; x < buffer.width; ++x)
                pixels[y * buffer.stride + x] = 0xc0000000; // premultiplied black
        const char* lines[] = {text.app.c_str(), text.display.c_str()};
        for (int line = 0; line < 2; ++line)
            for (int i = 0; lines[line][i] && i < 15; ++i) {
                auto bits = glyph(lines[line][i]);
                for (int row = 0; row < 7; ++row)
                    for (int col = 0; col < 5; ++col)
                        if (bits[row] & (1 << (4 - col)))
                            for (int yy = 0; yy < 4; ++yy)
                                for (int xx = 0; xx < 4; ++xx) {
                                    int y = 10 + line * 42 + row * 4 + yy;
                                    int x = 10 + i * 24 + col * 4 + xx;
                                    pixels[y * buffer.stride + x] = 0xffc0ffc0;
                                }
            }
    }
    // A stop racing an in-flight lock still completes unlock/post to return the
    // producer buffer. It performs no new paint or subsequent lock after stop.
    int posted = ANativeWindow_unlockAndPost(worker.window);
    if (!valid) GXR_FPS_LOG("FPS HUD disabled: unexpected CPU buffer format=%d size=%dx%d stride=%d", buffer.format, buffer.width, buffer.height, buffer.stride);
    if (posted != 0) GXR_FPS_LOG("FPS HUD disabled: CPU Surface post failed=%d", posted);
    return posted == 0 && valid;
}
#else
static bool paint(Worker& worker, const Text&) {
    if (worker.stop.load()) return true;
    testPaintEntered = true;
    while (testBlockPaint.load()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    ++testPostCount; // complete the already-started lock/post even if stopped
    testPaintExited = true;
    return testSurfaceAvailable;
}
#endif

static void paintLoop(const std::shared_ptr<Worker>& worker) {
    bool available = true;
#ifndef GXR_POLICY_TEST
    int geometry = ANativeWindow_setBuffersGeometry(worker->window, Width, Height, WINDOW_FORMAT_RGBA_8888);
    if (geometry != 0) {
        GXR_FPS_LOG("FPS HUD disabled: CPU Surface geometry failed=%d", geometry);
        available = false;
    }
#endif
    auto next = std::chrono::steady_clock::now();
    while (available && !worker->stop.load()) {
        std::shared_ptr<const Text> text;
        {
            std::unique_lock<std::mutex> lock(worker->mutex);
            worker->changed.wait_until(lock, next, [&] { return worker->stop.load(); });
            if (worker->stop.load()) break;
            text = worker->text;
        }
        if (text) {
            available = paint(*worker, *text);
            if (available && !worker->stop.load() && !worker->ready.exchange(true)) {
                GXR_FPS_LOG("FPS HUD ready: worker CPU post succeeded; APP FPS counts submissions");
                worker->changed.notify_all();
            }
        }
        next = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
    }
    if (!available) worker->failed = true;
    {
        std::lock_guard<std::mutex> lock(worker->mutex);
        worker->done = true;
    }
    worker->changed.notify_all();
}

static bool publish(Session& s) {
    if (!s.worker) return false;
    char app[32], display[32];
    if (s.metrics.valid()) snprintf(app, sizeof app, "APP FPS %.1f", s.metrics.fps());
    else snprintf(app, sizeof app, "APP FPS --.-");
    if (s.hz > 0) snprintf(display, sizeof display, "DISPLAY HZ %.0f", s.hz);
    else snprintf(display, sizeof display, "DISPLAY HZ --");
    try {
        auto text = std::make_shared<const Text>(Text{app, display});
        std::lock_guard<std::mutex> lock(s.worker->mutex);
        s.worker->text = std::move(text); // worker copies an immutable snapshot
        return true;
    } catch (...) {
        GXR_FPS_LOG("FPS HUD disabled: text snapshot allocation failed");
        return false;
    }
}

static void release(Session& s) {
    auto worker = s.worker;
    if (worker) { worker->stop = true; worker->changed.notify_all(); }
    // KHR requires all writes drained before EndSession. Never detach: a late
    // post would violate the Surface lifecycle. A stalled vendor buffer call
    // can delay teardown; recovery is terminating this app, not a system patch.
    if (s.painter.joinable()) {
        s.painter.join();
    }
    if (s.swapchain && s.instance.destroySwapchain) s.instance.destroySwapchain(s.swapchain);
    if (s.view && s.instance.destroySpace) s.instance.destroySpace(s.view);
    s.swapchain = XR_NULL_HANDLE;
    s.view = XR_NULL_HANDLE;
    s.worker.reset(); s.lastPublish = 0;
}

static bool initialize(Session& s) {
    if (s.failed) return false;
    if (s.swapchain && s.view) return true;
    auto& f = s.instance;
    if (!f.vm || !f.createSurface || !f.createSpace || !f.destroySpace || !f.destroySwapchain) {
        GXR_FPS_LOG("FPS HUD unavailable: VM=%d Surface=%d createSpace=%d destroySpace=%d destroySwapchain=%d",
                    f.vm != nullptr, f.createSurface != nullptr, f.createSpace != nullptr,
                    f.destroySpace != nullptr, f.destroySwapchain != nullptr);
        s.failed = true;
        return false;
    }
    XrReferenceSpaceCreateInfo space{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    space.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
    space.poseInReferenceSpace.orientation.w = 1;
    XrResult spaceResult = f.createSpace(s.session, &space, &s.view);
    if (XR_FAILED(spaceResult)) {
        GXR_FPS_LOG("FPS HUD disabled: view-space creation failed=%d", spaceResult);
        s.failed = true; return false;
    }
    XrSwapchainCreateInfo info{XR_TYPE_SWAPCHAIN_CREATE_INFO};
    info.width = Width; info.height = Height;
    // KHR Android Surface rules require format/sampleCount/faceCount/arraySize/
    // mipCount all zero. This swapchain must NEVER be acquired/enumerated.
    SurfaceObject surface = nullptr;
#ifndef GXR_POLICY_TEST
    Env env(f.vm);
    if (!env.env) {
        GXR_FPS_LOG("FPS HUD disabled: could not attach current thread to app VM");
        s.failed = true; release(s); return false;
    }
    LocalFrame local(env.env);
    if (!local.valid) {
        if (env.env->ExceptionCheck()) env.env->ExceptionClear();
        GXR_FPS_LOG("FPS HUD disabled: JNI local-frame allocation failed");
        s.failed = true; release(s); return false;
    }
#endif
    XrResult result = f.createSurface(s.session, &info, &s.swapchain, &surface);
    bool available = XR_SUCCEEDED(result) && s.swapchain && surface;
#ifndef GXR_POLICY_TEST
    ANativeWindow* window = available ? ANativeWindow_fromSurface(env.env, surface) : nullptr;
    // LocalFrame releases any JNI local references created by this call; the
    // ANativeWindow acquire is balanced separately by release(). Never delete
    // a runtime-owned global reference or release its Java Surface directly.
    if (env.env->ExceptionCheck()) {
        env.env->ExceptionClear(); available = false;
        GXR_FPS_LOG("FPS HUD disabled: Android Surface conversion raised JNI exception");
    }
    available = available && window;
    if (!available) {
        GXR_FPS_LOG("FPS HUD disabled: Android Surface creation result=%d object=%d window=%d", result, surface != nullptr, window != nullptr);
        if (window) ANativeWindow_release(window);
    }
#else
    available = available && testSurfaceAvailable;
#endif
    if (!available) { s.failed = true; release(s); return false; }
    try {
        s.worker = std::make_shared<Worker>();
#ifndef GXR_POLICY_TEST
        s.worker->window = window; // transfer the acquired producer reference
#endif
        if (!publish(s)) { s.failed = true; release(s); return false; }
        s.painter = std::thread(paintLoop, s.worker);
    } catch (...) {
#ifndef GXR_POLICY_TEST
        if (!s.worker && window) ANativeWindow_release(window);
#endif
        GXR_FPS_LOG("FPS HUD disabled: producer worker allocation/start failed");
        s.failed = true; release(s); return false;
    }
    GXR_FPS_LOG("FPS HUD Surface created: %dx%d; CPU producer still needs first successful post", Width, Height);
    return true;
}
} // namespace GxrFpsHud

static void gxrFpsRememberLoaderInit(const XrLoaderInitInfoBaseHeaderKHR* info) {
    struct AndroidLoaderInfo { XrStructureType type; const void* next; GxrFpsHud::Vm* vm; void* context; };
    for (auto node = reinterpret_cast<const XrBaseInStructure*>(info); node; node = node->next)
        if (node->type == XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR) {
            auto vm = reinterpret_cast<const AndroidLoaderInfo*>(node)->vm;
            if (vm) {
                std::lock_guard<std::mutex> lock(GxrFpsHud::mutex);
                GxrFpsHud::loaderVm = vm;
            }
        }
}
static void gxrFpsRememberInstance(XrInstance instance, const XrInstanceCreateInfo* info, bool surfaceExtensionEnabled) {
    if (!gxrFpsHudRequested()) return;
    if (!surfaceExtensionEnabled) {
        GXR_FPS_LOG("FPS HUD unavailable: real runtime Android Surface swapchain extension not enabled");
        return;
    }
    GxrFpsHud::Instance state;
    struct AndroidInfo { XrStructureType type; const void* next; GxrFpsHud::Vm* vm; void* activity; };
    for (auto node = info ? static_cast<const XrBaseInStructure*>(info->next) : nullptr; node; node = node->next)
        if (node->type == XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR)
            state.vm = reinterpret_cast<const AndroidInfo*>(node)->vm;
    if (!state.vm) {
        std::lock_guard<std::mutex> lock(GxrFpsHud::mutex);
        state.vm = GxrFpsHud::loaderVm;
    }
    state.createSurface = proc<GxrFpsHud::CreateSurface>(instance, "xrCreateSwapchainAndroidSurfaceKHR");
    state.createSpace = proc<PFN_xrCreateReferenceSpace>(instance, "xrCreateReferenceSpace");
    state.destroySpace = proc<PFN_xrDestroySpace>(instance, "xrDestroySpace");
    state.destroySwapchain = proc<PFN_xrDestroySwapchain>(instance, "xrDestroySwapchain");
    state.refresh = proc<PFN_xrGetDisplayRefreshRateFB>(instance, "xrGetDisplayRefreshRateFB");
    std::lock_guard<std::mutex> lock(GxrFpsHud::mutex);
    GxrFpsHud::instances[instance] = state;
}
static void gxrFpsRememberSession(XrInstance instance, XrSession session, uint32_t maxLayerCount) {
    std::lock_guard<std::mutex> lock(GxrFpsHud::mutex);
    auto found = GxrFpsHud::instances.find(instance);
    if (found == GxrFpsHud::instances.end()) return;
    auto state = std::make_unique<GxrFpsHud::Session>();
    state->instance = found->second; state->owner = instance;
    state->session = session; state->maxLayers = maxLayerCount;
    if (!maxLayerCount) GXR_FPS_LOG("FPS HUD unavailable: runtime maximum layer count not established");
    GxrFpsHud::sessions[session] = std::move(state);
}
static void gxrFpsOnSessionState(XrSession session, XrSessionState state) {
    std::lock_guard<std::mutex> lock(GxrFpsHud::mutex);
    auto found = GxrFpsHud::sessions.find(session);
    if (found == GxrFpsHud::sessions.end()) return;
    auto& s = *found->second;
    if (s.state != state) { s.metrics.reset(); s.lastPublish = 0; }
    s.state = state;
    if (!GxrFpsHud::visible(s)) {
        s.frames.reset();
        GxrFpsHud::release(s);
    }
}
static void gxrFpsStopSession(XrSession session) {
    // Call BEFORE the underlying xrEndSession. Completion guarantees no
    // background writer can post after that runtime call or a nonvisible event.
    gxrFpsOnSessionState(session, XR_SESSION_STATE_IDLE);
}
static void gxrFpsOnWaitFrame(XrSession session, const XrFrameState* frame) {
    std::lock_guard<std::mutex> lock(GxrFpsHud::mutex);
    auto found = GxrFpsHud::sessions.find(session);
    if (found != GxrFpsHud::sessions.end() && frame) found->second->frames.wait(frame->shouldRender);
}
static void gxrFpsOnBeginFrame(XrSession session) {
    std::lock_guard<std::mutex> lock(GxrFpsHud::mutex);
    auto found = GxrFpsHud::sessions.find(session);
    if (found != GxrFpsHud::sessions.end()) found->second->frames.begin();
}
static XrResult gxrFpsEndFrame(XrSession session, const XrFrameEndInfo* info, PFN_xrEndFrame backend) {
    if (!backend) return XR_ERROR_FUNCTION_UNSUPPORTED;
    std::unique_lock<std::mutex> lock(GxrFpsHud::mutex);
    auto found = GxrFpsHud::sessions.find(session);
    if (found == GxrFpsHud::sessions.end()) { lock.unlock(); return backend(session, info); }
    auto& s = *found->second;
    bool shouldRender = s.frames.finish();
    bool eligible = GxrFpsHud::visible(s) && shouldRender && GxrFpsHud::projection(info);
    XrFrameEndInfo copy{};
    XrCompositionLayerQuad quad{XR_TYPE_COMPOSITION_LAYER_QUAD};
    std::array<const XrCompositionLayerBaseHeader*, 65> layers{};
    bool appended = false;
    auto now = gxrFpsNow();
    if (eligible && info->layerCount < s.maxLayers && info->layerCount < layers.size() && GxrFpsHud::initialize(s)) {
        if (s.worker->failed.load()) { s.failed = true; GxrFpsHud::release(s); }
        if (!s.failed && (!s.lastPublish || now - s.lastPublish >= 500000000ULL)) {
            if (s.instance.refresh) {
                float hz = 0;
                if (XR_SUCCEEDED(s.instance.refresh(session, &hz))) s.hz = hz;
            }
            if (!GxrFpsHud::publish(s)) { s.failed = true; GxrFpsHud::release(s); }
            else s.lastPublish = now;
        }
        if (!s.failed && s.worker->ready.load() && !s.worker->stop.load()) {
            std::copy(info->layers, info->layers + info->layerCount, layers.begin());
            quad.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
            quad.space = s.view; quad.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
            quad.subImage.swapchain = s.swapchain;
            quad.subImage.imageRect.extent = {GxrFpsHud::Width, GxrFpsHud::Height};
            quad.pose.orientation.w = 1;
            quad.pose.position = {-0.27f, 0.26f, -1.0f};
            quad.size = {0.34f, 0.085f};
            layers[info->layerCount] = reinterpret_cast<const XrCompositionLayerBaseHeader*>(&quad);
            copy = *info; copy.layerCount = info->layerCount + 1; copy.layers = layers.data();
            appended = true;
        }
    }
    XrResult result = backend(session, appended ? &copy : info);
    // Count only accepted original app projection submissions, never the HUD,
    // xrWaitFrame calls, zero-layer frames, or errors. No end-frame retry.
    if (s.metrics.sample(gxrFpsNow(), result == XR_SUCCESS && eligible)) {
        if (s.instance.refresh) {
            float hz = 0;
            if (XR_SUCCEEDED(s.instance.refresh(session, &hz))) s.hz = hz;
        }
        GXR_FPS_LOG("FPS(sub)=%.2f Hz=%.2f HUD=%d (accepted app projection submissions; not GPU completion)",
                    s.metrics.fps(), s.hz, appended);
    }
    if (XR_FAILED(result) && appended) {
        GXR_FPS_LOG("FPS HUD disabled after end-frame error=%d; no retry; following frames preserve app layers", result);
        s.failed = true; GxrFpsHud::release(s);
    }
    return result;
}
static void gxrFpsForgetSession(XrSession session) {
    std::lock_guard<std::mutex> lock(GxrFpsHud::mutex);
    auto found = GxrFpsHud::sessions.find(session);
    if (found == GxrFpsHud::sessions.end()) return;
    GxrFpsHud::release(*found->second);
    GxrFpsHud::sessions.erase(found);
}
static void gxrFpsForgetInstance(XrInstance instance) {
    std::lock_guard<std::mutex> lock(GxrFpsHud::mutex);
    for (auto it = GxrFpsHud::sessions.begin(); it != GxrFpsHud::sessions.end();) {
        if (it->second->owner == instance) {
            GxrFpsHud::release(*it->second);
            it = GxrFpsHud::sessions.erase(it);
        } else ++it;
    }
    GxrFpsHud::instances.erase(instance);
}
#undef GXR_FPS_LOG
