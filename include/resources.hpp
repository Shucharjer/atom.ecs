#pragma once
#include <string>
#include "containers.hpp"

namespace resources::garbage_collect {

struct enable_garbage_collect {
    bool value = false;
};

} // namespace resources::garbage_collect

namespace atom::ecs {

template <typename Ty>
class manager {
public:
    manager()                                   = default;
    manager(const manager&)                     = delete;
    manager& operator=(const manager&)          = delete;
    manager& operator=(manager&& that) noexcept = delete;
    ~manager()                                  = default;

    manager(manager&& that) noexcept : resources_(std::move(that.resources_)) {}

    std::unique_ptr<Ty>& operator[](const std::string& name) noexcept { return resources_[name]; }
    const std::unique_ptr<Ty>& operator[](const std::string& name) const noexcept {
        return resources_[name];
    }

    template <typename T>
    auto emplace(const std::string& name, T&& val) {
        return resources_.emplace(name, std::make_unique<Ty>(std::forward<T>(val)));
    }

    template <typename T>
    auto emplace(std::string&& name, T&& val) {
        return resources_.emplace(std::move(name), std::make_unique<Ty>(std::forward<T>(val)));
    }

    void erase(const std::string& name) noexcept { resources_.erase(name); }

private:
    map<std::string, std::unique_ptr<Ty>> resources_;
};

} // namespace atom::ecs
