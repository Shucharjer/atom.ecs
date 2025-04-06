#pragma once
#include <memory>
#include <memory/allocator.hpp>
#include "memory/pool.hpp"
#include "thread.hpp"

namespace atom::ecs {

struct scheduler {
    scheduler() = delete;

    [[nodiscard]] static auto synchronized_pool() noexcept -> atom::utils::synchronized_pool*;

    template <typename Ty>
    [[nodiscard]] static auto allocator() noexcept
        -> utils::allocator<Ty, utils::synchronized_pool> {
        return utils::allocator<Ty, utils::synchronized_pool>(synchronized_pool());
    }

    [[nodiscard]] static auto thread_pool() noexcept -> atom::utils::thread_pool&;

    template <typename Ty>
    [[nodiscard]] static auto allocate(std::size_t count = 1) {
        return synchronized_pool()->allocate<Ty>(count);
    }
};

template <typename Ty, typename... Args>
inline auto make_shared(Args&&... args) {
    return nullptr;
}

} // namespace atom::ecs
