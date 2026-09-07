// BrazilMR — pools de objetos e buffers (zero-allocation no frame loop).
#pragma once

#include <cstdint>
#include <vector>

namespace brazilmr {

// Pool de objetos de tamanho fixo com freelist O(1).
// Não usa exceções (build com -fno-exceptions): acquire retorna nullptr
// quando esgotado — o chamador degrada graciosamente.
template <typename T, std::size_t kCapacity>
class ObjectPool {
public:
    ObjectPool() : freeHead_(0) {
        for (std::size_t i = 0; i < kCapacity; ++i) {
            slots_[i].next = static_cast<std::size_t>(i + 1);
        }
        slots_[kCapacity - 1].next = kInvalid;
    }

    template <typename... Args>
    T* acquire(Args&&... args) {
        if (freeHead_ == kInvalid) return nullptr;
        Slot& s = slots_[freeHead_];
        std::size_t idx = freeHead_;
        freeHead_ = s.next;
        T* obj = &s.obj;
        new (obj) T(std::forward<Args>(args)...);
        return obj;
    }

    void release(T* obj) {
        if (!obj) return;
        Slot* s = reinterpret_cast<Slot*>(obj);
        s->next = freeHead_;
        freeHead_ = static_cast<std::size_t>(s - slots_);
        obj->~T();
    }

    std::size_t capacity() const { return kCapacity; }

private:
    static constexpr std::size_t kInvalid = static_cast<std::size_t>(-1);
    union Slot {
        std::size_t next;
        T obj;
        Slot() {}
    };
    Slot slots_[kCapacity];
    std::size_t freeHead_;
};

// Pool de buffers de bytes de tamanho fixo (ImageReader/VO/hands).
// Buffers são reciclados — evita cópias e GC no lado Java.
class BufferPool {
public:
    explicit BufferPool(std::size_t bufferSize, std::size_t maxBuffers = 6)
        : bufferSize_(bufferSize) {
        (void)maxBuffers;
    }

    std::vector<uint8_t>* acquire() {
        if (!free_.empty()) {
            std::vector<uint8_t>* b = free_.back();
            free_.pop_back();
            return b;
        }
        pool_.emplace_back(bufferSize_);
        return &pool_.back();
    }

    void release(std::vector<uint8_t>* b) {
        if (b) free_.push_back(b);
    }

    std::size_t bufferSize() const { return bufferSize_; }

private:
    std::size_t bufferSize_;
    std::vector<std::vector<uint8_t>> pool_;
    std::vector<std::vector<uint8_t>*> free_;
};

} // namespace brazilmr
