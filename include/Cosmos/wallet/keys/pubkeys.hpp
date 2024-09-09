#ifndef COSMOS_WALLET_KEYS_PUBKEYS
#define COSMOS_WALLET_KEYS_PUBKEYS

#include <Cosmos/database/write.hpp>
#include <Cosmos/wallet/keys/sequence.hpp>

namespace Cosmos {

    struct pubkeys {
        data::map<pubkey, derivation> Derivations;
        data::map<string, address_sequence> Sequences;

        // name of the receive sequence of addresses.
        string Receive;
        // name of the sequences of addresses for change outputs.
        string Change;

        pubkeys (): Derivations {}, Sequences {}, Receive {}, Change {} {}

        explicit pubkeys (data::map<pubkey, derivation> db, data::map<string, address_sequence> x,
            const string &receive = "receive", const string &change = "change"):
            Derivations {db}, Sequences {x}, Receive {receive}, Change {change} {}

        explicit pubkeys (const JSON &);
        operator JSON () const;

        pubkeys next (const string &name) const;
        // TODO: this should be an xpub rather than an address.
        entry<Bitcoin::address, signing> last (const string &name) const;

        pubkeys insert (const pubkey &name, const derivation &x) {
            return pubkeys {Derivations.insert (name, x), Sequences, Receive, Change};
        }

        // indicate that addresses in this sequence have been used up to last.
        pubkeys update (const string &name, uint32 last) const {
            return pubkeys {Derivations, Sequences.insert (name, address_sequence {},
                [last] (const address_sequence &o, const address_sequence &n) -> address_sequence {
                    return address_sequence (o.Key, o.Path, last);
                }), Receive, Change};
        }

        Bitcoin::pubkey derive (const derivation &) const;
        Bitcoin::pubkey derive (HD::BIP_32::path) const;
    };

    pubkeys inline read_pubkeys_from_file (const std::string &filename) {
        return pubkeys (read_from_file (filename));
    }

    pubkeys inline pubkeys::next (const string &name) const {
        return pubkeys {Derivations, data::replace_part (Sequences, name, Sequences[name].next ()), Receive, Change};
    }

    entry<Bitcoin::address, signing> inline pubkeys::last (const string &name) const {
        auto seq = Sequences[name];
        auto der = Derivations[seq.Key];
        return address_sequence {HD::BIP_32::pubkey {der.Key}, seq.Path, seq.Last}.last ();
    }

    Bitcoin::pubkey inline pubkeys::derive (const derivation &d) const {
        return HD::BIP_32::pubkey {Derivations[d.Key].Key}.derive (d.Path).Pubkey;
    }
}

#endif
