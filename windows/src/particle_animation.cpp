#include "particle_animation.hpp"
#include <zlib.h>
#include <fstream>
#include <iterator>
#include <limits>

namespace {
std::uint32_t read32(const unsigned char* p) {
    return std::uint32_t(p[0]) | (std::uint32_t(p[1]) << 8) |
           (std::uint32_t(p[2]) << 16) | (std::uint32_t(p[3]) << 24);
}
}

ParticleAnimation::~ParticleAnimation() { clear(); }

void ParticleAnimation::clear() {
    delete texture_; texture_ = nullptr;
    name_.clear(); bytes_.clear(); frames_.clear(); pixels_.clear();
    width_ = height_ = 0; duration_ = 0; current_ = static_cast<size_t>(-1);
}

bool ParticleAnimation::load(const std::string& name) {
    clear();
    // Remember failures too, so a missing/corrupt asset is not read every frame.
    name_ = name;
    std::ifstream input("particles/" + name + ".jpa", std::ios::binary | std::ios::ate);
    if (!input) return false;
    const std::streamoff size = input.tellg();
    if (size < 12 || size > 64 * 1024 * 1024) return false;
    bytes_.resize(static_cast<size_t>(size)); input.seekg(0);
    if (!input.read(reinterpret_cast<char*>(bytes_.data()), size)) return false;
    const unsigned char* p = bytes_.data();
    if (p[0]!='J' || p[1]!='P' || p[2]!='A' || p[3]!='1') return false;
    width_ = std::uint32_t(p[4]) | (std::uint32_t(p[5]) << 8);
    height_ = std::uint32_t(p[6]) | (std::uint32_t(p[7]) << 8);
    const std::uint32_t count = read32(p + 8);
    if (!width_ || !height_ || width_ > 512 || height_ > 512 || !count || count > 4096) return false;
    size_t offset = 12;
    for (std::uint32_t i = 0; i < count; ++i) {
        if (offset > bytes_.size() || bytes_.size() - offset < 8) { frames_.clear(); return false; }
        const std::uint32_t delay = read32(p + offset), length = read32(p + offset + 4);
        offset += 8;
        if (!delay || delay > 60000 || !length || length > bytes_.size() - offset) { frames_.clear(); return false; }
        frames_.push_back({delay, static_cast<std::uint32_t>(offset), length});
        duration_ += delay; offset += length;
    }
    if (offset != bytes_.size()) { frames_.clear(); return false; }
    pixels_.resize(static_cast<size_t>(width_) * height_ * 4);
    return true;
}

const JUI::Texture* ParticleAnimation::frame(JUI::Graphics2D* graphics, const std::string& name,
                                             std::uint64_t elapsed_ms) {
    if (name != name_ && !load(name)) return nullptr;
    if (frames_.empty() || !duration_) return nullptr;
    std::uint64_t position = elapsed_ms % duration_;
    size_t index = 0;
    while (index + 1 < frames_.size() && position >= frames_[index].duration)
        position -= frames_[index++].duration;
    if (index != current_) {
        const Frame& f = frames_[index];
        uLongf length = static_cast<uLongf>(pixels_.size());
        if (uncompress(pixels_.data(), &length, bytes_.data() + f.offset, f.length) != Z_OK || length != pixels_.size())
            return nullptr;
        if (!texture_) {
            GLuint id = graphics->create_texture(pixels_.data(), width_, height_, GL_RGBA);
            if (!id) return nullptr;
            texture_ = new JUI::Texture(id, width_, height_);
        } else {
            glBindTexture(GL_TEXTURE_2D, texture_->get_texture());
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width_, height_, GL_RGBA, GL_UNSIGNED_BYTE, pixels_.data());
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        current_ = index;
    }
    return texture_;
}
