#pragma once

#include <jui/gfx/graphics_2d.hpp>
#include <cstdint>
#include <string>
#include <vector>

// Only the currently hovered effect has an animation loaded in memory.
class ParticleAnimation {
public:
    ~ParticleAnimation();
    const JUI::Texture* frame(JUI::Graphics2D* graphics, const std::string& name,
                              std::uint64_t elapsed_ms);
    void clear();
private:
    struct Frame { std::uint32_t duration, offset, length; };
    bool load(const std::string& name);
    std::string name_;
    std::vector<unsigned char> bytes_;
    std::vector<Frame> frames_;
    std::vector<unsigned char> pixels_;
    std::uint32_t width_ = 0, height_ = 0;
    std::uint64_t duration_ = 0;
    size_t current_ = static_cast<size_t>(-1);
    JUI::Texture* texture_ = nullptr;
};
