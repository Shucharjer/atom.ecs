#pragma once
#include <map>
#include <utility>
#include "ecs.hpp"
#include "memory.hpp"
#include "memory/allocator.hpp"
#include "memory/pool.hpp"
#include "memory/storage.hpp"
#include "reflection.hpp"
#include "structures/dense_map.hpp"
#include "structures/linear.hpp"
#include "structures/map.hpp"

namespace atom::ecs {

enum priority : int {
    normal_priority = 0,
    only_main_thread = -1
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
#ifndef ATOM_USE_PMR_CONTAINER
    utils::sync_vector<entity::index_t> free_indices_;
    utils::sync_vector<entity::generation_t> generations_;
    utils::sync_vector<entity::id_t> living_entities_;
    utils::sync_vector<entity::id_t> pending_destroy_;
    utils::sync_vector<
        std::tuple<void (*)(void*, utils::basic_allocator*), void*, utils::basic_allocator*>>
        pending_components_;

    utils::sync_dense_map<
        component::id_t,
        std::tuple<
            utils::sync_dense_map<entity::id_t, void*>,
            utils::basic_allocator*,
            utils::basic_reflected*>>
        component_storage_;

    utils::sync_dense_map<resource::id_t, utils::basic_storage*> resource_storage_;

    std::multimap<priority, void (*)(ecs::command&, ecs::queryer&)> startup_systems_;
    std::multimap<priority, void (*)(ecs::command&, ecs::queryer&, float)> update_systems_;
    std::multimap<priority, void (*)(ecs::command&, ecs::queryer&)> shutdown_systems_;
#else
    std::pmr::vector<entity::index_t> free_indices_;
    std::pmr::vector<entity::generation_t> generations_;
    std::pmr::vector<entity::id_t> living_entities_;
    std::pmr::vector<entity::id_t> pending_destroy_;
    std::pmr::vector<std::tuple<void(*)(void*, utils::basic_allocator*), void*, utils::basic_allocator*>>
        pending_components_;

    utils::pmr::dense_map<component::id_t, std::tuple<utils::pmr::dense_map<entity::id_t, void*>, utils::basic_allocator*, utils::basic_reflected*>>
        component_storage_;

    utils::pmr::dense_map<resource::id_t, utils::basic_storage*> resource_storage_;

    std::pmr::multimap<int, void (*)(ecs::command&, ecs::queryer&)> startup_systems_;
    std::pmr::multimap<int, void (*)(ecs::command&, ecs::queryer&, float)> update_systems_;
    std::pmr::multimap<int, void (*)(ecs::command&, ecs::queryer&)> shutdown_systems_;
#endif
};

} // namespace atom::ecs
