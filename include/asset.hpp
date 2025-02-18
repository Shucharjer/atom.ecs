#pragma once
#include <string>
#include "ecs.hpp"

namespace atom::ecs {

/**
 * @brief Asset type.
 *
 */
enum class asset_type : unsigned char {
    unknown,
    text,
    sound,
    model,
    shader,
    texture
};

/**
 * @brief Properties about the asset.
 *
 */
struct basic_asset {
    ECS asset_type type = ECS asset_type::unknown;
    std::string path;
};

/**
 * @brief Resources that would be used in the whole game project.
 *
 * It is a description tells us these files may be used in game in one or more world.
 *
 */
template <typename Ty>
struct asset : public basic_asset {
    constexpr asset() : basic_asset() {}
    virtual ~asset()                         = default;
    asset(const asset&)                      = default;
    asset& operator=(const asset&)           = default;
    asset(asset&& other) noexcept            = default;
    asset& operator=(asset&& other) noexcept = default;
};

} // namespace atom::ecs

#if __has_include(<nlohmann/json.hpp>)
    #include <nlohmann/json.hpp>

namespace atom::ecs {
static void to_json(nlohmann::json& json, const basic_asset& asset) {
    json = nlohmann::json{
        { "type", asset.type },
        { "path", asset.path }
    };
}

static void from_json(const nlohmann::json& json, basic_asset& asset) {
    json.at("type").get_to(asset.type);
    json.at("path").get_to(asset.path);
}
} // namespace atom::ecs
#endif

// NOLINTBEGIN(cppcoreguidelines-macro-usage)
#define REGISTER_ASSET(type_name)                                                                  \
    template <>                                                                                    \
    struct ECS asset<type_name> : public basic_asset {                                             \
        using type = type_name;                                                                    \
                                                                                                   \
        ~asset()                        = default;                                                 \
        asset(const asset&)             = default;                                                 \
        asset& operator=(const asset&)  = delete;                                                  \
        asset(asset&& other) noexcept   = default;                                                 \
        asset& operator=(asset&& other) = delete;                                                  \
                                                                                                   \
        //

#define ASSET_TYPE(asset_type_)                                                                    \
    constexpr asset() : basic_asset{ ECS asset_type::asset_type_, {} } {}                          \
    //

#define END_REGISTER()                                                                             \
    }                                                                                              \
    ;                                                                                              \
    //
// NOLINTEND(cppcoreguidelines-macro-usage)
