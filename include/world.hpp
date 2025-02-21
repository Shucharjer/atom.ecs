#pragma once
#include <map>
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

namespace internal {

struct world_attorney;

} // namespace internal

class world final {
    friend class ECS command;
    friend class ECS queryer;

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

    void add_startup(void (*func)(command&, queryer&), int priority = 0);
    void add_update(void (*func)(command&, queryer&, float), int priority = 0);
    void add_shutdown(void (*func)(command&, queryer&), int priority = 0);

    void startup();
    void update(float delta_time);
    void shutdown();

    [[nodiscard]] auto query() noexcept -> ecs::queryer;
    [[nodiscard]] auto command() noexcept -> ecs::command;

private:
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

    std::multimap<int, void (*)(ecs::command&, ecs::queryer&)> startup_systems_;
    std::multimap<int, void (*)(ecs::command&, ecs::queryer&, float)> update_systems_;
    std::multimap<int, void (*)(ecs::command&, ecs::queryer&)> shutdown_systems_;
};

} // namespace atom::ecs
