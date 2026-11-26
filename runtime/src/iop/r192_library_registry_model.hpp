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
    bool contains(const Table *value) const { return tables_.count(const_cast<Table *>(value)) != 0; }
    // Seed an explicitly proved existing-import precondition. This does NOT
    // implement/admit the complete Link/UnLinkLibraryEntries entrypoints.
    void seed_client(std::shared_ptr<Client> client, Table *owner = nullptr) {
        if (!client || clients_.count(client.get()) || (owner && !contains(owner))) refuse("unproved_client_seed");
        if (owner && (owner->name != client->name || (owner->version >> 8) != (client->version >> 8))) refuse("unproved_client_compatibility");
        auto *raw = client.get(); clients_.emplace(raw, std::move(client));
        raw->targets.resize(raw->ordinals.size());
        if (owner) {
            for (std::size_t i = 0; i < raw->ordinals.size(); ++i)
                raw->targets[i] = raw->ordinals[i] < owner->functions.size() ? owner->functions[raw->ordinals[i]] : nullptr;
            raw->flags = (raw->flags & ~6u) | 2u; raw->next = owner->clients; owner->clients = raw;
        } else { raw->next = pending_; pending_ = raw; }
        validate();
    }
    std::int32_t register_table(std::shared_ptr<Table> table) {
        if (!table) refuse("unadmitted_null_table");
        validate();
        if (table->marker != Marker::canonical) return illegal;
        for (auto *entry : table->functions) if (!entry) refuse("unproved_callable_table");
        Client *moving{};
        for (auto *old = head_; old; old = old->previous) {
            if (old->name != table->name || (old->version >> 8) != (table->version >> 8)) continue;
            if ((table->version & 255u) <= (old->version & 255u)) return found;
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
        auto *raw = table.get(); set_clients(raw, nullptr);
        while (moving) {
            auto *next = moving->next;
            moving->targets.resize(moving->ordinals.size());
            for (std::size_t i = 0; i < moving->ordinals.size(); ++i) {
                const auto ordinal = moving->ordinals[i];
                moving->targets[i] = ordinal < raw->functions.size() ? raw->functions[ordinal] : nullptr;
                emit({.field=Field::stub_target, .client=moving, .target=moving->targets[i], .slot=i});
            }
            moving->flags = (moving->flags & ~6u) | 2u;
            emit({.field=Field::client_flags, .client=moving, .flags=moving->flags});
            set_next(moving, raw->clients); set_clients(raw, moving); moving = next;
        }
        raw->flags &= ~1u; emit({.field=Field::table_flags, .table=raw, .flags=raw->flags});
        raw->previous = head_; raw->marker = Marker::linked;
        emit({.field=Field::table_previous, .table=raw, .table_value=head_});
        head_ = raw; tables_.emplace(raw, std::move(table)); emit({.field=Field::head, .table_value=raw});
        validate(); return ok;
    }
    std::int32_t release_table(const std::shared_ptr<Table> &table) {
        if (!table) refuse("unadmitted_null_table");
        validate(); Table *previous{}, *current = head_;
        while (current && current != table.get()) { previous = current; current = current->previous; }
        if (current != table.get()) return not_found;
        if (current->clients) return in_use;
        set_clients(current, nullptr);
        if (previous) { previous->previous = current->previous; emit({.field=Field::table_previous, .table=previous, .table_value=current->previous}); }
        else { head_ = current->previous; emit({.field=Field::head, .table_value=head_}); }
        current->marker = Marker::canonical; emit({.field=Field::table_marker, .table=current});
        tables_.erase(current); validate(); return ok;
    }
    void validate() const {
        std::unordered_set<Table *> tables; std::unordered_set<Client *> clients;
        auto chain = [&](Client *client) {
            for (; client; client = client->next)
                if (!clients_.count(client) || !clients.insert(client).second) refuse("unproved_client_graph");
        };
        for (auto *table = head_; table; table = table->previous) {
            if (!tables_.count(table) || !tables.insert(table).second || table->marker != Marker::linked) refuse("unproved_library_graph");
            chain(table->clients);
        }
        if (tables.size() != tables_.size()) refuse("unproved_library_membership");
        chain(pending_);
    }
private:
    Table *head_{}; Client *pending_{};
    std::unordered_map<Table *, std::shared_ptr<Table>> tables_;
    std::unordered_map<Client *, std::shared_ptr<Client>> clients_;
    [[noreturn]] static void refuse(const char *why) { throw std::runtime_error(why); }
    void emit(const Change &change) { if (observe) observe(change); }
    void set_clients(Table *table, Client *client) { table->clients = client; emit({.field=Field::table_clients, .table=table, .client_value=client}); }
    void set_next(Client *client, Client *next) { client->next = next; emit({.field=Field::client_next, .client=client, .client_value=next}); }
};
} // namespace ps2::iop::library_model
