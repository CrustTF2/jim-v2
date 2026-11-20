#pragma once
#include <cstdint>
#include <cstddef>

namespace GCWire {
struct Slice { const uint8_t* data; size_t size; };

inline bool varint(Slice s, size_t& pos, uint64_t& value) {
    value = 0;
    for (unsigned shift = 0; shift < 70; shift += 7) {
        if (pos >= s.size) return false;
        uint8_t byte = s.data[pos++];
        if (shift == 63 && (byte & 0xfe)) return false;
        value |= uint64_t(byte & 0x7f) << shift;
        if (!(byte & 0x80)) return true;
    }
    return false;
}

struct Field { uint32_t number; uint32_t wire; uint64_t value; Slice bytes; };
inline bool next(Slice s, size_t& pos, Field& f) {
    uint64_t tag = 0;
    if (!varint(s, pos, tag) || !tag || (tag >> 3) > 0x1fffffff) return false;
    f = {static_cast<uint32_t>(tag >> 3), static_cast<uint32_t>(tag & 7), 0, {nullptr, 0}};
    if (f.wire == 0) return varint(s, pos, f.value);
    if (f.wire == 1) {
        if (s.size - pos < 8) return false;
        for (unsigned i=0;i<8;++i) f.value |= uint64_t(s.data[pos++]) << (8*i);
        return true;
    }
    if (f.wire == 2) {
        uint64_t length=0;
        if (!varint(s, pos, length) || length > s.size - pos) return false;
        f.bytes = {s.data + pos, static_cast<size_t>(length)};
        pos += static_cast<size_t>(length);
        return true;
    }
    if (f.wire == 5) {
        if (s.size - pos < 4) return false;
        pos += 4; return true;
    }
    return false;
}

inline bool owner(Slice payload, uint64_t& owner_id) {
    size_t pos=0; bool found=false; owner_id=0;
    while (pos < payload.size) {
        Field f;
        if (!next(payload,pos,f)) return false;
        if (f.number==1 && f.wire==1) { owner_id=f.value; found=true; }
    }
    return found;
}

inline bool item_group(Slice group, size_t& count, bool& is_item_group) {
    size_t pos=0, objects=0; uint64_t type=0; bool found_type=false;
    while (pos < group.size) {
        Field f;
        if (!next(group,pos,f)) return false;
        if (f.number==1 && f.wire==0) { type=f.value; found_type=true; }
        if (f.number==2 && f.wire==2) ++objects;
    }
    is_item_group = found_type && type==1;
    if (is_item_group) count += objects;
    return true;
}

inline bool inventory_count(Slice payload, uint64_t expected_owner, size_t& count, bool& has_items) {
    size_t pos=0; count=0; has_items=false; bool found_owner=false;
    while (pos < payload.size) {
        Field f;
        if (!next(payload,pos,f)) return false;
        if (f.number==1 && f.wire==1) {
            if (f.value != expected_owner) return false;
            found_owner=true;
        }
        if (f.number==2 && f.wire==2) {
            bool is_items=false;
            if (!item_group(f.bytes,count,is_items)) return false;
            has_items |= is_items;
        }
    }
    return found_owner;
}

inline bool payload(Slice message, Slice& output) {
    if (message.size < 8) return false;
    uint32_t header = uint32_t(message.data[4]) | uint32_t(message.data[5]) << 8 |
                      uint32_t(message.data[6]) << 16 | uint32_t(message.data[7]) << 24;
    if (header > message.size - 8) return false;
    output = {message.data + 8 + header, message.size - 8 - header};
    return true;
}
}
