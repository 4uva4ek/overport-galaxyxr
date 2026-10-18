// App-local postprocessing of the runtime's R8G8 fragment-density map.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>
namespace gxr_density {
inline bool gazeMap(uint8_t* map,uint32_t width,uint32_t height,float x,float y,bool valid,float area,float gain,float density) {
    if(!map || width<2 || height<2 || width>512 || height>512) return false;
    if(!valid || !std::isfinite(x) || !std::isfinite(y) || std::abs(x)>1 || std::abs(y)>1 ||
       !std::isfinite(area) || !std::isfinite(gain) || !std::isfinite(density) ||
       area<0 || area>2 || gain<0 || gain>32 || density<.125f || density>1 || gain==0 || density==1) {
        std::fill(map,map+size_t(width)*height*2,255); return true;
    }
    // NDC is up-positive; Vulkan image rows are down-positive.
    float radius=std::max(.01f,area*.5f);
    for(uint32_t row=0;row<height;++row) for(uint32_t col=0;col<width;++col) {
        float dx=(2.f*(col+.5f)/width-1)-x;
        float dy=(1-2.f*(row+.5f)/height)-y;
        float outside=std::max(0.f,std::hypot(dx,dy)-radius);
        float value=std::max(density,1.f/(1.f+gain*outside));
        auto v=uint8_t(std::lround(255.f*value));
        map[(size_t(row)*width+col)*2]=map[(size_t(row)*width+col)*2+1]=v;
    }
    return true;
}
// Preserve the runtime's focal location and its separate gaze-offset mechanism.
// A uniform map is a runtime fallback: never manufacture a focal region from it.
inline bool tune(uint8_t* map, uint32_t width, uint32_t height,
                 float area, float gain, float density) {
    if (!map || width < 2 || height < 2 || width > 512 || height > 512 ||
        !std::isfinite(area) || !std::isfinite(gain) || !std::isfinite(density) ||
        area < 0 || area > 2 || gain < 0 || gain > 32 || density < .125f || density > 1) return false;
    const size_t count = size_t(width) * height;
    if (gain == 0 || density == 1) { std::fill(map, map + count * 2, 255); return true; }
    unsigned low[2]{255,255}, high[2]{};
    double sumX=0, sumY=0, weight=0;
    for (size_t i=0;i<count;++i) for(unsigned c=0;c<2;++c) {
        low[c]=std::min(low[c],unsigned(map[2*i+c]));
        high[c]=std::max(high[c],unsigned(map[2*i+c]));
    }
    if (high[0]-low[0]<8 || high[1]-low[1]<8 || high[0]<250 || high[1]<250) return false;
    // Locate the native full-quality plateau independently for each layer.
    for (size_t i=0;i<count;++i) if(map[2*i]>=250 && map[2*i+1]>=250) {
        sumX+=i%width; sumY+=i/width; ++weight;
    }
    if (!weight) return false;
    float cx=float(sumX/weight), cy=float(sumY/weight);
    float scale=std::sqrt(std::max(area,.01f));
    float exponent=std::max(gain/4.f,.0125f);
    std::vector<uint8_t> source(map,map+count*2);
    for(uint32_t y=0;y<height;++y) for(uint32_t x=0;x<width;++x) {
        float sx=cx+(float(x)-cx)/scale, sy=cy+(float(y)-cy)/scale;
        for(unsigned c=0;c<2;++c) {
            float t=0;
            if(sx>=0 && sy>=0 && sx<=width-1 && sy<=height-1) {
                unsigned ix=unsigned(sx), iy=unsigned(sy);
                unsigned jx=std::min(ix+1,width-1), jy=std::min(iy+1,height-1);
                float fx=sx-ix, fy=sy-iy;
                auto at=[&](unsigned xx,unsigned yy){return float(source[(size_t(yy)*width+xx)*2+c]);};
                float v=(at(ix,iy)*(1-fx)+at(jx,iy)*fx)*(1-fy)+(at(ix,jy)*(1-fx)+at(jx,jy)*fx)*fy;
                t=std::clamp((v-low[c])/float(high[c]-low[c]),0.f,1.f);
            }
            map[(size_t(y)*width+x)*2+c]=uint8_t(std::lround(255.f*(density+(1-density)*std::pow(t,exponent))));
        }
    }
    return true;
}
}
