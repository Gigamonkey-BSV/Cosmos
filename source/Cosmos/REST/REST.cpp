#include <Cosmos/REST/REST.hpp>

namespace Cosmos {

    net::HTTP::request REST::request (const generate_request_options &o) const {
        std::stringstream query_stream;
        bool need_amp = false;

        if (o.WalletType) {
            query_stream << "wallet_type=" << *o.WalletType;
            need_amp = true;
        }

        if (o.RestoreWalletStyle != Cosmos::restore_wallet_style::unset) {
            if (need_amp) query_stream << "&";
            query_stream << "style=" << o.RestoreWalletStyle;
            need_amp = true;
        }

        if (bool (o.CoinTypeDerivationParameter)) {
            if (need_amp) query_stream << "&";
            query_stream << "coin_type=" << *o.CoinTypeDerivationParameter;
            need_amp = true;
        }

        if (o.mnemonic_style () != mnemonic_style::none)
            query_stream << "&mnemonic=" << o.mnemonic_style () << "&number_of_words=" << o.number_of_words ();
        return this->operator () (net::HTTP::method::post, string::write ("/generate/", o.name ())).query (query_stream.str ());
    }

    net::HTTP::request REST::request (const restore_request_options &o) const {

        dispatch<UTF8, UTF8> q {};

        using entry = data::entry<const UTF8, UTF8>;

        if (o.Key)
            q <<= entry ("key", string::write (*o.Key));

        if (o.MaxLookAhead)
            q <<= entry ("max_look_ahead", string::write (*o.MaxLookAhead));

        if (o.Mnemonic)
            q <<= entry ("mnemonic", string::write (*o.Mnemonic));

        if (o.Entropy)
            q <<= entry ("entropy", string::write (*o.Entropy));

        if (o.MasterKeyType)
            q <<= entry ("master_key_type", string::write (*o.MasterKeyType));

        if (o.CoinTypeDerivationParameter)
            q <<= entry ("coin_type", string::write (*o.CoinTypeDerivationParameter));

        if (o.RestoreWalletStyle != Cosmos::restore_wallet_style::unset)
            q <<= entry ("style", string::write (o.RestoreWalletStyle));

        if (o.DerivationStyle != Cosmos::derivation_style::unset)
            q <<= entry ("derivation_style", string::write (o.DerivationStyle));

        return this->operator () (net::HTTP::method::put, string::write ("/restore/", o.name ())).query_map (q);
    }
}

namespace Cosmos::command {

    authority read_authority (const args::parsed &p) {
        DATA_LOG (normal) << "read authority";

        auto result = data::schema::validate (p.Options, +call_options ());

        if (result.template is<net::IP::TCP::endpoint> ())
            return result.template get<net::IP::TCP::endpoint> ();

        auto [port, other_thing] = std::get<1> (result);

        if (other_thing.template is<net::IP::address> ())
            return net::IP::TCP::endpoint {other_thing.template get<net::IP::address> (), port};

        if (other_thing.template is<net::authority> ()) {
            auto auth = other_thing.template get<net::authority> ();

            maybe<net::IP::address> addr = auth.address ();

            if (!addr) return {};

            maybe<uint16> port_number = auth.port_number ();

            if (bool (port_number) && *port_number != port) return {};

            return net::IP::TCP::endpoint {*addr, port};
        }

        // the only allowed domain is localhost
        net::domain_name ddd = other_thing.template get<net::domain_name> ();

        if (ddd != net::domain_name {"localhost"}) return {};

        return authority {ddd, port};

    }

}
