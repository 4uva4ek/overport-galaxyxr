// Optional app-local eye foveation. Preserve the application's level and off profiles.
// Keep eye profiles alive internally for per-frame updates after the app releases them.
struct EyeProfile { XrSession session; bool eye; bool released = false; };
struct EyeSwapchain { XrSession session; XrFoveationProfileFB profile = XR_NULL_HANDLE; bool forced = false; };
static std::unordered_map<XrFoveationProfileFB, EyeProfile> eyeProfiles;
static std::unordered_map<XrSwapchain, EyeSwapchain> eyeSwapchains;

static void forgetEyeSession(XrSession session) { // stateMutex held; runtime already destroyed children.
    for (auto it = eyeProfiles.begin(); it != eyeProfiles.end();)
        if (it->second.session == session) it = eyeProfiles.erase(it); else ++it;
    for (auto it = eyeSwapchains.begin(); it != eyeSwapchains.end();)
        if (it->second.session == session) it = eyeSwapchains.erase(it); else ++it;
}

static void collectEyeProfile(XrFoveationProfileFB profile) { // stateMutex held
    auto it = eyeProfiles.find(profile);
    if (it == eyeProfiles.end() || !it->second.released) return;
    for (const auto& entry : eyeSwapchains) if (entry.second.profile == profile) return;
    auto session = sessions.find(it->second.session);
    if (session != sessions.end() && session->second.policy.destroyFoveation)
        session->second.policy.destroyFoveation(profile);
    eyeProfiles.erase(it);
}

extern "C" XRAPI_ATTR XrResult XRAPI_CALL xrCreateFoveationProfileFB(XrSession session, const XrFoveationProfileCreateInfoFB* info, XrFoveationProfileFB* profile) {
    auto policy = sessionPolicy(session);
    if (!policy.createFoveation) return XR_ERROR_FUNCTION_UNSUPPORTED;
    bool eye = false;
    XrResult result;
    auto level = info ? static_cast<const XrFoveationLevelProfileCreateInfoFB*>(info->next) : nullptr;
    // Unknown chains and existing eye profiles remain entirely application-controlled.
    if (policy.eye && level && level->type == XR_TYPE_FOVEATION_LEVEL_PROFILE_CREATE_INFO_FB && !level->next && level->level != XR_FOVEATION_LEVEL_NONE_FB) {
        XrFoveationEyeTrackedProfileCreateInfoMETA eyeInfo{XR_TYPE_FOVEATION_EYE_TRACKED_PROFILE_CREATE_INFO_META};
        auto levelCopy = *level;
        levelCopy.next = &eyeInfo;
        auto copy = *info;
        copy.next = &levelCopy;
        result = policy.createFoveation(session, &copy, profile);
        eye = XR_SUCCEEDED(result);
        LOG("Eye foveation profile level=%d dynamic=%d result=%d", level->level, level->dynamic, result);
        if (!eye) result = policy.createFoveation(session, info, profile);
    } else result = policy.createFoveation(session, info, profile);
    if (XR_SUCCEEDED(result) && profile) {
        std::lock_guard<std::mutex> lock(stateMutex);
        eyeProfiles[*profile] = {session, eye};
    }
    return result;
}

extern "C" XRAPI_ATTR XrResult XRAPI_CALL xrDestroyFoveationProfileFB(XrFoveationProfileFB profile) {
    std::lock_guard<std::mutex> lock(stateMutex);
    auto it = eyeProfiles.find(profile);
    if (it == eyeProfiles.end() || it->second.released) return XR_ERROR_HANDLE_INVALID;
    auto session = sessions.find(it->second.session);
    if (session == sessions.end() || !session->second.policy.destroyFoveation) return XR_ERROR_HANDLE_INVALID;
    if (it->second.eye) {
        it->second.released = true;
        collectEyeProfile(profile);
        return XR_SUCCESS;
    }
    auto result = session->second.policy.destroyFoveation(profile);
    if (XR_SUCCEEDED(result)) eyeProfiles.erase(it);
    return result;
}

extern "C" XRAPI_ATTR XrResult XRAPI_CALL xrCreateSwapchain(XrSession session, const XrSwapchainCreateInfo* info, XrSwapchain* swapchain) {
    auto fn = symbol<PFN_xrCreateSwapchain>("xrCreateSwapchain");
    if (!fn) return XR_ERROR_FUNCTION_UNSUPPORTED;
    auto policy = sessionPolicy(session);
    // Explicit eye mode also handles runtimes that consume the app's foveation
    // calls internally. Limit this direct path to stereo GLES color arrays;
    // depth, overlays, static images and Vulkan keep the application path.
    bool direct = policy.eye && policy.gles && info && info->arraySize == 2 &&
        info->faceCount == 1 && info->mipCount == 1 && info->width >= 512 && info->height >= 512 &&
        (info->usageFlags & XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT) &&
        !(info->createFlags & XR_SWAPCHAIN_CREATE_STATIC_IMAGE_BIT);
    bool existing = false;
    if (info) for (auto node = static_cast<const XrBaseInStructure*>(info->next); node; node = node->next)
        if (node->type == XR_TYPE_SWAPCHAIN_CREATE_INFO_FOVEATION_FB) existing = true;
    XrSwapchainCreateInfoFoveationFB foveation{XR_TYPE_SWAPCHAIN_CREATE_INFO_FOVEATION_FB,
        info ? const_cast<void*>(info->next) : nullptr, XR_SWAPCHAIN_CREATE_FOVEATION_SCALED_BIN_BIT_FB};
    XrSwapchainCreateInfo copy{};
    if (info) copy = *info;
    if (direct && !existing) copy.next = &foveation;
    auto result = fn(session, direct ? &copy : info, swapchain);
    if (XR_FAILED(result) && direct && !existing) {
        LOG("Eye-capable swapchain refused=%d; retrying original", result);
        direct = false;
        result = fn(session, info, swapchain);
    }
    if (eyeFoveation() && info) LOG("Swapchain eye candidate=%d GLES=%d array=%u size=%ux%u foveationChain=%d result=%d",
        direct, policy.gles, info->arraySize, info->width, info->height, existing, result);
    if (XR_SUCCEEDED(result) && swapchain) {
        XrFoveationProfileFB profile = XR_NULL_HANDLE;
        if (direct) {
            XrFoveationEyeTrackedProfileCreateInfoMETA eye{XR_TYPE_FOVEATION_EYE_TRACKED_PROFILE_CREATE_INFO_META};
            XrFoveationLevelProfileCreateInfoFB level{XR_TYPE_FOVEATION_LEVEL_PROFILE_CREATE_INFO_FB,
                &eye, XR_FOVEATION_LEVEL_MEDIUM_FB, 0, XR_FOVEATION_DYNAMIC_DISABLED_FB};
            XrFoveationProfileCreateInfoFB create{XR_TYPE_FOVEATION_PROFILE_CREATE_INFO_FB, &level};
            auto created = policy.createFoveation(session, &create, &profile);
            LOG("Direct eye profile level=2 result=%d", created);
            if (XR_SUCCEEDED(created)) {
                XrSwapchainStateFoveationFB state{XR_TYPE_SWAPCHAIN_STATE_FOVEATION_FB, nullptr, 0, profile};
                auto applied = policy.updateSwapchain(*swapchain, reinterpret_cast<const XrSwapchainStateBaseHeaderFB*>(&state));
                LOG("Direct eye profile applied result=%d", applied);
                if (XR_FAILED(applied)) { policy.destroyFoveation(profile); profile = XR_NULL_HANDLE; }
            } else profile = XR_NULL_HANDLE;
        }
        std::lock_guard<std::mutex> lock(stateMutex);
        eyeSwapchains[*swapchain] = {session, profile, profile != XR_NULL_HANDLE};
        if (profile) eyeProfiles[profile] = {session, true, true};
    }
    return result;
}

extern "C" XRAPI_ATTR XrResult XRAPI_CALL xrUpdateSwapchainFB(XrSwapchain swapchain, const XrSwapchainStateBaseHeaderFB* state) {
    std::lock_guard<std::mutex> lock(stateMutex);
    auto chain = eyeSwapchains.find(swapchain);
    if (chain == eyeSwapchains.end()) return XR_ERROR_HANDLE_INVALID;
    auto session = sessions.find(chain->second.session);
    if (session == sessions.end() || !session->second.policy.updateSwapchain) return XR_ERROR_FUNCTION_UNSUPPORTED;
    XrSwapchainStateFoveationFB forced{XR_TYPE_SWAPCHAIN_STATE_FOVEATION_FB, nullptr, 0, chain->second.profile};
    bool override = chain->second.forced && state && !state->next && state->type == XR_TYPE_SWAPCHAIN_STATE_FOVEATION_FB;
    auto result = session->second.policy.updateSwapchain(swapchain, override ? reinterpret_cast<const XrSwapchainStateBaseHeaderFB*>(&forced) : state);
    if (override) return result;
    if (XR_SUCCEEDED(result) && state && state->type == XR_TYPE_SWAPCHAIN_STATE_FOVEATION_FB) {
        auto fov = reinterpret_cast<const XrSwapchainStateFoveationFB*>(state);
        auto profile = eyeProfiles.find(fov->profile);
        auto old = chain->second.profile;
        chain->second.profile = profile != eyeProfiles.end() && profile->second.eye && !state->next ? fov->profile : XR_NULL_HANDLE;
        if (chain->second.profile != old) {
            LOG("Eye foveation swapchain active=%d result=%d", chain->second.profile != XR_NULL_HANDLE, result);
            collectEyeProfile(old);
        }
    }
    return result;
}

extern "C" XRAPI_ATTR XrResult XRAPI_CALL xrDestroySwapchain(XrSwapchain swapchain) {
    auto fn = symbol<PFN_xrDestroySwapchain>("xrDestroySwapchain");
    auto result = fn ? fn(swapchain) : XR_ERROR_FUNCTION_UNSUPPORTED;
    if (XR_SUCCEEDED(result)) {
        std::lock_guard<std::mutex> lock(stateMutex);
        auto it = eyeSwapchains.find(swapchain);
        if (it != eyeSwapchains.end()) {
            auto profile = it->second.profile;
            eyeSwapchains.erase(it);
            collectEyeProfile(profile);
        }
    }
    return result;
}

extern "C" XRAPI_ATTR XrResult XRAPI_CALL xrWaitFrame(XrSession session, const XrFrameWaitInfo* info, XrFrameState* state) {
    auto fn = symbol<PFN_xrWaitFrame>("xrWaitFrame");
    auto result = fn ? fn(session, info, state) : XR_ERROR_FUNCTION_UNSUPPORTED;
    if (XR_SUCCEEDED(result)) gxrFpsOnWaitFrame(session, state);
    if (XR_FAILED(result) || !eyeFoveation()) return result;
    std::lock_guard<std::mutex> lock(stateMutex);
    auto entry = sessions.find(session);
    if (entry == sessions.end() || !entry->second.policy.eye) return result;
    auto& current = entry->second;
    bool active = false;
    XrResult update = XR_SUCCESS;
    for (auto& chain : eyeSwapchains) {
        if (chain.second.session != session || !chain.second.profile) continue;
        XrSwapchainStateFoveationFB fov{XR_TYPE_SWAPCHAIN_STATE_FOVEATION_FB, nullptr, 0, chain.second.profile};
        update = current.policy.updateSwapchain(chain.first, reinterpret_cast<const XrSwapchainStateBaseHeaderFB*>(&fov));
        active = true;
        if (XR_FAILED(update)) {
            LOG("Eye foveation frame update refused=%d; stopping additional updates", update);
            current.policy.eye = false;
            break;
        }
    }
    if (active) {
        XrFoveationEyeTrackedStateMETA gaze{XR_TYPE_FOVEATION_EYE_TRACKED_STATE_META};
        auto status = current.policy.eyeState(session, &gaze);
        if ((current.eyeFrames++ % 450) == 0)
            LOG("Eye foveation frame: update=%d state=%d valid=%d", update, status,
                XR_SUCCEEDED(status) && (gaze.flags & XR_FOVEATION_EYE_TRACKED_STATE_VALID_BIT_META) != 0);
    }
    return result;
}
