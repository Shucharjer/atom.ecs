#include <cstddef>
#include <exception>
#include <format>
#include <iostream>
#include <string>
#include <core.hpp>
#include <core/pipeline.hpp>
#include <memory/pool.hpp>
#include <ranges/to.hpp>
#include "asset.hpp"
#include "command.hpp"
#include "ecs.hpp"
#include "queryer.hpp"
#include "resources.hpp"
#include "world.hpp"

using namespace atom::utils;
using namespace atom::ecs;

const auto print   = [](const auto&... val) { ((std::cout << val << ' '), ...); };
const auto println = [](const auto& val) { std::cout << val << '\n'; };
const auto newline = []() { std::cout << '\n'; };

void startup(command& command, queryer& queryer) {
    auto first_entity = command.spawn<std::string>("the first");
    println(first_entity);
    command.kill(first_entity);

    auto second_entity = command.spawn<std::string>("the second entity has string");
    println(second_entity);

    auto entities = queryer.query_all_of<>();
    std::ranges::for_each(entities, print);
    newline();
}

void update(command& command, queryer& queryer, float delta_time) {
    auto e = command.spawn<std::string>("siajdioasjdoijasd");

    auto entities = queryer.query_any_of<std::string>();
    for (const auto& entity : entities) {
        println(std::format(
            "I'm entity {}. My index is {}, and my generation is {}. besides, my string is: {}",
            entity,
            queryer.index(entity),
            queryer.generation(entity),
            queryer.get<std::string>(entity)
        ));
    }

    auto front = entities.front();
    println(std::format("current front: {}.", front));
    command.detach<std::string>(front);
    queryer.find<resources::garbage_collect::enable_garbage_collect>()->value = true;
}

void shutdown(command& command, queryer& queryer) {
    auto entities       = queryer.query_any_of<std::string>();
    auto owned_entities = ranges::to<std::vector>(entities);
    for (const auto& entity : owned_entities) {
        command.kill(entity);
        println("killed a entity");
    }
}

struct model {
    struct proxy {};

    using proxy_type = proxy;
    using key_type   = std::string;

    [[nodiscard]] auto type() -> asset_type { return asset_type::model; }

    [[nodiscard]] auto get_handle() const noexcept { return handle_; }

    auto set_handle(const resource_handle handle) noexcept { handle_ = handle; }

    [[nodiscard]] auto path() const noexcept -> const std::string& { return path_; }

private:
    std::string path_;
    resource_handle handle_;
};

void startup_model(command& command, queryer& queryer) {}

void update_model(command& command, queryer& queryer, float delta_time) {
    auto non_model_entities = queryer.query_non_of<model>();

    for (auto entity : non_model_entities) {
        command.attach<model>(entity);
        auto& curr_model = queryer.get<model>(entity);

        auto& hub           = hub::instance();
        library<model>& lib = hub.libs<model>();
        auto& table         = hub.tables<model>();
        auto& path          = curr_model.path();
        if (!table.contains(path)) {
            auto [handle, ptr] = lib.install(model::proxy{});
            table.emplace(path, handle);
        }
    }
}

void shutdown_model(command& command, queryer& queryer) {}

int main() {
    try {
        world world;

        world.add_startup(startup);
        world.add_update(update);
        world.add_shutdown(shutdown);

        // world.add_startup(startup_model);
        // world.add_update(update_model);
        // world.add_shutdown(shutdown_model);

        world.startup();
        world.update(0.F);
        world.update(0.F);
        world.shutdown();
    }
    catch (const std::exception& e) {
        println(e.what());
    }

    return 0;
}
