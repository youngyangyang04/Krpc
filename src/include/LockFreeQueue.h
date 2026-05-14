#ifndef LOCK_FREE_QUEUE_H
#define LOCK_FREE_QUEUE_H

#include <atomic>
#include <vector>
#include <stdexcept>

template <typename T>
class MPMCQueue {
public:
    // capacity 必须是 2 的幂次方，方便取模优化
    explicit MPMCQueue(size_t capacity)
        : capacity_(capacity), mask_(capacity - 1), buffer_(capacity) {
        if ((capacity & mask_) != 0) {
            throw std::invalid_argument("Capacity must be a power of 2");
        }
        for (size_t i = 0; i < capacity_; ++i) {
            buffer_[i].sequence.store(i, std::memory_order_relaxed);
        }
        enqueue_pos_.store(0, std::memory_order_relaxed);
        dequeue_pos_.store(0, std::memory_order_relaxed);
    }

    // 入队 (生产者)
    bool push(T const& data) {
        Cell* cell;
        size_t pos = enqueue_pos_.load(std::memory_order_relaxed);
        for (;;) {
            cell = &buffer_[pos & mask_];
            size_t seq = cell->sequence.load(std::memory_order_acquire);
            intptr_t diff = (intptr_t)seq - (intptr_t)pos;

            if (diff == 0) {
                // 如果当前位置的序列号等于预期位置，说明该位置为空，尝试抢占
                if (enqueue_pos_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                    break;
            } else if (diff < 0) {
                // 队列已满
                return false;
            } else {
                // 读旧了，重新获取
                pos = enqueue_pos_.load(std::memory_order_relaxed);
            }
        }
        cell->data = data;
        // 更新序列号，通知消费者可以消费
        cell->sequence.store(pos + 1, std::memory_order_release);
        return true;
    }

    // 出队 (消费者)
    bool pop(T& data) {
        Cell* cell;
        size_t pos = dequeue_pos_.load(std::memory_order_relaxed);
        for (;;) {
            cell = &buffer_[pos & mask_];
            size_t seq = cell->sequence.load(std::memory_order_acquire);
            intptr_t diff = (intptr_t)seq - (intptr_t)(pos + 1);

            if (diff == 0) {
                // 如果当前位置的序列号是 pos+1，说明该位置有数据，尝试抢占
                if (dequeue_pos_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                    break;
            } else if (diff < 0) {
                // 队列为空
                return false;
            } else {
                // 读旧了
                pos = dequeue_pos_.load(std::memory_order_relaxed);
            }
        }
        data = cell->data;
        // 更新序列号，通知生产者该位置已空（循环回去了）
        cell->sequence.store(pos + mask_ + 1, std::memory_order_release);
        return true;
    }

private:
    struct Cell {
        std::atomic<size_t> sequence;
        T data;
    };

    const size_t capacity_;
    const size_t mask_;
    std::vector<Cell> buffer_;
    // 强制缓存行对齐，防止“伪共享” (False Sharing) 破坏性能
    alignas(64) std::atomic<size_t> enqueue_pos_;
    alignas(64) std::atomic<size_t> dequeue_pos_;
};

#endif