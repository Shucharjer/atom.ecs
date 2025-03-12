#pragma once
#include <utility>
#include "containers.hpp"
#include "ecs.hpp"
#include "memory.hpp"
#include "memory/allocator.hpp"
#include "memory/pool.hpp"
#include "memory/storage.hpp"
#include "reflection.hpp"

namespace atom::ecs {

enum priority : int {
    early_main_thread = INT_MAX >> 1,
    normal_priority   = 0,
    late_main_thread  = INT_MIN >> 1
};

class world final {
    friend class command;
    friend class queryer;

public:
    using pool_t = ::atom::utils::synchronized_pool;

    template <typename Ty>
    using allocator_t = ::atom::utils::allocator<Ty, pool_t::shared_type>;

    struct identifier {
        struct component {};
        struct resource {};
    };

    world();

    world(const world&)            = delete;
    world& operator=(const world&) = delete;
    world& operator=(world&& that) = delete;
    world(world&& that) noexcept   = delete;

    // the move construct of this class need release a series of data that allocated from the pool.
    ~world();

    /**
     * @brief Systems with higher priority will be startuped eralier.
     */
    template <typename Sys>
    requires requires { Sys::startup(std::declval<command>(), std::declval<queryer>()); } ||
             requires {
                 Sys::update(
                     std::declval<command>(), std::declval<queryer>(), std::declval<float>()
                 );
             } || requires { Sys::shutdown(std::declval<command>(), std::declval<queryer>()); }
    void add_system() {
        if constexpr (requires {
                          Sys::startup(std::declval<ecs::command>(), std::declval<queryer>());
                      }) {
            if constexpr (requires { Sys::startup_priority_v; }) {
                add_startup(&Sys::startup, Sys::startup_priority_v);
            }
            else {
                add_startup(&Sys::startup);
            }
        }

        if constexpr (requires {
                          Sys::update(
                              std::declval<ecs::command>(),
                              std::declval<queryer>(),
                              std::declval<float>()
                          );
                      }) {
            if constexpr (requires { Sys::update_priority_v; }) {
                add_update(&Sys::update, Sys::update_priority_v);
            }
            else {
                add_update(&Sys::update);
            }
        }

        if constexpr (requires {
                          Sys::shutdown(std::declval<ecs::command>(), std::declval<queryer>());
                      }) {
            if constexpr (requires { Sys::shutdown_priority_v; }) {
                add_shutdown(&Sys::shutdown, Sys::shutdown_priority_v);
            }
            else {
                add_shutdown(&Sys::shutdown);
            }
        }
    }

    void add_startup(void (*func)(command&, queryer&), const priority = normal_priority);
    void add_update(void (*func)(command&, queryer&, float), const priority = normal_priority);
    void add_shutdown(void (*func)(command&, queryer&), const priority = normal_priority);

    void startup();
    void update(float delta_time);
    void shutdown();

    [[nodiscard]] auto query() noexcept -> ecs::queryer;
    [[nodiscard]] auto command() noexcept -> ecs::command;

private:
    bool shutdown_;
    vector<entity::index_t> free_indices_;
    vector<entity::generation_t> generations_;
    unordered_set<entity::id_t> living_entities_;
    vector<entity::id_t> pending_destroy_;
    vector<std::tuple<void (*)(void*, utils::basic_allocator*), void*, utils::basic_allocator*>>
        pending_components_;

    dense_map<
        component::id_t,
        std::tuple<
            dense_map<entity::index_t, void*>,
            utils::basic_allocator*,
            utils::basic_reflected*>>
        component_storage_;

    dense_map<resource::id_t, utils::basic_storage*> resource_storage_;

    multimap<int, void (*)(ecs::command&, ecs::queryer&)> startup_systems_;
    multimap<int, void (*)(ecs::command&, ecs::queryer&, float)> update_systems_;
    multimap<int, void (*)(ecs::command&, ecs::queryer&)> shutdown_systems_;
};

} // namespace atom::ecs
