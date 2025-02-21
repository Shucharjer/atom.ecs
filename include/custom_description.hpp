#pragma once
#include <reflection.hpp>

namespace atom::ecs::bits {

enum description_bits : utils::description_bits_base {
    is_component = 0b0000000000000000000000000000000100000000000000000000000000000000,
    is_resource  = 0b0000000000000000000000000000001000000000000000000000000000000000,
};

} // namespace atom::ecs::bits
