#pragma once
#include <memory>
#include <utility>
#include <unordered_map>
#include <shared_mutex>
#include <reflection.hpp>
#include <memory/allocator.hpp>
#include <structures/dense_map.hpp>

namespace atom::ecs {

template <typename Alloc = std::allocator<std::pair<default_id_t, std::pair<void(*)(void*, const std::any&), std::vector<void*, std::allocator<void*>>>>>
class transient_collection {
    template <typename Ty>
    using allocator_t = typename utils::rebind_allocator<Alloc>::template to_t<Ty>;

    using deletor_type = void(*)(internal_pointer, const Alloc&);
    using internal_pointer = void*;
    using internal_allocator = allocator_t<internal_pointer>;

    using alty = allocator_t<std::pair<deletor_type, std::vector<internal_pointer, internal_allocator>>>;
    using alty_traits = std::allocator_traits<alty>;
public:
    using value_type = std::pair<default_id_t, >
    using allocator_type = alty;

    template <typename Al>
    transient_collection(std::allocator_arg_t, const Al& al) : events_(std::allocator_arg, al) {}

    template <typename Al>
    transient_collection(const Al& al) : events_(std::allocator_arg, al) {}

    transient_collection(const transient_collection&) = delete;

    transient_collection(transient_collection&& that) noexcept : events_(std::move(that.events_)) {}

    transient_collection& operator=(const transient_collection&) = delete;

    transient_collection& operator=(transient_collection&& that) {
        if (this != &that) {
            // NOTE: if move directly, events in this transient collection would cause a memory leak.
            // so, the suitable way to impl move this function is neither swap nor pop before moving.
            // the further could move quickly, but the latter could release memory blocks earlier.
            std::swap(events_, that.events_);
        }
        return *this;
    }

    ~transient_collection() {
        pop();
    }

    template <typename EventType, typename Event, size_t hash = utils::hash_of<EventType>()>
    void push(Event&& event) {
        using event_allocator = allocator_t<EventType>;

        auto alloc = events_.get_allocator();
        event_allocator allocator { alloc };
        auto* const ptr = allocator.allocate(1);
        std::allocator_traits<event_allocator>::construct(allocator, ptr, std::forward<Event>(event));
        auto* e = std::launder(ptr);

        const auto ident = identity(hash);
        std::unique_lock<std::shared_mutex> ulock(mutex_);

        if (auto iter = events_.find(ident); iter != events_.end()) {
            auto& [destroyer, vector] = iter->second;
            vector.emplace_back(e);
        }
        else {
            std::pair<void(*)(void*, const allocator_type&), std::vector<internal_pointer, internal_allocator>> pair([](internal_pointer* ptr, const Alloc& alloc){
                EventType* event = static_cast<EventType*>(ptr);
                event_allocator allocator { alloc };
                std::allocator_traits<event_allocator>::destroy(allocator, event);
                allocator.deallocate(event, 1);
            }, alloc);
            pair.second.emplace_back(e);
            events_.emplace(ident, std::move(pair));
        }
    }

    void pop() {
        auto alloc = events_.get_allocator();
        Alloc _alloc { alloc };
        std::unique_lock<std::shared_mutex> ulock(mutex_);
        for (auto& [ident, pair] : events_) {
            auto& [deletor, vector] = pair;
            for (auto ptr : vector) {
                deletor(ptr, _alloc);
            }
            vector.clear();
        }
    }

    template <typename EventType, size_t hash = utils::hash_of<EventType>()>
    decltype(auto) _() noexcept {
        // TODO: a function name
        // WARNING: events got at this time, but maybe after calling this function, events were changed.
        const auto ident = identity(hash);
        std::shared_lock<std::shared_mutex> slock(mutex_);
        if (auto iter = events_.find(ident); iter != events_.end()) {
            auto& pair = iter->second;
            return pair->second;
        }
    }

private:
    static default_id_t identity(const size_t hash) {
        static std::unordered_map<size_t, default_id_t> map;
        if (auto iter = map.find(hash); iter != map.end()) [[likely]] {
            return iter->second;
        }
        else {
            auto ident = map.size();
            map_.emplace(hash, ident);
            return ident;
        }
    }

    std::shared_mutex mutex_;
    utils::dense_map<default_id_t, std::pair<void(*)(void*), std::vector<internal_pointer, internal_allocator>>, alty> events_;
};

}
