#include "scheduler.hpp"
#include <memory/pool.hpp>
#include "thread/thread_pool.hpp"

auto atom::ecs::scheduler::synchronized_pool() noexcept -> atom::utils::synchronized_pool* {
    static ::atom::utils::synchronized_pool pool;
    return &pool;
}

auto atom::ecs::scheduler::thread_pool() noexcept -> atom::utils::thread_pool& {
    static atom::utils::thread_pool pool;
    return pool;
}
