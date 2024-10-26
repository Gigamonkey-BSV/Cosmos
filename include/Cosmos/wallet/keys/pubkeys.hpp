#ifndef COSMOS_WALLET_KEYS_PUBKEYS
#define COSMOS_WALLET_KEYS_PUBKEYS

#include <Cosmos/database/write.hpp>
#include <Cosmos/wallet/keys/sequence.hpp>
#include <data/tools/base_map.hpp>

namespace Cosmos {

    // master pubkeys and their derivations from secret keys.
    struct pubkeys : base_map<pubkey, derivation, pubkeys> {
        using base_map<pubkey, derivation, pubkeys>::base_map;
        explicit pubkeys (const JSON &);
        operator JSON () const;

        derivation operator [] (const derivation &d) const {
            if (const auto *v = this->contains (d.Parent); bool (v))
                return derivation {v->Parent, v->Path + d.Path};
            return derivation {};
        }

        derivation operator [] (const derived_pubkey &d) const {
            if (const auto *v = this->contains (pubkey (d.Parent)); bool (v))
                return derivation {v->Parent, v->Path + d.Path};
            return derivation {};
        }
    };

    struct addresses {
        data::map<string, address_sequence> Sequences;

        // name of the receive sequence of addresses.
        string Receive;
        // name of the sequences of addresses for change outputs.
        string Change;

        address_sequence receive () const {
            return Sequences[Receive];
        }

        address_sequence change () const {
            return Sequences[Change];
        }

        addresses (): Sequences {}, Receive {}, Change {} {}

        explicit addresses (data::map<string, address_sequence> x,
            const string &receive = "receive", const string &change = "change"):
            Sequences {x}, Receive {receive}, Change {change} {}

        explicit addresses (const JSON &);
        operator JSON () const;

        addresses next (const string &name) const;
        derived_pubkey last (const string &name) const;

        // indicate that addresses in this sequence have been used up to last.
        addresses update (const string &name, uint32 last) const {
            return addresses {Sequences.insert (name, address_sequence {},
                [last] (const address_sequence &o, const address_sequence &n) -> address_sequence {
                    return address_sequence (o.Parent, o.Path, last);
                }), Receive, Change};
        }
    };

    pubkeys inline read_pubkeys_from_file (const std::string &filename) {
        return pubkeys (read_from_file (filename));
    }

    addresses inline read_addresses_from_file (const std::string &filename) {
        return addresses (read_from_file (filename));
    }

    addresses inline addresses::next (const string &name) const {
        return addresses {data::replace_part (Sequences, name, Sequences[name].next ()), Receive, Change};
    }

    derived_pubkey inline addresses::last (const string &name) const {
        return Sequences[name].last ();
    }
}

#endif
