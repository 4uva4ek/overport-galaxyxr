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
static bool handFramesFix() { static bool enabled = config().find("handframes=runtime\n") == std::string::npos; return enabled; }
static bool handBindEdited() { static bool enabled = config().find("handbind=edited\n") != std::string::npos; return enabled; }

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

// Two live joint frames of the Galaxy XR runtime are rolled about their bone axes differently from the Meta runtime,
// whose conventions Quest hand rigs were built against (OVR skeleton rest pose, Oculus Integration OVRSkeletonData):
// captured SM-I610 data rolls the wrist +Y 13 degrees toward the thumb from the palm-plane normal while the palm
// joint rolls 4 degrees and the finger joints fan exactly like the Meta rest twists only when measured against the
// palm joint; the little metacarpal +Y rolls 5 degrees toward the thumb where the Meta Pinky0 bone rolls 24 degrees
// away from it. OVRPlugin derives the hand root from the wrist and every bone rotation relative to its parent joint,
// so the wrist takes its roll from the palm joint (position and Z axis unchanged; the fingers keep their world
// orientation) and the little metacarpal is rolled toward the ulnar side. Disabled by handframes=runtime.
static const float kLittleMetacarpalRoll = 24.0f * 3.14159265f / 180.0f;
static void alignHandFrames(XrHandJointLocationEXT* joints) {
    using namespace handmath;
    const uint64_t needed = XR_SPACE_LOCATION_ORIENTATION_VALID_BIT | XR_SPACE_LOCATION_POSITION_VALID_BIT;
    auto& wrist = joints[XR_HAND_JOINT_WRIST_EXT];
    const auto& palm = joints[XR_HAND_JOINT_PALM_EXT];
    if ((wrist.locationFlags & needed) != needed || !(palm.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT)) return;
    XrVector3f z = rotate(wrist.pose.orientation, {0, 0, 1});
    XrVector3f palmUp = rotate(palm.pose.orientation, {0, 1, 0});
    XrVector3f y = sub(palmUp, scale(z, dot(palmUp, z)));
    if (length(y) < 0.2f) return;
    y = normalize(y);
    wrist.pose.orientation = fromAxes(cross(y, z), y, z);
    auto& meta = joints[XR_HAND_JOINT_LITTLE_METACARPAL_EXT];
    const auto& prox = joints[XR_HAND_JOINT_LITTLE_PROXIMAL_EXT];
    if ((meta.locationFlags & needed) != needed || !(prox.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT)) return;
    if (length(sub(prox.pose.position, meta.pose.position)) < 0.01f) return;
    // The ulnar side is where the little finger lies: -X of the wrist frame on the left hand, +X on the right.
    float ulnar = dot(sub(prox.pose.position, wrist.pose.position), rotate(wrist.pose.orientation, {1, 0, 0})) < 0 ? -1.0f : 1.0f;
    XrVector3f up = rotate(wrist.pose.orientation, {ulnar * std::sin(kLittleMetacarpalRoll), std::cos(kLittleMetacarpalRoll), 0});
    meta.pose.orientation = boneFrame(meta.pose.position, prox.pose.position, up);
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

// Hand-edited bind pose (handbind=edited): left-hand joints in the wrist frame of the runtime bind data (metres, xyzw),
// exported from the ANTR hand map on 2026-10-14 after visual tuning against the live joints and the Meta rest pose.
// Only the positions and the roll (+Y direction) of each joint are used; the orientations are rebuilt with the chain
// rules of rebuildHandSkeleton so every frame keeps +Z along its bone, except the finger tips, which keep the table
// orientation. The right hand mirrors X. Enabled by handbind=edited (the CLI default), off with handbind=rebuild.
static const float kEditedBind[XR_HAND_JOINT_COUNT_EXT][7] = {
    {0.00000f, 0.00000f, 0.00000f, 0.00000f, 0.00000f, 0.00000f, 1.00000f},  // palm
    {0.00000f, 0.00000f, 0.00000f, 0.00000f, 0.00000f, 0.00000f, 1.00000f},  // wrist
    {0.03070f, 0.00510f, -0.04300f, 0.00515f, -0.33431f, 0.03920f, 0.94163f},  // th_meta
    {0.05080f, 0.00630f, -0.06790f, -0.02848f, -0.28867f, 0.01958f, 0.95680f},  // th_prox
    {0.06400f, 0.00530f, -0.08760f, -0.02845f, -0.28867f, 0.01949f, 0.95681f},  // th_dist
    {0.07700f, 0.00180f, -0.11260f, -0.08681f, -0.28932f, 0.00183f, 0.95329f},  // th_tip
    {0.01210f, 0.00220f, -0.02880f, 0.02318f, -0.08067f, 0.10494f, 0.99093f},  // ix_meta
    {0.02370f, 0.00690f, -0.10280f, -0.02985f, -0.05350f, 0.00304f, 0.99812f},  // ix_prox
    {0.02810f, 0.00450f, -0.14350f, -0.07414f, -0.04606f, -0.00013f, 0.99618f},  // ix_inter
    {0.03010f, 0.00130f, -0.16470f, -0.07414f, -0.04606f, -0.00013f, 0.99618f},  // ix_dist
    {0.03200f, -0.00220f, -0.18420f, -0.07414f, -0.04606f, -0.00013f, 0.99618f},  // ix_tip
    {-0.00010f, 0.00130f, -0.02930f, 0.02430f, -0.00869f, 0.07824f, 0.99660f},  // md_meta
    {0.00100f, 0.00510f, -0.10530f, -0.03110f, -0.02641f, 0.01914f, 0.99898f},  // md_prox
    {0.00330f, 0.00240f, -0.14870f, -0.07104f, -0.02468f, -0.01864f, 0.99699f},  // md_inter
    {0.00430f, -0.00070f, -0.17020f, -0.07104f, -0.02466f, -0.01843f, 0.99700f},  // md_dist
    {0.00570f, -0.00420f, -0.19000f, -0.07104f, -0.02466f, -0.01843f, 0.99700f},  // md_tip
    {-0.01160f, -0.00050f, -0.02610f, 0.01219f, 0.04047f, 0.05171f, 0.99777f},  // rg_meta
    {-0.01760f, 0.00100f, -0.09820f, -0.02231f, 0.01671f, 0.00409f, 0.99960f},  // rg_prox
    {-0.01880f, -0.00070f, -0.13540f, -0.06735f, 0.01084f, -0.05725f, 0.99603f},  // rg_inter
    {-0.01960f, -0.00420f, -0.16140f, -0.06735f, 0.01084f, -0.05725f, 0.99603f},  // rg_dist
    {-0.01950f, -0.01010f, -0.18110f, -0.19678f, 0.01823f, -0.05535f, 0.97871f},  // rg_tip
    {-0.02150f, -0.00300f, -0.02100f, 0.00241f, 0.11355f, 0.05072f, 0.99223f},  // lt_meta
    {-0.03730f, -0.00350f, -0.08920f, -0.01783f, 0.05619f, 0.01223f, 0.99819f},  // lt_prox
    {-0.04120f, -0.00480f, -0.12360f, -0.03650f, 0.03867f, 0.00764f, 0.99856f},  // lt_inter
    {-0.04250f, -0.00600f, -0.14060f, -0.03649f, 0.03867f, 0.00768f, 0.99856f},  // lt_dist
    {-0.04320f, -0.00880f, -0.16140f, -0.12339f, 0.03785f, 0.01102f, 0.99158f},  // lt_tip
};
static void applyEditedBind(XrPosef* bind, const XrVector3f& wrist, const XrQuaternionf& wristQ, int side) {
    using namespace handmath;
    const float mirror = side == 1 ? -1.0f : 1.0f;
    XrVector3f p[XR_HAND_JOINT_COUNT_EXT], up[XR_HAND_JOINT_COUNT_EXT];
    for (uint32_t i = 0; i < XR_HAND_JOINT_COUNT_EXT; ++i) {
        const float* e = kEditedBind[i];
        p[i] = add(wrist, rotate(wristQ, {mirror * e[0], e[1], e[2]}));
        XrVector3f u = rotate(XrQuaternionf{e[3], e[4], e[5], e[6]}, {0, 1, 0});
        up[i] = rotate(wristQ, {mirror * u.x, u.y, u.z});
    }
    struct Chain { int meta, prox, inter, dist, tip; };
    const Chain chains[] = {{2, 3, -1, 4, 5}, {6, 7, 8, 9, 10}, {11, 12, 13, 14, 15}, {16, 17, 18, 19, 20}, {21, 22, 23, 24, 25}};
    for (const auto& c : chains) {
        const int beforeDist = c.inter >= 0 ? c.inter : c.prox;
        bind[c.meta] = {boneFrame(p[c.meta], p[c.prox], up[c.meta]), p[c.meta]};
        bind[c.prox] = {boneFrame(p[c.prox], p[c.inter >= 0 ? c.inter : c.dist], up[c.prox]), p[c.prox]};
        if (c.inter >= 0) bind[c.inter] = {boneFrame(p[c.inter], p[c.dist], up[c.inter]), p[c.inter]};
        XrVector3f forward = normalize(sub(p[c.dist], p[beforeDist]));
        bind[c.dist] = {boneFrame(p[c.dist], add(p[c.dist], forward), up[c.dist]), p[c.dist]};
        // Tips are leaves, so their orientation is taken from the table as is (mirrored across X for the right hand)
        // instead of copying the distal frame; this lets the map tilt the tip bones of the rig.
        const float* e = kEditedBind[c.tip];
        XrQuaternionf tq{e[3], mirror * e[4], mirror * e[5], e[6]};
        float n = std::sqrt(tq.x * tq.x + tq.y * tq.y + tq.z * tq.z + tq.w * tq.w);
        if (n > 1e-6f) { tq.x /= n; tq.y /= n; tq.z /= n; tq.w /= n; } else tq = {0, 0, 0, 1};
        bind[c.tip] = {mul(wristQ, tq), p[c.tip]};
    }
    bind[XR_HAND_JOINT_WRIST_EXT] = {wristQ, wrist};
    bind[XR_HAND_JOINT_PALM_EXT] = bind[XR_HAND_JOINT_WRIST_EXT];
}

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
    if (side < 0) {
        float thumbSide = dot(sub(bind[2].position, wrist), rotate(wristQ, {1, 0, 0}));
        side = thumbSide >= 0 ? 0 : 1;
    }
    if (handBindEdited()) applyEditedBind(bind, wrist, wristQ, side);
    // Root frame expected by OVRPlugin: the whole bind pose, vertices and normals rotate by C of this hand.
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
        if (handFramesFix()) {
            alignHandFrames(locations->jointLocations);
            static bool logged = false;
            if (!logged) { logged = true; LOG("Hand joint frames aligned to the Meta conventions: wrist roll from the palm joint, little metacarpal rolled %.0f deg", kLittleMetacarpalRoll * 180.0f / 3.14159265f); }
        }
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
        LOG("Hand skeleton rebuilt from runtime bind joints (live sample=%d, edited bind=%d)", live.valid, handBindEdited());
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
