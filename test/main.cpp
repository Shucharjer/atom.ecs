#include <iostream>
#include <string>
#include <core.hpp>
#include <core/pipeline.hpp>
#include <memory/pool.hpp>
#include <ranges/to.hpp>
#include "command.hpp"
#include "queryer.hpp"
#include "world.hpp"

using namespace atom;

const auto print = [](const auto& val) { std::cout << val << '\n'; };

void startup(ecs::command& command, ecs::queryer& queryer) {
    auto first_entity = command.spawn<std::string>(std::string{ "asoijdasjd" });
    print(first_entity);
}

void update(ecs::command& command, ecs::queryer& queryer) {
    auto entities = queryer.query_any_of<std::string>();
    for (const auto& entity : entities) {
        print(entity);
        print(queryer.get<std::string>(entity));
    }
}

void shutdown(ecs::command& command, ecs::queryer& queryer) {
    auto entities       = queryer.query_any_of<std::string>();
    auto owned_entities = utils::ranges::to<std::vector>(entities);
    for (const auto& entity : owned_entities) {
        command.kill(entity);
        print("killed a entity\n");
    }
}

int main() {
    ecs::world world;
    world.add_startup(startup);
    world.add_update(update);
    world.add_shutdown(shutdown);
    world.startup();
    world.update();
    world.shutdown();

    return 0;
}
