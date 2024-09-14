#ifndef COSMOS_TAX
#define COSMOS_TAX

#include <gigamonkey/satoshi.hpp>
#include <Cosmos/database/price_data.hpp>
#include <Cosmos/history.hpp>

namespace Cosmos {

    struct tax {
        struct capital_gain {
            double Loss {0};
            double LongTerm {0};
            double ShortTerm {0};

            friend std::ostream &operator << (std::ostream &, const capital_gain &);
        };

        // since we don't know where the money came
        // from, we don't know if it is income in an
        // taxable sense. These founds could have come
        // from another wallet that also belongs to us
        // or from an exchange or something. The user
        // will have to go through these and figure out
        // what needs to be reported as income.
        struct potential_income {
            Bitcoin::TXID TXID;

            // total income for this tx not counting
            // money that was moved from our own wallet.
            Bitcoin::satoshi Income;

            // the price at the time of these events.
            double Price;

            // list of incoming events for this tx.
            // it is possible for the total value
            // of all these events to be greater than the
            // value of the total income.
            ordered_list<ray> Incoming;

            potential_income (): TXID {}, Income {0}, Price {0}, Incoming {} {}

            friend std::ostream &operator << (std::ostream &, const potential_income &);
        };

        capital_gain CapitalGain;

        // list of incoming events to the wallet.
        list<potential_income> Income;

        using account = std::map<Bitcoin::outpoint, Bitcoin::output>;

        // the account at the end of the tax period.
        account Account;

        static tax calculate (txdb &, ptr<price_data>, const events::history &);

        tax () : CapitalGain {}, Income {}, Account {} {}

        tax (const capital_gain &cg, list<potential_income> income, const account &a) :
            CapitalGain {cg}, Income {income}, Account {a} {}
    };

    std::ostream inline &operator << (std::ostream &o, const tax &) {
        // TODO
        return o;
    }

    std::ostream inline &operator << (std::ostream &o, const tax::capital_gain &cg) {
        return o << "\n\tcapital loss: " << cg.Loss << "\n\tcapital gain (short term): " <<
            cg.LongTerm << "\n\t capital gain (long term)" << cg.ShortTerm <<std::endl;
    }
}

#endif

