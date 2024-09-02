#ifndef COSMOS_WALLET_KEYS_CHAIN
#define COSMOS_WALLET_KEYS_CHAIN

#include <Cosmos/wallet/write.hpp>
#include <Cosmos/wallet/keys/sequence.hpp>

namespace Cosmos {

    struct pubkeychain {
        data::map<pubkey, derivation> Derivations;
        data::map<string, address_sequence> Sequences;

        // name of the receive sequence of addresses.
        string Receive;
        // name of the sequences of addresses for change outputs.
        string Change;

        pubkeychain (): Derivations {}, Sequences {}, Receive {}, Change {} {}

        explicit pubkeychain (data::map<pubkey, derivation> db, data::map<string, address_sequence> x,
            const string &receive = "receive", const string &change = "change"):
            Derivations {db}, Sequences {x}, Receive {receive}, Change {change} {}

        explicit pubkeychain (const JSON &);
        operator JSON () const;

        pubkeychain next (const string &name) const;
        entry<Bitcoin::address, signing> last (const string &name) const;

        pubkeychain insert (const pubkey &name, const derivation &x) {
            return pubkeychain {Derivations.insert (name, x), Sequences, Receive, Change};
        }

        // indicate that addresses in this sequence have been used up to last.
        pubkeychain update (const string &name, uint32 last) {
            return pubkeychain {Derivations, Sequences.insert (name, address_sequence {},
                [last] (const address_sequence &o, const address_sequence &n) -> address_sequence {
                    return address_sequence (o.Key, o.Path, last);
                }), Receive, Change};
        }

        Bitcoin::pubkey derive (const derivation &) const;
        Bitcoin::pubkey derive (HD::BIP_32::path) const;
    };

    pubkeychain inline read_pubkeychain_from_file (const std::string &filename) {
        return pubkeychain (read_from_file (filename));
    }

    pubkeychain inline pubkeychain::next (const string &name) const {
        return pubkeychain {Derivations, data::replace_part (Sequences, name, Sequences[name].next ()), Receive, Change};
    }

    entry<Bitcoin::address, signing> inline pubkeychain::last (const string &name) const {
        auto seq = Sequences[name];
        auto der = Derivations[seq.Key];
        return address_sequence {HD::BIP_32::pubkey {der.Key}, seq.Path, seq.Last}.last ();
    }

    Bitcoin::pubkey inline pubkeychain::derive (const derivation &d) const {
        return HD::BIP_32::pubkey {Derivations[d.Key].Key}.derive (d.Path).Pubkey;
    }
}

#endif
