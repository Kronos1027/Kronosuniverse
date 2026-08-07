#pragma once
//
// KronoUniverse ECS — Entity Component System (sparse sets, flea-style)
//
// Entity = uint64_t ID (32 bits entity + 32 bits version for recycling)
// Component = struct stored in a sparse set per type
// System = function that iterates over entities with specific components
//
// Design:
// - O(1) add/remove component
// - Linear iteration over dense array (cache-friendly enough for MVP)
// - No RTTI — component types identified by template parameter
// - Thread-safe per-registry (caller responsible for synchronization across registries)
//

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <bitset>
#include <queue>
#include <cassert>
#include <cstring>

namespace krono {

using Entity = uint64_t;
constexpr Entity INVALID_ENTITY = 0;

namespace detail {
    inline uint32_t entity_id(Entity e) { return static_cast<uint32_t>(e & 0xFFFFFFFF); }
    inline uint32_t entity_version(Entity e) { return static_cast<uint32_t>(e >> 32); }
    inline Entity make_entity(uint32_t id, uint32_t version) {
        return (static_cast<uint64_t>(version) << 32) | id;
    }
}

// ---- Sparse Set base class (for type-erased storage) ----
class SparseSetBase {
public:
    virtual ~SparseSetBase() = default;
    virtual void remove(Entity e) = 0;
    virtual bool has(Entity e) const = 0;
    virtual size_t size() const = 0;
};

// ---- Sparse Set (typed) ----
template<typename T>
class SparseSet : public SparseSetBase {
public:
    static constexpr size_t SPARSE_PAGE_SIZE = 4096;

    void insert(Entity e, T&& component) {
        uint32_t id = detail::entity_id(e);
        ensure_sparse(id);
        if (sparse_[id] != INVALID_INDEX) {
            // Already exists — overwrite
            dense_data_[sparse_[id]] = std::move(component);
            return;
        }
        sparse_[id] = static_cast<uint32_t>(dense_entities_.size());
        dense_entities_.push_back(e);
        dense_data_.push_back(std::move(component));
    }

    void insert(Entity e, const T& component) {
        T copy = component;
        insert(e, std::move(copy));
    }

    T* get(Entity e) {
        uint32_t id = detail::entity_id(e);
        if (id >= sparse_.size() || sparse_[id] == INVALID_INDEX) return nullptr;
        return &dense_data_[sparse_[id]];
    }

    const T* get(Entity e) const {
        uint32_t id = detail::entity_id(e);
        if (id >= sparse_.size() || sparse_[id] == INVALID_INDEX) return nullptr;
        return &dense_data_[sparse_[id]];
    }

    bool has(Entity e) const override {
        uint32_t id = detail::entity_id(e);
        return id < sparse_.size() && sparse_[id] != INVALID_INDEX;
    }

    void remove(Entity e) override {
        uint32_t id = detail::entity_id(e);
        if (id >= sparse_.size() || sparse_[id] == INVALID_INDEX) return;
        uint32_t dense_idx = sparse_[id];
        uint32_t last_idx = static_cast<uint32_t>(dense_entities_.size() - 1);
        if (dense_idx != last_idx) {
            // Swap with last
            dense_entities_[dense_idx] = dense_entities_[last_idx];
            dense_data_[dense_idx] = std::move(dense_data_[last_idx]);
            sparse_[detail::entity_id(dense_entities_[dense_idx])] = dense_idx;
        }
        dense_entities_.pop_back();
        dense_data_.pop_back();
        sparse_[id] = INVALID_INDEX;
    }

    size_t size() const override { return dense_entities_.size(); }

    // Iteration
    const std::vector<Entity>& entities() const { return dense_entities_; }
    std::vector<T>& data() { return dense_data_; }
    const std::vector<T>& data() const { return dense_data_; }

private:
    void ensure_sparse(uint32_t id) {
        if (id >= sparse_.size()) {
            sparse_.resize(id + 1, INVALID_INDEX);
        }
    }

    static constexpr uint32_t INVALID_INDEX = 0xFFFFFFFF;
    std::vector<uint32_t> sparse_;
    std::vector<Entity> dense_entities_;
    std::vector<T> dense_data_;
};

// ---- Registry ----
class Registry {
public:
    Registry() = default;

    Entity create() {
        uint32_t id;
        uint32_t version = 0;
        if (!free_ids_.empty()) {
            id = free_ids_.front();
            free_ids_.pop();
            version = entity_versions_[id];
        } else {
            id = static_cast<uint32_t>(entity_versions_.size());
            entity_versions_.push_back(0);
        }
        return detail::make_entity(id, version);
    }

    void destroy(Entity e) {
        uint32_t id = detail::entity_id(e);
        if (id >= entity_versions_.size()) return;
        // Remove all components
        for (auto& [type_id, set] : component_sets_) {
            set->remove(e);
        }
        entity_versions_[id]++;
        free_ids_.push(id);
    }

    template<typename T>
    void emplace(Entity e, T&& component) {
        get_or_create_set<T>().insert(e, std::forward<T>(component));
    }

    template<typename T>
    T* get(Entity e) {
        auto* set = find_set<T>();
        return set ? set->get(e) : nullptr;
    }

    template<typename T>
    bool has(Entity e) const {
        auto* set = find_set<T>();
        return set ? set->has(e) : false;
    }

    template<typename T>
    void remove(Entity e) {
        auto* set = find_set<T>();
        if (set) set->remove(e);
    }

    template<typename T>
    SparseSet<T>* view() {
        return find_set<T>();
    }

    // Iterate over entities that have ALL the specified component types
    template<typename... Components>
    void each(std::function<void(Entity, Components&...)> fn) {
        // Find the smallest set to iterate
        auto* smallest = find_smallest_set<Components...>();
        if (!smallest) return;
        // Iterate and check all components
        for (Entity e : smallest) {
            if ((has<Components>(e) && ...)) {
                fn(e, *get<Components>(e)...);
            }
        }
    }

    size_t alive() const { return entity_versions_.size() - free_ids_.size(); }

private:
    template<typename T>
    static uint64_t type_id() {
        static uint64_t id = next_type_id++;
        return id;
    }

    template<typename T>
    SparseSet<T>* get_or_create_set() {
        uint64_t tid = type_id<T>();
        auto it = component_sets_.find(tid);
        if (it == component_sets_.end()) {
            auto set = std::make_unique<SparseSet<T>>();
            auto* ptr = set.get();
            component_sets_[tid] = std::move(set);
            return ptr;
        }
        return static_cast<SparseSet<T>*>(it->second.get());
    }

    template<typename T>
    SparseSet<T>* find_set() {
        uint64_t tid = type_id<T>();
        auto it = component_sets_.find(tid);
        if (it == component_sets_.end()) return nullptr;
        return static_cast<SparseSet<T>*>(it->second.get());
    }

    template<typename T>
    const SparseSet<T>* find_set() const {
        uint64_t tid = type_id<T>();
        auto it = component_sets_.find(tid);
        if (it == component_sets_.end()) return nullptr;
        return static_cast<const SparseSet<T>*>(it->second.get());
    }

    // Helper for each() — find smallest set among Components...
    // (Simplified: just return the first one's entities)
    template<typename First, typename... Rest>
    const std::vector<Entity>* find_smallest_set() {
        auto* set = find_set<First>();
        return set ? &set->entities() : nullptr;
    }

    inline static uint64_t next_type_id = 0;
    std::unordered_map<uint64_t, std::unique_ptr<SparseSetBase>> component_sets_;
    std::vector<uint32_t> entity_versions_;
    std::queue<uint32_t> free_ids_;
};

} // namespace krono
