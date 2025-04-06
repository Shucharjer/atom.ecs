#pragma once
#include <cstdint>
#include <memory>
#include <shared_mutex>
#include <auxiliary/singleton.hpp>
#include <concepts/type.hpp>
#include <core/langdef.hpp>
#include <reflection.hpp>
#include "containers.hpp"
#include "ecs.hpp"
#include "thread/coroutine.hpp"

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
    material,
    texture,
    shader,
    shader_program
};

template <asset_type type>
struct asset_string {
    constexpr static std::string_view value = "unknown";
};

template <typename Asset>
struct proxy {
    using type = Asset::proxy_type;
};

template <typename Asset>
using proxy_t = typename proxy<Asset>::type;

class basic_library {
public:
    basic_library()                                = default;
    basic_library(const basic_library&)            = delete;
    basic_library(basic_library&&)                 = delete;
    basic_library& operator=(const basic_library&) = delete;
    basic_library& operator=(basic_library&&)      = delete;
    virtual ~basic_library()                       = default;
};

template <typename Asset>
class library : public basic_library {
public:
    using proxy_type = proxy_t<Asset>;

    library() : mutex_(), generator_(generate()), assets_() {}
    library(const library&)            = delete;
    library(library&&)                 = delete;
    library& operator=(const library&) = delete;
    library& operator=(library&&)      = delete;
    ~library() override                = default;

    auto install(proxy_t<Asset>&& proxy) -> std::pair<resource_handle, shared_ptr<proxy_t<Asset>>> {
        auto handle    = generator_.get();
        auto proxy_ptr = std::make_shared<proxy_type>(std::move(proxy));
        std::unique_lock<std::shared_mutex> ulock(mutex_);
        assets_.emplace(handle, proxy_ptr);
        ulock.unlock();
        return std::make_pair(handle, proxy_ptr);
    }

    auto install(const shared_ptr<proxy_t<Asset>>& proxy) -> resource_handle {
        auto handle = generator_.get();
        std::unique_lock<std::shared_mutex> ulock(mutex_);
        assets_.emplace(handle, proxy);
        ulock.unlock();
        return handle;
    }

    auto install(shared_ptr<proxy_t<Asset>>&& proxy) -> resource_handle {
        auto handle = generator_.get();
        std::unique_lock<std::shared_mutex> ulock(mutex_);
        assets_.emplace(handle, std::move(proxy));
        ulock.unlock();
        return handle;
    }

    [[nodiscard]] bool contains(const resource_handle handle) noexcept {
        if (!handle) {
            return false;
        }

        std::shared_lock<std::shared_mutex> slock(mutex_);
        return assets_.contains(handle);
    }

    [[nodiscard]] auto fetch(const resource_handle handle) noexcept -> shared_ptr<proxy_t<Asset>> {
        std::shared_lock<std::shared_mutex> slock(mutex_);
        return assets_.at(handle);
    }

    auto uninstall(const resource_handle handle) noexcept {
        std::unique_lock<std::shared_mutex> ulock(mutex_);
        assets_.erase(handle);
    }

private:
    [[nodiscard]] static utils::thread_safe_coroutine<resource_handle> generate() noexcept {
        resource_handle current{};
        while (true) {
            // NOTE: start from 1, which means 0 is invaild.
            co_yield ++current;
        }
    }

    std::shared_mutex mutex_;
    utils::thread_safe_coroutine<resource_handle> generator_;
    map<resource_handle, shared_ptr<proxy_t<Asset>>> assets_;
};

class basic_table {
public:
    basic_table()                              = default;
    basic_table(const basic_table&)            = delete;
    basic_table(basic_table&&)                 = delete;
    basic_table& operator=(const basic_table&) = delete;
    basic_table& operator=(basic_table&&)      = delete;
    virtual ~basic_table()                     = default;
};

template <typename Asset>
class table : public basic_table {
public:
    table() : mutex_(), mapping_() {}
    table(const table&)            = delete;
    table(table&&)                 = delete;
    table& operator=(const table&) = delete;
    table& operator=(table&&)      = delete;
    ~table() override              = default;

    using key_type = typename Asset::key_type;

    bool contains(const key_type& key) noexcept {
        std::shared_lock<std::shared_mutex> slock(mutex_);
        return mapping_.contains(key);
    }

    /**
     * @brief Emplace a pair to table.
     * If the key is already exists, its counter will increase by 1.
     *
     */
    void emplace(const key_type& key, const resource_handle handle) noexcept {
        std::unique_lock<std::shared_mutex> ulock(mutex_);
        if (auto iter = mapping_.find(key); iter != mapping_.end()) {
            ++(iter->second.second);
        }
        else {
            mapping_.emplace(key, std::pair<resource_handle, uint32_t>(handle, 1));
        }
    }

    resource_handle at(const key_type& key) noexcept {
        std::shared_lock<std::shared_mutex> slock(mutex_);
        return mapping_.at(key).first;
    }

    uint32_t count(const key_type& key) noexcept {
        std::shared_lock<std::shared_mutex> slock(mutex_);
        return mapping_.at(key).second;
    }

    /**
     * @brief Erase a pair in table.
     * Counter dec.
     */
    void erase(const key_type& key) noexcept {
        std::shared_lock<std::shared_mutex> slock(mutex_);
        if (auto iter = mapping_.find(key); iter != mapping_.end()) [[likely]] {
            auto& [handle, count] = iter->second;
            slock.unlock();
            std::unique_lock<std::shared_mutex> ulock(mutex_);
            --count;
            if (!count) {
                mapping_.erase(iter);
            }
        }
    }

    /**
     * @brief Erase a pair in table.
     * Counter dec.
     */
    void erase(library<Asset>& library, const key_type& key) noexcept {
        std::shared_lock<std::shared_mutex> slock(mutex_);
        if (auto iter = mapping_.find(key); iter != mapping_.end()) [[likely]] {
            auto& [handle, count] = iter->second;
            slock.unlock();
            std::unique_lock<std::shared_mutex> ulock(mutex_);
            --count;
            if (!count) {
                ulock.unlock();
                library.uninstall(handle);
                ulock.lock();

                mapping_.erase(iter);
            }
        }
    }

private:
    std::shared_mutex mutex_;
    map<typename Asset::key_type, std::pair<resource_handle, uint32_t>> mapping_;
};

class hub : public utils::singleton<hub> {
    friend class utils::singleton<hub>;
    hub() = default;

    template <concepts::asset Asset, std::size_t hash = utils::hash_of<Asset>()>
    auto find_lib() -> std::unique_ptr<basic_library>& {
        if (auto iter = libs_.find(hash); iter != libs_.end()) [[likely]] {
            return iter->second;
        }
        else [[unlikely]] {
            auto ptr  = std::make_unique<ecs::library<Asset>>();
            auto& lib = libs_[hash] = std::move(ptr);
            return lib;
        }
    }

    template <concepts::asset Asset, std::size_t hash = utils::hash_of<Asset>()>
    auto find_table() -> std::unique_ptr<basic_table>& {
        if (auto iter = tables_.find(hash); iter != tables_.end()) [[likely]] {
            return iter->second;
        }
        else [[unlikely]] {
            auto ptr    = std::make_unique<ecs::table<Asset>>();
            auto& table = tables_[hash] = std::move(ptr);
            return table;
        }
    }

public:
    // NOTE: generally, we will not uninstall a whole library.
    template <concepts::asset Asset>
    [[nodiscard]] auto library() -> library<Asset>& {
        static_assert(utils::concepts::pure<Asset>);
        std::unique_ptr<basic_library>& lib = find_lib<Asset>();
        auto* ptr                           = lib.get();
        return *static_cast<ecs::library<Asset>*>(ptr);
    }

    template <concepts::asset Asset>
    [[nodiscard]] auto table() -> table<Asset>& {
        static_assert(utils::concepts::pure<Asset>);
        std::unique_ptr<basic_table>& tab = find_table<Asset>();
        auto* ptr                         = tab.get();
        return *static_cast<ecs::table<Asset>*>(ptr);
    }

private:
    // NOTE: when running for a while, we will not emplace or erase library, we need not to lock
    // these containers.
    map<std::size_t, std::unique_ptr<basic_library>> libs_;
    map<std::size_t, std::unique_ptr<basic_table>> tables_;
};

} // namespace atom::ecs

template <>
struct atom::ecs::asset_string<atom::ecs::asset_type::text> {
    constexpr static std::string_view value = "text";
};

template <>
struct atom::ecs::asset_string<atom::ecs::asset_type::sound> {
    constexpr static std::string_view value = "sound";
};

template <>
struct atom::ecs::asset_string<atom::ecs::asset_type::model> {
    constexpr static std::string_view value = "model";
};

template <>
struct atom::ecs::asset_string<atom::ecs::asset_type::material> {
    constexpr static std::string_view value = "material";
};

template <>
struct atom::ecs::asset_string<atom::ecs::asset_type::texture> {
    constexpr static std::string_view value = "texture";
};

template <>
struct atom::ecs::asset_string<atom::ecs::asset_type::shader> {
    constexpr static std::string_view value = "shader";
};

template <>
struct atom::ecs::asset_string<atom::ecs::asset_type::shader_program> {
    constexpr static std::string_view value = "shader_program";
};
