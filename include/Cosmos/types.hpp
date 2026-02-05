#ifndef COSMOS_TYPES
#define COSMOS_TYPES

#include <data/tuple.hpp>
#include <data/math.hpp>
#include <data/numbers.hpp>
#include <data/net/JSON.hpp>
#include <Gigamonkey.hpp>
#include <gigamonkey/script/pattern/pay_to_address.hpp>
#include <gigamonkey/script/pattern/pay_to_address.hpp>

namespace net = data::net;
namespace encoding = data::encoding;

using uint32 = data::uint32;
using uint64 = data::uint64;
using int64 = data::int64;
using int32 = data::int32;

using uint16 = data::uint16;

using byte = data::byte;
using bytes = data::bytes;

using int32_little = data::int32_little;
using uint32_little = data::uint32_little;

template <size_t size> using byte_array = data::byte_array<size>;

template <typename X> using ptr = data::ptr<X>;
template <typename X> using awaitable = data::awaitable<X>;
template <typename X> using maybe = data::maybe<X>;
template <typename ... X> using either = data::either<X...>;

using string_view = data::string_view;
using string = data::string;

using UTF8 = data::UTF8;

template <typename ...X> using tuple = data::tuple<X...>;

namespace Bitcoin = Gigamonkey::Bitcoin;
namespace Merkle = Gigamonkey::Merkle;
namespace secp256k1 = Gigamonkey::secp256k1;

using digest512 = Gigamonkey::digest512;
using digest256 = Gigamonkey::digest256;
using digest160 = Gigamonkey::digest160;
using pay_to_address = Gigamonkey::pay_to_address;
using filepath = std::filesystem::path;

template <typename X> using list = data::list<X>;
template <typename K, typename V> using map = data::map<K, V>;
template <typename K, typename V> using entry = data::entry<K, V>;
template <typename K, typename V> using dispatch = data::dispatch<K, V>;
template <typename X> using set = data::set<X>;

using JSON = data::JSON;

using exception = data::exception;

namespace Cosmos {

    // 1580790300
    const Bitcoin::timestamp &genesis_update_time ();

    // an inpoint is similar to an outpoint except that it points
    // to an input rather than to an output.
    struct inpoint : Bitcoin::outpoint {
        using Bitcoin::outpoint::outpoint;
        explicit inpoint (const Bitcoin::outpoint &o) : outpoint {o} {}
    };
}

#endif
