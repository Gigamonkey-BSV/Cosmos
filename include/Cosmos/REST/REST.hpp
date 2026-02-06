#ifndef COSMOS_REST_REST
#define COSMOS_REST_REST

#include <data/net/REST.hpp>

#include <Cosmos/REST/method.hpp>
#include <Cosmos/REST/problem.hpp>
#include <Cosmos/REST/generate.hpp>
#include <Cosmos/REST/restore.hpp>

namespace schema = data::schema;

namespace Cosmos {

    auto inline prefix (method m) {
        return schema::list::value<std::string> () + schema::list::equal<method> (m);
    }

    auto inline no_params (method m) {
        return args::command (set<std::string> {}, prefix (m), schema::map::empty ());
    }

    auto inline call_options () {
        return +*schema::map::key<net::authority> ("authority") ||
        +*schema::map::key<net::domain_name> ("domain") ||
        +(*schema::map::key<net::IP::address> ("ip_address") && *schema::map::key<uint32> ("port"));
    }

    struct next_request_options {
        Diophant::symbol Name;
        maybe<Diophant::symbol> Sequence;
        next_request_options () {}
        next_request_options (const args::parsed &);
        next_request_options (Diophant::symbol wallet_name, map<UTF8, UTF8> query):
        Name {wallet_name}, Sequence {schema::validate<> (query,
            *schema::map::key<Diophant::symbol> ("sequence"))} {}

        next_request_options &name (const Diophant::symbol &name) {
            Name = name;
            return *this;
        }

        next_request_options &sequence (const Diophant::symbol &sequence) {
            Sequence = sequence;
            return *this;
        }
    };

    struct REST : net::HTTP::REST {
        using net::HTTP::REST::REST;

        net::HTTP::request request_shutdown () const;

        net::HTTP::request request_add_entropy (const string &entropy) const;

        net::HTTP::request request_list_wallets () const;

        net::HTTP::request request_value (const Diophant::symbol &wallet_name) const;

        net::HTTP::request request (const next_request_options &) const;
        net::HTTP::request request (const generate_request_options &) const;
        net::HTTP::request request (const restore_request_options &) const;

    };

    net::HTTP::response inline ok_response () {
        return net::HTTP::response (204);
    }

    net::HTTP::response inline JSON_response (const JSON &j) {
        return net::HTTP::response (200, {{"content-type", "application/json"}}, bytes (data::string (j.dump ())));
    }

    net::HTTP::response inline string_response (const string &str) {
        return net::HTTP::response (200, {{"content-type", "text/plain"}}, bytes (str));
    }

    net::HTTP::response inline data_response (const bytes &str) {
        return net::HTTP::response (200, {{"content-type", "application/octet-stream"}}, str);
    }

    net::HTTP::response inline boolean_response (bool b) {
        return JSON_response (b);
    }

    net::HTTP::response inline value_response (Bitcoin::satoshi x) {
        return JSON_response (JSON (int64 (x)));
    }

    net::HTTP::request inline REST::request_shutdown () const {
        return this->operator () (net::HTTP::method::put, "/shutdown");
    }

    net::HTTP::request inline REST::request_add_entropy (const string &entropy) const {
        return this->operator () (net::HTTP::method::post, "/add_entropy").body (bytes (entropy));
    }

    net::HTTP::request inline REST::request_list_wallets () const {
        return this->GET ("/list_wallets");
    }

    net::HTTP::request inline REST::request_value (const Diophant::symbol &wallet_name) const {
        return this->GET (string::write ("/value/", wallet_name));
    }

    inline next_request_options::next_request_options (const args::parsed &p) {
        auto [name, sequence_name] = std::get<2> (
            args::validate (p,
                args::command {
                    set<std::string> {},
                    prefix (method::NEXT),
                    schema::map::key<Diophant::symbol> ("name") &&
                    *schema::map::key<Diophant::symbol> ("sequence")}));

        Name = name;
        Sequence = sequence_name;
    }

    net::HTTP::request inline REST::request (const next_request_options &r) const {
        auto m = this->operator () (net::HTTP::method::post, string::write ("/next/", r.Name));
        if (r.Sequence) return m.query_map ({{"sequence", UTF8 (*r.Sequence)}});
        return m;
    }

}

#endif
