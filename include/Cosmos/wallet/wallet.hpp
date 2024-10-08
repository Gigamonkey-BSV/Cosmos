#ifndef COSMOS_WALLET_WALLET
#define COSMOS_WALLET_WALLET

#include <Cosmos/database/write.hpp>
#include <Cosmos/wallet/account.hpp>
#include <Cosmos/wallet/keys/pubkeys.hpp>
#include <Cosmos/wallet/keys/secret.hpp>
#include <Cosmos/wallet/select.hpp>
#include <Cosmos/wallet/change.hpp>
#include <gigamonkey/redeem.hpp>
#include <chrono>

namespace Cosmos {

    using redeem = Gigamonkey::redeem;
    using extended_transaction = Gigamonkey::extended::transaction;

    // can't use namespace unsigend since 'unsigned' is a reserved word.
    // nosig is for transactions that can have their signatures inserted later.
    namespace nosig {
        struct sigop {
            Bitcoin::sighash::directive Directive;
            derivation Derivation;
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

            transaction sign (const keychain &k) const;

            explicit transaction (const JSON &);
            explicit operator JSON () const;

        };

        using write_scripts = function<script (pubkeys &, const Bitcoin::output &, const Bitcoin::sighash::document &,
            list<Bitcoin::sighash::directive>, const bytes &script_code)>;

        script p2pkh_and_p2pk (pubkeys &, const Bitcoin::output &, const Bitcoin::sighash::document &,
            list<Bitcoin::sighash::directive>, const bytes &script_code);

    }

    struct wallet {
        pubkeys Pubkeys;
        addresses Addresses;
        account Account;

        Bitcoin::satoshi value () const {
            return Account.value ();
        }

        wallet (): Pubkeys {}, Addresses {}, Account {} {}
        wallet (pubkeys p, addresses ad, account ac) : Pubkeys {p}, Addresses {ad}, Account {ac} {}

        bool valid () const {
            return data::valid (Account) && data::valid (Addresses) && data::valid (Pubkeys);
        }
    };

    Bitcoin::secret find_secret (keychain k, pubkeys p, derivation d);

    struct spend {
        select Select;
        make_change Change;
        data::crypto::random &Random;

        struct spent {
            list<std::pair<extended_transaction, account_diff>> Transactions;
            addresses Addresses;

            bool valid () const {
                return data::size (Transactions) != 0;
            }

            spent () : Transactions {}, Addresses {} {}
            spent (list<std::pair<extended_transaction, account_diff>> txs, addresses a) : Transactions {txs}, Addresses {a} {}
        };

        spent operator () (redeem,
            keychain, wallet,
            // payee's outputs
            list<Bitcoin::output> to,
            satoshis_per_byte fees = {1, 100},
            uint32 lock = 0) const;

        struct spent_unsigned {
            // since we don't know the txid of these transactions until they are signed,
            // that field in account_diff will be zero.
            list<std::pair<nosig::transaction, account_diff>> Transactions;
            pubkeys Pubkeys;

            bool valid () const {
                return data::size (Transactions) != 0;
            }

            spent_unsigned () : Transactions {}, Pubkeys {} {}

            spent sign (const keychain &) const;
        };

        spent_unsigned operator () (nosig::write_scripts,
            const pubkeys &, const account &,
            list<Bitcoin::output> to,
            satoshis_per_byte fees = {1, 100},
            uint32 lock = 0) const;
    };

    tuple<keychain, account> inline import_output (
        keychain k, account a,
        const Bitcoin::prevout &p,
        const Bitcoin::secret &x,
        uint64 expected_size,
        const bytes &script_code = {}) {
        return {k.insert (secret {x}),
            a.insert (p.Key, redeemable {p.Value, list<derivation> {derivation {x.to_public (), {}}}, expected_size})};
    }

}

#endif
