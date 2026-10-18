// Copyright 2017-2026, The Khronos Group Inc.
// SPDX-License-Identifier: Apache-2.0 OR MIT
// Minimal XR_ANDROID_eye_tracking ABI from Khronos OpenXR-SDK.
// https://github.com/KhronosGroup/OpenXR-SDK/blob/main/include/openxr/openxr.h
#pragma once
#define XR_TYPE_EYES_ANDROID static_cast<XrStructureType>(1000456000)
#define XR_TYPE_EYE_TRACKER_CREATE_INFO_ANDROID static_cast<XrStructureType>(1000456001)
#define XR_TYPE_EYES_GET_INFO_ANDROID static_cast<XrStructureType>(1000456002)
#define XR_TYPE_SYSTEM_EYE_TRACKING_PROPERTIES_ANDROID static_cast<XrStructureType>(1000456004)
// XR_ANDROID_eye_tracking is a preprocessor guard. Do not pass it to API calls.
#define XR_ANDROID_eye_tracking 1
XR_DEFINE_HANDLE(XrEyeTrackerANDROID)
#define XR_EYE_MAX_ANDROID                2
#define XR_ANDROID_eye_tracking_SPEC_VERSION 1
#define XR_ANDROID_EYE_TRACKING_EXTENSION_NAME "XR_ANDROID_eye_tracking"

typedef enum XrEyeIndexANDROID {
    XR_EYE_INDEX_LEFT_ANDROID = 0,
    XR_EYE_INDEX_RIGHT_ANDROID = 1,
    XR_EYE_INDEX_MAX_ENUM_ANDROID = 0x7FFFFFFF
} XrEyeIndexANDROID;

typedef enum XrEyeStateANDROID {
    XR_EYE_STATE_INVALID_ANDROID = 0,
    XR_EYE_STATE_GAZING_ANDROID = 1,
    XR_EYE_STATE_SHUT_ANDROID = 2,
    XR_EYE_STATE_MAX_ENUM_ANDROID = 0x7FFFFFFF
} XrEyeStateANDROID;

typedef enum XrEyeTrackingModeANDROID {
    XR_EYE_TRACKING_MODE_NOT_TRACKING_ANDROID = 0,
    XR_EYE_TRACKING_MODE_RIGHT_ANDROID = 1,
    XR_EYE_TRACKING_MODE_LEFT_ANDROID = 2,
    XR_EYE_TRACKING_MODE_BOTH_ANDROID = 3,
    XR_EYE_TRACKING_MODE_MAX_ENUM_ANDROID = 0x7FFFFFFF
} XrEyeTrackingModeANDROID;
// XrSystemEyeTrackingPropertiesANDROID extends XrSystemProperties
typedef struct XrSystemEyeTrackingPropertiesANDROID {
    XrStructureType       type;
    void* XR_MAY_ALIAS    next;
    XrBool32              supportsEyeTracking;
} XrSystemEyeTrackingPropertiesANDROID;

typedef struct XrEyeANDROID {
    XrEyeStateANDROID    eyeState;
    XrPosef              eyePose;
} XrEyeANDROID;

typedef struct XrEyesANDROID {
    XrStructureType             type;
    void* XR_MAY_ALIAS          next;
    XrEyeANDROID                eyes[XR_EYE_MAX_ANDROID];
    XrEyeTrackingModeANDROID    mode;
} XrEyesANDROID;

typedef struct XrEyesGetInfoANDROID {
    XrStructureType             type;
    const void* XR_MAY_ALIAS    next;
    XrTime                      time;
    XrSpace                     baseSpace;
} XrEyesGetInfoANDROID;

typedef struct XrEyeTrackerCreateInfoANDROID {
    XrStructureType             type;
    const void* XR_MAY_ALIAS    next;
} XrEyeTrackerCreateInfoANDROID;

typedef XrResult (XRAPI_PTR *PFN_xrCreateEyeTrackerANDROID)(XrSession session, const XrEyeTrackerCreateInfoANDROID* createInfo, XrEyeTrackerANDROID* eyeTracker);
typedef XrResult (XRAPI_PTR *PFN_xrDestroyEyeTrackerANDROID)(XrEyeTrackerANDROID eyeTracker);
typedef XrResult (XRAPI_PTR *PFN_xrGetFineTrackingEyesInfoANDROID)(XrEyeTrackerANDROID eyeTracker, const XrEyesGetInfoANDROID* getInfo, XrEyesANDROID* eyesOutput);
typedef XrResult (XRAPI_PTR *PFN_xrGetCoarseTrackingEyesInfoANDROID)(XrEyeTrackerANDROID eyeTracker, const XrEyesGetInfoANDROID* getInfo, XrEyesANDROID* eyesOutput);
