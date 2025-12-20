#ifndef COSMOS_WALLET_KEYS
#define COSMOS_WALLET_KEYS

#include <gigamonkey/types.hpp>
#include <gigamonkey/schema/hd.hpp>

#include <Diophant/symbol.hpp>

namespace secp256k1 = Gigamonkey::secp256k1;
namespace Bitcoin = Gigamonkey::Bitcoin;
namespace HD = Gigamonkey::HD;
namespace encoding = data::encoding;
using digest160 = Gigamonkey::digest160;

using int32 = data::int32;
using uint32 = data::uint32;
using uint64 = data::uint64;

using bytes = data::bytes;
using string = data::string;

namespace Cosmos {

    template <typename X> using list = data::list<X>;

    // an expression that evaluates to a key type.
    struct key_expression : string {
        using string::string;

        key_expression (const secp256k1::secret &);
        key_expression (const secp256k1::pubkey &);
        key_expression (const Bitcoin::WIF &);
        key_expression (const Bitcoin::secret &);
        key_expression (const Bitcoin::pubkey &);
        key_expression (const HD::BIP_32::secret &);
        key_expression (const HD::BIP_32::pubkey &);

        operator secp256k1::secret () const;
        operator secp256k1::pubkey () const;
        operator Bitcoin::secret () const;
        operator Bitcoin::WIF () const;
        operator HD::BIP_32::secret () const;
        operator Bitcoin::pubkey () const;
        operator HD::BIP_32::pubkey () const;
        operator Bitcoin::address () const;
        operator Bitcoin::address::decoded () const;

        bool valid () const {
            return *this != "";
        }
    };

    // information required to make signatures to redeem an output.
    struct signing {

        // something that references the keys required to
        // sign this tx.
        list<key_expression> Keys;

        // expected size of the completed input script.
        uint64 ExpectedScriptSize;

        // we may need a partially completed script.
        bytes UnlockScriptSoFar;

        signing () : Keys {}, ExpectedScriptSize {}, UnlockScriptSoFar {} {}
        signing (list<key_expression> d, uint64 ez, const bytes &script_code = {}) :
            Keys {d}, ExpectedScriptSize {ez}, UnlockScriptSoFar {script_code} {}

        uint64 expected_input_size () const {
            return ExpectedScriptSize + Bitcoin::var_int::size (ExpectedScriptSize) + 40;
        }

        static signing pay_to_address (const key_expression &);
        static signing pay_to_pubkey (const key_expression &);
    };

    data::entry<Bitcoin::address, signing> inline make_pay_to_address (const key_expression &key) {
        static_cast<secp256k1::secret> (key); // just make sure this is possible.
        return {static_cast<Bitcoin::address> (key), signing::pay_to_address (key)};
    }

    data::entry<Bitcoin::pubkey, signing> inline make_pay_to_pubkey (const key_expression &key) {
        static_cast<secp256k1::secret> (key); // just make sure this is possible.
        return {static_cast<Bitcoin::pubkey> (key), signing::pay_to_pubkey (key)};
    }

    // a function that derives an ith key from a given key.
    struct key_derivation : string {
        using string::string;
        key_expression operator () (key_expression, int32) const;
    };

    struct key_sequence {
        key_expression Key;
        key_derivation Derivation;

        key_expression operator () (int32) const;

        operator std::string () const;
    };

    struct key_source {
        uint32 Index;
        key_sequence Sequence;

        key_source operator + () const;

        key_expression operator * () const;

        key_source &operator ++ ();

        key_source next () const;

        operator std::string () const;
    };

    std::string inline net_string (Bitcoin::net net) {
        if (net == Bitcoin::net::Test) return "net.Test";
        if (net == Bitcoin::net::Main) return "net.Main";
        throw data::exception {} << "invalid network";
    }

    inline key_expression::key_expression (const secp256k1::secret &x):
        string {data::string::write ("secret ", std::dec, x.Value)} {}

    inline key_expression::key_expression (const Bitcoin::secret &x):
        string {data::string::write ("WIF [secret ", std::dec, x.Secret, ", ",
            net_string (x.Network), ", ", x.Compressed, "]")} {}

    inline key_expression::key_expression (const Bitcoin::WIF &x):
        string {data::string::write ("WIF \"", x, "\"")} {}

    inline key_expression::key_expression (const Bitcoin::pubkey &p):
        string {data::string::write ("pubkey `", p, "`")} {}

    inline key_expression::key_expression (const secp256k1::pubkey &p):
        string {data::string::write ("pubkey `", encoding::hex::write (p), "`")} {}

    inline key_expression::key_expression (const HD::BIP_32::secret &x):
        string {data::string::write ("HD.secret [secret ", std::dec, x.Secret, ", `",
            encoding::hex::write (x.ChainCode), "`, ", net_string (x.Network), ", ",
                x.Depth, ", ", x.Parent, ", ", x.Sequence, "]")} {}

        inline key_expression::key_expression (const HD::BIP_32::pubkey &p):
        string {data::string::write ("HD.pubkey [pubkey `", p.Pubkey, "`, `",
            encoding::hex::write (p.ChainCode), "`, ", net_string (p.Network), ", ",
                p.Depth, ", ", p.Parent, ", ", p.Sequence, "]")} {}

    key_expression inline key_sequence::operator () (int32 i) const {
        return Derivation (Key, i);
    }

}

#endif

