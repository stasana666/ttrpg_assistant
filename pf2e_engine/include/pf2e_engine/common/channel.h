#pragma once

#include <atomic>
#include <cassert>
#include <optional>
#include <stdexcept>
#include <vector>

template <class T>
class TChannel {
    struct Cell {
        std::optional<T> value;
        std::atomic<size_t> index;
    };

public:
    class TConsumer {
    private:
        explicit TConsumer(TChannel* queue)
            : queue_(queue)
        {
        }

        TChannel* queue_;

        friend class TChannel;

    public:
        bool Dequeue(T& value) {
            return queue_->Dequeue(value);
        }
    };

    class TProducer {
    private:
        explicit TProducer(TChannel* queue)
            : queue_(queue)
        {
        }

        TChannel* queue_;

        friend class TChannel;

    public:
        bool Enqueue(const T& value) {
            return queue_->Enqueue(value);
        }
    };

    explicit TChannel(int size)
        : max_size_(size)
        , mask_(max_size_ - 1)
        , data_(size)
    {
        if (max_size_ != (max_size_ & -max_size_)) {
            throw std::runtime_error("channel size must be power of two");
        }
        for (size_t i = 0; i < max_size_; ++i) {
            data_[i].index = i; // memory order
        }
    }

    TProducer MakeProducer()
    {
        return TProducer{this};
    }

    TConsumer MakeConsumer()
    {
        return TConsumer{this};
    }

    bool Enqueue(const T& value) {
        Cell* cell;
        size_t index;
        size_t cell_index;
        while (true) {
            if (enqueue_index_.load() - dequeue_index_.load() == max_size_) {
                return false;
            }
            index = enqueue_index_.load();
            cell = &data_[index & mask_];
            cell_index = cell->index.load();
            if (cell_index == index) {
                if (enqueue_index_.compare_exchange_weak(index, index + 1)) {
                    break;
                }
            }
        };
        cell->value = value;
        cell->index.store(index + 1);
        return true;
    }

    bool Dequeue(T& data) {
        Cell* cell;
        size_t index;
        while (true) {
            if (enqueue_index_.load() == dequeue_index_.load()) {
                return false;
            }
            index = dequeue_index_.load();
            cell = &data_[index & mask_];
            size_t cell_index = cell->index.load();
            if (cell_index == index + 1) {
                if (dequeue_index_.compare_exchange_weak(index, index + 1)) {
                    break;
                }
            }
        }
        data = cell->value.value();
        cell->index.store(index + max_size_);
        return true;
    }

private:
    size_t max_size_;
    size_t mask_;
    std::vector<Cell> data_;
    std::atomic<size_t> enqueue_index_{};
    std::atomic<size_t> dequeue_index_{};
};
