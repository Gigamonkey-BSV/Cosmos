#include <data/crypto/encrypted.hpp>
#include <data/crypto/block.hpp>
#include <data/crypto/block/cryptopp.hpp>
#include <data/crypto/PKCS5_PBKDF2_HMAC.hpp>
#include <data/io/wait_for_enter.hpp>
#include <Cosmos/files.hpp>
#include <data/random.hpp>
#include <filesystem>
#include <fstream>

namespace Cosmos {

    void write_to_file (const std::string &x, const std::string &filename) {
        std::fstream file;
        file.open (filename, std::ios::out);
        if (!file) throw data::exception {"could not open file"};
        file << static_cast<const std::string &> (x);
        file.close ();
    }

    struct file_writer final : data::message_writer<void, byte> {
        std::fstream Filestream;

        file_writer (const std::string &filename): Filestream {} {
            Filestream.open (filename, std::ios::out);
            if (!Filestream) throw data::exception {"could not open file"};
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
            using namespace data::crypto;
            bytes msg (string (j.Payload.dump ()));

            // generate new initialization vector.
            initialization_vector<16> iv;

            data::random::get () >> iv;

            bytes ciphertext = encrypt (
                block_cipher<cipher::block::AES, cipher::block::mode::OFB> {iv},
                *j.Key, msg);

            // we use this byte to prepend the file
            // because then we know it cannot be JSON.
            // TODO this format sucks, I hate it.
            file << byte ('X') << iv << msg;

        } else file << j.Payload.dump (2, ' ');
        file.complete ();
    }

    crypto::symmetric_key<32> get_user_key_from_password () {
        return crypto::PKCS5_PBKDF2_HMAC<32, CryptoPP::SHA256> (
            data::get_user_password ("Please provide the password to decript your keys"), 2048);
    }

    file read_from_file (const std::string &filename, crypto::symmetric_key<32> (*get_key) ()) {

        std::filesystem::path p {filename};
        if (!std::filesystem::exists (p)) return JSON (nullptr);

        std::ifstream fi;
        fi.open (filename, std::ios::in);
        if (!fi) throw data::exception {"could not open file"};

        if (fi.eof ()) throw data::exception {"invalid file format"};

        char prefix = fi.get ();

        // encrypted file.
        if (prefix == 'X') {
            using namespace crypto;

            symmetric_key<32> key = get_key ();

            initialization_vector<16> iv;

            for (int i = 0; i < 16; i++) {
                if (fi.eof ()) throw data::exception {} << "invalid file format";

                char next_char;
                fi >> next_char;
                iv[i] = byte (next_char);
            }

            bytes ciphertext;
            while (!fi.eof ()) {
                char next_char;
                fi >> next_char;
                ciphertext.push_back (next_char);
            }

            return file {JSON::parse (string (decrypt (block_cipher<cipher::block::AES, cipher::block::mode::OFB> {iv}, key, ciphertext))), key};

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
            throw data::exception {} << "file " << x << " already loaded";
        else {
            file fi = read_from_file (x);
            return {fi.Payload, this->insert (x, fi.Key)};
        }
    }

}

