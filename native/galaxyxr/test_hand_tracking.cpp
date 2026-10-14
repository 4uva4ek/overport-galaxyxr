// Focused host test: hand skeleton rebuild from the SM-I610 runtime bind data captured on 2026-10-14.
#define GXR_POLICY_TEST
#include "policy.cpp"
#include <cassert>
#include <cmath>

static void* testSymbol(const char*) { return nullptr; }

// Left hand bind poses exactly as returned by the Galaxy XR runtime (position xyz, orientation xyzw).
static const float kRuntimeBind[26][7] = {
    {0, 0, 0, 0, 0, 0, 1}, {0, 0, 0, 0, 0, 0, 1},
    {0.0337f, -0.0170f, -0.0415f, 0.0901f, -0.2695f, -0.4911f, 0.8234f},
    {0.0512f, -0.0219f, -0.0669f, 0.0077f, -0.3701f, -0.5164f, 0.7722f},
    {0.0669f, -0.0322f, -0.0869f, 0.0205f, -0.3030f, -0.5231f, 0.7963f},
    {0.0280f, -0.0094f, -0.1409f, -0.2760f, -0.0300f, 0.0469f, 0.9595f},
    {0.0296f, -0.0210f, -0.1595f, -0.4074f, 0.0005f, 0.0365f, 0.9125f},
    {0.0255f, -0.0022f, -0.1029f, -0.1082f, -0.0242f, 0.0522f, 0.9925f},
    {0.0280f, -0.0094f, -0.1409f, -0.2760f, -0.0300f, 0.0469f, 0.9595f},
    {0.0296f, -0.0210f, -0.1595f, -0.4074f, 0.0005f, 0.0365f, 0.9125f},
    {-0.0193f, -0.0075f, -0.0922f, -0.1958f, 0.0660f, 0.1602f, 0.9652f},
    {-0.0216f, -0.0234f, -0.1280f, -0.3447f, 0.0092f, 0.1667f, 0.9238f},
    {0.0003f, -0.0009f, -0.1010f, -0.1607f, 0.0427f, 0.1054f, 0.9804f},
    {-0.0018f, -0.0140f, -0.1403f, -0.3439f, -0.0217f, 0.1078f, 0.9326f},
    {0.0014f, -0.0329f, -0.1633f, -0.4304f, -0.0434f, 0.1051f, 0.8954f},
    {-0.0430f, -0.0447f, -0.1265f, -0.3710f, 0.0322f, 0.1822f, 0.9100f},
    {0, 0, 0, 0, 0, 0, 1},
    {-0.0193f, -0.0075f, -0.0922f, -0.1958f, 0.0660f, 0.1602f, 0.9652f},
    {-0.0216f, -0.0234f, -0.1280f, -0.3447f, 0.0092f, 0.1667f, 0.9238f},
    {-0.0187f, -0.0433f, -0.1519f, -0.4242f, -0.0210f, 0.1482f, 0.8931f},
    {0, 0, 0, 0, 0, 0, 1}, {0, 0, 0, 0, 0, 0, 1},
    {-0.0381f, -0.0150f, -0.0791f, -0.2241f, 0.1196f, 0.2010f, 0.9461f},
    {-0.0416f, -0.0320f, -0.1084f, -0.3021f, 0.0958f, 0.2066f, 0.9257f},
    {-0.0430f, -0.0447f, -0.1265f, -0.3710f, 0.0322f, 0.1822f, 0.9100f},
    {0, 0, 0, 0, 0, 0, 1},
};
static const int kRuntimeParents[26] = {-1, -1, 1, 2, 3, -1, -1, 4, 1, 1, -1, -1, 7, 2, 2, -1, -1, 1, 1, 12, -1, -1, 3, 3, 1, -1};
// One live open-hand sample of the same hand (radius, position xyz, orientation xyzw).
static const float kLive[26][8] = {
    {0.0100f, -0.2141f, 1.4333f, -0.2492f, 0.0399f, -0.1250f, 0.1220f, 0.9838f},
    {0.0164f, -0.2292f, 1.4263f, -0.1857f, 0.0549f, -0.1171f, 0.0338f, 0.9910f},
    {0.0128f, -0.2010f, 1.4242f, -0.2064f, 0.0455f, -0.4375f, -0.1516f, 0.8852f},
    {0.0096f, -0.1607f, 1.4216f, -0.2377f, 0.2570f, -0.4299f, -0.4646f, 0.7302f},
    {0.0096f, -0.1370f, 1.4208f, -0.2514f, 0.2727f, -0.3710f, -0.4535f, 0.7631f},
    {0.0096f, -0.1134f, 1.4220f, -0.2695f, 0.2727f, -0.3710f, -0.4535f, 0.7631f},
    {0.0187f, -0.2097f, 1.4298f, -0.2110f, 0.0282f, -0.2029f, 0.1181f, 0.9716f},
    {0.0097f, -0.1808f, 1.4374f, -0.2794f, -0.0180f, -0.1407f, 0.0768f, 0.9869f},
    {0.0096f, -0.1698f, 1.4380f, -0.3166f, -0.0659f, -0.1415f, 0.0695f, 0.9853f},
    {0.0096f, -0.1636f, 1.4355f, -0.3375f, -0.1412f, -0.1094f, 0.0586f, 0.9822f},
    {0.0096f, -0.1582f, 1.4286f, -0.3588f, -0.1412f, -0.1094f, 0.0586f, 0.9822f},
    {0.0177f, -0.2224f, 1.4294f, -0.2150f, 0.0399f, -0.1250f, 0.1220f, 0.9838f},
    {0.0104f, -0.2057f, 1.4372f, -0.2833f, -0.0310f, -0.0315f, 0.1195f, 0.9919f},
    {0.0096f, -0.2029f, 1.4354f, -0.3247f, -0.0730f, -0.0803f, 0.1147f, 0.9875f},
    {0.0096f, -0.1978f, 1.4318f, -0.3541f, -0.0924f, -0.0938f, 0.1105f, 0.9851f},
    {0.0096f, -0.1926f, 1.4268f, -0.3771f, -0.0924f, -0.0938f, 0.1105f, 0.9851f},
    {0.0163f, -0.2345f, 1.4262f, -0.2154f, 0.0102f, -0.0651f, 0.1193f, 0.9907f},
    {0.0098f, -0.2263f, 1.4286f, -0.2802f, 0.0331f, -0.0058f, 0.1730f, 0.9843f},
    {0.0096f, -0.2260f, 1.4309f, -0.3193f, -0.0497f, -0.0505f, 0.1650f, 0.9838f},
    {0.0096f, -0.2225f, 1.4285f, -0.3503f, -0.0948f, -0.0684f, 0.1402f, 0.9832f},
    {0.0096f, -0.2187f, 1.4240f, -0.3712f, -0.0948f, -0.0684f, 0.1402f, 0.9832f},
    {0.0148f, -0.2453f, 1.4219f, -0.2136f, -0.0259f, 0.0167f, 0.1245f, 0.9917f},
    {0.0096f, -0.2468f, 1.4186f, -0.2727f, 0.0148f, 0.0551f, 0.2000f, 0.9781f},
    {0.0096f, -0.2496f, 1.4178f, -0.3066f, -0.0455f, 0.0368f, 0.1980f, 0.9785f},
    {0.0096f, -0.2511f, 1.4161f, -0.3286f, -0.1088f, -0.0147f, 0.1575f, 0.9814f},
    {0.0096f, -0.2508f, 1.4113f, -0.3468f, -0.1088f, -0.0147f, 0.1575f, 0.9814f},
};

static XrPosef bindPoses[26];
static float radii[26];
static XrHandJointEXT parents[26];
static XrResult mockGetHandMesh(XrHandTrackerEXT, XrHandTrackingMeshFB* mesh) {
    mesh->jointCountOutput = 26;
    if (mesh->jointCapacityInput < 26) return XR_SUCCESS;
    for (int i = 0; i < 26; ++i) {
        const float* b = kRuntimeBind[i];
        mesh->jointBindPoses[i] = {{b[3], b[4], b[5], b[6]}, {b[0], b[1], b[2]}};
        mesh->jointRadii[i] = 0;
        mesh->jointParents[i] = static_cast<XrHandJointEXT>(kRuntimeParents[i]);
    }
    return XR_SUCCESS;
}
static XrResult mockLocate(XrHandTrackerEXT, const XrHandJointsLocateInfoEXT*, XrHandJointLocationsEXT* locations) {
    locations->isActive = XR_TRUE;
    locations->jointCount = 26;
    for (int i = 0; i < 26; ++i) {
        const float* l = kLive[i];
        locations->jointLocations[i] = {0xf, {{l[4], l[5], l[6], l[7]}, {l[1], l[2], l[3]}}, l[0]};
    }
    return XR_SUCCESS;
}
static XrResult mockCreateTracker(XrSession, const XrHandTrackerCreateInfoEXT* info, XrHandTrackerEXT* tracker) {
    *tracker = reinterpret_cast<XrHandTrackerEXT>(info && info->hand == XR_HAND_RIGHT_EXT ? 8 : 7); return XR_SUCCESS;
}

// Fraction of the way from the wrist to the target at which the point projects (0 at the wrist).
static float along(const XrVector3f& wrist, const XrVector3f& target, const XrVector3f& point, float fallback) {
    using namespace handmath;
    XrVector3f axis = sub(target, wrist);
    float d = dot(axis, axis);
    return d > 1e-6f ? dot(sub(point, wrist), axis) / d : fallback;
}
static float angleDeg(const XrVector3f& a, const XrVector3f& b) {
    using namespace handmath;
    float c = dot(normalize(a), normalize(b));
    return std::acos(c > 1 ? 1 : c < -1 ? -1 : c) * 180.0f / 3.14159265f;
}
static XrHandTrackingMeshFB queryMesh(uintptr_t handle = 7) {
    XrHandTrackingMeshFB mesh{XR_TYPE_HAND_TRACKING_MESH_FB};
    mesh.jointCapacityInput = 26; mesh.jointBindPoses = bindPoses; mesh.jointRadii = radii; mesh.jointParents = parents;
    assert(XR_SUCCEEDED(xrGetHandMeshFB(reinterpret_cast<XrHandTrackerEXT>(handle), &mesh)));
    return mesh;
}
// handbind=edited: the bind joints come from the hand-tuned table (right hand mirrored), every finger frame keeps +Z along
// its bone toward the parent, the table roll is preserved, parents and radii are still rebuilt. Run as `test_hand_tracking edited`.
static int runEdited() {
    using namespace handmath;
    testConfig += "handbind=edited\n";
    origGetHandMesh = mockGetHandMesh; origLocateHandJoints = mockLocate; origCreateHandTracker = mockCreateTracker;
    for (int side = 0; side < 2; ++side) {
        XrHandTrackerEXT tracker = XR_NULL_HANDLE;
        XrHandTrackerCreateInfoEXT create{XR_TYPE_HAND_TRACKER_CREATE_INFO_EXT, nullptr, side == 0 ? XR_HAND_LEFT_EXT : XR_HAND_RIGHT_EXT, XR_HAND_JOINT_SET_DEFAULT_EXT};
        assert(XR_SUCCEEDED(xrCreateHandTrackerEXT(XR_NULL_HANDLE, &create, &tracker)));
        XrHandTrackingMeshFB mesh = queryMesh(side == 0 ? 7 : 8);
        const XrPosef* b = mesh.jointBindPoses;
        const XrQuaternionf C = kOvrHandBasis[side];
        const float mirror = side == 0 ? 1.0f : -1.0f;
        for (int i = 0; i < 26; ++i) {
            const float* e = kEditedBind[i];
            assert(length(sub(b[i].position, rotate(C, {mirror * e[0], e[1], e[2]}))) < 1e-5f);
            assert(static_cast<int32_t>(mesh.jointParents[i]) == kHandJointParents[i]);
            assert(mesh.jointRadii[i] > 0.005f && mesh.jointRadii[i] < 0.03f);
        }
        assert(angleDeg(rotate(b[1].orientation, {0, 0, 1}), rotate(C, {0, 0, 1})) < 0.1f);
        const int chains[5][5] = {{2, 3, 4, 5, -1}, {6, 7, 8, 9, 10}, {11, 12, 13, 14, 15}, {16, 17, 18, 19, 20}, {21, 22, 23, 24, 25}};
        for (const auto& chain : chains) {
            int count = chain[4] >= 0 ? 5 : 4;
            // Tips may sit off the bone line (they were moved by hand), so the distal frame is checked against the previous bone.
            for (int k = 0; k + 2 < count; ++k) assert(angleDeg(rotate(b[chain[k]].orientation, {0, 0, -1}), sub(b[chain[k + 1]].position, b[chain[k]].position)) < 0.1f);
            int dist = chain[count - 2];
            assert(angleDeg(rotate(b[dist].orientation, {0, 0, -1}), sub(b[dist].position, b[chain[count - 3]].position)) < 0.1f);
            {   // The tip keeps the table orientation (mirrored for the right hand).
                const float* e = kEditedBind[chain[count - 1]];
                XrQuaternionf tq{e[3], mirror * e[4], mirror * e[5], e[6]};
                for (const XrVector3f axis : {XrVector3f{0, 1, 0}, XrVector3f{0, 0, 1}})
                    assert(angleDeg(rotate(b[chain[count - 1]].orientation, axis), rotate(C, rotate(tq, axis))) < 0.1f);
            }
            // The table roll is the +Y of the table orientation projected onto the plane normal to the rebuilt bone
            // (two table frames were rolled after their joints were dragged, so their own +Z is off the bone by 4-9 deg).
            for (int k = 0; k + 1 < count; ++k) {
                const float* e = kEditedBind[chain[k]];
                XrVector3f up = rotate(XrQuaternionf{e[3], e[4], e[5], e[6]}, {0, 1, 0});
                up = rotate(C, {mirror * up.x, up.y, up.z});
                XrVector3f z = rotate(b[chain[k]].orientation, {0, 0, 1});
                assert(angleDeg(rotate(b[chain[k]].orientation, {0, 1, 0}), sub(up, scale(z, dot(up, z)))) < 0.1f);
            }
        }
    }
    printf("hand skeleton edited bind: ok\n");
    return 0;
}
static void checkRebuilt(const XrHandTrackingMeshFB& mesh, bool live) {
    using namespace handmath;
    for (int i = 0; i < 26; ++i) assert(static_cast<int32_t>(mesh.jointParents[i]) == kHandJointParents[i]);
    // Left hand: the wrist bind orientation must equal the OVRPlugin basis quaternion and sit at the origin.
    assert(angleDeg(rotate(mesh.jointBindPoses[1].orientation, {0, 0, 1}), rotate(kOvrHandBasis[0], {0, 0, 1})) < 0.1f);
    assert(angleDeg(rotate(mesh.jointBindPoses[1].orientation, {0, 1, 0}), rotate(kOvrHandBasis[0], {0, 1, 0})) < 0.1f);
    assert(length(mesh.jointBindPoses[1].position) < 1e-5f);
    assert(mesh.jointParents[1] == XR_HAND_JOINT_PALM_EXT && mesh.jointParents[0] == XR_HAND_JOINT_WRIST_EXT);
    assert(length(sub(mesh.jointBindPoses[0].position, mesh.jointBindPoses[1].position)) < 1e-6f);
    for (int i = 0; i < 26; ++i) assert(mesh.jointRadii[i] > 0.005f && mesh.jointRadii[i] < 0.03f);
    if (live) assert(std::fabs(mesh.jointRadii[1] - 0.0164f) < 1e-4f);
    const XrPosef* b = mesh.jointBindPoses;
    const int chains[5][5] = {{2, 3, 4, 5, -1}, {6, 7, 8, 9, 10}, {11, 12, 13, 14, 15}, {16, 17, 18, 19, 20}, {21, 22, 23, 24, 25}};
    for (const auto& chain : chains) {
        for (int k = 0; k < 4 && chain[k + 1] >= 0; ++k) {
            int j = chain[k], child = chain[k + 1];
            XrVector3f bone = sub(b[child].position, b[j].position);
            assert(length(bone) > 0.005f);
            assert(angleDeg(rotate(b[j].orientation, {0, 0, -1}), bone) < 8.0f);
        }
        int meta = chain[0], prox = chain[1];
        int tip = chain[4] >= 0 ? chain[4] : chain[3];
        int dist = chain[4] >= 0 ? chain[3] : chain[2];
        // Finger frames follow the wrist, the thumb chain keeps its own twist with the metacarpal following the wrist.
        XrVector3f wristUp = rotate(b[1].orientation, {0, 1, 0});
        if (meta == 2) {
            // Left hand capture: the thumb metacarpal rolls toward +X of the wrist frame.
            XrVector3f expected = rotate(b[1].orientation, normalize({0.35f, 0.94f, 0.0f}));
            assert(angleDeg(rotate(b[meta].orientation, {0, 1, 0}), expected) < 5.0f);
        } else if (meta == 21 && live) {
            // The little metacarpal follows the aligned live frame: rolled about 24 degrees toward the ulnar side.
            float roll = angleDeg(rotate(b[meta].orientation, {0, 1, 0}), wristUp);
            assert(roll > 18.0f && roll < 30.0f);
        } else assert(angleDeg(rotate(b[meta].orientation, {0, 1, 0}), wristUp) < 20.0f);
        assert(angleDeg(rotate(b[tip].orientation, {0, 1, 0}), rotate(b[dist].orientation, {0, 1, 0})) < 1.0f);
        // The captured bind hand is relaxed (about 22 degrees of curl), so compare twist, not curl.
        if (meta != 2) for (int k = 1; k < 4; ++k) assert(angleDeg(rotate(b[chain[k]].orientation, {0, 1, 0}), wristUp) < 45.0f);
        float tipLength = length(sub(b[tip].position, b[dist].position));
        assert(tipLength > 0.012f && tipLength < 0.035f);
        if (meta != 2) {
            float t = along(b[1].position, b[prox].position, b[meta].position, -1.0f);
            assert(t > 0.05f && t < 0.95f);
        }
    }
    if (live) {
        // The live index metacarpal sits about 6 mm off the wrist-proximal line toward the thumb; the bind one must too.
        XrVector3f axis = normalize(sub(b[7].position, b[1].position)), rel = sub(b[6].position, b[1].position);
        XrVector3f lateral = sub(rel, scale(axis, dot(rel, axis)));
        assert(length(lateral) > 0.003f && length(lateral) < 0.012f);
        assert(dot(lateral, sub(b[2].position, b[1].position)) > 0);
    }
    // Index tip length follows the live sample (|j10 - j9| of the captured hand is about 23 mm).
    if (live) assert(std::fabs(length(sub(b[10].position, b[9].position)) - 0.0230f) < 0.0015f);
}
int main(int argc, char** argv) {
    using namespace handmath;
    if (argc > 1 && std::string(argv[1]) == "edited") return runEdited();
    // Quaternion construction round-trips an arbitrary orthonormal basis.
    XrVector3f z = normalize({0.3f, -0.5f, 0.8f}), up{0, 1, 0};
    XrVector3f y = normalize(sub(up, scale(z, dot(up, z)))), x = cross(y, z);
    XrQuaternionf q = fromAxes(x, y, z);
    assert(angleDeg(rotate(q, {1, 0, 0}), x) < 0.1f && angleDeg(rotate(q, {0, 1, 0}), y) < 0.1f && angleDeg(rotate(q, {0, 0, 1}), z) < 0.1f);

    origGetHandMesh = mockGetHandMesh; origLocateHandJoints = mockLocate; origCreateHandTracker = mockCreateTracker;
    XrHandTrackerEXT tracker = XR_NULL_HANDLE;
    XrHandTrackerCreateInfoEXT create{XR_TYPE_HAND_TRACKER_CREATE_INFO_EXT, nullptr, XR_HAND_LEFT_EXT, XR_HAND_JOINT_SET_DEFAULT_EXT};
    assert(XR_SUCCEEDED(xrCreateHandTrackerEXT(XR_NULL_HANDLE, &create, &tracker)));

    // Without a live sample: defaults are used.
    checkRebuilt(queryMesh(), false);

    // With a live sample: radii and proportions follow the sample.
    XrHandJointLocationEXT joints[26];
    XrHandJointLocationsEXT locations{XR_TYPE_HAND_JOINT_LOCATIONS_EXT, nullptr, XR_FALSE, 26, joints};
    XrHandJointsLocateInfoEXT locate{XR_TYPE_HAND_JOINTS_LOCATE_INFO_EXT};
    assert(XR_SUCCEEDED(xrLocateHandJointsEXT(tracker, &locate, &locations)));
    {
        // Frame alignment: the wrist keeps its position and Z axis and takes its roll from the palm joint; the
        // little metacarpal keeps its bone axis and rolls 24 degrees toward the ulnar side; other joints are untouched.
        auto live = [](int j) { const float* l = kLive[j]; return XrPosef{{l[4], l[5], l[6], l[7]}, {l[1], l[2], l[3]}}; };
        const XrPosef& wrist = joints[1].pose;
        assert(length(sub(wrist.position, live(1).position)) < 1e-6f);
        assert(angleDeg(rotate(wrist.orientation, {0, 0, 1}), rotate(live(1).orientation, {0, 0, 1})) < 0.1f);
        XrVector3f z = rotate(wrist.orientation, {0, 0, 1}), palmUp = rotate(live(0).orientation, {0, 1, 0});
        XrVector3f expectedUp = normalize(sub(palmUp, scale(z, dot(palmUp, z))));
        assert(angleDeg(rotate(wrist.orientation, {0, 1, 0}), expectedUp) < 0.1f);
        float wristRoll = angleDeg(rotate(wrist.orientation, {0, 1, 0}), rotate(live(1).orientation, {0, 1, 0}));
        assert(wristRoll > 5.0f && wristRoll < 15.0f);  // captured hand: about 9 degrees
        const XrPosef& meta = joints[21].pose;
        assert(length(sub(meta.position, live(21).position)) < 1e-6f);
        assert(angleDeg(rotate(meta.orientation, {0, 0, -1}), sub(joints[22].pose.position, meta.position)) < 1.0f);
        float metaRoll = angleDeg(rotate(meta.orientation, {0, 1, 0}), rotate(wrist.orientation, {0, 1, 0}));
        assert(metaRoll > 20.0f && metaRoll < 28.0f);
        XrVector3f wristX = rotate(wrist.orientation, {1, 0, 0});
        assert(dot(rotate(meta.orientation, {0, 1, 0}), wristX) * dot(sub(joints[22].pose.position, wrist.position), wristX) > 0);
        for (int j : {0, 2, 7, 12, 22}) assert(angleDeg(rotate(joints[j].pose.orientation, {0, 1, 0}), rotate(live(j).orientation, {0, 1, 0})) < 0.1f);
    }
    checkRebuilt(queryMesh(), true);

    // A count query must pass through untouched.
    XrHandTrackingMeshFB counts{XR_TYPE_HAND_TRACKING_MESH_FB};
    assert(XR_SUCCEEDED(xrGetHandMeshFB(tracker, &counts)) && counts.jointCountOutput == 26);
    printf("hand skeleton rebuild: ok\n");
    return 0;
}
