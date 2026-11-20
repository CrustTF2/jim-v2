#ifndef JUI_VIEWPORT_HPP
#define JUI_VIEWPORT_HPP

#include <algorithm>
#include <cmath>

namespace JUI {

// A centered, aspect-preserving viewport shared by rendering and mouse input.
// Coordinates use a top-left origin, including the unused margins.
struct Viewport {
    int x = 0, y = 0, width = 0, height = 0;
    int logical_width = 0, logical_height = 0;
    bool contains(int px, int py) const {
        return width > 0 && height > 0 && px >= x && py >= y && px < x+width && py < y+height;
    }
    int to_logical_x(int px) const {
        return width > 0 ? static_cast<int>(std::floor((px-x)*double(logical_width)/width)) : -1;
    }
    int to_logical_y(int py) const {
        return height > 0 ? static_cast<int>(std::floor((py-y)*double(logical_height)/height)) : -1;
    }
};

inline Viewport fit_viewport(int logical_width, int logical_height, int client_width, int client_height) {
    Viewport result;
    result.logical_width = logical_width;
    result.logical_height = logical_height;
    if (logical_width <= 0 || logical_height <= 0 || client_width <= 0 || client_height <= 0) return result;
    const double scale = (std::min)(double(client_width)/logical_width,double(client_height)/logical_height);
    result.width = (std::max)(1,static_cast<int>(std::floor(logical_width*scale+1e-6)));
    result.height = (std::max)(1,static_cast<int>(std::floor(logical_height*scale+1e-6)));
    result.x = (client_width-result.width)/2;
    result.y = (client_height-result.height)/2;
    return result;
}

}
#endif
