#include <Cosmos/REST/REST.hpp>

namespace Cosmos {

    net::HTTP::request REST::request (const generate_request_options &o) const {
        std::stringstream query_stream;
        query_stream << "style=" << o.wallet_style ();
        if (!bool (o.coin_type ())) query_stream << "&coin_type=none";
        else query_stream << "&coin_type=" << *o.coin_type ();
        if (o.mnemonic_style () != mnemonic_style::none)
            query_stream << "&mnemonic=" << o.mnemonic_style () << "&number_of_words=" << o.number_of_words ();
        return this->operator () (net::HTTP::method::post, string::write ("/generate/", o.name ())).query (query_stream.str ());
    }

}
