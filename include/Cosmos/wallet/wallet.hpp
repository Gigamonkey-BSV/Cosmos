#ifndef COSMOS_WALLET_WALLET
#define COSMOS_WALLET_WALLET

#include <Cosmos/wallet/write.hpp>
#include <Cosmos/wallet/account.hpp>
#include <Cosmos/wallet/select.hpp>
#include <Cosmos/wallet/change.hpp>
#include <gigamonkey/redeem.hpp>
#include <chrono>

namespace Cosmos {

    using redeem = Gigamonkey::redeem;

    struct watch_wallet {
        account Account;
        pubkeychain Pubkeys;

        Bitcoin::satoshi value () const {
            return Account.value ();
        }

        watch_wallet &operator = (const watch_wallet &w) {
            Account = w.Account;
            Pubkeys = w.Pubkeys;
            return *this;
        }

        watch_wallet (): Account {}, Pubkeys {} {}
        watch_wallet (account a, pubkeychain p) : Account {a}, Pubkeys {p} {}

        bool valid () const {
            return data::valid (Account) && data::valid (Pubkeys);
        }
    };

    struct wallet : watch_wallet {
        keychain Keys;

        wallet (): watch_wallet {}, Keys {} {}
        wallet (const account &acc, const pubkeychain &p, const keychain &k) : watch_wallet {acc, p}, Keys {k} {}

        wallet &operator = (const wallet &w) {
            watch_wallet::operator = (static_cast<const watch_wallet &> (w));
            Keys = w.Keys;
            return *this;
        }

        void import_output (const Bitcoin::prevout &p, const Bitcoin::secret &x, uint64 expected_size, const bytes &script_code = {});

        bool valid () const {
            return data::valid (static_cast<watch_wallet> (*this)) && data::valid (Keys);
        }
    };

    struct spent {
        Bitcoin::transaction Transaction;
        wallet Wallet;

        bool valid () const {
            return data::size (Transaction) != 0 && data::valid (Wallet);
        }

        spent () : Transaction {}, Wallet {} {}
        spent (const Bitcoin::transaction &tx, const wallet &w) : Transaction {tx}, Wallet {w} {}
    };

    spent spend (wallet, select, make_change, redeem,
        data::crypto::random &,
        list<Bitcoin::output> to,
        satoshi_per_byte fees = {1, 100},
        uint32 lock = 0);

}

#endif
