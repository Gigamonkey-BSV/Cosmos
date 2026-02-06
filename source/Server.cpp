#include <charconv>

#include <cstdlib>  // for std::getenv
#include <laserpants/dotenv/dotenv.h>

#include <data/async.hpp>
#include <data/net/URL.hpp>
#include <data/net/JSON.hpp>
#include <data/net/HTTP_server.hpp>
#include <data/io/random.hpp>
#include <data/io/error.hpp>

// TODO: in here we have 'using namespace data' and that causes a
// problem if the includes are in the wrong order. This should be fixed. 
#include "Cosmos.hpp"
#include "server/server.hpp"

#include <Cosmos/options.hpp>
#include <Cosmos/Diophant.hpp>

using error = data::io::error;

void run (const options &);

void signal_handler (int signal);

template <typename fun, typename ...args>
requires std::regular_invocable<fun, args...>
error catch_all (fun f, args... a) {
    try {
        std::invoke (std::forward<fun> (f), std::forward<args> (a)...);
    } catch (const data::method::unimplemented &x) {
        return error {error::programmer_action, x.what ()};
    } catch (const net::HTTP::exception &x) {
        return error {error::operator_action, x.what ()};
    } catch (const JSON::exception &x) {
        return error {error::operator_action, x.what ()};
    } catch (const CryptoPP::Exception &x) {
        return error {error::programmer_action, x.what ()};
    }

    return error {};
}

int main (int arg_count, char **arg_values) {
    std::signal (SIGINT, signal_handler);
    std::signal (SIGTERM, signal_handler);

    error err = catch_all (run, options {args::parsed {arg_count, arg_values}});

    if (bool (err)) {
        if (err.Message) DATA_LOG (error) << "Fail code " << err.Code << ": " << *err.Message << std::endl;
        else DATA_LOG (error) << "Fail code " << err.Code << "." << std::endl;
    }

    return err.Code;

}

// we use this to handle all concurrent programming.
boost::asio::io_context IO;

ptr<database> DB;
std::unique_ptr<Cosmos::network> Network;
std::unique_ptr<net::HTTP::server> Server;

std::atomic<bool> Shutdown {false};

bool ShutdownInProgress {false};
std::mutex ShutdownMutex;

void shutdown () noexcept {
    std::lock_guard<std::mutex> lock {ShutdownMutex};
    if (ShutdownInProgress) return;
    std::cout << "\nShut down!" << std::endl;
    ShutdownInProgress = true;
    if (Server != nullptr) Server->close ();
}

void signal_handler (int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        Shutdown = true;
        shutdown ();
    }
}

Cosmos::random::user_entropy UserEntropy;

namespace data::random {
    bytes Personalization {string {"Cosmos wallet v1alpha, woop zoop dedoop!_^(G0899[p9.[09954g2[]]])"}};
}

void init_random (const options &program_options) {

    auto mn = program_options.nonce ();

    data::random::init ({
        .secure = true,
        .seed = program_options.seed (),
        .nonce = bool (mn) ? *mn : data::bytes {},
        .additional = program_options.incorporate_user_entropy () ? &UserEntropy : nullptr,
        .strength = 256
    });
}

using namespace Cosmos;

void run (const options &program_options) {
    // print version string.
    if (program_options.has ("version")) {
        version (std::cout);
        return;
    }

    // TODO what about help?

    {
        // path to an env file containing program options.
        auto envpath_param = program_options.env ();
        // if the user provided no path, we look for a file called ".env" in the working directory.
        std::filesystem::path envpath = bool (envpath_param) ? *envpath_param : ".env";

        if (bool (envpath_param)) DATA_LOG (note) << "No env path provided with --env. Using default " << envpath;
        DATA_LOG (note) << "Searching for env path " << envpath << std::endl;

        std::error_code ec;
        if (!std::filesystem::exists (envpath, ec)) {
            DATA_LOG (note) << "No file " << envpath << " found" << std::endl;
            // If the user provided a path, it is an error if it is not found.
            // Otherwise we look in the default location but don't throw an
            // error if we don't find it.
            if (bool (envpath_param)) throw data::exception {} << "file " << envpath << " does not exist ";
            else dotenv::init ();
        } else if (ec) throw data::exception {} << "could not access file " << envpath;
        else dotenv::init (envpath.c_str ());
    }

    // initialize random number generator.
    init_random (program_options);

    // set up db
    DB = load_DB (program_options.db_options ());
    Cosmos::diophant::initialize (DB);

    // set up network
    if (!program_options.local ()) {
        Network = std::unique_ptr<Cosmos::network> (new Cosmos::network {IO.get_executor ()});

        // TODO: check health of network

        // TODO: update pending transactions
    }

    {
        net::IP::TCP::endpoint endpoint = program_options.endpoint ();

        if (!endpoint.valid ()) throw data::exception {} <<
            "invalid tcp endpoint " << endpoint << "; it should look something like tcp://127.0.0.1:4567";

        if (endpoint.address () == net::IP::address {"0.0.0.0"})
            DATA_LOG (warning) << "WARNING: the program has been set to accept connections over the Internet. In this mode of operation, "
                "the user must ensure that all connections to the program are authorized. Right now, Cosmos does nothing to hide "
                "keys from unauthorized access.";

        DATA_LOG (note) << "running with endpoint " << endpoint << std::endl;
        DATA_LOG (note) << "running with ip address " << endpoint.address () << " and listening on port " << endpoint.port () << std::endl;
        DATA_LOG (note) << "connect to " <<
          net::URL (net::URL::make ().protocol ("http").address (endpoint.address ()).port (endpoint.port ())) <<
          " to see the GUI." << std::endl;

        Server = std::unique_ptr<net::HTTP::server> {new net::HTTP::server
            (IO.get_executor (), endpoint, server {program_options.spend_options (), *DB, Network.get (), &UserEntropy})};
    }

    // We should be able to work with multiple threads now except
    // that not everything we are using is thread safe.
    //   * Need a thread-safe database
    //   * Need a thread-safe internet stream.
    //   * thread safe random number generator.
    uint32 num_threads = program_options.threads ();
    if (num_threads < 1) throw data::exception {} <<
        "We cannot run with zero threads. There is already one thread running to read in the input you have provided.";

    if (num_threads > 1) throw data::exception {} <<
        "We only support 1 thread right now.";

    // spawn a coroutine for each thread.
    for (int i = 0; i < num_threads; i++)
        data::spawn (IO.get_executor (), [&] () -> awaitable<void> {
            int id = i;
            DATA_LOG (debug) << "awaiting server accept on coroutine " << id;
            while (co_await Server->accept ()) {
                DATA_LOG (debug) << "successfully responded to incoming request on coroutine " << id;
            }
        });

    // Logic for handling exceptions thrown by threads.
    std::exception_ptr stored_exception = nullptr;
    std::mutex exception_mutex;

    std::vector<std::thread> threads {};

    auto main_loop = [&]() {
        try {
            while (!Shutdown) try {
                IO.run ();
            } catch (boost::system::system_error &e) {
                if (e.code () == boost::beast::http::error::bad_method ||
                    e.code () == boost::beast::http::error::need_more ||
                    e.code () == boost::beast::http::error::bad_version ||
                    e.code () == boost::beast::http::error::bad_line_ending ||
                    e.code () == boost::beast::http::error::bad_target ||
                    e.code () == boost::beast::http::error::bad_field) {
                    DATA_LOG (error) << "Unable to read incoming stream from client. "
                      "This can happen if you try to connect with https (or any other protocol) instead of http. "
                      "Please ensure you are using the correct protocol and try again.";
                }
            }
        } catch (...) {
            std::lock_guard<std::mutex> lock (exception_mutex);
            // Capture first exception
            if (!stored_exception) stored_exception = std::current_exception (); 
            Shutdown = true;
        }
            
        shutdown ();
    };

    // everybody runs main loop.
    for (int i = 1; i < num_threads; i++) threads.emplace_back (main_loop);

    main_loop ();

    for (int i = 1; i < num_threads; i++) threads[i - 1].join ();

    if (stored_exception) std::rethrow_exception (stored_exception);

    IO.stop ();
}

std::ostream &version (std::ostream &o) {
    return o << "Cosmos Wallet version 0.0.2 alpha";
}

std::ostream &help (std::ostream &o, method meth) {
    switch (meth) {
        default :
            return version (o) << "\n" << "input should be <method> <args>... where method is "
                "\n\tgenerate   -- create a new wallet."
                "\n\tupdate     -- get Merkle proofs for txs that were pending last time the program ran."
                "\n\tvalue      -- print the total value in the wallet."
                "\n\trequest    -- generate a payment_request."
                "\n\tpay        -- create a transaction based on a payment request."
                "\n\treceive    -- accept a new transaction for a payment request."
                "\n\tsign       -- sign an unsigned transaction."
                "\n\timport     -- add a utxo to this wallet."
                "\n\tsend       -- send to an address or script. (depricated)"
                "\n\tboost      -- boost content."
                "\n\tsplit      -- split an output into many pieces"
                "\n\trestore    -- restore a wallet from words, a key, or many other options."
                "\nuse help \"method\" for information on a specific method";
        case method::GENERATE :
            return o << "Generate a new wallet in terms of 24 words (BIP 39) or as an extended private key."
                "\narguments for method generate:"
                "\n\t(--name=)<wallet name>"
                "\n\t(--words) (use BIP 39)"
                "\n\t(--no_words) (don't use BIP 39)"
                "\n\t(--accounts=<uint32> (=10)) (how many accounts to pre-generate)"
                // TODO: as in method restore, a string should be accepted here which
                // could have "bitcoin" "bitcoin_cash" or "bitcoinSV" as its values,
                // ignoring case, spaces, and '_'.
                "\n\t(--coin_type=<uint32> (=0)) (value of BIP 44 coin_type)";
        case method::VALUE :
            return o << "Print the value in a wallet. No parameters.";
        case method::REQUEST :
            return o << "Generate a new payment request."
                "\narguments for method request:"
                "\n\t(--name=)<wallet name>"
                "\n\t(--payment_type=\"pubkey\"|\"address\"|\"xpub\") (= \"address\")"
                "\n\t(--expires=<number of minutes before expiration>)"
                "\n\t(--memo=\"<explanation of the nature of the payment>\")"
                "\n\t(--amount=<expected amount of payment>)";
        case method::PAY :
            return o << "Respond to a payment request by creating a payment."
                "\narguments for method pay:"
                "\n\t(--name=)<wallet name>"
                "\n\t(--request=)<payment request>"
                "\n\t(--address=<address to pay to>)"
                "\n\t(--amount=<amount to pay>)"
                "\n\t(--memo=<what is the payment about>)"
                "\n\t(--output=<output in hex>)"
                "\n\t(--min_sats_per_output=<float>) (= " << Cosmos::spend_options::DefaultMinSatsPerOutput << ")"
                "\n\t(--max_sats_per_output=<float>) (= " << Cosmos::spend_options::DefaultMaxSatsPerOutput << ")"
                "\n\t(--mean_sats_per_output=<float>) (= " << Cosmos::spend_options::DefaultMeanSatsPerOutput << ") ";
        case method::ACCEPT :
            return o << "Accept a payment."
                "\narguments for method accept:"
                "\n\t(--payment=)<payment tx in BEEF or SPV envelope>";
        case method::SIGN :
            return o << "arguments for method sign not yet available.";
        case method::IMPORT :
            return o << "arguments for method import not yet available.";
        case method::SEND :
            return o << "This method is DEPRICATED";
        case method::SPEND :
            return o << "Spend coins";
        case method::BOOST :
            return o << "arguments for method boost not yet available.";
        case method::SPLIT :
            return o << "Split outputs in your wallet into many tiny outputs with small values over a triangular distribution. "
                "\narguments for method split:"
                "\n\t(--name=)<wallet name>"
                "\n\t(--address=)<address | xpub | script hash>"
                "\n\t(--max_look_ahead=)<integer> (= 10) ; (only used if parameter 'address' is provided as an xpub"
                "\n\t(--min_sats_per_output=<float>) (= " << Cosmos::spend_options::DefaultMinSatsPerOutput << ")"
                "\n\t(--max_sats_per_output=<float>) (= " << Cosmos::spend_options::DefaultMaxSatsPerOutput << ")"
                "\n\t(--mean_sats_per_output=<float>) (= " << Cosmos::spend_options::DefaultMeanSatsPerOutput << ") ";
        case method::RESTORE :
            o << "arguments for method restore:"
                "\n\t(--name=)<wallet name>"
                "\n\t(--key=)<xpub | xpriv>"
                "\n\t(--max_look_ahead=)<integer> (= 10)"
                "\n\t(--words=<string>)"
                "\n\t(--key_type=\"HD_sequence\"|\"BIP44_account\"|\"BIP44_master\") (= \"HD_sequence\")"
                "\n\t(--coin_type=\"Bitcoin\"|\"BitcoinCash\"|\"BitcoinSV\"|<integer>)"
                "\n\t(--wallet_type=\"RelayX\"|\"ElectrumSV\"|\"SimplyCash\"|\"CentBee\"|<string>)"
                "\n\t(--entropy=<string>)";
        case method::ENCRYPT_KEY:
            o << "Encrypt the private key file so that it can only be accessed with a password. No parameters.";
        case method::DECRYPT_KEY :
            return o << "Decrypt the private key file again. No parameters.";
    }

}

net::HTTP::response HTML_JS_UI_response () {
    data::string ui =
R"--(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8" />
  <title>Node API Interface</title>
  <style>
    body {
      font-family: sans-serif;
      padding: 1em;
      background: #f0f0f0;
    }
    details {
      margin-bottom: 1em;
      background: #fff;
      border-radius: 8px;
      box-shadow: 0 0 5px rgba(0,0,0,0.1);
      padding: 1em;
    }
    summary {
      font-weight: bold;
      cursor: pointer;
    }
    button {
      margin-top: 0.5em;
      margin-bottom: 0.5em;
    }
    pre {
      background: #eee;
      padding: 0.5em;
      border-radius: 4px;
      white-space: pre-wrap;
      word-break: break-word;
    }
  </style>
</head>
<body>
  <h1>Cosmos Wallet</h1>

  <h2>Basic Functions</h2>

  <details>
    <summary>Version</summary>
    <p>Get version string.</p>
    <code>GET /version</code>
    <form id="form-version">
      <button type="button" id="submit-version" onclick="callApi('GET', 'version')">Get Version</button>
    </form>
    <pre id="output-version"></pre>
  </details>

  <details>
  <summary>Help</summary>
    <p>General help or get help with a specific function.</p>
    <p>
      <code>GET /help</code>
      or 
      <code>GET /help/&lt;method&gt;</code>
    </p>
    <form id="form-help">
      <label><input type="radio" name="method" value="shutdown" onclick="toggleRadio(this)">shutdown</label>
      <br>
      <label><input type="radio" name="method" value="add_entropy" onclick="toggleRadio(this)">add_entropy</label>
      <br>
      <label><input type="radio" name="method" value="invert_hash" onclick="toggleRadio(this)">invert_hash</label>
      <br>
      <label><input type="radio" name="method" value="add_key" onclick="toggleRadio(this)">add_key</label>
      <br>
      <label><input type="radio" name="method" value="to_private" onclick="toggleRadio(this)">to_private</label>
      <br>
      <label><input type="radio" name="method" value="list_wallets" onclick="toggleRadio(this)">list_wallets</label>
      <br>
      <label><input type="radio" name="method" value="make_wallet" onclick="toggleRadio(this)">make_wallet</label>
      <br>
      <label><input type="radio" name="method" value="add_key_sequence" onclick="toggleRadio(this)">add_key_sequence</label>
      <br>
      <label><input type="radio" name="method" value="value" onclick="toggleRadio(this)">value</label>
      <br>
      <label><input type="radio" name="method" value="details" onclick="toggleRadio(this)">details</label>
      <br>
      <label><input type="radio" name="method" value="generate" onclick="toggleRadio(this)">generate</label>
      <br>
      <button type="button" id="submit-help" onclick="callHelp()">Get Help</button>
    </form>
    <pre id="output-help"></pre>
  </details>

  <details>
    <summary>Shutdown</summary>
    <p>
      Shutdown the program.
    </p>
    <p>
      <code>PUT /shutdown</code>
    </p>
    <form id="form-shutdown">
      <button type="button" id="submit-shutdown" onclick="callApi('PUT', 'shutdown')">Shutdown</button>
    </form>
    <pre id="output-shutdown"></pre>
  </details>

  <details>
    <summary>Add Entropy</summary>
    <p>
      The cryptographic random number generator needs entropy periodically. Here we add it manually.
      Type in some random text with your fingers to provide it.
    </p>
    <p>
      <code>POST /add_entropy</code>
      <br>
      Provide entropy in the body of the request. <code>content-type</code> 
      should be <code>text/plain</code> or <code>application/octet-stream</code>
    </p>
    <form id="form-add_entropy">
      <textarea id="user_entropy" name="user_entropy" rows="4" cols="50"></textarea>
      <br>
      <button type="button" id="submit-add_entropy" onclick="callAddEntropy()">Add Entropy</button>
    </form>
    <pre id="output-add_entropy"></pre>
  </details>

  <h2>Keys and data</h2>

  <details>
    <summary>Invert Hash</summary>
    <p>
      Add data and its hash, which can be retrieved later by hash.
    </p>
    <p>
      <code>POST /invert_hash?digest=&lt;hex&gt;&function=&lt;hash function name&gt;&</code> or <code>GET /invert_hash?digest=&lt;hex&gt;</code>
      <br>
      Supported hash functions are <code>SHA1</code>, <code>SHA2_256</code>, <code>RIPEMD160</code>, 
      <code>Hash256</code>, and <code>Hash160</code>, 
    </p>
    <form id="form-invert_hash">
      <label><input type="radio" name="HTTP_method" value="GET">GET</label>
      <label><input type="radio" name="HTTP_method" value="POST">POST</label>
      <br>
      <b>digest: (hex) </b><input name="hash_digest" type="text">
      <label for="digest_format"></label>
      <select name="digest_format">
        <option value="HEX">hex</option>
        <option value="BASE58_CHECK">base 58 check</option>
        <option value="BASE64">base 64</option>
      </select>
      <br>
      <label><input type="radio" name="function" value="SHA1" onclick="toggleRadio (this)">SHA1</label>
      <br>
      <label><input type="radio" name="function" value="SHA2_256" onclick="toggleRadio (this)">SHA2 256</label>
      <br>
      <label><input type="radio" name="function" value="RIPEMD160" onclick="toggleRadio (this)">RIPEMD160</label>
      <br>
      <label><input type="radio" name="function" value="Hash256" onclick="toggleRadio (this)">Hash256</label>
      <br>
      <label><input type="radio" name="function" value="Hash160" onclick="toggleRadio (this)">Hash160</label>
      <br>
      <label><input type="radio" name="data_format" value="HEX" onclick="toggleRadio (this)">hex</label>
      <br>
      <label><input type="radio" name="data_format" value="ASCII" onclick="toggleRadio (this)">ASCII</label>
      <br>
      <textarea id="hashed_data" name="hashed_data" rows="4" cols="50"></textarea>
      <br>
      <button type="button" id="submit-invert_hash" onclick="callInvertHash()">Invert Hash</button>
    </form>
  </details>

  <details>
    <summary>Key</summary>
    <p>
      Add or retrieve a key. This could be any kind of key: public, private,
      symmetric, whatever. You can enter it in the form of a Bitcoin
      Calculator expression or generate a random key of a specific type.
    </p>
    <p>
      <code>POST /key?</code> or <code>GET /key?</code>
    </p>
    <form id="form-add_key">
      <label><input type="radio" name="HTTP_method" value="POST" checked>POST</label>
      <label><input type="radio" name="HTTP_method" value="GET">GET</label>
      <br>
      <b>key name: </b><input name="name" type="text">
      <br>
      <label><input type="radio" name="method-type" value="expression">
        <input name="value" type="text">
      </label>
      <br>
      <label><input type="radio" name="method-type" value="random-secp256k1">random secp256k1</label>
      <br>
      <label><input type="radio" name="method-type" value="random-WIF.">random secp256k1</label>
      <br>
      <label><input type="radio" name="method-type" value="random-xpriv">random xpriv</label>
      <br>
      <button type="button" id="submit-key" onclick="callKey ()">Set Key</button>
    </form>
    <pre id="output-key"></pre>
  </details>

  <details>
    <summary>To Private</summary>
    <p>
      Provide an expression to derive the private key of a public key named
      <b>key name</b>.
    </p>
    <p>
      <code>POST /to_private?</code> or <code>GET /to_private?</code>
    </p>
    <form id="form-to_private">
      <label><input type="radio" name="HTTP_method" value="POST" checked>POST</label>
      <br>
      <label><input type="radio" name="HTTP_method" value="GET" >GET</label>
      <br>
      <b>key name: </b><input name="key_name" type="text">
      <br>
      <b>value of private key: </b><input name="key_value" type="text">
      <br>
      <button type="button" id="submit-to_private" onclick="callToPrivate()">To Private</button>
    </form>
    <pre id="output-to_private"></pre>
  </details>

  <h2>Wallet Generation</h2>

  <details>
    <summary>Make Wallet</summary>
    <p>
      Make an empty wallet. The wallet has no keys associated with it. You have to build it up.
    </p>
    <p>
      <code>POST /make_wallet?name=<i>wallet_name</i></code>
    </p>
    <form id="form-make_wallet">
      <b>wallet name: </b><input name="make_wallet-wallet-name" type="text">
      <br>
      <button type="button" onclick="callMakeWallet()">Make Wallet</button>
    </form>
    <pre id="output-make_wallet"></pre>
  </details>

  <details>
    <summary>Key Sequence</summary>
    <p>
      Add a key sequence to a wallet.
    </p>
    <p>
      <code>POST /set_key_sequence?</code>
    </p>
    <form id="form-set_key_sequence">
      <b>wallet name: </b><input name="wallet-name" type="text">
      <br>
      <b>sequence name: </b><input name="sequence-name" type="text">
      <br>
      <b>key name: </b><input name="key-name" type="text">
      <br>
      <b>key expression: </b><input name="expression" type="text">
      <br>
      <button type="button" id="submit-set_key_sequence" onclick="callSetKeySequence ()">Set Key Sequence</button>
    </form>
    <pre id="output-set_key_sequence"></pre>
  </details>

  <details>
    <summary>Generate</summary>
    <p>
      Generate a wallet.
    </p>
    <p>
      <code>POST /generate/<i>wallet name</i>?<i>option</i>=<i>value</i>&<i>...</i></code>
      <br>
      Query options:
      <ul>
        <li><code>wallet_style=BIP_44|BIP_44+|experimental</code></li>
        <li><code>derivation_style=BIP_44|centbee</code><li>
        <li><code>coin_type=Bitcoin|Bitcoin_Cash|Bitcoin_SV|none</code>
        <li><code>mnemonic_style=BIP_39|Electrum_SV|none</code></li>
        <li><code>number_of_words=12|24</code></li>
      </ul>
      Response is an error if the operation fails. If it succeeds, it is the mnemonic string if one was requested, otherwise it is empty.
    </p>
    <form id="form-generate">
      <b>wallet name: </b><input name="wallet_name" type="text">
      <br>
      <b>Use mnemonic: </b><input name="mnemonic" type="checkbox" checked>
      <br>Number of words:<br>
      <label><input type="radio" name="number_of_words" value="12" onclick="toggleRadio(this)" checked>
        12 words
      </label>
      <br>
      <label><input type="radio" name="number_of_words" value="24" onclick="toggleRadio(this)">
        24 words
      </label>
      <br>Schema:<br>
      <label><input type="radio" name="mnemonic_style" value="BIP_39" onclick="toggleRadio(this)" checked>
        BIP 39 (standard)
      </label>
      <br>
      <label><input type="radio" name="mnemonic_style" value="ElectrumSV" onclick="toggleRadio(this)">
        Electrum SV
      </label>
      <br>
      <label>
      </label>
      <br>
      <label><input type="radio" name="wallet_style" value="BIP_44" onclick="toggleRadio(this)">
        bip 44
      </label>
      <br>
      <label><input type="radio" name="wallet_style" value="BIP_44+" onclick="toggleRadio(this)" checked>
        bip 44+
      </label>
      <br>
      <label><input type="radio" name="wallet_style" value="experimental" onclick="toggleRadio(this)">
        experimental
      </label>
      <br>
      <button type="button" id="submit-generate" onclick="callGenerate()">Generate</button>
    </form>
    <pre id="output-generate"></pre>
  </details>

  <details>
    <summary>restore</summary>
    Restore a wallet (not yet implemented)
    <form id="form-restore">
      <button type="button" id="submit-spend">Value</button>
    </form>
      <button type="button" id="submit-restore" onclick="callRestore()">Restore</button>
    <pre id="output-restore"></pre>
  </details>

  <h2>Wallets</h2>

  <details>
    <summary>list_wallets</summary>
    <p>
      List wallets.
    </p>
    <form id="form-list_wallets">
      <button type="button" id="submit-list_wallets" onclick="callAPI('GET', 'list_wallets)">List Wallets</button>
    </form>
    <pre id="output-list_wallets"></pre>
  </details>

  <details>
    <summary>value</summary>
    <p>
      Get wallet value.
    </p>
    <form id="form-value">
      <b>wallet name: </b><input name="wallet-name" type="text">
      <br>
      <label><input type="radio" name="choice" value="BSV" onclick="toggleRadio(this)" checked>BSV sats</label>
      <br>
      <button type="button" id="submit-value" onclick="callGetValue()">Value</button>
    </form>
    <pre id="output-value"></pre>
  </details>

  <details>
    <summary>details</summary>
    <p>
      Get wallet details (includes and information about the outputs it's stored in).
    </p>
    <form id="form-details">
      <b>wallet name: </b><input name="wallet-name" type="text">
      <br>
      <button type="button" id="submit-details" onclick="callGetWalletDetails()">Details</button>
    </form>
    <pre id="output-details"></pre>
  </details>

  <details>
    <summary>Next Key</summary>
    <p>
      Get next receive key.
    </p>
    <form id="form-next_key">
      <b>wallet name: </b><input name="wallet-name" type="text">
      <br>
      <b>sequence name (optional): </b><input name="sequence_name" type="text" value="receive">
      <br>
      <button type="button" id="submit-next_key" onclick="callWalletNextKey()">Details</button>
    </form>
    <pre id="output-next_key"></pre>
  </details>

  <details>
    <summary>Next XPub</summary>
    <p>
      Get next XPUB (if supported; BIP44+ and experimental wallet formats only.)
    </p>
    <form id="form-next_xpub">
      <b>wallet name: </b><input name="wallet-name" type="text">
      <br>
      <b>sequence name (optional): </b><input name="sequence_name" type="text" value="receivex">
      <br>
      <button type="button" id="submit-next_xpub" onclick="callWalletNextXPub()">Details</button>
    </form>
    <pre id="output-next_xpub"></pre>
  </details>

  <details>
    <summary>Import</summary>
    <form id="form-accept">
      <b>wallet name: </b><input name="wallet-name" type="text">
      <br>
      <button type="button" id="submit-accept" onclick="callWalletImport()">Import</button>
    </form>
    <pre id="output-import"></pre>
  </details>

  <details>
    <summary>Accept</summary>
    <form id="form-accept">
      <b>wallet name: </b><input name="wallet-name" type="text">
      <br>
      <button type="button" id="submit-accept" onclick="callWalletAccept()">Accept</button>
    </form>
    <pre id="output-accept"></pre>
  </details>

  <details>
    <summary>Spend</summary>
    Spend some coins.
    <form id="form-spend">
      <b>wallet name: </b><input name="wallet-name" type="text">
      <br>
      <b>amount: </b><input name="amount" type="text">
      <br>
      <b>send to: </b><input name="to" type="text"> (Can be an address or an xpub.)
      <br>
      <button type="button" id="submit-spend" onclick="callSpend()">Spend</button>
    </form>
    <pre id="output-spend"></pre>
  </details>

  <script>
    let lastChecked = {};

    function toggleRadio(radio) {
      const group = radio.name;

      if (lastChecked[group] === radio) {
          radio.checked = false;
          lastChecked[group] = null;
      } else {
          lastChecked[group] = radio;
      }
    }

    async function callApi (http_method, endpoint) {

      const form = document.getElementById (`form-${endpoint}`);
      const formData = new FormData (form);
      let url = `/${endpoint}`;

      let options = {
          method: http_method,
          headers: {}
      };

      if (http_method === 'GET') {
        // Convert formData to URL query parameters
        const params = new URLSearchParams (formData).toString ();
        url += `?${params}`;
      } else if (http_method === 'POST') {
        // Send form data as URL-encoded body
        options.body = new URLSearchParams (formData);
        options.headers['Content-Type'] = 'application/x-www-form-urlencoded';
      }

      try {
        const res = await fetch (url, options);
        const text = await res.text ();
        document.getElementById (`output-${endpoint}`).textContent = text;
      } catch (err) {
        document.getElementById (`output-${endpoint}`).textContent = 'Error: ' + err;
      }
    }

    async function callHelp () {

      const selected = document.getElementById ('form-help').elements["method"];

      const target;

      if (selected) {
        target = '/help/' + selected.value
      } else {
        target = '/help';
      }

      try {
        const response = await fetch(target, {
          method: 'GET'
        });

        const result = await response.text(); // or .json() depending on response type
        document.getElementById (`output-add_entropy`).textContent = result;
      } catch (err) {
        document.getElementById (`output-add_entropy`).textContent = 'Error: ' + err;
      }
    }

    async function callAddEntropy () {

      try {
        const response = await fetch ('/add_entropy', {
          method: 'POST',
          headers: {
            'Content-Type': 'text/plain',
          },
          body: document.getElementById ('form-add_entropy').elements["user_entropy"].value
        });

        const result = await response.text (); // or .json() depending on response type
        document.getElementById (`output-add_entropy`).textContent = result;
      } catch (err) {
        document.getElementById (`output-add_entropy`).textContent = 'Error: ' + err;
      }
    }

    async function callGenerate () {
      const elements = document.getElementById ('form-generate').elements;
      var target = '/generate/' + elements["wallet-name"].value + '?'

      // Whether we use a mnemonic at all. 
      if (elements["mnemonic"].checked) {
      
        target += 'mnemonic=true&mnemonic_style=' + 
            elements["mnemonic_style"].value + 
            '&number_of_words=' + elements["number_of_words"].value + '&';
      
      }

      target += 'wallet_style=' + elements["wallet_style"].value + '&coin_type=' + elements["coin_type"].value;

      try {
        const response = await fetch (target, {
          method: 'POST',
        });

        const result = await response.text (); // or .json() depending on response type
        document.getElementById (`output-generate`).textContent = result;
      } catch (err) {
        document.getElementById (`output-generate`).textContent = 'Error: ' + err;
      }

    }

    async function callWalletMethod (http_method, wallet_method, wallet_name, params) {
    
      let target = '/' + wallet_method + '/' + wallet_name;

      if (params =!= undefined) {
        target += '?' + new URLSearchParams (params).toString ();
      }

      let output = 'output-' + wallet_method;

      try {
        const response = await fetch (target, {
          method: http_method,
        });

        const result = await response.text (); // or .json() depending on response type
        document.getElementById (output).textContent = result;
      } catch (err) {
        document.getElementById (output).textContent = 'Error: ' + err;
      }
    }

    async function callMakeWallet () {
      callWalletMethod ('POST', 'make_wallet', document.getElementById ('form-make_wallet').elements['wallet-name']);
    }

    async function callGetValue () {
      callWalletMethod ('GET', 'value', document.getElementById ('form-value').elements['wallet-name']);
    }

    async function callGetWalletDetails () {
      callWalletMethod ('GET', 'details', document.getElementById ('form-value').elements['wallet-name']);
    }

    async function callNextAddress () {
      const elements = document.getElementById ('form-next_address').elements;
      let sequence_name = elements['sequence_name'];
      let params = {};
      if (sequence_name === "") params['sequence_name'] = sequence_name;
      callWalletMethod ('POST', 'next_address', elements['wallet_name'], params);
    }

    async function callNextXPub () {
      const elements = document.getElementById ('form-next_xpub').elements;
      let sequence_name = elements['sequence_name'];
      let params = {};
      if (sequence_name === "") params['sequence_name'] = sequence_name;
      callWalletMethod ('POST', 'next_address', elements['wallet_name'], params);
    }

    async function callInvertHash () {
      const elements = document.getElementById ('form-invert_hash').elements;
      const http_method = elements['HTTP_method'];

      let params = {
        function = element['function'];
      };

      const digest = elements['digest'];

      if (digest =!= "") {
        params['digest'] = digest;
        params['digest_format'] = elements['digest_format'];
      } else if (http_method == 'POST') {
        // TODO: this is an error
      }

      if (http_method == 'POST') {
        // TODO get the data to hash
      }

      // TODO make the query
    }

    async function callAddKey () {
      const elements = document.getElementById ('form-add_key').elements;
      const http_method = elements['HTTP_method'];
      if (http_method == 'GET') {

      } else {
        
      }
    }

    async function callToPrivate () {
      const elements = document.getElementById ('form-to_private').elements;
      const http_method = elements['HTTP_method'];
      if (http_method == 'GET') {
        
      } else {
        
      }
    }

    async function callImport () {
      const elements = document.getElementById ('form-import').elements;
    }

    async function callSpend () {
      const elements = document.getElementById ('form-spend').elements;
    }

    async function callRestore () {
      const elements = document.getElementById ('form-restore').elements;
    }
  </script>
</body>
</html>
)--";

    return net::HTTP::response (200,
        {{"content-type", "text/html"}},
        bytes (ui));
}
