#pragma once
#include <algorithm>
inline bool can_start_selection_box(bool left_shift, unsigned int selected_count) {
    return left_shift && selected_count==0;
}
struct SelectionBounds {
    int left,top,right,bottom;
    static SelectionBounds from_points(int x1,int y1,int x2,int y2) {
        return {(std::min)(x1,x2),(std::min)(y1,y2),(std::max)(x1,x2),(std::max)(y1,y2)};
    }
    bool intersects(int x,int y,int w,int h) const {
        return w>0 && h>0 && right>=x && left<=x+w && bottom>=y && top<=y+h;
    }
};
