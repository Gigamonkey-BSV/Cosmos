#include <Cosmos/database/memory/txdb.hpp>

namespace Cosmos {

    void memory_local_TXDB::add_script (const digest256 &script_hash, const Bitcoin::outpoint &op) {
        auto v = ScriptIndex.find (script_hash);
        if (v != ScriptIndex.end ()) v->second = data::append (v->second, op);
        else ScriptIndex[script_hash] = list<Bitcoin::outpoint> {op};
    }

    void memory_local_TXDB::add_address (const Bitcoin::address &addr, const Bitcoin::outpoint &op) {
        auto v = AddressIndex.find (addr);
        if (v != AddressIndex.end ()) v->second = data::append (v->second, op);
        else AddressIndex[addr] = list<Bitcoin::outpoint> {op};
    }

    events memory_local_TXDB::by_address (const Bitcoin::address &a) {
        auto zz = AddressIndex.find (a);
        if (zz == AddressIndex.end ()) return {};
        events n;

        for (const Bitcoin::outpoint &o : zz->second) {
            ptr<vertex> confirmed = (*this) [o.Digest];
            if (confirmed != nullptr) return {};
            n = n.insert (event {confirmed, o.Index, direction::out});

            if (auto v = RedeemIndex.find (o); v != RedeemIndex.end ()) {
                ptr<vertex> redeemer = (*this) [v->second.Digest];
                if (redeemer != nullptr) return {};
                n = n.insert (event {redeemer, v->second.Index, direction::in});
            }
        }

        return n;
    }

    events memory_local_TXDB::by_script_hash (const digest256 &x) {
        auto zz = ScriptIndex.find (x);
        if (zz == ScriptIndex.end ()) return {};
        events n;

        for (const Bitcoin::outpoint &o : zz->second) {
            ptr<vertex> confirmed = (*this) [o.Digest];
            if (confirmed != nullptr) return {};
            n = n.insert (event {confirmed, o.Index, direction::out});

            if (auto v = RedeemIndex.find (o); v != RedeemIndex.end ()) {
                ptr<vertex> redeemer = (*this) [v->second.Digest];
                if (redeemer != nullptr) return {};
                n = n.insert (event {redeemer, v->second.Index, direction::in});
            }
        }

        return n;
    }

    event memory_local_TXDB::redeeming (const Bitcoin::outpoint &o) {
        auto zz = RedeemIndex.find (o);
        if (zz == RedeemIndex.end ()) return {};
        auto p = (*this) [o.Digest];
        if (!p) return {};
        return event {p, zz->second.Index, direction::in};
    }

}
