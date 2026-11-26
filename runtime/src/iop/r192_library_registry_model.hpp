#pragma once
// Isolated ownership/transition model, NOT installed into the runtime.
// Original LOADCORE Register6/Release7 + pinned SDK are the authority.
// Guest record addresses, encoded instructions and interrupt adapters belong
// only to the independent test projection, never to this native model.
#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ps2::iop::library_model {
struct CallableIdentity {};
struct Client;
enum class Marker { canonical, linked, malformed };
struct Table {
    std::array<char, 8> name{};
    std::uint16_t version{}, flags{};
    Marker marker{Marker::canonical};
    Table *previous{};
    Client *clients{};
    std::vector<const CallableIdentity *> functions;
};
struct Client {
    std::array<char, 8> name{};
    std::uint16_t version{}, flags{};
    Client *next{};
    std::vector<std::uint16_t> ordinals;
    std::vector<const CallableIdentity *> targets;
    std::uint64_t binding_generation{};
    bool retired{};
};
enum class Field { table_clients, table_previous, table_marker, table_flags, client_next, client_flags, stub_target, head, pending };
struct Change {
    Field field{};
    Table *table{}, *table_value{};
    Client *client{}, *client_value{};
    const CallableIdentity *target{};
    std::size_t slot{};
    std::uint16_t flags{};
};
class Registry {
public:
    static constexpr std::int32_t ok = 0, found = -212, not_found = -213, illegal = -214, in_use = -215;
    std::function<void(const Change &)> observe;
    Table *head() const { return head_; }
    Client *pending() const { return pending_; }
    bool poisoned() const { return poisoned_; }
    bool contains(const Table *value) const { return tables_.count(const_cast<Table *>(value)) != 0; }
    // Seed an explicitly proved existing-import precondition. This does NOT
    // implement/admit the complete Link/UnLinkLibraryEntries entrypoints.
    void seed_client(std::shared_ptr<Client> client, Table *owner = nullptr) {
        if (!client || client->retired || clients_.count(client.get()) || (owner && !contains(owner))) refuse("unproved_client_seed");
        if (owner && (owner->name != client->name || (owner->version >> 8) != (client->version >> 8))) refuse("unproved_client_compatibility");
        Transition transition(*this);
        auto *raw = client.get(); clients_.emplace(raw, std::move(client));
        raw->targets.resize(raw->ordinals.size());
        if (owner) {
            for (std::size_t i = 0; i < raw->ordinals.size(); ++i)
                raw->targets[i] = raw->ordinals[i] < owner->functions.size() ? owner->functions[raw->ordinals[i]] : nullptr;
            raw->flags = (raw->flags & ~6u) | 2u; raw->next = owner->clients; owner->clients = raw;
            ++raw->binding_generation;
        } else { raw->next = pending_; pending_ = raw; }
        validate(); transition.finish();
    }
    std::int32_t register_table(std::shared_ptr<Table> table) {
        if (!table) refuse("unadmitted_null_table");
        validate();
        if (table->marker != Marker::canonical) return illegal;
        for (auto *entry : table->functions) if (!entry) refuse("unproved_callable_table");
        // Reject before moving clients or allocating a new ownership node.
        for (auto *old = head_; old; old = old->previous)
            if (old->name == table->name && (old->version >> 8) == (table->version >> 8) &&
                (table->version & 255u) <= (old->version & 255u)) return found;
        Transition transition(*this);
        auto *raw = table.get(); tables_.emplace(raw, table);
        Client *moving{};
        for (auto *old = head_; old; old = old->previous) {
            if (old->name != table->name || (old->version >> 8) != (table->version >> 8)) continue;
            auto *current = old->clients; set_clients(old, nullptr);
            Client *last_locked{};
            while (current) {
                auto *next = current->next;
                if (current->flags & 1u) {
                    if (last_locked) set_next(last_locked, current); else set_clients(old, current);
                    last_locked = current; set_next(current, nullptr);
                } else { set_next(current, moving); moving = current; }
                current = next;
            }
        }
        Client *previous{}, *current = pending_;
        while (current) {
            auto *next = current->next;
            if (current->name == table->name && (current->version >> 8) == (table->version >> 8)) {
                set_next(current, moving); moving = current;
                if (previous) set_next(previous, next); else { pending_ = next; emit({.field=Field::pending, .client_value=next}); }
            } else previous = current;
            current = next;
        }
        set_clients(raw, nullptr);
        while (moving) {
            auto *next = moving->next;
            moving->targets.resize(moving->ordinals.size());
            for (std::size_t i = 0; i < moving->ordinals.size(); ++i) {
                const auto ordinal = moving->ordinals[i];
                moving->targets[i] = ordinal < raw->functions.size() ? raw->functions[ordinal] : nullptr;
                emit({.field=Field::stub_target, .client=moving, .target=moving->targets[i], .slot=i});
            }
            moving->flags = (moving->flags & ~6u) | 2u;
            ++moving->binding_generation;
            emit({.field=Field::client_flags, .client=moving, .flags=moving->flags});
            set_next(moving, raw->clients); set_clients(raw, moving); moving = next;
        }
        raw->flags &= ~1u; emit({.field=Field::table_flags, .table=raw, .flags=raw->flags});
        raw->previous = head_; raw->marker = Marker::linked;
        emit({.field=Field::table_previous, .table=raw, .table_value=head_});
        head_ = raw; emit({.field=Field::head, .table_value=raw});
        validate(); transition.finish(); return ok;
    }
    std::int32_t release_table(const std::shared_ptr<Table> &table) {
        if (!table) refuse("unadmitted_null_table");
        validate(); Table *previous{}, *current = head_;
        while (current && current != table.get()) { previous = current; current = current->previous; }
        if (current != table.get()) return not_found;
        if (current->clients) return in_use;
        Transition transition(*this);
        set_clients(current, nullptr);
        if (previous) { previous->previous = current->previous; emit({.field=Field::table_previous, .table=previous, .table_value=current->previous}); }
        else { head_ = current->previous; emit({.field=Field::head, .table_value=head_}); }
        current->marker = Marker::canonical; emit({.field=Field::table_marker, .table=current});
        current->previous = nullptr; // Canonical marker replaces this union role.
        tables_.erase(current); validate(); transition.finish(); return ok;
    }
    // One source-validated inline import record. Full memory-range scanning is
    // an adapter responsibility, not an invented native pointer traversal.
    std::int32_t link_client(std::shared_ptr<Client> client) {
        if (!client) refuse("unadmitted_null_client");
        if (client->retired) refuse("client_owner_retired");
        validate();
        if (client->flags & 7u) return ok;
        Table *owner = head_;
        while (owner && ((owner->flags & 1u) || owner->name != client->name || (owner->version >> 8) != (client->version >> 8))) owner = owner->previous;
        if (!owner) return -1;
        Transition transition(*this);
        auto *raw = client.get(); raw->targets.resize(raw->ordinals.size());
        clients_.emplace(raw, client);
        for (std::size_t i = 0; i < raw->ordinals.size(); ++i) {
            const auto ordinal = raw->ordinals[i];
            raw->targets[i] = ordinal < owner->functions.size() ? owner->functions[ordinal] : nullptr;
            emit({.field=Field::stub_target, .client=raw, .target=raw->targets[i], .slot=i});
        }
        raw->flags = (raw->flags & ~6u) | 2u; emit({.field=Field::client_flags, .client=raw, .flags=raw->flags});
        set_next(raw, owner->clients); set_clients(owner, raw);
        ++raw->binding_generation;
        validate(); transition.finish(); return ok;
    }
    std::int32_t unlink_client(const std::shared_ptr<Client> &client) {
        if (!client) refuse("unadmitted_null_client");
        if (client->retired) refuse("client_owner_retired");
        validate(); auto *raw = client.get();
        for (auto *table = head_; table; table = table->previous) {
            Client *previous{}, *current = table->clients;
            while (current && current != raw) { previous = current; current = current->next; }
            if (current != raw) continue;
            Transition transition(*this);
            if (previous) { set_next(previous, raw->next); set_next(raw, nullptr); }
            else set_clients(table, raw->next); // Original head removal retains the detached next word.
            raw->flags &= ~7u; emit({.field=Field::client_flags, .client=raw, .flags=raw->flags});
            for (std::size_t i = 0; i < raw->targets.size(); ++i) {
                raw->targets[i] = nullptr; emit({.field=Field::stub_target, .client=raw, .slot=i});
            }
            ++raw->binding_generation;
            validate(); transition.finish(); return ok;
        }
        Client *previous{}, *current = pending_;
        while (current && current != raw) { previous = current; current = current->next; }
        if (current == raw) {
            Transition transition(*this);
            raw->flags &= ~7u; emit({.field=Field::client_flags, .client=raw, .flags=raw->flags});
            for (std::size_t i = 0; i < raw->targets.size(); ++i) {
                raw->targets[i] = nullptr; emit({.field=Field::stub_target, .client=raw, .slot=i});
            }
            auto *next = raw->next; set_next(raw, nullptr);
            if (previous) set_next(previous, next); else { pending_ = next; emit({.field=Field::pending, .client_value=next}); }
            ++raw->binding_generation;
            validate(); transition.finish();
        }
        return ok;
    }
    void retire_client(const std::shared_ptr<Client> &client) {
        if (!client) refuse("unadmitted_null_client");
        if (client->retired) refuse("client_owner_retired");
        validate();
        for (auto *table = head_; table; table = table->previous)
            for (auto *c = table->clients; c; c = c->next) if (c == client.get()) refuse("client_still_linked");
        for (auto *c = pending_; c; c = c->next) if (c == client.get()) refuse("client_still_pending");
        for (const auto &[pointer, owner] : clients_)
            if (pointer != client.get() && pointer->next == client.get()) refuse("client_has_retained_next_reference");
        clients_.erase(client.get());
        client->retired = true; client->next = nullptr; client->targets.clear(); ++client->binding_generation;
    }
    void validate() const {
        if (poisoned_) refuse("registry_transition_fault_requires_rebuild");
        std::unordered_set<Table *> tables; std::unordered_set<Client *> clients;
        auto chain = [&](Client *client) {
            for (; client; client = client->next)
                if (!clients_.count(client) || !clients.insert(client).second || client->retired || client->targets.size() != client->ordinals.size()) refuse("unproved_client_graph");
        };
        for (auto *table = head_; table; table = table->previous) {
            if (!tables_.count(table) || !tables.insert(table).second || table->marker != Marker::linked) refuse("unproved_library_graph");
            chain(table->clients);
        }
        if (tables.size() != tables_.size()) refuse("unproved_library_membership");
        chain(pending_);
    }
private:
    // Allocation, projection/observer or validation exceptions never become a
    // success. A possibly partial transition permanently poisons this instance;
    // the caller must abort and reconstruct from owned authoritative state.
    struct Transition {
        Registry &registry; bool complete{};
        explicit Transition(Registry &value) : registry(value) {
            if (registry.poisoned_ || registry.transition_active_) refuse("registry_transition_unavailable");
            registry.transition_active_ = true;
        }
        void finish() { complete = true; }
        ~Transition() { if (!complete) registry.poisoned_ = true; registry.transition_active_ = false; }
    };
    bool poisoned_{}, transition_active_{};
    Table *head_{}; Client *pending_{};
    std::unordered_map<Table *, std::shared_ptr<Table>> tables_;
    std::unordered_map<Client *, std::shared_ptr<Client>> clients_;
    [[noreturn]] static void refuse(const char *why) { throw std::runtime_error(why); }
    void emit(const Change &change) { if (observe) observe(change); }
    void set_clients(Table *table, Client *client) { table->clients = client; emit({.field=Field::table_clients, .table=table, .client_value=client}); }
    void set_next(Client *client, Client *next) { client->next = next; emit({.field=Field::client_next, .client=client, .client_value=next}); }
};
} // namespace ps2::iop::library_model
