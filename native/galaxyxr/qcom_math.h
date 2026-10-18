// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cmath>
// Rotate a gaze direction into the eye view, then project through its asymmetric FOV.
static bool qcomProject(const XrQuaternionf& gaze, const XrView& view, float& x, float& y) {
    const auto& v = view.pose.orientation;
    XrQuaternionf q{
        v.w*gaze.x-v.x*gaze.w-v.y*gaze.z+v.z*gaze.y,
        v.w*gaze.y+v.x*gaze.z-v.y*gaze.w-v.z*gaze.x,
        v.w*gaze.z-v.x*gaze.y+v.y*gaze.x-v.z*gaze.w,
        v.w*gaze.w+v.x*gaze.x+v.y*gaze.y+v.z*gaze.z};
    float norm = q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w;
    if (!std::isfinite(norm) || std::abs(norm-1.f) > .02f) return false;
    float dx = -2.f*(q.x*q.z+q.w*q.y), dy = 2.f*(q.w*q.x-q.y*q.z);
    float dz = -(1.f-2.f*(q.x*q.x+q.y*q.y));
    float l=std::tan(view.fov.angleLeft), r=std::tan(view.fov.angleRight);
    float b=std::tan(view.fov.angleDown), t=std::tan(view.fov.angleUp);
    if (dz >= -.01f || !(r>l) || !(t>b)) return false;
    x = 2.f*(dx/-dz-l)/(r-l)-1.f;
    y = 2.f*(dy/-dz-b)/(t-b)-1.f;
    return std::isfinite(x) && std::isfinite(y) && std::abs(x)<=1.f && std::abs(y)<=1.f;
}
