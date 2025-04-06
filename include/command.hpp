#pragma once
#include <ranges>
#include <type_traits>
#include <core/langdef.hpp>
#include <memory/pool.hpp>
#include <memory/storage.hpp>
#include <reflection.hpp>
#include "asset.hpp"
#include "core.hpp"
#include "ecs.hpp"
#include "memory.hpp"
#include "memory/allocator.hpp"
#include "schedule.hpp"
#include "world.hpp"

namespace atom::ecs {
class command {
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
                    dense_map<entity::index_t, void*>(),
                    new utils::allocator<RawComponentType, utils::synchronized_pool>(
                        scheduler::synchronized_pool()
                    ),
                    new utils::reflected<RawComponentType>()
                )
            );
        }
    }

    template <utils::concepts::pure Component, std::size_t hash = utils::hash_of<Component>()>
    requires std::is_default_constructible_v<Component>
    void attach_impl(const entity::index_t index) {
        ATOM_DEBUG_SHOW_FUNC

        const auto identity = component_registry::identity(hash);
        check_map_existance<Component>(identity);

        auto& [map, basic_allocator, reflected] = world_->component_storage_.at(identity);
        if (!map.contains(index)) [[likely]] {
            map.emplace(index, nullptr);
        }
    }

public:
    template <utils::concepts::pure... Components>
    auto attach(const ECS entity::id_t entity) -> void {
        const entity::index_t index = entity >> num_thirty_two;
        (attach_impl<Components>(index), ...);
    }

private:
    template <
        utils::concepts::pure Component,
        typename ComponentTy,
        auto hash = utils::hash_of<Component>()>
    void attach_impl(const entity::index_t index, ComponentTy&& value) {
        ATOM_DEBUG_SHOW_FUNC

        const auto identity = component_registry::identity(hash);
        check_map_existance<Component>(identity);

        auto& [map, basic_allocator, basic_reflected] = world_->component_storage_.at(identity);
        if (!map.contains(index)) [[likely]] {
            auto* allocator =
                static_cast<utils::allocator<Component, utils::synchronized_pool>*>(basic_allocator
                );
            Component* ptr = allocator->allocate(1);
            ::new (ptr) Component(std::forward<ComponentTy>(value));
            map.emplace(index, std::launder(ptr));
        }
    }

public:
    template <utils::concepts::pure... Components, typename... ComponentTys>
    void attach(const entity::id_t entity, ComponentTys&&... components) {
        const entity::index_t index = entity >> num_thirty_two;
        (attach_impl<Components>(index, std::forward<ComponentTys>(components)), ...);
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

        entity::id_t entity = ((entity::id_t)index << num_thirty_two) | world_->generations_[index];

        world_->living_entities_.emplace(entity);

        return entity;
    }

    template <utils::concepts::pure... Components>
    requires std::conjunction_v<std::is_same<std::remove_cvref_t<Components>, Components>...>
    auto spawn() -> ECS entity::id_t {
        auto entity                 = spawn();
        const entity::index_t index = entity >> num_thirty_two;
        (attach_impl<Components>(index), ...);
        return entity;
    }

    template <utils::concepts::pure... Components, typename... ComponentTys>
    auto spawn(ComponentTys&&... components) -> entity::id_t {
        auto entity                 = spawn();
        const entity::index_t index = entity >> num_thirty_two;
        (attach_impl<Components>(index, std::forward<ComponentTys>(components)), ...);
        return entity;
    }

private:
    template <
        utils::concepts::pure Component,
        typename ComponentTy,
        auto hash = utils::hash_of<Component>()>
    void modify_impl(const entity::index_t index, ComponentTy&& value) {
        const auto identity = ecs::component_registry::identity(hash);

        if (auto iter = world_->component_storage_.find(identity);
            iter != world_->component_storage_.cend()) [[likely]] {
            auto& [map, allocator, reflected] = iter->second;
            if (auto iterator = map.find(index); iterator != map.cend()) [[likely]] {
                (*static_cast<Component*>(iterator->second)) = std::forward<ComponentTy>(value);
            }
        }
    }

public:
    template <typename... Components>
    auto modify(const ECS entity::id_t entity, Components&&... components) -> void {
        const entity::index_t index = entity >> num_thirty_two;
        (modify_impl<Components>(index, std::forward<Components>(components)), ...);
    }

private:
    template <utils::concepts::pure Component, auto hash = utils::hash_of<Component>()>
    void detach_impl(const entity::index_t index) {
        const auto identity = component_registry::identity(hash);

        if (auto iter = world_->component_storage_.find(identity);
            iter != world_->component_storage_.cend()) [[likely]] {
            auto& [map, allocator, reflected] = iter->second;
            if (map.contains(index)) [[likely]] {
                auto fn = [](void* ptr, utils::basic_allocator* allocator) -> void {
                    if (ptr) [[likely]] {
                        std::destroy_at(static_cast<Component*>(ptr));
                        allocator->dealloc(ptr);
                    }
                };
                world_->pending_components_.emplace_back(fn, map.at(index), allocator);
                map.erase(index);
            }
        }
    }

public:
    template <typename... Components>
    auto detach(const ECS entity::id_t entity) -> void {
        const entity::index_t index = entity >> num_thirty_two;
        (detach_impl<Components>(index), ...);
    }

    auto kill(const ::atom::ecs::entity::id_t entity) -> void {
        world_->pending_destroy_.emplace_back(entity);
        world_->living_entities_.erase(entity);
    }

#if _HAS_CXX23
    template <std::ranges::range Rng>
    requires std::is_same_v<std::iter_value_t<Rng>, entity::id_t>
    auto kill(Rng&& range) -> void {
        world_->pending_destroy_.append_range(std::forward<Rng>(range));
        // TODO: erase
    }
#else
    template <std::ranges::range Rng>
    requires std::is_same_v<std::iter_value_t<Rng>, entity::id_t>
    auto kill(const Rng& range) {
        for (const entity::id_t entity : range) {
            world_->pending_destroy_.emplace_back(entity);
            world_->living_entities_.erase(entity);
        }
    }
#endif

    ///////////////////////////////////////////////////////////////////////
    // Resources
    ///////////////////////////////////////////////////////////////////////

private:
    template <typename Resource, auto hash = utils::hash_of<Resource>()>
    requires(!concepts::asset<Resource>)
    void add_impl() {
        const auto identity = resource_registry::identity(hash);

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
    template <
        utils::concepts::pure Resource,
        typename ResourceTy,
        auto hash = utils::hash_of<Resource>()>
    void add_impl(ResourceTy&& val) {
        const auto identity = resource_registry::identity(hash);

        if constexpr (concepts::asset<Resource>) {
            // TODO:
            auto& hub = hub::instance();
        }

        using storage_t = utils::unique_storage<Resource>;
        if (!world_->resource_storage_.contains(identity)) {
            auto* storage = new storage_t(std::forward<ResourceTy>(val));
            world_->resource_storage_.emplace(identity, storage);
        }
    }

public:
    template <utils::concepts::pure... Resource, typename... ResourceTys>
    void add(ResourceTys&&... resources) {
        (add_impl<Resource>(std::forward<ResourceTys>(resources)), ...);
    }

private:
    template <
        utils::concepts::pure Resource,
        typename ResourceTy,
        auto hash = utils::hash_of<Resource>()>
    void set_impl(ResourceTy&& val) {
        ATOM_DEBUG_SHOW_FUNC

        const auto identity = resource_registry::identity(hash);

        using storage_t = UTILS unique_storage<Resource>;
        if (auto iter = world_->resource_storage_.find(identity);
            iter != world_->resource_storage_.cend()) [[likely]] {
            utils::basic_storage* basic_storage = world_->resource_storage_.at(identity);
            auto* storage                       = static_cast<storage_t*>(basic_storage);
            *storage                            = std::forward<ResourceTy>(val);
        }
    }

public:
    template <utils::concepts::pure... Resources, typename... ResourceTys>
    void set(ResourceTys&&... resources) {
        (set_impl<Resources>(std::forward<ResourceTys>(resources)), ...);
    }

private:
    template <utils::concepts::pure Resource, auto hash = utils::hash_of<Resource>()>
    void remove_impl() {
        const auto identity = resource_registry::identity(hash);
        if (world_->resource_storage_.contains(identity)) [[likely]] {
            utils::basic_storage* storage = world_->resource_storage_.at(identity);
            delete storage;
            world_->resource_storage_.erase_without_check(identity);
        }
    }

public:
    template <utils::concepts::pure... Resources>
    void remove() {
        (remove_impl<Resources>(), ...);
    }

private:
    void update_garbage_collect() {
        // single component
        for (auto& [del, ptr, allocator] : world_->pending_components_) {
            del(ptr, allocator);
        }
        world_->pending_components_.clear();

        // whole entity
        for (auto entity : world_->pending_destroy_) {
            const entity::index_t index = entity >> num_thirty_two;
            for (auto& [map, allocator, reflected] :
                 world_->component_storage_ | std::views::values) {
                if (auto iter = map.find(index); iter != map.end()) {
                    if (iter->second) [[likely]] {
                        reflected->destroy(iter->second);
                        allocator->dealloc(iter->second);
                    }
                    map.erase(index);
                }
            }
            world_->free_indices_.emplace_back(static_cast<entity::index_t>(index));
            ++world_->generations_[index];
        }
        world_->pending_destroy_.clear();
    }

    void shutdown_garbage_collect() {
        // entities and their components

        for (auto& [del, ptr, allocator] : world_->pending_components_) {
            del(ptr, allocator);
        }
        world_->pending_components_.clear();

        for (auto& [map, allocator, reflected] : world_->component_storage_ | std::views::values) {
            for (auto* ptr : map | std::views::values) {
                if (ptr) [[likely]] {
                    reflected->destroy(ptr);
                    allocator->dealloc(ptr);
                }
            }

            delete allocator;
            delete reflected;
            allocator = nullptr;
            reflected = nullptr;
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
