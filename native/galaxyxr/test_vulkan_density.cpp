#include "vulkan_density.h"
#include <cassert>
#include <cstdio>
int main() {
    constexpr unsigned n=65;
    std::vector<uint8_t> original(n*n*2);
    for(unsigned y=0;y<n;++y) for(unsigned x=0;x<n;++x) {
        float r=std::hypot(float(x)-32,float(y)-32);
        auto v=uint8_t(std::lround(255*std::clamp(1-(r-12)/24,.25f,1.f)));
        original[(y*n+x)*2]=original[(y*n+x)*2+1]=v;
    }
    auto small=original, large=original, steep=original, dense=original;
    assert(gxr_density::tune(small.data(),n,n,.25f,4,.25f));
    assert(gxr_density::tune(large.data(),n,n,1,4,.25f));
    assert(gxr_density::tune(steep.data(),n,n,1,28,.25f));
    assert(gxr_density::tune(dense.data(),n,n,1,4,.5f));
    auto sharp=[](const auto& m){return std::count_if(m.begin(),m.end(),[](uint8_t v){return v>=250;});};
    assert(sharp(small)<sharp(large));
    assert(small[(32*n+32)*2]==255);
    for(size_t i=0;i<large.size();++i) {
        assert(steep[i]<=large[i]); assert(dense[i]>=large[i]);
        assert(small[i]>=64);
    }
    std::vector<uint8_t> full(n*n*2,255);
    assert(!gxr_density::tune(full.data(),n,n,.25f,28,.5f));
    assert(std::all_of(full.begin(),full.end(),[](auto x){return x==255;}));
    assert(!gxr_density::tune(full.data(),n,n,NAN,28,.5f));
    assert(gxr_density::tune(small.data(),n,n,.5f,0,.25f));
    assert(std::all_of(small.begin(),small.end(),[](auto x){return x==255;}));
    auto left=original,right=original;
    assert(gxr_density::gazeMap(left.data(),n,n,-.5f,.3f,true,.15f,28,.125f));
    assert(gxr_density::gazeMap(right.data(),n,n,.5f,.3f,true,.15f,28,.125f));
    assert(left[(22*n+16)*2]>right[(22*n+16)*2]);
    assert(right[(22*n+48)*2]>left[(22*n+48)*2]);
    assert(gxr_density::gazeMap(left.data(),n,n,NAN,0,true,.15f,28,.125f));
    assert(std::all_of(left.begin(),left.end(),[](auto x){return x==255;}));
    // Production gaze map: each control must change the intended property.
    auto base=original, wide=original, slow=original, floor=original, down=original;
    gxr_density::gazeMap(base.data(),n,n,0,.5f,true,.15f,28,.125f);
    gxr_density::gazeMap(wide.data(),n,n,0,.5f,true,.5f,28,.125f);
    gxr_density::gazeMap(slow.data(),n,n,0,.5f,true,.15f,4,.125f);
    gxr_density::gazeMap(floor.data(),n,n,0,.5f,true,.15f,28,.5f);
    gxr_density::gazeMap(down.data(),n,n,0,-.5f,true,.15f,28,.125f);
    assert(sharp(wide)>sharp(base));
    assert(base[(16*n+32)*2]>down[(16*n+32)*2]);
    assert(down[(48*n+32)*2]>base[(48*n+32)*2]);
    bool gainChanged=false, densityChanged=false;
    for(size_t i=0;i<base.size();++i) {
        assert(slow[i]>=base[i]); assert(floor[i]>=base[i]);
        gainChanged|=slow[i]>base[i]; densityChanged|=floor[i]>base[i];
    }
    assert(gainChanged && densityChanged);
    puts("PASS: area shrinks plateau, gain steepens falloff, density bounds map, invalid/uniform fallback");
}
