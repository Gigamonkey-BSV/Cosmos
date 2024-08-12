#ifndef COSMOS_WALLET_KEYS
#define COSMOS_WALLET_KEYS

#include <gigamonkey/wif.hpp>
#include <gigamonkey/schema/hd.hpp>
#include <gigamonkey/script/pattern/pay_to_address.hpp>
#include <Cosmos/types.hpp>
#include <Cosmos/wallet/write.hpp>

namespace Cosmos {
    using namespace data;
    namespace Bitcoin = Gigamonkey::Bitcoin;
    namespace HD = Gigamonkey::HD;
    using pay_to_address = Gigamonkey::pay_to_address;

    // derivation path for a key. Includes a name and a path, which may be empty.
    struct derivation {
        // the name of the key that this was derived from.
        string Name;
        // the derivation path
        HD::BIP_32::path Path;

        derivation () : Name {}, Path {} {}
        derivation (const string &n, HD::BIP_32::path p): Name {n}, Path {p} {}

        explicit operator JSON () const;
        derivation (const JSON &);

        bool operator == (const derivation &d) const {
            return Name == d.Name && Path == d.Path;
        }
    };

    std::ostream inline &operator << (std::ostream &o, const derivation &d) {
        return o << "derivation {" << d.Name << ", " << d.Path << "}";
    }

    // data required to make signatures to redeem an output.
    struct signing {

        // derivation paths to required keys.
        list<derivation> Derivation;

        // expected size of the completed input script.
        uint64 ExpectedScriptSize;

        // we may need a partially completed script.
        bytes UnlockScriptSoFar;

        signing () : Derivation {}, ExpectedScriptSize {}, UnlockScriptSoFar {} {}
        signing (list<derivation> d, uint64 ez, const bytes &script_code = {}) :
            Derivation {d}, ExpectedScriptSize {ez}, UnlockScriptSoFar {script_code} {}

        uint64 expected_input_size () const {
            return ExpectedScriptSize + Bitcoin::var_int::size (ExpectedScriptSize) + 40;
        }
    };

    struct derived_pubkey {
        Bitcoin::pubkey Pubkey;
        derivation Derivation;

        derived_pubkey (): Pubkey {}, Derivation {} {}
        derived_pubkey (const Bitcoin::pubkey &u, const derivation &p) : Pubkey {u}, Derivation {p} {}

        explicit derived_pubkey (const JSON &);
        explicit operator JSON () const;

        bool operator == (const derived_pubkey &p) const {
            return Pubkey == p.Pubkey && Derivation == p.Derivation;
        }

        entry<digest160, signing> address () const {
            return entry<digest160, signing> {Pubkey.address_hash (), signing {{Derivation}, pay_to_address::redeem_expected_size ()}};
        }
    };

    struct hd_pubkey {
        HD::BIP_32::pubkey Pubkey;
        derivation Derivation;

        hd_pubkey (): Pubkey {}, Derivation {} {}
        hd_pubkey (const HD::BIP_32::pubkey &s, const derivation &p): Pubkey {s}, Derivation {p} {}
        hd_pubkey (const HD::BIP_32::pubkey &s) : Pubkey {s}, Derivation {} {}

        bool valid () const {
            return Pubkey.valid ();
        }
    };

    struct hd_pubkey_sequence : hd_pubkey {
        uint32 Last;

        hd_pubkey_sequence () : hd_pubkey {}, Last {0} {}
        hd_pubkey_sequence (const HD::BIP_32::pubkey &s, const derivation &p, uint32 l = 0) : hd_pubkey {s, p}, Last {l} {}
        hd_pubkey_sequence (const hd_pubkey &z, uint32 l = 0) : hd_pubkey {z}, Last {l} {}

        hd_pubkey_sequence next () const;

        derived_pubkey last () const;

        explicit hd_pubkey_sequence (const JSON &);
        explicit operator JSON () const;

        bool operator == (const hd_pubkey_sequence &x) const;
    };

    struct pubkey : either<derived_pubkey, hd_pubkey_sequence> {
        bool valid () const;
        using either<derived_pubkey, hd_pubkey_sequence>::either;
        pubkey (HD::BIP_32::pubkey s, const derivation &p, uint32 l = 0);

        Bitcoin::pubkey derive (HD::BIP_32::path) const;

        explicit pubkey (const JSON &);
        explicit operator JSON () const;

        pubkey next () const;
        derived_pubkey last () const;

    private:
        pubkey ();
    };

    struct secret : either<Bitcoin::secret, HD::BIP_32::secret> {
        using either<Bitcoin::secret, HD::BIP_32::secret>::either;
        bool valid () const;
        Bitcoin::secret derive (HD::BIP_32::path) const;

        operator string () const;
        secret (const string &);
    };

    struct pubkeychain {
        data::map<string, pubkey> Map;
        string Receive;
        string Change;

        pubkeychain (): Map {}, Receive {}, Change {} {}
        explicit pubkeychain (const string &def, const pubkey x) :
            Map {{def, x}}, Receive {def}, Change {def} {}

        explicit pubkeychain (const string &receive_name, const pubkey receive, const string &change_name, const pubkey change) :
            Map {{receive_name, receive}, {change_name, change}},
                Receive {receive_name}, Change {change_name} {}

        explicit pubkeychain (data::map<string, pubkey> db, const string &receive_name, const string &change_name):
            Map {db}, Receive {receive_name}, Change {change_name} {}

        explicit pubkeychain (const JSON &);
        operator JSON () const;

        pubkeychain next (const string &name) const;
        derived_pubkey last (const string &name) const;

        pubkeychain insert (const string &name, const pubkey &x) {
            return pubkeychain {Map.insert (name, x), Receive, Change};
        }

        Bitcoin::pubkey derive (const derivation &) const;
        Bitcoin::pubkey derive (HD::BIP_32::path) const;

        bool valid () const {
            return data::size (Map) > 0 && Receive != "" && Change != "";
        }
    };

    struct keychain {
        map<string, secret> Map;
        string Master;

        keychain (): Map {}, Master {} {}
        explicit keychain (const string def, const secret x) :
            Map {{def, x}}, Master {def} {}

        explicit keychain (map<string, secret> map, string master):
            Map {map}, Master {master} {}

        explicit keychain (const JSON &);
        operator JSON () const;

        Bitcoin::secret derive (const derivation &) const;

        keychain insert (const string &name, const secret &x) {
            return keychain {Map.insert (name, x), Master};
        }

        bool valid () const {
            return data::size (Map) > 0 && Master != "";
        }
    };

    keychain inline read_keychain_from_file (const std::string &filename) {
        return keychain (read_from_file (filename));
    }

    pubkeychain inline read_pubkeychain_from_file (const std::string &filename) {
        return pubkeychain (read_from_file (filename));
    }

    hd_pubkey_sequence inline hd_pubkey_sequence::next () const {
        return hd_pubkey_sequence {this->Pubkey, this->Derivation, Last + 1};
    }

    derived_pubkey inline hd_pubkey_sequence::last () const {
        auto path = Derivation.Path << Last;
        return derived_pubkey {Bitcoin::pubkey (this->Pubkey.derive (path).Pubkey), derivation {this->Derivation.Name, path}};
    }

    bool inline hd_pubkey_sequence::operator == (const hd_pubkey_sequence &x) const {
        return Pubkey == x.Pubkey && Derivation == x.Derivation && Last == x.Last;
    }

    inline pubkey::pubkey (HD::BIP_32::pubkey s, const derivation &p, uint32 l) :
        either<derived_pubkey, hd_pubkey_sequence> {hd_pubkey_sequence {s, p, l}} {}

    pubkeychain inline pubkeychain::next (const string &name) const {
        return pubkeychain {data::replace_part (Map, name, (Map)[name].next ()), Receive, Change};
    }

    derived_pubkey inline pubkeychain::last (const string &name) const {
        return Map[name].last ();
    }

    Bitcoin::pubkey inline pubkeychain::derive (const derivation &d) const {
        return Map[d.Name].derive (d.Path);
    }

    Bitcoin::secret inline keychain::derive (const derivation &d) const {
        return Map[d.Name].derive (d.Path);
    }
}

#endif
