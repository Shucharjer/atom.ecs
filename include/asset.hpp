#pragma once
#include <memory>
#include <mutex>
#include <string>
#include <type_traits>
#include <auxiliary/singleton.hpp>
#include <reflection.hpp>
#include "containers.hpp"
#include "ecs.hpp"

namespace atom::ecs {

/**
 * @brief Asset type.
 *
 */
enum class asset_type : std::uint8_t {
    unknown,
    text,
    sound,
    model,
    shader,
    texture
};

template <asset_type type>
struct asset_string {
    constexpr static std::string_view value = "unknown";
};

/**
 * @brief If a resources is a kind of asset, it should extends this class. (CRTP)
 *
 * @tparam Ty
 */
template <concepts::asset Ty>
class asset {};

class basic_manager {};

template <typename Ty>
requires std::is_base_of_v<asset<Ty>, Ty>
class manager : public basic_manager {
public:
    manager()                          = default;
    manager(const manager&)            = delete;
    manager(manager&&)                 = delete;
    manager& operator=(const manager&) = delete;
    manager& operator=(manager&&)      = delete;
    ~manager()                         = default;

private:
    map<std::string_view, std::shared_ptr<Ty>> assets_;
};

class hub : public utils::singleton<hub> {
    friend class utils::singleton<hub>;
    hub() = default;

public:
    template <typename Asset>
    void add(Asset&& asset) {
        using pure          = std::remove_cvref_t<Asset>;
        constexpr auto hash = utils::hash_of<pure>();

        if (auto iter = managers_.find(hash); iter != managers_.end()) [[likely]] {
            auto& mng = iter->second;
        }
        else {
            auto& mng = managers_[hash] = std::make_unique<manager<Asset>>();
        }
    }

    template <typename Asset>
    void remove(Asset&& asset);

private:
    map<std::size_t, std::unique_ptr<basic_manager>> managers_;
};

} // namespace atom::ecs

#if __has_include(<nlohmann/json.hpp>)
    #include <nlohmann/json.hpp>

namespace atom::ecs {
template <typename Ty>
requires concepts::asset<Ty>
inline void to_json(nlohmann::json& json, const Ty& asset) {
    json = nlohmann::json{
        { "type", asset.type },
        { "path", asset.path }
    };
}

// template <typename Ty>
// requires concepts::asset<Ty>
// inline void from_json(const nlohmann::json& json, Ty& asset) {
//     json.at("type").get_to(asset.type);
//     json.at("path").get_to(asset.path);
// }

} // namespace atom::ecs
#endif

template <>
struct ::atom::ecs::asset_string<atom::ecs::asset_type::text> {
    constexpr static std::string_view value = "text";
};

template <>
struct ::atom::ecs::asset_string<atom::ecs::asset_type::sound> {
    constexpr static std::string_view value = "text";
};

template <>
struct ::atom::ecs::asset_string<atom::ecs::asset_type::model> {
    constexpr static std::string_view value = "text";
};

template <>
struct ::atom::ecs::asset_string<atom::ecs::asset_type::shader> {
    constexpr static std::string_view value = "text";
};

template <>
struct ::atom::ecs::asset_string<atom::ecs::asset_type::texture> {
    constexpr static std::string_view value = "text";
};
