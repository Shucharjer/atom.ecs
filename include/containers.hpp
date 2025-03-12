#pragma once
#include <concepts>
#include <deque>
#include <functional>
#include <list>
#include <map>
#include <queue>
#include <set>
#include <stack>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <structures.hpp>
#include <structures/dense_map.hpp>
#include <structures/dense_set.hpp>

namespace atom {

namespace ecs {

#define ATOM_USE_PMR_CONTAINER
#ifndef ATOM_USE_PMR_CONTAINER

// TODO: containers

#else
template <typename Ty>
using vector = std::pmr::vector<Ty>;

template <typename Ty>
using list = std::pmr::list<Ty>;

template <typename Ty>
using deque = std::pmr::deque<Ty>;

template <typename Ty, typename Container = deque<Ty>>
using queue = std::queue<Ty, Container>;

template <typename Ty, typename Container = vector<Ty>, typename Pr = std::less<Ty>>
using priority_queue = std::priority_queue<Ty, Container, Ty>;

template <typename Ty, typename Container = deque<Ty>>
using stack = std::stack<Ty, Container>;

template <typename Kty, typename Pr = std::less<Kty>>
using set = std::pmr::set<Kty, Pr>;

template <typename Kty, typename Pr = std::less<Kty>>
using multiset = std::pmr::multiset<Kty, Pr>;

template <typename Kty, typename Ty, typename Pr = std::less<Kty>>
using map = std::pmr::map<Kty, Ty, Pr>;

template <typename Kty, typename Ty, typename Pr = std::less<Kty>>
using multimap = std::pmr::multimap<Kty, Ty, Pr>;

template <typename Kty, typename Hasher = std::hash<Kty>, typename Keyeq = std::equal_to<Kty>>
using unordered_set = std::pmr::unordered_set<Kty, Hasher, Keyeq>;

template <typename Kty, typename Hasher = std::hash<Kty>, typename Keyeq = std::equal_to<Kty>>
using unordered_multiset = std::pmr::unordered_multiset<Kty, Hasher, Keyeq>;

template <
    typename Kty,
    typename Val,
    typename Hasher = std::hash<Kty>,
    typename Keyeq  = std::equal_to<Kty>>
using unordered_map = std::pmr::unordered_map<Kty, Val, Hasher, Keyeq>;

template <
    typename Kty,
    typename Val,
    typename Hasher = std::hash<Kty>,
    typename Keyeq  = std::equal_to<Kty>>
using unordered_multimap = std::pmr::unordered_multimap<Kty, Val, Hasher, Keyeq>;

template <
    std::unsigned_integral Kty,
    typename Ty,
    std::size_t PageSize = utils::k_default_page_size>
using dense_map = utils::pmr::dense_map<Kty, Ty, PageSize>;

#endif

} // namespace ecs

template <typename Ty>
using vector = ecs::vector<Ty>;

template <typename Ty>
using list = ecs::list<Ty>;

template <typename Ty>
using deque = ecs::deque<Ty>;

template <typename Ty, typename Container = deque<Ty>>
using queue = ecs::queue<Ty, Container>;

template <typename Ty, typename Container = vector<Ty>, typename Pr = std::less<Ty>>
using priority_queue = ecs::priority_queue<Ty, Container, Ty>;

template <typename Ty, typename Container = deque<Ty>>
using stack = ecs::stack<Ty, Container>;

template <typename Kty, typename Pr = std::less<Kty>>
using set = ecs::set<Kty, Pr>;

template <typename Kty, typename Pr = std::less<Kty>>
using multiset = ecs::multiset<Kty, Pr>;

template <typename Kty, typename Ty, typename Pr = std::less<Kty>>
using map = ecs::map<Kty, Ty, Pr>;

template <typename Kty, typename Ty, typename Pr = std::less<Kty>>
using multimap = ecs::multimap<Kty, Ty, Pr>;

template <typename Kty, typename Hasher = std::hash<Kty>, typename Keyeq = std::equal_to<Kty>>
using unordered_set = ecs::unordered_set<Kty, Hasher, Keyeq>;

template <typename Kty, typename Hasher = std::hash<Kty>, typename Keyeq = std::equal_to<Kty>>
using unordered_multiset = ecs::unordered_multiset<Kty, Hasher, Keyeq>;

template <
    typename Kty,
    typename Val,
    typename Hasher = std::hash<Kty>,
    typename Keyeq  = std::equal_to<Kty>>
using unordered_map = ecs::unordered_map<Kty, Val, Hasher, Keyeq>;

template <
    typename Kty,
    typename Val,
    typename Hasher = std::hash<Kty>,
    typename Keyeq  = std::equal_to<Kty>>
using unordered_multimap = ecs::unordered_multimap<Kty, Val, Hasher, Keyeq>;

template <
    std::unsigned_integral Kty,
    typename Ty,
    std::size_t PageSize = utils::k_default_page_size>
using dense_map = ecs::dense_map<Kty, Ty, PageSize>;

template <typename Ty>
using shared_ptr = std::shared_ptr<Ty>;

} // namespace atom
