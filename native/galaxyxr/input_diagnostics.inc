// Optional local diagnostic build; no input event logging in release builds.
static std::mutex inputMutex;
static std::unordered_map<XrAction, std::string> actionNames;
static std::unordered_map<std::string, uint32_t> booleanStates;
static std::string pathName(XrInstance instance, XrPath path) {
    auto fn = symbol<PFN_xrPathToString>("xrPathToString");
    char buffer[XR_MAX_PATH_LENGTH]{};
    uint32_t size = 0;
    return fn && XR_SUCCEEDED(fn(instance, path, sizeof(buffer), &size, buffer)) ? buffer : "<unknown>";
}
extern "C" XRAPI_ATTR XrResult XRAPI_CALL xrSuggestInteractionProfileBindings(XrInstance instance, const XrInteractionProfileSuggestedBinding* info) {
    auto fn = symbol<PFN_xrSuggestInteractionProfileBindings>("xrSuggestInteractionProfileBindings");
    auto result = fn ? fn(instance, info) : XR_ERROR_FUNCTION_UNSUPPORTED;
    if (info) LOG("INPUT profile=%s result=%d count=%u", pathName(instance, info->interactionProfile).c_str(), result, info->countSuggestedBindings);
    return result;
}
extern "C" XRAPI_ATTR XrResult XRAPI_CALL xrGetActionStateBoolean(XrSession session, const XrActionStateGetInfo* info, XrActionStateBoolean* state) {
    auto fn = symbol<PFN_xrGetActionStateBoolean>("xrGetActionStateBoolean");
    auto result = fn ? fn(session, info, state) : XR_ERROR_FUNCTION_UNSUPPORTED;
    if (XR_SUCCEEDED(result)) {
        std::lock_guard<std::mutex> lock(inputMutex);
        auto key = std::to_string(reinterpret_cast<uintptr_t>(info->action)) + ":" + std::to_string(info->subactionPath);
        uint32_t bits = (state->isActive << 1) | state->currentState;
        auto it = booleanStates.find(key);
        if (it == booleanStates.end() || it->second != bits) {
            LOG("INPUT boolean %s sub=%llu active=%u state=%u changed=%u", actionNames[info->action].c_str(), static_cast<unsigned long long>(info->subactionPath), state->isActive, state->currentState, state->changedSinceLastSync);
            booleanStates[key] = bits;
        }
    }
    return result;
}
// Read the original Quest OVRPlugin's cached controller state after sync. No hooks,
// code modification, extra threads or fabricated input; the next frame exposes transitions.
extern "C" XRAPI_ATTR XrResult XRAPI_CALL xrSyncActions(XrSession session, const XrActionsSyncInfo* info) {
    auto fn = symbol<PFN_xrSyncActions>("xrSyncActions");
    auto result = fn ? fn(session, info) : XR_ERROR_FUNCTION_UNSUPPORTED;
    using GetState = int (*)(uint32_t, void*);
    static GetState get = [] {
        Dl_info self{};
        if (!dladdr(reinterpret_cast<void*>(&backend), &self) || !self.dli_fname) return static_cast<GetState>(nullptr);
        std::string path(self.dli_fname); path.resize(path.find_last_of('/') + 1);
        void* handle = dlopen((path + "libOVRPlugin.so").c_str(), RTLD_NOW | RTLD_NOLOAD);
        auto function = handle ? reinterpret_cast<GetState>(dlsym(handle, "ovrp_GetControllerState4")) : nullptr;
        LOG("INPUT OVRPlugin state observer available=%d", function != nullptr);
        return function;
    }();
    if (get && XR_SUCCEEDED(result)) {
        alignas(16) uint32_t state[24]{};
        if (get(3, state) == 0) {
            static uint32_t lastButtons = ~0u, lastConnected = ~0u;
            if (state[0] != lastConnected || state[1] != lastButtons) {
                LOG("INPUT OVRPlugin connected=%08x buttons=%08x", state[0], state[1]);
                lastConnected = state[0]; lastButtons = state[1];
            }
        }
    }
    return result;
}
static void logInputAction(const XrActionCreateInfo* info, XrAction action) {
    std::lock_guard<std::mutex> lock(inputMutex);
    actionNames[action] = info->actionName;
    LOG("INPUT action %p name=%s type=%d", action, info->actionName, info->actionType);
}
extern "C" XRAPI_ATTR XrResult XRAPI_CALL xrGetCurrentInteractionProfile(XrSession session, XrPath topLevel, XrInteractionProfileState* state) {
    auto fn = symbol<PFN_xrGetCurrentInteractionProfile>("xrGetCurrentInteractionProfile");
    auto result = fn ? fn(session, topLevel, state) : XR_ERROR_FUNCTION_UNSUPPORTED;
    if (XR_SUCCEEDED(result)) {
        XrInstance instance = XR_NULL_HANDLE;
        { std::lock_guard<std::mutex> lock(stateMutex); auto it = sessions.find(session); if (it != sessions.end()) instance = it->second.instance; }
        LOG("INPUT current %s -> %s", pathName(instance, topLevel).c_str(), pathName(instance, state->interactionProfile).c_str());
    }
    return result;
}
