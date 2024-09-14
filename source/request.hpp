#include "interface.hpp"
#include "Cosmos.hpp"

void command_request (const arg_parser &p) {
    using namespace Cosmos;
    Cosmos::Interface e {};
    Cosmos::read_pubkeys_options (e, p);
    e.update<void> (Cosmos::update_pending_transactions);

    Bitcoin::timestamp now = Bitcoin::timestamp::now ();
    payments::request request {now};

    maybe<uint32> expiration_minutes;
    p.get ("expires", expiration_minutes);
    if (bool (expiration_minutes)) request.Expires = Bitcoin::timestamp {uint32 (now) + *expiration_minutes * 60};

    maybe<int64> satoshi_amount;
    p.get ("amount", satoshi_amount);
    if (bool (satoshi_amount)) request.Amount = Bitcoin::satoshi {*satoshi_amount};

    maybe<std::string> memo;
    p.get ("memo", memo);
    if (bool (memo)) request.Memo = *memo;

    maybe<std::string> payment_option_string;
    p.get ("payment_type", payment_option_string);

    payments::type payment_option = bool (payment_option_string) ?
        [] (const std::string &payment_option) -> payments::type {
            std::cout << "payment type is " << payment_option << std::endl;
            if (payment_option == "address") return payments::type::address;
            if (payment_option == "pubkey") return payments::type::pubkey;
            if (payment_option == "xpub") return payments::type::xpub;
            throw exception {} << "could not read payment type";
        } (sanitize (*payment_option_string)) : payments::type::address;

    if (payment_option == payments::type::xpub || payment_option == payments::type::pubkey)
        std::cout << "NOTE: the payment type you have chosen is not safe against a quantum attack. "
            "Please don't use it if you think your customer may have access to a quantum computer." << std::endl;

    e.update<void> ([&request, &payment_option] (Cosmos::Interface::writable u) {
        const auto *addrs = u.get ().addresses ();
        const auto *pay = u.get ().payments ();
        if (!bool (addrs) || !bool (pay)) throw exception {} << "could not read wallet";

        auto pr = payments::request_payment (payment_option, *pay, *addrs, request);
        std::cout << "Show the following string to your customer to request payment. " << std::endl;
        std::cout << "\t" << payments::write_payment_request (pr.Request) << std::endl;
        u.set_addresses (pr.Addresses);
        u.set_payments (pr.Payments);
    });
}
