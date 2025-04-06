#pragma once
#include <core/type_traits.hpp>
#include "containers.hpp"

namespace atom::ecs {

/**
 * @brief Id generator.
 *
 * generate(), destroy(), then emplace().
 * Now you could store object via `dense_map`.
 * @tparam IdType
 */
template <typename IdType = std::uint32_t>
class generator {
public:
    using id_type   = IdType;
    using half_type = utils::half_size_t<IdType>;

    constexpr static size_t half_bits = sizeof(half_type) * 8;

    generator() : free_indices_(), generations_(1, 0), vaild_ids_() {}
    generator(const generator&)            = delete;
    generator(generator&&)                 = delete;
    generator& operator=(const generator&) = delete;
    generator& operator=(generator&&)      = delete;
    ~generator()                           = default;

    auto generate() -> id_type {
        if (free_indices_.empty()) {
            half_type index = (half_type)generations_.size();
            generations_.emplace_back(0);
            id_type id = (id_type)index << half_bits;
            vaild_ids_.emplace(id);
            return id;
        }
        else {
            half_type index = free_indices_.back();
            ++generations_[index];
            id_type id = ((id_type)index << half_bits) + generations_[index];
            free_indices_.pop_back();
            vaild_ids_.emplace(id);
            return id;
        }
    }

    auto destroy(const id_type id) {
        if (auto iter = vaild_ids_.find(id); iter != vaild_ids_.end()) [[likely]] {
            vaild_ids_.erase(iter);
        }
    }

    void emplace(const id_type id) { free_indices_.emplace_back(id >> half_bits); }

    constexpr auto separate(const id_type id) const noexcept -> std::pair<half_type, half_type> {
        half_type idx = id >> half_bits;
        half_type gen = (id << half_bits) >> half_bits;
        return { idx, gen };
    }

    auto living() const noexcept -> const set<IdType>& { return vaild_ids_; }

private:
    vector<half_type> free_indices_;
    vector<half_type> generations_;
    set<IdType> vaild_ids_;
    // define this at outer has more freedom.
    // vector<IdType> pending_destroy_;
};

} // namespace atom::ecs
