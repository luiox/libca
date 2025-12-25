#ifndef LIBCA_BASE_BYTE_BUFFER_HPP
#define LIBCA_BASE_BYTE_BUFFER_HPP

#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <limits>
#include <algorithm>
#include "DllExport.hpp"

namespace ca {

class LIBCA_API ByteBuffer
{
private:
    size_t                      position_{0};
    size_t                      limit_{0};
    size_t                      capacity_{0};
    uint8_t*                    buffer_{nullptr};
    std::unique_ptr<uint8_t[]>  owned_;
    bool                        owns_{false};
    size_t                      mark_{std::numeric_limits<size_t>::max()};

    void ensureRemaining(size_t n) const;
    void checkIndex(size_t index) const;

public:
    ByteBuffer() = default;
    explicit ByteBuffer(size_t size);

    // non-copyable
    ByteBuffer(const ByteBuffer&) = delete;
    ByteBuffer& operator=(const ByteBuffer&) = delete;

    // movable
    ByteBuffer(ByteBuffer&&) noexcept = default;
    ByteBuffer& operator=(ByteBuffer&&) noexcept = default;

    ~ByteBuffer() = default;

    static ByteBuffer allocate(size_t size) { return ByteBuffer(size); }

    // wrap existing (non-owning)
    static ByteBuffer wrap(uint8_t* array, size_t size);
    static ByteBuffer wrap(uint8_t* array, size_t offset, size_t length);

    // position/limit/capacity
    [[nodiscard]] size_t capacity() const;
    [[nodiscard]] size_t position() const;
    void position(size_t newPosition);

    [[nodiscard]] size_t limit() const;
    void limit(size_t newLimit);

    [[nodiscard]] size_t remaining() const;
    [[nodiscard]] bool hasRemaining() const;

    // mark/reset/clear/flip/rewind
    void mark();
    void reset();
    void clear();
    void flip();
    void rewind();

    // basic get/put
    uint8_t get();
    uint8_t get(size_t index) const;
    void get(uint8_t* dst, size_t offset, size_t length);

    void put(uint8_t b);
    void put(size_t index, uint8_t b);
    void put(const uint8_t* src, size_t offset, size_t length);

    // slice/compact
    ByteBuffer slice() const;
    void compact();

    // raw access
    uint8_t* data() noexcept;
    const uint8_t* data() const noexcept;
};

}   // namespace ca


#endif   // !LIBCA_BASE_BYTE_BUFFER_HPP