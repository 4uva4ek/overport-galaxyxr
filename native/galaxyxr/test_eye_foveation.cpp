// Focused lifecycle test for injected eye profiles and app-released profiles.
#define GXR_POLICY_TEST
#include "policy.cpp"
#include <cassert>
static int destroyed = 0, updated = 0, queried = 0, created = 0;
static bool rejectEye = false;
static bool rejectSwapchain = false;
static int swapchainCreates = 0;
static XrResult createSwapchain(XrSession, const XrSwapchainCreateInfo* info, XrSwapchain* swapchain) {
    ++swapchainCreates;
    if (info->next && rejectSwapchain) return XR_ERROR_FEATURE_UNSUPPORTED;
    *swapchain = reinterpret_cast<XrSwapchain>(22);
    return XR_SUCCESS;
}
static XrResult createProfile(XrSession, const XrFoveationProfileCreateInfoFB* info, XrFoveationProfileFB* profile) {
    ++created;
    auto level = static_cast<const XrFoveationLevelProfileCreateInfoFB*>(info->next);
    if (level && level->next) {
        assert(static_cast<const XrBaseInStructure*>(level->next)->type == XR_TYPE_FOVEATION_EYE_TRACKED_PROFILE_CREATE_INFO_META);
        if (rejectEye) return XR_ERROR_PERMISSION_INSUFFICIENT;
    }
    *profile = reinterpret_cast<XrFoveationProfileFB>(10 + created);
    return XR_SUCCESS;
}
static XrResult destroyProfile(XrFoveationProfileFB) { ++destroyed; return XR_SUCCESS; }
static XrResult update(XrSwapchain, const XrSwapchainStateBaseHeaderFB*) { ++updated; return XR_SUCCESS; }
static XrResult eyeState(XrSession, XrFoveationEyeTrackedStateMETA* state) { ++queried; state->flags = XR_FOVEATION_EYE_TRACKED_STATE_VALID_BIT_META; return XR_SUCCESS; }
static XrResult waitFrame(XrSession, const XrFrameWaitInfo*, XrFrameState*) { return XR_SUCCESS; }
static XrResult destroySwapchain(XrSwapchain) { return XR_SUCCESS; }
static void* testSymbol(const char* name) {
    if (!strcmp(name, "xrCreateSwapchain")) return reinterpret_cast<void*>(&createSwapchain);
    if (!strcmp(name, "xrWaitFrame")) return reinterpret_cast<void*>(&waitFrame);
    if (!strcmp(name, "xrDestroySwapchain")) return reinterpret_cast<void*>(&destroySwapchain);
    return nullptr;
}
int main() {
    testConfig = "performance=max\nfoveation=eye\n";
    auto session = reinterpret_cast<XrSession>(1);
    auto swapchain = reinterpret_cast<XrSwapchain>(2);
    Policy p;
    p.eye = true; p.createFoveation = createProfile; p.destroyFoveation = destroyProfile;
    p.updateSwapchain = update; p.eyeState = eyeState;
    sessions[session] = {XR_NULL_HANDLE, p}; eyeSwapchains[swapchain] = {session};
    XrFoveationLevelProfileCreateInfoFB level{XR_TYPE_FOVEATION_LEVEL_PROFILE_CREATE_INFO_FB, nullptr, XR_FOVEATION_LEVEL_HIGH_FB, 0, XR_FOVEATION_DYNAMIC_DISABLED_FB};
    XrFoveationProfileCreateInfoFB info{XR_TYPE_FOVEATION_PROFILE_CREATE_INFO_FB, &level};
    XrFoveationProfileFB profile;
    assert(xrCreateFoveationProfileFB(session, &info, &profile) == XR_SUCCESS);
    assert(level.next == nullptr && eyeProfiles.at(profile).eye);
    XrSwapchainStateFoveationFB state{XR_TYPE_SWAPCHAIN_STATE_FOVEATION_FB, nullptr, 0, profile};
    assert(xrUpdateSwapchainFB(swapchain, reinterpret_cast<XrSwapchainStateBaseHeaderFB*>(&state)) == XR_SUCCESS);
    assert(xrDestroyFoveationProfileFB(profile) == XR_SUCCESS && destroyed == 0);
    assert(xrWaitFrame(session, nullptr, nullptr) == XR_SUCCESS && updated == 2 && queried == 1);
    assert(xrDestroySwapchain(swapchain) == XR_SUCCESS && destroyed == 1 && eyeProfiles.empty());
    rejectEye = true;
    assert(xrCreateFoveationProfileFB(session, &info, &profile) == XR_SUCCESS && created == 3);
    assert(!eyeProfiles.at(profile).eye);
    assert(xrDestroyFoveationProfileFB(profile) == XR_SUCCESS && destroyed == 2);
    level.level = XR_FOVEATION_LEVEL_NONE_FB;
    assert(xrCreateFoveationProfileFB(session, &info, &profile) == XR_SUCCESS && created == 4);
    assert(!eyeProfiles.at(profile).eye);
    forgetEyeSession(session);
    assert(eyeProfiles.empty() && eyeSwapchains.empty());
    rejectEye = false;
    sessions[session].policy.gles = true;
    XrSwapchainCreateInfo chainInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};
    chainInfo.arraySize = 2; chainInfo.width = 3552; chainInfo.height = 3840;
    chainInfo.faceCount = chainInfo.mipCount = 1;
    chainInfo.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
    assert(xrCreateSwapchain(session, &chainInfo, &swapchain) == XR_SUCCESS);
    assert(chainInfo.next == nullptr && eyeSwapchains.at(swapchain).forced);
    assert(xrWaitFrame(session, nullptr, nullptr) == XR_SUCCESS);
    // App off profile must not undo an explicit eye-mode profile.
    state.profile = XR_NULL_HANDLE;
    auto retained = eyeSwapchains.at(swapchain).profile;
    assert(xrUpdateSwapchainFB(swapchain, reinterpret_cast<XrSwapchainStateBaseHeaderFB*>(&state)) == XR_SUCCESS);
    assert(eyeSwapchains.at(swapchain).profile == retained);
    assert(xrDestroySwapchain(swapchain) == XR_SUCCESS && eyeProfiles.empty());
    rejectSwapchain = true;
    int before = swapchainCreates;
    assert(xrCreateSwapchain(session, &chainInfo, &swapchain) == XR_SUCCESS && swapchainCreates == before + 2);
    assert(!eyeSwapchains.at(swapchain).forced);
    assert(xrDestroySwapchain(swapchain) == XR_SUCCESS);
    rejectSwapchain = false; rejectEye = true;
    assert(xrCreateSwapchain(session, &chainInfo, &swapchain) == XR_SUCCESS && !eyeSwapchains.at(swapchain).forced);
    assert(xrDestroySwapchain(swapchain) == XR_SUCCESS);
    rejectEye = false; chainInfo.usageFlags = XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    assert(xrCreateSwapchain(session, &chainInfo, &swapchain) == XR_SUCCESS && !eyeSwapchains.at(swapchain).forced);
    assert(xrDestroySwapchain(swapchain) == XR_SUCCESS);
    // Vulkan: only adopt existing density maps. Never add an attachment contract
    // to a game whose render passes were created without one.
    sessions[session].policy.gles = false;
    sessions[session].policy.vulkan = true;
    chainInfo.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
    int profilesBefore = created;
    assert(xrCreateSwapchain(session, &chainInfo, &swapchain) == XR_SUCCESS);
    assert(!eyeSwapchains.at(swapchain).forced && created == profilesBefore);
    assert(xrDestroySwapchain(swapchain) == XR_SUCCESS);
    XrSwapchainCreateInfoFoveationFB density{XR_TYPE_SWAPCHAIN_CREATE_INFO_FOVEATION_FB, nullptr,
        XR_SWAPCHAIN_CREATE_FOVEATION_FRAGMENT_DENSITY_MAP_BIT_FB};
    chainInfo.next = &density;
    rejectEye = false;
    testConfig = "performance=max\nfoveation=runtime-eye\n";
    assert(xrCreateSwapchain(session, &chainInfo, &swapchain) == XR_SUCCESS);
    assert(chainInfo.next == &density && density.next == nullptr && eyeSwapchains.at(swapchain).forced);
    assert(eyeSwapchains.at(swapchain).alternate);
    eyeAcquire(swapchain,0);
    eyePrepareWait(swapchain);
    assert(eyeSwapchains.at(swapchain).imageAlternate.at(0));
    eyeRelease(swapchain);
    eyeAcquire(swapchain,0);
    eyePrepareWait(swapchain);
    assert(!eyeSwapchains.at(swapchain).imageAlternate.at(0));
    eyeRelease(swapchain);
    int queriesBefore = queried, updatesBefore = updated;
    assert(xrWaitFrame(session, nullptr, nullptr) == XR_SUCCESS);
    assert(queried == queriesBefore + 1 && updated == updatesBefore + 1);
    assert(xrDestroySwapchain(swapchain) == XR_SUCCESS && eyeProfiles.empty());
    density.flags = XR_SWAPCHAIN_CREATE_FOVEATION_SCALED_BIN_BIT_FB;
    assert(xrCreateSwapchain(session, &chainInfo, &swapchain) == XR_SUCCESS && !eyeSwapchains.at(swapchain).forced);
    assert(xrDestroySwapchain(swapchain) == XR_SUCCESS);
    density.flags = XR_SWAPCHAIN_CREATE_FOVEATION_FRAGMENT_DENSITY_MAP_BIT_FB;
    sessions[session].policy.eye = false;
    profilesBefore = created;
    assert(xrCreateSwapchain(session, &chainInfo, &swapchain) == XR_SUCCESS && !eyeSwapchains.at(swapchain).forced);
    assert(created == profilesBefore);
    assert(xrDestroySwapchain(swapchain) == XR_SUCCESS);
    puts("PASS: immutable chain, eye update, deferred release, permission fallback, off profile, cleanup");
}
