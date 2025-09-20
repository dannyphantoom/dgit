#include "dgit/sha1.hpp"
#include <fstream>
#include <iomanip>
#include <sstream>
#include <cstring>

namespace dgit {

// SHA-1 constants
static const uint32_t K[4] = {
    0x5A827999, 0x6ED9EBA1, 0x8F1BBCDC, 0xCA62C1D6
};

// Left rotate helper
inline uint32_t left_rotate(uint32_t value, uint32_t count) {
    return (value << count) | (value >> (32 - count));
}

SHA1::SHA1() : h0_(0x67452301), h1_(0xEFCDAB89), h2_(0x98BADCFE),
               h3_(0x10325476), h4_(0xC3D2E1F0), message_length_(0),
               buffer_length_(0), finalized_(false) {}

void SHA1::update(const uint8_t* data, size_t length) {
    if (finalized_) {
        throw GitException("SHA1: Cannot update after finalization");
    }

    for (size_t i = 0; i < length; ++i) {
        buffer_[buffer_length_++] = data[i];
        message_length_ += 8;

        if (buffer_length_ == 64) {
            process_block();
            buffer_length_ = 0;
        }
    }
}

void SHA1::update(const std::string& data) {
    update(reinterpret_cast<const uint8_t*>(data.c_str()), data.length());
}

std::string SHA1::final() {
    if (finalized_) {
        return binary_to_hex(reinterpret_cast<uint8_t*>(&h0_), 20);
    }

    pad_message();
    process_block();
    finalized_ = true;

    // Convert hash to hex string
    uint8_t hash[20];
    uint32_t* hash_words = reinterpret_cast<uint32_t*>(hash);
    hash_words[0] = h0_;
    hash_words[1] = h1_;
    hash_words[2] = h2_;
    hash_words[3] = h3_;
    hash_words[4] = h4_;

    return binary_to_hex(hash, 20);
}

void SHA1::process_block() {
    uint32_t w[80];

    // Prepare message schedule
    for (int i = 0; i < 16; ++i) {
        w[i] = (buffer_[i * 4] << 24) | (buffer_[i * 4 + 1] << 16) |
               (buffer_[i * 4 + 2] << 8) | buffer_[i * 4 + 3];
    }

    for (int i = 16; i < 80; ++i) {
        w[i] = left_rotate(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    }

    // Initialize working variables
    uint32_t a = h0_, b = h1_, c = h2_, d = h3_, e = h4_;

    // Main rounds
    for (int i = 0; i < 80; ++i) {
        uint32_t f, k;
        if (i < 20) {
            f = (b & c) | (~b & d);
            k = K[0];
        } else if (i < 40) {
            f = b ^ c ^ d;
            k = K[1];
        } else if (i < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = K[2];
        } else {
            f = b ^ c ^ d;
            k = K[3];
        }

        uint32_t temp = left_rotate(a, 5) + f + e + k + w[i];
        e = d;
        d = c;
        c = left_rotate(b, 30);
        b = a;
        a = temp;
    }

    // Update hash values
    h0_ += a;
    h1_ += b;
    h2_ += c;
    h3_ += d;
    h4_ += e;
}

void SHA1::pad_message() {
    // Append '1' bit
    buffer_[buffer_length_++] = 0x80;

    // Pad with zeros until buffer length is 56 mod 64
    while (buffer_length_ % 64 != 56) {
        if (buffer_length_ == 64) {
            process_block();
            buffer_length_ = 0;
        }
        buffer_[buffer_length_++] = 0x00;
    }

    // Append original message length in bits
    for (int i = 7; i >= 0; --i) {
        buffer_[buffer_length_++] = (message_length_ >> (i * 8)) & 0xFF;
    }
}

std::string SHA1::hash(const std::string& data) {
    SHA1 sha1;
    sha1.update(data);
    return sha1.final();
}

std::string SHA1::hash_file(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file) {
        throw GitException("Cannot open file: " + filepath);
    }

    SHA1 sha1;
    char buffer[4096];
    while (file.read(buffer, sizeof(buffer))) {
        sha1.update(reinterpret_cast<uint8_t*>(buffer), file.gcount());
    }
    sha1.update(reinterpret_cast<uint8_t*>(buffer), file.gcount());

    return sha1.final();
}

// Utility functions
std::string hex_to_binary(const std::string& hex) {
    std::string binary;
    binary.reserve(hex.length() / 2);

    for (size_t i = 0; i < hex.length(); i += 2) {
        uint8_t byte = std::stoi(hex.substr(i, 2), nullptr, 16);
        binary.push_back(static_cast<char>(byte));
    }

    return binary;
}

std::string binary_to_hex(const uint8_t* data, size_t length) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');

    for (size_t i = 0; i < length; ++i) {
        oss << std::setw(2) << static_cast<int>(data[i]);
    }

    return oss.str();
}

} // namespace dgit
