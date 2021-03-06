/*
FLIF - Free Lossless Image Format

Copyright 2010-2016, Jon Sneyers & Pieter Wuille

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/


#pragma once

#include "../image/image.hpp"
#include "../image/color_range.hpp"
#include "transform.hpp"
#include <algorithm>

#define clip(x,l,u)   if ((x) < (l)) {(x)=(l);} else if ((x) > (u)) {(x)=(u);}

/*
    Explanation of lossless YCoCg:

     Y = Luminance (near* weighted average of RGB in 1:2:1).
     C = Chroma.
    Co = Amount of [o]range chrome. Max = orange, 0 = gray, min = blue.
    Cg = Amount of [g]reen chrome. Max = green, 0 = gray, min = purple.

    RGB -> YCoCg
    ------------
    Co = R - B          <1> | This makes sense, if R = B, then we are gray.
                            | If maximally orange, B = 0, R = max.
                            | If maximally blue, B = max, R = 0.

     p = (R + B)/2          | [P]urple, is the average of red/blue, truncated.
     p = (2B + R - B)/2     | All these steps should be obviously
     p = B + (R - B)/2      | equal, losslessly.
     p = B + Co/2       <2>

    Cg = G - p          <3> | Again, makes sense, green vs. purple.

     Y = p + Cg/2       <4> | Near* weighted average of RGB in 1:2:1.
     Y = p + (G - p)/2
     Y ~= p + G/2 - p/2     | *These steps are not lossless (they can be off by
     Y ~= p/2 + G/2         | 1 or perhaps 2), but illustrate that Y is a near
     Y ~= (R + B)/4 + G/2   | weighted average of RGB in 1:2:1.

    YCoCg -> RGB
    ------------
    p = Y - Cg/2       <4>
    G = Cg + p         <3>
    B = p - Co/2       <2>
    R = Co + B         <1>
*/



ColorVal static inline get_min_y(int) {
    return 0;
}

ColorVal static inline get_max_y(int par) {
    return par*4-1;
}

ColorVal static inline get_min_co(int par, ColorVal y) {
    assert(y >= get_min_y(par));
    assert(y <= get_max_y(par));

    if (y<par-1) {
      return -4-4*y;
    } else if (y>=3*par) {
      return 3+4*(y-4*par);
    } else {
      return -4*par;
    }
}

ColorVal static inline get_max_co(int par, ColorVal y) {
    assert(y >= get_min_y(par));
    assert(y <= get_max_y(par));

    if (y<par-1) {
      return 2+4*y;
    } else if (y>=3*par) {
      return 4*par-5-4*(y-3*par);
    } else {
      return 4*par-2;
    }
}

ColorVal static inline get_min_cg(int par, ColorVal y, ColorVal co) {
    assert(y >= get_min_y(par));
    assert(y <= get_max_y(par));
    if (co < get_min_co(par,y)) return 8*par; //invalid value
    if (co > get_max_co(par,y)) return 8*par; //invalid value
    // assert(i >= get_min_co(par,y));
    // assert(i <= get_max_co(par,y));

    if (y<par-1) {
      return -2-2*y+(abs(co+1)/2)*2;
    } else if (y>=3*par) {
      return -1-2*(4*par-1-y);
    } else {
      return std::max(-4*par + 1+(y-2*par)*2, -2*par-(y-par+1)*2+(abs(co+1)/2)*2);
    }
}

ColorVal static inline get_max_cg(int par, ColorVal y, ColorVal co) {
    assert(y >= get_min_y(par));
    assert(y <= get_max_y(par));

    if (co < get_min_co(par,y)) return -8*par; //invalid value
    if (co > get_max_co(par,y)) return -8*par; //invalid value
    // assert(i >= get_min_co(par,y));
    // assert(i <= get_max_co(par,y));

    if (y<par-1) {
      return 2*y;
    } else if (y>=3*par) {
      return -1+2*(4*par-1-y)-((1+abs(co+1))/2)*2;
    } else {
      return std::min(2*par-2+(y-par+1)*2, 2*par-1+(3*par-1-y)*2-((1+abs(co+1))/2)*2);
    }

}


class ColorRangesYCoCg final : public ColorRanges {
protected:
//    const int par=64; // range: [0..4*par-1]
    const int par;
    const ColorRanges *ranges;
public:
    ColorRangesYCoCg(int parIn, const ColorRanges *rangesIn)
            : par(parIn), ranges(rangesIn) {
    //        if (parIn != par) printf("OOPS: using YCoCg transform on something other than rgb888 ?\n");
    }
    bool isStatic() const { return false; }
    int numPlanes() const { return ranges->numPlanes(); }

    ColorVal min(int p) const {
      switch(p) {
        case 0: return 0;
        case 1: return -4*par;
        case 2: return -4*par;
        default: return ranges->min(p);
      };
    }
    ColorVal max(int p) const {
      switch(p) {
        case 0: return 4*par-1;
        case 1: return 4*par-2;
        case 2: return 4*par-2;
        default: return ranges->max(p);
      };
    }

    void minmax(const int p, const prevPlanes &pp, ColorVal &minv, ColorVal &maxv) const {
         if (p==1) { minv=get_min_co(par, pp[0]); maxv=get_max_co(par, pp[0]); return; }
         else if (p==2) { minv=get_min_cg(par, pp[0], pp[1]); maxv=get_max_cg(par, pp[0], pp[1]); return; }
         else if (p==0) { minv=0; maxv=get_max_y(par); return;}
         else ranges->minmax(p,pp,minv,maxv);
    }
};


template <typename IO>
class TransformYCoCg : public Transform<IO> {
protected:
    int par;
    const ColorRanges *ranges;

public:
    bool virtual init(const ColorRanges *srcRanges) {
        if (srcRanges->numPlanes() < 3) return false;
        if (srcRanges->min(0) < 0 || srcRanges->min(1) < 0 || srcRanges->min(2) < 0) return false;
        if (srcRanges->min(0) == srcRanges->max(0) || srcRanges->min(1) == srcRanges->max(1) || srcRanges->min(2) == srcRanges->max(2)) return false;
        int max = std::max(std::max(srcRanges->max(0), srcRanges->max(1)), srcRanges->max(2));
        par = max/4+1;
        ranges = srcRanges;
        return true;
    }

    const ColorRanges *meta(Images&, const ColorRanges *srcRanges) {
        return new ColorRangesYCoCg(par, srcRanges);
    }

#ifdef HAS_ENCODER
    void data(Images& images) const {
//        printf("TransformYCoCg::data: par=%i\n", par);
        ColorVal R,G,B,Y,Co,Cg;
        for (Image& image : images)
        for (uint32_t r=0; r<image.rows(); r++) {
            for (uint32_t c=0; c<image.cols(); c++) {
                R=image(0,r,c);
                G=image(1,r,c);
                B=image(2,r,c);

                Y = (((R + B)>>1) + G)>>1;
                Co = (R - B) - 1;
                Cg = (((R + B)>>1) - G) - 1;

                image.set(0,r,c, Y);
                image.set(1,r,c, Co);
                image.set(2,r,c, Cg);
            }
        }
    }
#endif
    void invData(Images& images) const {
        ColorVal R,G,B,Y,Co,Cg;
        const ColorVal max[3] = {ranges->max(0), ranges->max(1), ranges->max(2)};
        for (Image& image : images) {
          image.undo_make_constant_plane(0);
          image.undo_make_constant_plane(1);
          image.undo_make_constant_plane(2);
          for (uint32_t r=0; r<image.rows(); r++) {
            for (uint32_t c=0; c<image.cols(); c++) {
                Y=image(0,r,c);
                Co=image(1,r,c);
                Cg=image(2,r,c);

                R = Y + ((Cg + 2)>>1) + ((Co + 2)>>1);
                G = Y - ((Cg + 1)>>1);
                B = Y + ((Cg + 2)>>1) - ((Co + 1)>>1);

                clip(R, 0, max[0]); // clipping only needed in case of lossy/partial decoding
                clip(G, 0, max[1]);
                clip(B, 0, max[2]);

                image.set(0,r,c, R);
                image.set(1,r,c, G);
                image.set(2,r,c, B);
            }
          }
        }
    }
};

