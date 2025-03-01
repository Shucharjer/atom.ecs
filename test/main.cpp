#include <cstddef>
#include <iostream>
#include <string>
#include <core.hpp>
#include <core/pipeline.hpp>
#include <memory/pool.hpp>
#include <ranges/to.hpp>
#include "command.hpp"
#include "queryer.hpp"
#include "resources.hpp"
#include "world.hpp"

using namespace atom;

const auto print = [](const auto&... val) { ((std::cout << val << ' '), ...); };

const auto newline = []() { std::cout << '\n'; };

void startup(ecs::command& command, ecs::queryer& queryer) {
    auto first_entity = command.spawn<std::string>("the first");
    print(first_entity);
    command.kill(first_entity);
    auto second_entity = command.spawn<std::string>("the second entity has string");
    print(second_entity);
    newline();
    auto entities = queryer.query_all_of<>();
    std::ranges::for_each(entities, print);
    newline();
}

void update(ecs::command& command, ecs::queryer& queryer, float delta_time) {
    command.spawn<std::string>();
    auto entities = queryer.query_any_of<std::string>();
    for (const auto& entity : entities) {
        print(
            "I'm entity",
            entity,
            "my index is",
            queryer.index(entity),
            "my generation is",
            queryer.generation(entity)
        );
        newline();
    }

    command.detach<std::string>(entities.front());
    command.set<resources::garbage_collect::enable_garbage_collect&&>({ true });
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
    world.update(0.F);
    world.update(0.F);
    world.shutdown();

    return 0;
}
