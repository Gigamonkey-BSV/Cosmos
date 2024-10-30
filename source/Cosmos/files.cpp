#include <data/crypto/encrypted.hpp>
#include <data/crypto/stream/one_way.hpp>
#include <data/crypto/block/cipher.hpp>
#include <data/crypto/PKCS5_PBKDF2_HMAC.hpp>
#include <data/io/wait_for_enter.hpp>
#include <Cosmos/files.hpp>
#include <Cosmos/random.hpp>
#include <filesystem>
#include <fstream>

namespace Cosmos {

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
                (get_user_password ("Please provide the password to decript your keys"), 2048);

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

    files files::write (const std::string &x, const JSON &j) const {
        if (const auto *v = this->contains (x); !bool (v)) {
            write_to_file (file {j}, x);
            return this->insert (x, {});
        } else {
            const maybe<crypto::symmetric_key<32>> &k = *v;
            if (bool (k)) write_to_file (file {j, *k}, x);
            else write_to_file (file {j}, x);
            return *this;
        }
    }

    tuple<JSON, files> files::load (const std::string &x) const {
        if (const auto *v = this->contains (x); bool (v))
            throw exception {} << "file " << x << " already loaded";
        else {
            file fi = read_from_file (x);
            return {fi.Payload, this->insert (x, fi.Key)};
        }
    }

}

