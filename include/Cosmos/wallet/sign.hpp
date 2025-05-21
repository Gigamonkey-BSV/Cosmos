#ifndef COSMOS_WALLET_SIGN
#define COSMOS_WALLET_SIGN

#include <gigamonkey/redeem.hpp>

#include <Cosmos/database/write.hpp>

#include <chrono>

namespace Cosmos {

    using redeem = Gigamonkey::redeem;
    using extended_transaction = Gigamonkey::extended::transaction;

    // can't use namespace unsigend since 'unsigned' is a reserved word.
    // nosig is for transactions that can have their signatures inserted later.
    namespace nosig {
        struct sigop {
            Bitcoin::sighash::directive Directive;
            key_expression Key;
        };

        using script = list<either<bytes, sigop>>;

        struct input : Bitcoin::incomplete::input {
            script Script;
            Bitcoin::output Prevout;
        };

        struct transaction {

            int32_little Version;
            list<input> Inputs;
            list<Bitcoin::output> Outputs;
            uint32_little LockTime;

            extended_transaction sign (redeem, keychain &k) const;

            Bitcoin::satoshi fee ();
            uint64 expected_size ();

            explicit transaction (const JSON &);
            explicit operator JSON () const;

        };

    }

}

#endif

