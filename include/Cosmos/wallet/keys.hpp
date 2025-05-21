#ifndef COSMOS_WALLET_KEYS
#define COSMOS_WALLET_KEYS

#include <gigamonkey/types.hpp>
#include <gigamonkey/schema/hd.hpp>

#include <Diophant/expression.hpp>

namespace Cosmos {
    namespace Bitcoin = Gigamonkey::Bitcoin;
    namespace HD = Gigamonkey::HD;
    using digest160 = Gigamonkey::digest160;

    using expression = const Diophant::expression;

    using int32 = data::int32;
    using uint64 = data::uint64;

    using bytes = data::bytes;

    template <typename X> using list = data::list<X>;

    template <typename X> bool castable_to (expression);
    template <typename X> X cast_to (expression);
    bool evaluatable (expression);

    struct key_expression : expression {
        using expression::expression;
        key_expression (const Bitcoin::secret &);
        key_expression (const Bitcoin::pubkey &);
        key_expression (const HD::BIP_32::secret &);
        key_expression (const HD::BIP_32::pubkey &);
    };

    struct key_derivation : expression {
        using expression::expression;
        key_expression operator () (key_expression, int32) const;
        static key_derivation HD_key_derivation (const HD::BIP_32::pubkey &, int32 = 0);
    };

    struct key_sequence {
        key_derivation Pubkey;
        key_derivation Secret;

        key_expression pubkey (int32) const;
        key_expression secret (int32) const;
    };

    struct key_source {
        int32_t Last;
        key_sequence Sequence;

        key_source next () const;

        key_expression last_public () const;
        key_expression last_private () const;
    };

    struct keychain {
        virtual Bitcoin::secret &recover (key_expression) = 0;
        virtual ~keychain () = 0;
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
    };

}

#endif

