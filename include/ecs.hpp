#pragma once
#include <concepts>
#include "core.hpp"
#include "reflection.hpp"

#define ECS ::atom::ecs::

namespace atom::ecs {

namespace entity {
using index_t      = uint32_t;
using generation_t = uint32_t;
using id_t         = uint64_t;
} // namespace entity

struct component {
    using id_t = default_id_t;
};

struct resource {
    using id_t = default_id_t;
};

using resource_handle = uint32_t;

enum class asset_type : unsigned char;

namespace concepts {

template <typename Ty>
concept asset = requires(Ty& obj) {
    typename Ty::proxy_type;
    { obj.get_handle() } -> std::same_as<resource_handle>;
    obj.set_handle(std::declval<resource_handle>());
    { obj.type() } -> std::same_as<asset_type>;
};

} // namespace concepts

class world;
class command;
class queryer;

struct scheduler;

using component_registry = utils::registry<component>;

using resource_registry = utils::registry<resource>;

// NOLINTBEGIN(cppcoreguidelines-macro-usage)

#define REGISTER_COMPONENT(component_name, register_name)                                          \
    namespace internal {                                                                           \
    static inline ::atom::utils::register<component_name, ::atom::utils::component>(register_name  \
    );                                                                                             \
    }                                                                                              \
    //

#define REGISTER_RESOURCE(resource_name, register_name)                                            \
    namespace internal {                                                                           \
    static inline ::atom::utils::register<resource_name, ::atom::utils::resource>(register_name);  \
    }                                                                                              \
    //

// NOLINTEND(cppcoreguidelines-macro-usage)

} // namespace atom::ecs
