// SPDX-License-Identifier: GPL-3.0-or-later
#include "fps_metrics.h"
#include <cassert>
#include <cmath>
#include <cstdio>

static void near(double actual, double expected) { assert(std::fabs(actual - expected) < 0.00001); }
int main() {
    GxrFpsMetrics m;
    assert(!m.valid());
    // 90 accepted projection submissions across exactly one second. The first
    // sample anchors the window; 90 intervals, not 91 timestamps, determine FPS.
    uint64_t start = 1000000000ULL;
    assert(!m.sample(start, true));
    for (uint64_t i = 1; i <= 90; ++i) {
        bool changed = m.sample(start + (i * 1000000000ULL) / 90, true);
        assert(changed == (i == 90));
    }
    assert(m.valid()); near(m.fps(), 90);
    // A failed end-frame, zero-layer, quad-only, or shouldRender=false call is
    // excluded by the caller and must not inflate the window's accepted count.
    for (uint64_t i = 1; i <= 90; ++i) {
        m.sample(start + 1000000000ULL + i * 10000000ULL, false);
    }
    for (uint64_t i = 1; i <= 45; ++i)
        m.sample(start + 1000000000ULL + (i * 1000000000ULL) / 45, true);
    near(m.fps(), 45);
    // More than two seconds without accepted projection frames resets stale
    // windows rather than averaging a menu/pause into new gameplay.
    assert(!m.sample(start + 6000000000ULL, true)); assert(!m.valid());
    for (uint64_t i = 1; i <= 72; ++i)
        m.sample(start + 6000000000ULL + (i * 1000000000ULL) / 72, true);
    near(m.fps(), 72);
    m.reset(); assert(!m.valid());
    // Backward/duplicate clocks cannot produce division by zero or old values.
    m.sample(start, true); m.sample(start, true); assert(!m.valid());
    m.sample(start - 1, true); assert(!m.valid());
    // Irregular cadence uses actual elapsed time, not nominal refresh rate.
    m.reset(); m.sample(start, true);
    for (uint64_t i = 1; i <= 60; ++i)
        m.sample(start + i * 20000000ULL, true);
    near(m.fps(), 50);
    m.reset(); m.sample(0, true);
    for (uint64_t i = 1; i <= 60; ++i) m.sample((i * 1000000000ULL) / 60, true);
    near(m.fps(), 60);
    GxrFpsFrameLatch frames;
    frames.wait(true); frames.begin(); frames.wait(false);
    assert(frames.finish()); // Wait(N+1) cannot change begun frame N.
    frames.begin(); assert(!frames.finish());
    assert(!frames.finish()); // duplicate end has no matching begin
    frames.begin(); assert(!frames.finish()); // begin without wait fails closed
    frames.wait(true); frames.wait(false); frames.begin();
    frames.begin(); assert(!frames.finish()); // previous begun frame discarded
    frames.reset();
    for (int i = 0; i < 9; ++i) frames.wait(true);
    frames.begin(); assert(!frames.finish()); // overflow never guesses a frame
    frames.reset(); frames.wait(true); frames.begin(); assert(frames.finish());
    puts("PASS: submission FPS 90/72/45, exclusions, pauses, clocks and irregular cadence");
}
