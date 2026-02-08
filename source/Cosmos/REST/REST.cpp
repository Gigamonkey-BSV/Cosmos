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

        using entry = data::entry<UTF8, UTF8>;

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

        return this->operator () (net::HTTP::method::post, string::write ("/restore/", o.name ())).query_map (q);
    }
}

namespace Cosmos::command {

    authority read_authority (const args::parsed &p) {
        maybe<authority> auth;
        p.get ("authority", auth);

        maybe<net::domain_name> dom;
        p.get ("domain", dom);

        maybe<net::IP::TCP::endpoint> ep;
        p.get ("endpoint", ep);

        maybe<net::IP::address> addr;
        p.get ("ip_address", addr);

        maybe<uint16> port;
        p.get ("port", port);

        if (auth && !data::valid (*auth))
            throw exception {3} << "invalid value for option 'authority': " << *auth;

        if (dom) {
            if (!data::valid (*dom))
                throw exception {3} << "invalid value for option 'domain': " << *dom;

            authority auth_from_dom;
            auth_from_dom = authority {*dom};
            if (auth && *auth != auth_from_dom)
                throw exception {3} << "authority from option 'authority' and option 'domain' disagree: " << *auth << " vs " << *dom;
            else auth = auth_from_dom;
        }

        if (ep) {
            if (!data::valid (ep))
                throw exception {3} << "invalid value for option 'endpoint': " << *ep;
            auto auth_from_ep = authority {*ep};
            if (dom) throw exception {3} << "options 'domain' and 'endpoint' cannot both be present simultaneously";
            if (auth && *auth != auth_from_ep)
                throw exception {3} << "authority from options 'authority' and 'endpoint' disagree: " << *auth << " vs " << *ep;
        }

        if (addr) {
            authority auth_from_ip_port;
            if (port) auth_from_ip_port = authority {*addr, *port};
            else auth_from_ip_port = authority {*addr};
            if (dom) throw exception {3} << "options 'domain' and 'ip_address' cannot both be present simultaneously";

            if (ep) {
                auto auth_from_ep = authority {*ep};
                if (auth_from_ep != auth_from_ip_port)
                    throw data::exception {3} << "authority from option 'endpoint' and options 'ip_address' and 'port' disagree: " <<
                        auth_from_ep << " vs " << auth_from_ip_port;
            }

            if (auth && *auth != auth_from_ip_port)
                throw data::exception {3} << "authority from option 'authority' and options 'ip_address' and 'port' disagree: " <<
                    *auth << " vs " << auth_from_ip_port;
            else auth = auth_from_ip_port;
        }

        if (auth) return *auth;
        return "localhost";

    }

}
