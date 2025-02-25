#pragma once
#include <exception>
#include <ranges>
#include <stdexcept>
#include <memory/pool.hpp>
#include <memory/storage.hpp>
#include "ecs.hpp"
#include "reflection.hpp"
#include "world.hpp"

namespace atom::ecs {
/**
 * @brief Just query the data, would not change data at most time.
 *
 */
class queryer {
public:
    explicit queryer(::atom::ecs::world* const world) noexcept : world_(world) {}

    queryer(const queryer&) noexcept            = default;
    queryer& operator=(const queryer&) noexcept = default;

    queryer(queryer&& that) noexcept : world_(std::exchange(that.world_, nullptr)) {}
    queryer& operator=(queryer&& that) noexcept {
        world_ = std::exchange(that.world_, nullptr);
        return *this;
    }

    ~queryer() noexcept = default;

    /**
     * @brief Query all entities has all of these components.
     *
     * @tparam Tys Component types.
     * @return Entity id in a `std::vector`.
     */
    template <typename... Components>
    [[nodiscard]] auto query_all_of() const {
        auto filt = [this](const entity::id_t entity) { return all_of<Components...>(entity); };
        return std::views::filter(world_->living_entities_, filt);
    }

    /**
     * @brief Query all entities has any of these components.
     *
     * @tparam Tys Component types.
     * @return Entity id in a `std::vector`.
     */
    template <typename... Components>
    [[nodiscard]] auto query_any_of() const {
        auto filt = [this](const entity::id_t entity) { return any_of<Components...>(entity); };
        return std::views::filter(world_->living_entities_, filt);
    }

    /**
     * @brief Query all entities has non of these components.
     *
     * @tparam Tys Component types.
     * @return Entity id in a `std::vector`.
     */
    template <typename... Components>
    [[nodiscard]] auto query_non_of() const {
        auto filt = [this](const entity::id_t entity) { return non_of<Components...>(entity); };
        return std::views::filter(world_->living_entities_, filt);
    }

    /**
     * @brief Query weather a entity has all of these components.
     *
     * @tparam Tys Component types.
     * @param entity Entity id of a entity.
     * @return `true` if this entity has all of these components, otherwise `false`
     */
    template <typename... Components>
    [[nodiscard]] auto all_of(const entity::id_t entity) const -> bool {
        return (has<Components>(entity) && ...);
    }

    /**
     * @brief Query weather a entity has all of these components.
     *
     * @tparam Tys Component types.
     * @param entity Entity id of a entity.
     * @return `true` if this entity has all of these components, otherwise `false`
     */
    template <typename... Components>
    [[nodiscard]] auto any_of(const entity::id_t entity) const -> bool {
        return (has<Components>(entity) || ...);
    }

    /**
     * @brief Query weather a entity has all of these components.
     *
     * @tparam Tys Component types.
     * @param entity Entity id of a entity.
     * @return `true` if this entity has all of these components, otherwise `false`
     */
    template <typename... Components>
    [[nodiscard]] auto non_of(const entity::id_t entity) const -> bool {
        return !any_of<Components...>(entity);
    }

    /**
     * @brief Return the existance of a entity.
     *
     * @warning Please do not call this function if you have other solutions.
     */
    [[nodiscard]] auto exist(const entity::id_t entity) const -> bool {
        for (auto e : world_->living_entities_) {
            if (e == entity) {
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Get a object of a entity
     *
     * This function assume that you have already assured this entity has this kind of
     * component.
     *
     * @tparam Ty Object type
     * @param entity entity id
     * @return Constant reference of the object
     */
    template <typename Component>
    [[nodiscard]] auto get(const entity::id_t entity) -> Component& {
        auto hash     = utils::hash_of<Component>();
        auto identity = component_registry::identity(hash);

        // find the tuple
        if (auto iter = world_->component_storage_.find(identity);
            iter != world_->component_storage_.end()) [[likely]] {
            auto& pair                              = *iter;
            auto& [map, basic_allocator, reflected] = pair.second;

            // get the component in the internal map
            if (auto iter = map.find(entity); iter != map.end()) [[likely]] {
                auto& pair = *iter;

                // if the component refers to a null pointer
                if (pair.second) {
                    return *static_cast<Component*>(pair.second);
                }
                else {
                    auto* allocator =
                        static_cast<utils::allocator<Component, utils::synchronized_pool>*>(
                            basic_allocator
                        );
                    Component* ptr = allocator->allocate(1);
                    ::new (ptr) Component();
                    pair.second = std::launder(static_cast<Component*>(ptr));
                    return *static_cast<Component*>(pair.second);
                }
            }
            else {
                throw std::runtime_error("Couldn't get not exist component!");
            }
        }

        throw std::runtime_error("Couldn't get not exist component!");
    }

    template <typename Component>
    [[nodiscard]] auto get(const entity::id_t entity) const -> const Component& {
        auto hash     = utils::hash_of<Component>();
        auto identity = component_registry::identity(hash);

        if (auto iter = world_->component_storage_.find(identity);
            iter != world_->component_storage_.cend()) [[likely]] {
            auto& [map, allocator, reflected] = iter->second;
            if (auto iter = map.find(entity); iter != map.cend()) [[likely]] {
                return *static_cast<Component*>(iter->second);
            }
            else {
                throw std::exception("Couldn't get not exist component!");
            }
        }
    }

    /**
     * @brief Find a type of resource.
     *
     * It would return `nullptr` when the resource is not exist.
     *
     * @tparam Ty Type of resource need to find.
     * @return shared_ptr of its initializer.
     */
    template <typename Resource>
    [[nodiscard]] auto find() const -> Resource* const {
        auto hash     = utils::hash_of<Resource>();
        auto identity = resource_registry::identity(hash);
        if (auto iter = world_->resource_storage_.find(identity);
            iter != world_->resource_storage_.end()) {
            utils::basic_storage* basic_storage = (*iter).second;
            return static_cast<Resource*>(basic_storage->raw());
        }
        else {
            return static_cast<Resource*>(nullptr);
        }
    }

private:
    template <typename Component>
    [[nodiscard]] auto has(const entity::id_t& entity) const noexcept -> bool {
        auto hash     = utils::hash_of<std::remove_cvref_t<Component>>();
        auto identity = component_registry::identity(hash);
        if (auto iter = world_->component_storage_.find(identity);
            iter != world_->component_storage_.cend()) [[likely]] {
            const auto& [map, allocator, reflected] = (*iter).second;
            return map.contains(entity);
        }
        else {
            return false;
        }
    }

    ::atom::ecs::world* world_;
};
} // namespace atom::ecs
