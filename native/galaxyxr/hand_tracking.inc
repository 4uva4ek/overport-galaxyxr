// Galaxy XR hand skeleton compatibility. On SM-I610 (2026-10-14) the system runtime returns an incomplete
// XR_FB_hand_tracking_mesh description: jointParents are garbage, jointRadii are zero, and the bind poses of
// every finger tip and finger metacarpal are missing (duplicates of other joints or identity), while the
// XR_EXT_hand_tracking joints follow the specification. OVRPlugin builds the Quest hand skeleton from this
// description, which twists the hands. The adapter rebuilds parents, radii and the missing bind joints from the
// valid bind joints and the latest live joints. Mesh vertices, weights and live joint data are never modified.
// Disabled by handmesh=runtime in the adapter configuration.
#include <cmath>
#ifdef GXR_HAND_DIAGNOSTICS
#include <chrono>
#include <memory>
#endif

struct HandSample { bool valid = false; int side = -1; bool skeletonLogged = false; XrHandJointLocationEXT joints[XR_HAND_JOINT_COUNT_EXT]{}; };
static PFN_xrCreateHandTrackerEXT origCreateHandTracker = nullptr;
static PFN_xrDestroyHandTrackerEXT origDestroyHandTracker = nullptr;
static PFN_xrLocateHandJointsEXT origLocateHandJoints = nullptr;
static PFN_xrGetHandMeshFB origGetHandMesh = nullptr;
static std::mutex handMutex;
static std::unordered_map<XrHandTrackerEXT, HandSample> handSamples;
#define HAND_HOOK(fn, var) if (!strcmp(name, #fn)) { var = reinterpret_cast<PFN_##fn>(*function); *function = reinterpret_cast<PFN_xrVoidFunction>(&fn); }

static bool handMeshFix() { static bool enabled = config().find("handmesh=runtime\n") == std::string::npos; return enabled; }

namespace handmath {
static XrVector3f sub(const XrVector3f& a, const XrVector3f& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
static XrVector3f add(const XrVector3f& a, const XrVector3f& b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
static XrVector3f scale(const XrVector3f& a, float s) { return {a.x * s, a.y * s, a.z * s}; }
static float dot(const XrVector3f& a, const XrVector3f& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static XrVector3f cross(const XrVector3f& a, const XrVector3f& b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
static float length(const XrVector3f& a) { return std::sqrt(dot(a, a)); }
static XrVector3f normalize(const XrVector3f& a) { float l = length(a); return l > 1e-6f ? scale(a, 1.0f / l) : XrVector3f{0, 0, 1}; }
static XrVector3f lerp(const XrVector3f& a, const XrVector3f& b, float t) { return add(a, scale(sub(b, a), t)); }
static XrQuaternionf conj(const XrQuaternionf& q) { return {-q.x, -q.y, -q.z, q.w}; }
static XrQuaternionf mul(const XrQuaternionf& a, const XrQuaternionf& b) {
    return {a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y, a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
            a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w, a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
}
static XrVector3f rotate(const XrQuaternionf& q, const XrVector3f& v) {
    XrVector3f u{q.x, q.y, q.z};
    XrVector3f t = scale(cross(u, v), 2.0f);
    return add(add(v, scale(t, q.w)), cross(u, t));
}
// Quaternion whose rotated basis vectors are the given orthonormal columns x, y, z.
static XrQuaternionf fromAxes(const XrVector3f& x, const XrVector3f& y, const XrVector3f& z) {
    float m00 = x.x, m01 = y.x, m02 = z.x, m10 = x.y, m11 = y.y, m12 = z.y, m20 = x.z, m21 = y.z, m22 = z.z;
    float trace = m00 + m11 + m22;
    XrQuaternionf q;
    if (trace > 0) { float s = std::sqrt(trace + 1.0f) * 2; q.w = 0.25f * s; q.x = (m21 - m12) / s; q.y = (m02 - m20) / s; q.z = (m10 - m01) / s; }
    else if (m00 > m11 && m00 > m22) { float s = std::sqrt(1.0f + m00 - m11 - m22) * 2; q.w = (m21 - m12) / s; q.x = 0.25f * s; q.y = (m01 + m10) / s; q.z = (m02 + m20) / s; }
    else if (m11 > m22) { float s = std::sqrt(1.0f + m11 - m00 - m22) * 2; q.w = (m02 - m20) / s; q.x = (m01 + m10) / s; q.y = 0.25f * s; q.z = (m12 + m21) / s; }
    else { float s = std::sqrt(1.0f + m22 - m00 - m11) * 2; q.w = (m10 - m01) / s; q.x = (m02 + m20) / s; q.y = (m12 + m21) / s; q.z = 0.25f * s; }
    return q;
}
// OpenXR hand joint frame: +Z points from the child back along the bone, +Y toward the given back-of-hand direction.
static XrQuaternionf boneFrame(const XrVector3f& joint, const XrVector3f& child, const XrVector3f& up) {
    XrVector3f z = normalize(sub(joint, child));
    XrVector3f y = normalize(sub(up, scale(z, dot(up, z))));
    return fromAxes(cross(y, z), y, z);
}
static float clamp(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
}

// Standard XR_EXT_hand_tracking hierarchy. The Overport dispatcher clamps parent values above 25 to the palm
// before OVRPlugin sees them, so the wrist lists the palm as its parent explicitly and the palm bind pose is
// kept identical to the wrist; OVRPlugin then resolves the wrist bone against an identity parent.
static const int32_t kHandJointParents[XR_HAND_JOINT_COUNT_EXT] = {
    1, 0, 1, 2, 3, 4, 1, 6, 7, 8, 9, 1, 11, 12, 13, 14, 1, 16, 17, 18, 19, 1, 21, 22, 23, 24};
static const float kHandJointRadii[XR_HAND_JOINT_COUNT_EXT] = {
    0.021f, 0.021f, 0.017f, 0.012f, 0.010f, 0.009f, 0.018f, 0.010f, 0.009f, 0.0085f, 0.008f, 0.018f, 0.010f, 0.009f, 0.0085f, 0.008f,
    0.016f, 0.010f, 0.009f, 0.0085f, 0.008f, 0.015f, 0.009f, 0.0085f, 0.008f, 0.0075f};

// OVRPlugin 1.88 converts FB bind poses with a fixed per-hand quaternion (OVR bone = joint x conj(C), C_left =
// (0.7071, 0, -0.7071, 0), C_right = (0, 0.7071, 0, 0.7071)) and composes each bone with the inverse of the
// converted wrist and parent poses; that SIMD code only yields clean unit results when the wrist bind orientation
// equals C, as in the Meta runtime. The rebuilt bind pose is therefore expressed in that root frame.
static const XrQuaternionf kOvrHandBasis[2] = {{0.70710677f, 0.0f, -0.70710677f, 0.0f}, {0.0f, 0.70710677f, 0.0f, 0.70710677f}};

// Rebuild parents, radii and a consistent bind pose in place. Only the runtime positions of the wrist,
// the thumb metacarpal and the proximal/intermediate/distal joints are trusted; every orientation is derived
// from those positions with the OpenXR joint convention (+Z from the child back along the bone, +Y toward the
// back of the hand), fingers are assumed straight at the distal joint, tips and finger metacarpals are placed
// from the live proportions when a sample exists.
static void rebuildHandSkeleton(XrHandTrackingMeshFB& mesh, const HandSample* live, int side) {
    using namespace handmath;
    XrPosef* bind = mesh.jointBindPoses;
    const XrVector3f wrist = bind[XR_HAND_JOINT_WRIST_EXT].position;
    const XrQuaternionf wristQ = bind[XR_HAND_JOINT_WRIST_EXT].orientation;
    const XrVector3f wristUp = rotate(wristQ, {0, 1, 0});
    // Live geometry expressed in the bind wrist frame: metacarpals and the palm are rigid relative to the wrist,
    // so their live offsets (scaled to the bind hand size) place them better than a point on the wrist-finger line.
    const XrVector3f liveWrist = live ? live->joints[1].pose.position : XrVector3f{};
    const XrQuaternionf liveWristQ = live ? live->joints[1].pose.orientation : XrQuaternionf{0, 0, 0, 1};
    auto liveToBind = [&](const XrVector3f& liveVector, float handScale) { return rotate(wristQ, scale(rotate(conj(liveWristQ), liveVector), handScale)); };
    auto handScale = [&](int joint) {
        float liveLength = live ? length(sub(live->joints[joint].pose.position, liveWrist)) : 0.0f;
        return liveLength > 0.02f ? clamp(length(sub(bind[joint].position, wrist)) / liveLength, 0.5f, 2.0f) : 1.0f;
    };
    struct Finger { int meta, prox, inter, dist, tip; bool synthMeta; float tipLength; };
    const Finger fingers[] = {{2, 3, -1, 4, 5, false, 0.025f}, {6, 7, 8, 9, 10, true, 0.022f}, {11, 12, 13, 14, 15, true, 0.023f},
        {16, 17, 18, 19, 20, true, 0.022f}, {21, 22, 23, 24, 25, true, 0.020f}};
    for (const auto& f : fingers) {
        // Fingers follow the wrist frame (matches the live joints within ~10 degrees); the thumb keeps its own twist.
        const XrVector3f up = f.synthMeta ? wristUp : rotate(bind[f.prox].orientation, {0, 1, 0});
        const int beforeDist = f.inter >= 0 ? f.inter : f.prox;
        XrVector3f metaUp = wristUp;
        if (f.synthMeta) {
            if (live) {
                bind[f.meta].position = add(wrist, liveToBind(sub(live->joints[f.meta].pose.position, liveWrist), handScale(f.prox)));
                metaUp = liveToBind(rotate(live->joints[f.meta].pose.orientation, {0, 1, 0}), 1.0f);
            } else bind[f.meta].position = lerp(wrist, bind[f.prox].position, 0.3f);
        }
        // The live thumb metacarpal frame rolls about 20 degrees from the wrist frame toward the thumb side
        // (captured open hands: left (+0.34, +0.94, 0), right (-0.37, +0.93, -0.09) in the wrist frame).
        if (!f.synthMeta) {
            float side = dot(sub(bind[f.meta].position, wrist), rotate(bind[XR_HAND_JOINT_WRIST_EXT].orientation, {1, 0, 0})) < 0 ? -1.0f : 1.0f;
            metaUp = rotate(bind[XR_HAND_JOINT_WRIST_EXT].orientation, normalize({side * 0.35f, 0.94f, 0.0f}));
        }
        bind[f.meta].orientation = boneFrame(bind[f.meta].position, bind[f.prox].position, metaUp);
        bind[f.prox].orientation = boneFrame(bind[f.prox].position, bind[f.inter >= 0 ? f.inter : f.dist].position, up);
        if (f.inter >= 0) bind[f.inter].orientation = boneFrame(bind[f.inter].position, bind[f.dist].position, up);
        // Straight finger: the distal continues the previous bone.
        XrVector3f forward = normalize(sub(bind[f.dist].position, bind[beforeDist].position));
        bind[f.dist].orientation = boneFrame(bind[f.dist].position, add(bind[f.dist].position, forward), up);
        float len = f.tipLength;
        if (live) { float l = length(sub(live->joints[f.tip].pose.position, live->joints[f.dist].pose.position)); if (l > 0.005f && l < 0.06f) len = l; }
        bind[f.tip].position = add(bind[f.dist].position, scale(forward, len));
        bind[f.tip].orientation = bind[f.dist].orientation;
    }
    bind[XR_HAND_JOINT_PALM_EXT] = bind[XR_HAND_JOINT_WRIST_EXT];
    if (mesh.jointParents) for (uint32_t i = 0; i < XR_HAND_JOINT_COUNT_EXT; ++i) mesh.jointParents[i] = static_cast<XrHandJointEXT>(kHandJointParents[i]);
    if (mesh.jointRadii) for (uint32_t i = 0; i < XR_HAND_JOINT_COUNT_EXT; ++i) {
        float r = live ? live->joints[i].radius : 0.0f;
        mesh.jointRadii[i] = r > 0.001f && r < 0.1f ? r : kHandJointRadii[i];
    }
    // Root frame expected by OVRPlugin: the whole bind pose, vertices and normals rotate by C of this hand.
    if (side < 0) {
        float thumbSide = dot(sub(bind[2].position, wrist), rotate(wristQ, {1, 0, 0}));
        side = thumbSide >= 0 ? 0 : 1;
    }
    const XrQuaternionf basis = kOvrHandBasis[side == 1 ? 1 : 0];
    for (uint32_t i = 0; i < XR_HAND_JOINT_COUNT_EXT; ++i) {
        bind[i].orientation = mul(basis, bind[i].orientation);
        bind[i].position = rotate(basis, sub(bind[i].position, wrist));
    }
    if (mesh.vertexPositions && mesh.vertexCapacityInput >= mesh.vertexCountOutput)
        for (uint32_t i = 0; i < mesh.vertexCountOutput; ++i) mesh.vertexPositions[i] = rotate(basis, sub(mesh.vertexPositions[i], wrist));
    if (mesh.vertexNormals && mesh.vertexCapacityInput >= mesh.vertexCountOutput)
        for (uint32_t i = 0; i < mesh.vertexCountOutput; ++i) mesh.vertexNormals[i] = rotate(basis, mesh.vertexNormals[i]);
}

#ifdef GXR_HAND_DIAGNOSTICS
static std::unordered_map<XrHandTrackerEXT, std::chrono::steady_clock::time_point> handLogTimes;
static std::string chainTypes(const void* next) {
    std::string out;
    for (auto node = static_cast<const XrBaseInStructure*>(next); node; node = node->next) out += std::to_string(node->type) + ",";
    return out.empty() ? "-" : out;
}
// OVRPlugin observer (diagnostics only): read the hand state and skeleton OVRPlugin derived from the runtime
// data, exactly as the game receives them. Nothing is modified; calls are rate limited and guarded against re-entry.
struct OvrpPosef { float qx, qy, qz, qw, px, py, pz; };
struct OvrpHandState { uint32_t status; OvrpPosef root; float bones[24][4]; uint32_t pinches; float pinchStrength[5]; OvrpPosef pointer; float handScale; int32_t handConfidence; int32_t fingerConfidence[5]; double requested; double sample; };
struct OvrpBone { int32_t id; int16_t parent; int16_t pad; OvrpPosef pose; };
struct OvrpCapsule { int16_t bone; int16_t pad; float start[3]; float end[3]; float radius; };
struct OvrpSkeleton2 { int32_t type; uint32_t numBones; uint32_t numCapsules; OvrpBone bones[70]; OvrpCapsule capsules[19]; };
static_assert(sizeof(OvrpHandState) == 512 && sizeof(OvrpBone) == 36 && sizeof(OvrpCapsule) == 32 && sizeof(OvrpSkeleton2) == 3140, "ovrp layouts");
// OVRPlugin 1.88 export forwards four arguments: step, frame index, hand, state.
using OvrpGetHandState2 = int (*)(int, int, int, OvrpHandState*);
using OvrpGetSkeleton2 = int (*)(int, OvrpSkeleton2*);
static void* ovrpHandle() {
#ifdef GXR_POLICY_TEST
    return nullptr;
#else
    static void* handle = [] {
        Dl_info self{};
        if (!dladdr(reinterpret_cast<void*>(&backend), &self) || !self.dli_fname) return static_cast<void*>(nullptr);
        std::string path(self.dli_fname); path.resize(path.find_last_of('/') + 1);
        void* result = dlopen((path + "libOVRPlugin.so").c_str(), RTLD_NOW | RTLD_NOLOAD);
        LOG("HAND ovrp observer available=%d", result != nullptr);
        return result;
    }();
    return handle;
#endif
}
static thread_local bool inOvrpObserver = false;
static void logOvrpHandState(int side) {
    if (side < 0 || inOvrpObserver) return;
    auto fn = ovrpHandle() ? reinterpret_cast<OvrpGetHandState2>(dlsym(ovrpHandle(), "ovrp_GetHandState2")) : nullptr;
    if (!fn) return;
    inOvrpObserver = true;
    OvrpHandState state{};
    const int attempts[4][2] = {{-1, -1}, {0, -1}, {-1, 0}, {0, 0}};
    int result = -1, used = -1;
    for (int a = 0; a < 4 && result != 0; ++a) { result = fn(attempts[a][0], attempts[a][1], side, &state); used = a; }
    inOvrpObserver = false;
    LOG("HAND ovrp state side=%d result=%d args=%d status=%x scale=%.3f conf=%d root=p(%.4f %.4f %.4f) q(%.4f %.4f %.4f %.4f)", side, result, used, state.status, state.handScale, state.handConfidence,
        state.root.px, state.root.py, state.root.pz, state.root.qx, state.root.qy, state.root.qz, state.root.qw);
    if (result != 0) return;
    for (int i = 0; i < 24; ++i) LOG("HAND ovrp bone side=%d b=%d q=(%.4f %.4f %.4f %.4f)", side, i, state.bones[i][0], state.bones[i][1], state.bones[i][2], state.bones[i][3]);
}
static void logOvrpSkeleton(int side) {
    if (side < 0 || inOvrpObserver) return;
    auto fn = ovrpHandle() ? reinterpret_cast<OvrpGetSkeleton2>(dlsym(ovrpHandle(), "ovrp_GetSkeleton2")) : nullptr;
    if (!fn) return;
    inOvrpObserver = true;
    std::vector<uint8_t> raw(8192, 0);
    auto* skeleton = reinterpret_cast<OvrpSkeleton2*>(raw.data());
    int result = fn(side, skeleton);
    inOvrpObserver = false;
    LOG("HAND ovrp skeleton side=%d result=%d type=%d bones=%u capsules=%u", side, result, skeleton->type, skeleton->numBones, skeleton->numCapsules);
    if (result != 0) return;
    for (size_t off = 0; off < 12 + 25 * 36; off += 128) {
        std::string hex;
        char byte[4];
        for (size_t i = off; i < off + 128; ++i) { snprintf(byte, sizeof(byte), "%02x", raw[i]); hex += byte; }
        LOG("HAND ovrp skelraw side=%d off=%u %s", side, static_cast<unsigned>(off), hex.c_str());
    }
    for (uint32_t i = 0; i < skeleton->numBones && i < 50; ++i) {
        const auto& b = skeleton->bones[i];
        LOG("HAND ovrp skelbone side=%d i=%u id=%d parent=%d p=(%.4f %.4f %.4f) q=(%.4f %.4f %.4f %.4f)", side, i, static_cast<int>(b.id), b.parent, b.pose.px, b.pose.py, b.pose.pz, b.pose.qx, b.pose.qy, b.pose.qz, b.pose.qw);
    }
    for (uint32_t i = 0; i < skeleton->numCapsules && i < 19; ++i) {
        const auto& c = skeleton->capsules[i];
        LOG("HAND ovrp capsule side=%d i=%u bone=%d r=%.4f", side, i, c.bone, c.radius);
    }
}
static void logBind(const XrHandTrackingMeshFB& mesh, const char* label) {
    for (uint32_t i = 0; i < mesh.jointCountOutput && i < XR_HAND_JOINT_COUNT_EXT; ++i) {
        const auto& p = mesh.jointBindPoses[i];
        LOG("HAND %s j=%u parent=%d r=%.4f p=(%.4f %.4f %.4f) q=(%.4f %.4f %.4f %.4f)", label, i,
            mesh.jointParents ? static_cast<int>(mesh.jointParents[i]) : -1, mesh.jointRadii ? mesh.jointRadii[i] : 0.0f,
            p.position.x, p.position.y, p.position.z, p.orientation.x, p.orientation.y, p.orientation.z, p.orientation.w);
    }
}
#endif

extern "C" XRAPI_ATTR XrResult XRAPI_CALL xrCreateHandTrackerEXT(XrSession session, const XrHandTrackerCreateInfoEXT* info, XrHandTrackerEXT* tracker) {
    auto result = origCreateHandTracker ? origCreateHandTracker(session, info, tracker) : XR_ERROR_FUNCTION_UNSUPPORTED;
    if (XR_SUCCEEDED(result) && tracker) {
        std::lock_guard<std::mutex> lock(handMutex);
        HandSample sample;
        if (info) sample.side = info->hand == XR_HAND_LEFT_EXT ? 0 : info->hand == XR_HAND_RIGHT_EXT ? 1 : -1;
        handSamples[*tracker] = sample;
    }
#ifdef GXR_HAND_DIAGNOSTICS
    LOG("HAND create hand=%d jointSet=%d next=%s result=%d", info ? info->hand : -1, info ? info->handJointSet : -1, info ? chainTypes(info->next).c_str() : "-", result);
#endif
    return result;
}

extern "C" XRAPI_ATTR XrResult XRAPI_CALL xrDestroyHandTrackerEXT(XrHandTrackerEXT tracker) {
    auto result = origDestroyHandTracker ? origDestroyHandTracker(tracker) : XR_ERROR_FUNCTION_UNSUPPORTED;
    if (XR_SUCCEEDED(result)) { std::lock_guard<std::mutex> lock(handMutex); handSamples.erase(tracker); }
    return result;
}

extern "C" XRAPI_ATTR XrResult XRAPI_CALL xrLocateHandJointsEXT(XrHandTrackerEXT tracker, const XrHandJointsLocateInfoEXT* info, XrHandJointLocationsEXT* locations) {
    auto result = origLocateHandJoints ? origLocateHandJoints(tracker, info, locations) : XR_ERROR_FUNCTION_UNSUPPORTED;
    if (XR_SUCCEEDED(result) && locations && locations->isActive && locations->jointLocations && locations->jointCount >= XR_HAND_JOINT_COUNT_EXT) {
        std::lock_guard<std::mutex> lock(handMutex);
        auto it = handSamples.find(tracker);
        if (it != handSamples.end()) {
            it->second.valid = true;
            for (uint32_t i = 0; i < XR_HAND_JOINT_COUNT_EXT; ++i) it->second.joints[i] = locations->jointLocations[i];
        }
    }
#ifdef GXR_HAND_DIAGNOSTICS
    auto now = std::chrono::steady_clock::now();
    {
        std::lock_guard<std::mutex> lock(handMutex);
        auto it = handLogTimes.find(tracker);
        if (it != handLogTimes.end() && now - it->second < std::chrono::seconds(2)) return result;
        handLogTimes[tracker] = now;
    }
    if (!locations) return result;
    int side = -1; bool logSkeleton = false;
    {
        std::lock_guard<std::mutex> lock(handMutex);
        auto it = handSamples.find(tracker);
        if (it != handSamples.end()) { side = it->second.side; logSkeleton = !it->second.skeletonLogged; it->second.skeletonLogged = true; }
    }
    LOG("HAND locate tracker=%p side=%d result=%d active=%d joints=%u infoNext=%s locNext=%s", reinterpret_cast<void*>(tracker), side, result, locations->isActive, locations->jointCount,
        info ? chainTypes(info->next).c_str() : "-", chainTypes(locations->next).c_str());
    if (logSkeleton) logOvrpSkeleton(side);
    logOvrpHandState(side);
    if (XR_SUCCEEDED(result) && locations->isActive && locations->jointLocations)
        for (uint32_t i = 0; i < locations->jointCount && i < XR_HAND_JOINT_COUNT_EXT; ++i) {
            const auto& j = locations->jointLocations[i];
            LOG("HAND joint tracker=%p j=%u flags=%llx r=%.4f p=(%.4f %.4f %.4f) q=(%.4f %.4f %.4f %.4f)", reinterpret_cast<void*>(tracker), i, static_cast<unsigned long long>(j.locationFlags), j.radius,
                j.pose.position.x, j.pose.position.y, j.pose.position.z, j.pose.orientation.x, j.pose.orientation.y, j.pose.orientation.z, j.pose.orientation.w);
        }
#endif
    return result;
}

extern "C" XRAPI_ATTR XrResult XRAPI_CALL xrGetHandMeshFB(XrHandTrackerEXT tracker, XrHandTrackingMeshFB* mesh) {
    auto result = origGetHandMesh ? origGetHandMesh(tracker, mesh) : XR_ERROR_FUNCTION_UNSUPPORTED;
    bool fixed = false;
    if (XR_SUCCEEDED(result) && handMeshFix() && mesh && mesh->jointBindPoses && mesh->jointCountOutput == XR_HAND_JOINT_COUNT_EXT && mesh->jointCapacityInput >= XR_HAND_JOINT_COUNT_EXT) {
        HandSample live;
        { std::lock_guard<std::mutex> lock(handMutex); auto it = handSamples.find(tracker); if (it != handSamples.end()) live = it->second; }
        rebuildHandSkeleton(*mesh, live.valid ? &live : nullptr, live.side);
        fixed = true;
        LOG("Hand skeleton rebuilt from runtime bind joints (live sample=%d)", live.valid);
    }
#ifdef GXR_HAND_DIAGNOSTICS
    if (mesh) {
        LOG("HAND mesh tracker=%p result=%d fixed=%d next=%s joints=%u/%u vertices=%u/%u indices=%u/%u", reinterpret_cast<void*>(tracker), result, fixed, chainTypes(mesh->next).c_str(),
            mesh->jointCapacityInput, mesh->jointCountOutput, mesh->vertexCapacityInput, mesh->vertexCountOutput, mesh->indexCapacityInput, mesh->indexCountOutput);
        if (fixed) logBind(*mesh, "bind");
    }
#else
    (void)fixed;
#endif
    return result;
}
