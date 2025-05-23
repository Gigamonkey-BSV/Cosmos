#ifndef COSMOS_WALLET_SIGN
#define COSMOS_WALLET_SIGN

#include <gigamonkey/redeem.hpp>

#include <Cosmos/database.hpp>

#include <chrono>

namespace Cosmos {

    //using redeem = Gigamonkey::redeem;
    namespace extended = Gigamonkey::extended;

    // can't use namespace unsigend since 'unsigned' is a reserved word.
    // nosig is for transactions that can have their signatures inserted later.
    namespace nosig {
        struct sigop {
            Bitcoin::sighash::directive Directive;
            key_expression Key;
        };

        using element = either<bytes, sigop>;
        using script = list<element>;

        struct input : Bitcoin::incomplete::input {
            script Script;
            Bitcoin::output Prevout;

            // throws if signatures are not complete.
            explicit operator extended::input () const;

            input (
                Bitcoin::satoshi val,
                const data::bytes &locking_script,
                const Bitcoin::outpoint &op,
                script unlocking_script,
                uint32_little Sequence);
        };

        struct transaction {

            int32_little Version;
            list<input> Inputs;
            list<Bitcoin::output> Outputs;
            uint32_little LockTime;

            transaction (int32_little, list<input>, list<Bitcoin::output>, uint32_little);

            // attempt to retrieve all keys and sign.
            transaction sign (database::readable &) const;

            // throws if signatures are not complete.
            explicit operator extended::transaction () const;
            explicit operator Bitcoin::incomplete::transaction () const;

            Bitcoin::satoshi fee ();
            uint64 expected_size ();

            explicit transaction (const JSON &);
            explicit operator JSON () const;

        };

    }

}

#endif

