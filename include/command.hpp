#pragma once
#include <ranges>
#include <type_traits>
#include <memory/pool.hpp>
#include <memory/storage.hpp>
#include "core.hpp"
#include "ecs.hpp"
#include "memory.hpp"
#include "memory/allocator.hpp"
#include "reflection/hash.hpp"
#include "scheduler.hpp"
#include "world.hpp"

static inline const auto k_thirty_two = 32;

namespace atom::ecs {
class command {
    using world_attorney = ECS internal::world_attorney;

public:
    struct command_attorney;
    friend struct command_attorney;

    command(ECS world& world) : world_(&world) {}
    command(ECS world* world) : world_(world) {}
    command(command&& other) noexcept : world_(std::exchange(other.world_, nullptr)) {}
    command(const command& other)      = default;
    command& operator=(const command&) = delete;
    command& operator=(command&&)      = delete;
    ~command()                         = default;

    ///////////////////////////////////////////////////////////////////////
    // Entity
    ///////////////////////////////////////////////////////////////////////
private:
    template <utils::concepts::pure RawComponentType>
    void check_map_existance(default_id_t identity) {
        if (!world_->component_storage_.contains(identity)) [[unlikely]] {
            world_->component_storage_.emplace(
                identity,
                std::make_tuple(
                    utils::sync_dense_map<entity::id_t, void*>(
                        utils::allocator<int, utils::synchronized_pool>(
                            scheduler::synchronized_pool()
                        )
                    ),
                    new utils::allocator<RawComponentType, utils::synchronized_pool>(
                        scheduler::synchronized_pool()
                    ),
                    new utils::reflected<RawComponentType>()
                )
            );
        }
    }

    template <utils::concepts::pure Component>
    requires std::is_default_constructible_v<Component>
    void attach_impl(const entity::id_t entity) {
        const auto hash     = utils::hash_of<Component>();
        const auto identity = component_registry::identity(hash);
        check_map_existance<Component>(identity);

        auto& [map, basic_allocator, reflected] = world_->component_storage_.at(identity);
        map.emplace(entity, nullptr);
    }

public:
    template <typename... Components>
    auto attach(const ECS entity::id_t entity) -> void {
        (attach_impl<Components>(entity), ...);
    }

private:
    template <typename Component>
    void attach_impl(const entity::id_t entity, Component&& value) {
        using pure          = std::remove_cvref_t<Component>;
        const auto hash     = utils::hash_of<pure>();
        const auto identity = component_registry::identity(hash);
        check_map_existance<pure>(identity);

        auto& [map, basic_allocator, reflected] = world_->component_storage_.at(identity);
        auto* allocator =
            static_cast<utils::allocator<pure, utils::synchronized_pool>*>(basic_allocator);
        pure* ptr = allocator->allocate(1);
        ::new (ptr) pure(std::forward<Component>(value));
        map.emplace(entity, std::launder(ptr));
    }

public:
    template <typename... Components>
    void attach(const ECS entity::id_t entity, Components&&... components) {
        (attach_impl<Components>(entity, std::forward<Components>(components)), ...);
    }

    auto spawn() -> ECS entity::id_t {
        entity::index_t index{};
        if (world_->free_indices_.size()) {
            index = world_->free_indices_.back();
            world_->free_indices_.pop_back();
        }
        else {
            index = world_->generations_.size();
            world_->generations_.emplace_back(0);
        }

        entity::id_t entity = ((entity::id_t)index << k_thirty_two) | world_->generations_[index];

        world_->living_entities_.emplace_back(entity);

        return index;
    }

    template <typename... Components>
    requires std::conjunction_v<std::is_same<std::remove_cvref_t<Components>, Components>...>
    auto spawn() -> ECS entity::id_t {
        auto entity = spawn();
        (attach_impl<Components>(entity), ...);
        return entity;
    }

    template <typename... Components>
    auto spawn(Components&&... components) -> ECS entity::id_t {
        auto entity = spawn();
        (attach_impl<Components>(entity, std::forward<Components>(components)), ...);
        return entity;
    }

private:
    template <typename Component>
    void modify_impl(const entity::id_t entity, Component&& value) {
        using pure = std::remove_cvref_t<Component>;
        static_assert(std::is_assignable_v<pure, Component>);

        auto hash     = utils::hash_of<pure>();
        auto identity = ecs::component_registry::identity(hash);

        if (auto iter = world_->component_storage_.find(identity);
            iter != world_->component_storage_.cend()) [[likely]] {
            auto& [map, allocator, reflected] = iter->second;
            if (auto iterator = map.find(entity); iterator != map.cend()) [[likely]] {
                (*static_cast<pure*>(iterator->second)) = std::forward<Component>(value);
            }
        }
    }

public:
    template <typename... Components>
    auto modify(const ECS entity::id_t entity, Components&&... components) -> void {
        (modify_impl<Components>(entity, std::forward<Components>(components)), ...);
    }

private:
    template <typename Component>
    void detach_impl(const entity::id_t entity) {
        auto hash     = utils::hash_of<Component>();
        auto identity = component_registry::identity(hash);

        if (auto iter = world_->component_storage_.find(identity);
            iter != world_->component_storage_.cend()) [[likely]] {
            auto& [map, allocator, reflected] = iter->second;
            if (map.contains(entity)) [[likely]] {
                world_->pending_components_.emplace_back(
                    reflected->cextend().info.destroy, map.at(entity)
                );
                map.erase(entity);
            }
        }
    }

public:
    template <typename... Components>
    auto detach(const ECS entity::id_t entity) -> void {
        (detach_impl<Components>(entity), ...);
    }

    auto kill(const ::atom::ecs::entity::id_t entity) -> void {
        world_->pending_destroy_.emplace_back(entity);
    }

#if _HAS_CXX23
    template <std::ranges::range Rng>
    requires std::is_same_v<std::iter_value_t<Rng>, entity::id_t>
    auto kill(Rng&& range) -> void {
        world_.pending_destroy_.append_range(std::forward<Rng>(range));
    }
#else
    template <std::ranges::range Rng>
    requires std::is_same_v<std::iter_value_t<Rng>, entity::id_t>
    auto kill(const Rng& range) {
        for (const entity::id_t entity : range) {
            world_->pending_destroy_.emplace_back(entity);
        }
    }
#endif

    ///////////////////////////////////////////////////////////////////////
    // Resources
    ///////////////////////////////////////////////////////////////////////

private:
    template <typename Resource>
    void add_impl() {
        auto hash     = utils::hash_of<Resource>();
        auto identity = resource_registry::identity(hash);

        using storage_t = UTILS unique_storage<Resource, UTILS builtin_storage_allocator<Resource>>;
        if (!world_->resource_storage_.contains(identity)) [[likely]] {
            world_->resource_storage_.emplace(identity, new storage_t(utils::construct_at_once));
        }
    }

public:
    template <typename... Resources>
    requires((std::is_same_v<std::remove_cvref_t<Resources>, Resources> && ...))
    void add() {
        (add_impl<Resources>(), ...);
    }

private:
    template <typename Resource>
    void add_impl(Resource&& val) {
        using pure    = std::remove_cvref_t<Resource>;
        auto hash     = utils::hash_of<pure>();
        auto identity = resource_registry::identity(hash);

        using storage_t = UTILS unique_storage<Resource>;
        if (!world_->resource_storage_.contains(identity)) {
            auto* storage = new storage_t();
            (*storage)    = std::forward<Resource>(val);
            world_->resource_storage_.emplace(identity, storage);
        }
    }

public:
    template <typename... Resources>
    void add(Resources&&... resources) {
        (add_impl<Resources...>(std::forward<Resources>(resources)), ...);
    }

private:
    template <typename Resource>
    void set_impl(Resource&& val) {
        auto hash     = utils::hash_of<Resource>();
        auto identity = resource_registry::identity(hash);

        using storage_t = UTILS unique_storage<Resource>;
        if (auto iter = world_->resource_storage_.find(identity);
            iter != world_->resource_storage_.cend()) [[likely]] {
            utils::basic_storage* basic_storage = world_->resource_storage_.at(identity);
            auto* storage                       = static_cast<storage_t*>(basic_storage);
            storage                             = std::forward<Resource>(val);
        }
    }

public:
    template <typename... Resources>
    void set(Resources&&... resources) {
        (set_impl<Resources...>(std::forward<Resources>(resources)), ...);
    }

private:
    template <typename Resource>
    void remove_impl() {
        auto hash     = utils::hash_of<Resource>();
        auto identity = resource_registry::identity(hash);
        if (world_->resource_storage_.contains(identity)) [[likely]] {
            utils::basic_storage* storage = world_->resource_storage_.at(identity);
            delete storage;
            world_->resource_storage_.erase_without_check(identity);
        }
    }

public:
    template <typename... Resources>
    requires((std::is_same_v<std::remove_cvref_t<Resources>, Resources> && ...))
    void remove() {
        (remove_impl<Resources>(), ...);
    }

private:
    void update_garbage_collect() {
        // single component
        for (auto& [destroy, ptr] : world_->pending_components_) {
            destroy(ptr);
        }

        // whole entity
        for (auto entity : world_->pending_destroy_) {
            for (auto& [map, allocator, reflected] :
                 world_->component_storage_ | std::views::values) {
                for (auto* ptr : map | std::views::values) {
                    reflected->destroy(ptr);
                }
            }
        }
    }

    void shutdown_garbage_collect() {
        // entities and their components

        for (auto& [destroy, ptr] : world_->pending_components_) {
            destroy(ptr);
        }
        world_->pending_components_.clear();

        for (auto& [map, allocator, reflected] : world_->component_storage_ | std::views::values) {
            for (auto* ptr : map | std::views::values) {
                if (ptr) {
                    reflected->destroy(ptr);
                }
            }
            map.clear();
        }
        world_->component_storage_.clear();

        // resources

        for (auto* storage : world_->resource_storage_ | std::views::values) {
            delete storage;
        }
        world_->resource_storage_.clear();
    }

private:
    ECS world* world_;
};

} // namespace atom::ecs
