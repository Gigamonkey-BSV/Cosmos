
#include <Cosmos/wallet/restore.hpp>

namespace Cosmos {

    restore::restored restore::operator () (
        txdb &TXDB,
        address_sequence m) {

        address_sequence last = m;
        uint32 last_used = 0;

        ordered_list<ray> events {};
        std::map<Bitcoin::outpoint, redeemable> a {};

        auto expected_p2pkh_size = pay_to_address::redeem_expected_size (true);

        Bitcoin::satoshi in_wallet = 0;
        Bitcoin::satoshi received = 0;
        Bitcoin::satoshi spent = 0;

        std::cout << " restoring wallet ..." << std::endl;

        while (true) {
            // generate next address
            entry<Bitcoin::address, signing> next = m.last ();
            const Bitcoin::address &new_addr = next.Key;

            std::cout << " recovering address " << m.Last << ": " << new_addr << std::endl;

            m = m.next ();

            // get all txs relating to this address.
            ordered_list<ray> ev = TXDB.by_address (new_addr);
            std::cout << " found " << ev.size () << " events for address " << new_addr << std::endl;

            if (ev.size () == 0) {
                if (last_used == MaxLookAhead) break;

                last_used++;
                continue;
            }

            last_used = 0;
            last = m;

            for (const auto &e : ev) {
                events = events.insert (e);
                std::cout << "    " << new_addr << " received " << e.Value << " on " << e.When;
                received += e.Value;

                // has this output been redeemed?
                auto r = TXDB.redeeming (e.Point);

                if (r != nullptr) {
                    std::cout << "; spent on " << r->When;
                    events = events.insert (*r);
                    spent += e.Value;
                } else {
                    a[e.Point] = redeemable {Bitcoin::output {e.Put}, next.Value};
                    in_wallet += e.Value;
                }

            }

            std::cout << "    total received  " << received << std::endl;
            std::cout << "    total spent     " << spent << std::endl;
            std::cout << "    total in wallet " << in_wallet << std::endl;

            std::cout << "    " << data::size (events) << " total events discovered for " << new_addr << std::endl;

        }

        return restored {events, a, last.Last};
    }

}


