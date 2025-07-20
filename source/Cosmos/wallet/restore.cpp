
#include <Cosmos/wallet/restore.hpp>

namespace Cosmos {

    restore::restored restore::operator () (TXDB &txdb, key_sequence m, int32 start) {

        if (CheckSubKeys) throw exception {} << "TODO: option CheckSubKeys enabled but not implemented.";

        key_sequence last = m;

        int32 next_index = start;
        int32 i = next_index;
        int32 since_last_used = 0;

        events v {};
        std::map<Bitcoin::TXID, map<Bitcoin::index, redeemable>> a {};

        auto expected_p2pkh_size = pay_to_address::redeem_expected_size (true);

        Bitcoin::satoshi total_in_wallet = 0;
        Bitcoin::satoshi total_received = 0;
        Bitcoin::satoshi total_spent = 0;

        while (true) {
            // generate next address
            entry<Bitcoin::address, signing> next = pay_to_address_signing (m.secret (i));
            const Bitcoin::address &new_addr = next.Key;

            std::cout << "  recovering address " << new_addr << std::endl;

            i++;

            // get all txs relating to this address.
            // TODO there is a potential problem here because we assume
            // that if the address is known locally at all, then we know
            // about all usages of it, which is not neccessarily true.
            events ev = txdb.by_address (new_addr);
            std::cout << "  found " << ev.size () << " events for address " << new_addr << std::endl;

            if (ev.size () == 0) {
                if (since_last_used == MaxLookAhead) break;

                since_last_used++;
                continue;
            }

            since_last_used = 0;
            next_index = i;

            v = v & ev;

            Bitcoin::satoshi in_wallet = 0;
            Bitcoin::satoshi received = 0;
            Bitcoin::satoshi spent = 0;

            for (const auto &e : ev) {
                Bitcoin::satoshi value = e.value ();
                std::cout << "    ";
                if (e.Direction == direction::out) {
                    std::cout << "received";
                    received += value;
                    in_wallet += value;
                } else {
                    std::cout << "spent";
                    spent += value;
                    in_wallet -= value;
                }
                std::cout << " " << e.value ();
                when w = e->when ();
                if (w == when::unconfirmed ()) std::cout << " recently";
                else std::cout << " on " << w;
                std::cout << std::endl;

            }

            total_received += received;
            total_spent += spent;
            total_in_wallet += in_wallet;

            std::cout << "    total received  " << total_received << std::endl;
            std::cout << "    total spent     " << total_spent << std::endl;
            std::cout << "    total in wallet " << total_in_wallet << std::endl;

        }

        list<account_diff> diffs;
        for (const auto &[txid, ins] : a) diffs <<= account_diff {txid, ins, {}};

        return restored {v, diffs, next_index};
    }

}


