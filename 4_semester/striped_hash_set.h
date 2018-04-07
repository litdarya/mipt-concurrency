#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <forward_list>
#include <functional>
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <utility>
#include <vector>

class ReadWriteMutex {
public:
    ReadWriteMutex(): readers_(0), writing_(false) {}

    void lock() {
        std::unique_lock<std::mutex> lock(mutex_);

        while(writing_ || readers_ > 0) {
            cond_var_.wait(lock);
        }

        writing_ = true;
    }

    void unlock() {
        std::unique_lock<std::mutex> lock(mutex_);
        writing_ = false;
        cond_var_.notify_all();
    }

    void lock_shared() {
        std::unique_lock<std::mutex> lock(mutex_);

        while(writing_) {
            cond_var_.wait(lock);
        }

        ++readers_;
    }

    void unlock_shared() {
        std::unique_lock<std::mutex> lock(mutex_);
        --readers_;
        cond_var_.notify_all();
    }

private:
    std::condition_variable cond_var_;
    int readers_;
    bool writing_;
    std::mutex mutex_;
};

template <typename T, class Hash = std::hash<T>>
class StripedHashSet {
public:
    explicit StripedHashSet(const size_t concurrency_level,
                            const size_t growth_factor = 3,
                            const double load_factor = 0.75):
            stripes_(concurrency_level),
            growth_factor_(growth_factor),
            load_factor_(load_factor),
            buckets_(growth_factor*concurrency_level),
            hash_() {
        element_number_ = 0;
    }

    void Rehash() {
        std::unique_lock<ReadWriteMutex> lock_zero(stripes_[0]);

        if (element_number_ / buckets_.size() >= load_factor_) {
            std::vector<std::unique_lock<ReadWriteMutex>> lock_stripes_;

            for (int i = 1; i < (int) stripes_.size(); ++i) {
                lock_stripes_.push_back(std::unique_lock<ReadWriteMutex>(stripes_[i]));
            }

            std::vector<std::forward_list<T>> buckets_copy(growth_factor_ *buckets_.size());

            for (auto bucket : buckets_) {
                for (auto elem : bucket) {
                    buckets_copy[hash_(elem) % buckets_copy.size()]
                            .push_front(std::move(elem));
                }
            }

            buckets_ = std::move(buckets_copy);
        }
    }

    bool Insert(const T& element) {
        std::unique_lock<ReadWriteMutex> lock_element(stripes_[GetStripeIndex(hash_(element))]);

        size_t bucket_number = GetBucketIndex(hash_(element));

        if (std::count(buckets_[bucket_number].begin(),
                       buckets_[bucket_number].end(),
                       element) != 0) {
            return false;
        }

        buckets_[bucket_number].push_front(element);
        element_number_.fetch_add(1);

        if (element_number_ / buckets_.size() >= load_factor_) {
            lock_element.unlock();
            Rehash();
            return true;
        }

        return true;
    }

    bool Remove(const T& element) {
        size_t hash = hash_(element);
        std::unique_lock<ReadWriteMutex> lock(stripes_[GetStripeIndex(hash)]);

        auto it = std::find(buckets_[GetBucketIndex(hash)].begin(),
                            buckets_[GetBucketIndex(hash)].end(), element);

        if (it == buckets_[GetBucketIndex(hash)].end()) {
            return false;
        }
        buckets_[GetBucketIndex(hash)].remove(*it);

        element_number_.fetch_sub(1);
        return true;
    }

    bool Contains(const T& element) {
        size_t hash = hash_(element);
        std::shared_lock<ReadWriteMutex> lock(stripes_[GetStripeIndex(hash)]);

        if (std::count(buckets_[GetBucketIndex(hash)].begin(),
                       buckets_[GetBucketIndex(hash)].end(),
                       element) == 0) {
            return false;
        }
        return true;
    }

    size_t Size() {
        return element_number_;
    }

private:
    size_t GetBucketIndex(const size_t element_hash_value) const {
        return element_hash_value % buckets_.size();
    }

    size_t GetStripeIndex(const size_t element_hash_value) const {
        return element_hash_value % stripes_.size();
    }

    std::vector<ReadWriteMutex> stripes_;
    const size_t growth_factor_;
    const double load_factor_;
    std::vector<std::forward_list<T>> buckets_;
    std::atomic<int> element_number_;
    Hash hash_;
};

template <typename T> using ConcurrentSet = StripedHashSet<T>;
