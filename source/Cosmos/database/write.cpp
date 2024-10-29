#include <data/crypto/encrypted.hpp>
#include <data/crypto/stream/one_way.hpp>
#include <data/crypto/block/cipher.hpp>
#include <data/crypto/PKCS5_PBKDF2_HMAC.hpp>
#include <data/io/wait_for_enter.hpp>
#include <Cosmos/database/write.hpp>
#include <Cosmos/random.hpp>
#include <filesystem>
#include <fstream>

namespace Cosmos {

    JSON write_path (HD::BIP_32::path p) {
        JSON::array_t a;
        a.resize (p.size ());
        int index = 0;
        for (const uint32 &u : p) a[index++] = u;
        return a;
    }

    HD::BIP_32::path read_path (const JSON &j) {
        if (!j.is_array ()) throw exception {} << "invalid JSON path format";
        HD::BIP_32::path p;
        for (const JSON &jj : j) {
            if (!jj.is_number ()) throw exception {} << " could not read path " << j;
            p <<= uint32 (jj);
        }
        return p;
    }

    Bitcoin::outpoint read_outpoint (const string &x) {
        list<string_view> z = data::split (x, ":");
        if (z.size () != 2) throw exception {} << "invalid outpoint format: " << x;
        Bitcoin::outpoint o;
        o.Digest = read_TXID (z[0]);
        o.Index = strtoul (std::string {z[1]}.c_str (), nullptr, 10);
        return o;
    }

    JSON write (const Bitcoin::output &j) {
        JSON::object_t op;
        op["value"] = write (j.Value);
        op["script"] = encoding::hex::write (j.Script);
        return op;
    }

    JSON write (const Bitcoin::input &o) {
        JSON::object_t ip;
        ip["reference"] = write (o.Reference);
        ip["script"] = encoding::hex::write (o.Script);
        ip["sequence"] = uint32 (o.Sequence);
        return ip;
    }

    std::string write (const Bitcoin::header &h) {
        return encoding::hex::write (h.write ());
    }

    Bitcoin::header read_header (const string &j) {
        if (j.size () != 160) throw exception {} << "invalid header size for " << j;
        byte_array<80> p;
        boost::algorithm::unhex (j.begin (), j.end (), p.begin ());
        return Bitcoin::header (p);
    }

    either<Bitcoin::pubkey, HD::BIP_32::pubkey> inline read_pubkey (const string &x) {
        if (HD::BIP_32::pubkey p {x}; p.valid ()) return p;
        if (Bitcoin::pubkey p {x}; p.valid ()) return p;
        throw exception {} << "could not read " << x << " as a bitcoin public key.";
    }

    void write_to_file (const std::string &x, const std::string &filename) {
        std::fstream file;
        file.open (filename, std::ios::out);
        if (!file) throw exception {"could not open file"};
        file << static_cast<const std::string &> (x);
        file.close ();
    }

    struct file_writer final : message_writer<void, byte> {
        std::fstream Filestream;

        file_writer (const std::string &filename): Filestream {} {
            Filestream.open (filename, std::ios::out);
            if (!Filestream) throw exception {"could not open file"};
        };

        void write (const byte *b, size_t size) final override {
            Filestream.write ((const char *) b, size);
        }

        void complete () {
            Filestream.close ();
        }
    };

    void write_to_file (const file &j, const std::string &filename) {
        file_writer file {filename};
        if (bool (j.Key)) {
            bytes msg (string (j.Payload.dump ()));

            // generate new initialization vector.
            crypto::initialization_vector<16> iv;
            *get_secure_random () >> iv;

            auto ofb = crypto::output_feedback_stream<16, 32, crypto::AES> (iv, *j.Key);
            for (byte &b : msg) b = static_cast<char> (ofb.crypt (b));

            file << byte ('X') << iv << msg;

        } else file << j.Payload.dump (2, ' ');
        file.complete ();
    }

    file read_from_file (const std::string &filename) {

        std::filesystem::path p {filename};
        if (!std::filesystem::exists (p)) return JSON (nullptr);

        std::ifstream fi;
        fi.open (filename, std::ios::in);
        if (!fi) throw exception {"could not open file"};

        if (fi.eof ()) throw exception {"invalid file format"};

        char prefix = fi.get ();

        // encrypted file.
        if (prefix == 'X') {

            // get key
            crypto::symmetric_key<32> key = crypto::PKCS5_PBKDF2_HMAC<32, CryptoPP::SHA256>
                (get_user_password ("Please provide the password to decript your keys"), 25);

            crypto::initialization_vector<16> iv;

            for (int i = 0; i < 16; i++) {
                if (fi.eof ()) throw exception {} << "invalid file format";

                char next_char;
                fi >> next_char;
                iv[i] = byte (next_char);
            }

            auto ofb = crypto::output_feedback_stream<16, 32, crypto::AES> (iv, key);

            bytes payload;
            while (!fi.eof ()) {
                char next_char;
                fi >> next_char;
                payload.push_back (ofb.crypt (byte (next_char)));
            }

            return file {JSON::parse (string (payload)), key};

        }

        fi.unget ();
        return JSON::parse (fi);
    }

}
