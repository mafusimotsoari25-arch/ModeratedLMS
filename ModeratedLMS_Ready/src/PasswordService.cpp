#include "PasswordService.h"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/crypto.h>
#include <array>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace {
constexpr int SaltBytes = 16;
constexpr int HashBytes = 32;
constexpr int Iterations = 600000;

std::string toHex(const unsigned char* data, std::size_t length) {
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (std::size_t i = 0; i < length; ++i) out << std::setw(2) << static_cast<int>(data[i]);
    return out.str();
}
std::vector<unsigned char> fromHex(const std::string& text) {
    if (text.size() % 2 != 0) throw std::invalid_argument("Invalid encoded password hash");
    std::vector<unsigned char> out;
    out.reserve(text.size() / 2);
    for (std::size_t i = 0; i < text.size(); i += 2) {
        unsigned int value = 0;
        std::istringstream in(text.substr(i, 2));
        in >> std::hex >> value;
        if (in.fail()) throw std::invalid_argument("Invalid encoded password hash");
        out.push_back(static_cast<unsigned char>(value));
    }
    return out;
}
std::vector<unsigned char> derive(const std::string& password,
                                  const std::vector<unsigned char>& salt) {
    std::vector<unsigned char> out(HashBytes);
    if (PKCS5_PBKDF2_HMAC(password.c_str(), static_cast<int>(password.size()),
                          salt.data(), static_cast<int>(salt.size()), Iterations,
                          EVP_sha256(), static_cast<int>(out.size()), out.data()) != 1) {
        throw std::runtime_error("Password key derivation failed");
    }
    return out;
}
}

std::string PasswordService::hashPassword(const std::string& password) {
    if (password.size() < 8) throw std::invalid_argument("Password must contain at least 8 characters");
    std::array<unsigned char, SaltBytes> salt{};
    if (RAND_bytes(salt.data(), static_cast<int>(salt.size())) != 1)
        throw std::runtime_error("Secure salt generation failed");
    auto hash = derive(password, std::vector<unsigned char>(salt.begin(), salt.end()));
    return "pbkdf2-sha256$" + std::to_string(Iterations) + "$" +
           toHex(salt.data(), salt.size()) + "$" + toHex(hash.data(), hash.size());
}

bool PasswordService::verifyPassword(const std::string& encodedHash,
                                     const std::string& password) {
    const auto first = encodedHash.find('$');
    const auto second = first == std::string::npos ? first : encodedHash.find('$', first + 1);
    const auto third = second == std::string::npos ? second : encodedHash.find('$', second + 1);
    if (first == std::string::npos || second == std::string::npos || third == std::string::npos ||
        encodedHash.substr(0, first) != "pbkdf2-sha256") return false;
    try {
        const int iterations = std::stoi(encodedHash.substr(first + 1, second - first - 1));
        if (iterations < 100000 || iterations > 2000000) return false;
        auto salt = fromHex(encodedHash.substr(second + 1, third - second - 1));
        auto expected = fromHex(encodedHash.substr(third + 1));
        if (salt.size() != SaltBytes || expected.size() != HashBytes) return false;
        std::vector<unsigned char> actual(HashBytes);
        if (PKCS5_PBKDF2_HMAC(password.c_str(), static_cast<int>(password.size()),
                              salt.data(), static_cast<int>(salt.size()), iterations,
                              EVP_sha256(), static_cast<int>(actual.size()), actual.data()) != 1)
            return false;
        return CRYPTO_memcmp(actual.data(), expected.data(), expected.size()) == 0;
    } catch (...) {
        return false;
    }
}
