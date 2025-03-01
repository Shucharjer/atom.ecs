#include "world.hpp"
#include <latch>
#include <memory/allocator.hpp>
#include <memory/pool.hpp>
#include <reflection.hpp>
#include <thread/thread_pool.hpp>
#include "command.hpp"
#include "ecs.hpp"
#include "queryer.hpp"
#include "resources.hpp"
#include "scheduler.hpp"

using namespace atom;

struct atom::ecs::command::command_attorney {
    static inline void update_garbage_collect(command& command) {
        command.update_garbage_collect();
        command.set<resources::garbage_collect::enable_garbage_collect>({false});
    }
    static inline void shutdown_garbage_collect(command& command) {
        command.shutdown_garbage_collect();
    }
};

ecs::world::world() : shutdown_(false), generations_() {
    // get index from 1
    generations_.emplace_back(0);
};

ecs::world::~world() {
    if (!shutdown_) {
        shutdown();
    }
}

void ecs::world::add_startup(void (*func)(ecs::command&, ecs::queryer&), const priority priority) {
    startup_systems_.emplace(priority, func);
}

void ecs::world::add_update(void (*func)(ecs::command&, ecs::queryer&, float), const priority priority) {
    update_systems_.emplace(priority, func);
}

void ecs::world::add_shutdown(void (*func)(ecs::command&, ecs::queryer&), const priority priority) {
    shutdown_systems_.emplace(priority, func);
}

/*! @cond TURN_OFF_DOXYGEN */

static inline void startup_garbage_collect(ecs::command& command) {
    command.add<resources::garbage_collect::enable_garbage_collect>({ false });
}

static inline void update_garbage_collect(
    ::atom::ecs::command& command, ::atom::ecs::queryer& queryer
) {
    auto* penable = queryer.find<resources::garbage_collect::enable_garbage_collect>();
    if (!penable) [[unlikely]] {
        command.add<resources::garbage_collect::enable_garbage_collect&&>({ false });
    }

    if (penable->value) [[unlikely]] {
        ::atom::ecs::command::command_attorney::update_garbage_collect(command);
    }
}

template <typename Systems, typename... Args>
static inline void call_each_system(Systems& systems, Args&... args) {
#ifndef ATOM_SINGLE_THREAD
    auto& thread_pool = ECS scheduler::thread_pool();
    for (auto iter = systems.crbegin(); iter != systems.crend();) {
        auto key         = iter->first;
        const auto count = systems.count(key);
        if (key != ecs::late_main_thread || ecs::early_main_thread) {
            std::latch latch(static_cast<std::ptrdiff_t>(count));
            for (auto i = 0; i < count; ++i) {
                auto backup = iter;
                auto fn     = [&latch, backup](Args&... args) {
                    backup->second(args...);
                    latch.count_down();
                };
                std::ignore = thread_pool.enqueue(fn, args...);
                ++iter;
            }
            latch.wait();
        }
        else {
            for (auto i = 0; i < count; ++i) {
                iter->second(args...);
                ++iter;
            }
        }
    }
#else
    for (auto iter = systems.crbegin(); iter != systems.crend(); ++iter) {
        iter->second(args...);
    }
#endif
}
/*! @endcond */

void ::atom::ecs::world::startup() {
    auto command = ::atom::ecs::command{ this };
    auto queryer = ::atom::ecs::queryer{ this };
    call_each_system(startup_systems_, command, queryer);
    startup_garbage_collect(command);
}

void ::atom::ecs::world::update(float delta_time) {
    auto command = ::atom::ecs::command{ this };
    auto queryer = ::atom::ecs::queryer{ this };
    call_each_system(update_systems_, command, queryer, delta_time);
    update_garbage_collect(command, queryer);
}

void ::atom::ecs::world::shutdown() {
    if (!shutdown_) [[likely]] {
        shutdown_    = true;
        auto command = ::atom::ecs::command{ this };
        auto queryer = ::atom::ecs::queryer{ this };
        call_each_system(shutdown_systems_, command, queryer);
        command::command_attorney::shutdown_garbage_collect(command);
    }
}

auto ::atom::ecs::world::query() noexcept -> ::atom::ecs::queryer {
    return ::atom::ecs::queryer{ this };
}

auto ::atom::ecs::world::command() noexcept -> ::atom::ecs::command {
    return ::atom::ecs::command{ this };
}
