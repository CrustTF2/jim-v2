#pragma once

namespace SlotGeometry {
constexpr int corner_radius = 7;
constexpr int stroke_width = 3;
// Keep the rectangular particle clip inside the curved inner edge as well.
constexpr int effect_inset = stroke_width + (corner_radius - stroke_width + 1) / 2;
}
