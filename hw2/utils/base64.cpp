#include "base64.h"
#include <vector>
#include <stdexcept>

namespace {
    const std::array<int, 3> mod_table = {0, 2, 1};
    
    std::vector<unsigned char> get_decoding_table() {
        static std::vector<unsigned char> decoding_table = []() {
            std::vector<unsigned char> table(256, 0);
            for (size_t i = 0; i < encoding_table.size(); ++i) {
                table[static_cast<unsigned char>(encoding_table[i])] = static_cast<unsigned char>(i);
            }
            return table;
        }();
        return decoding_table;
    }
}

std::string base64_encode(const std::string& data) {
    size_t input_length = data.size();
    size_t output_length = 4 * ((input_length + 2) / 3);
    
    std::string encoded_data;
    encoded_data.reserve(output_length);

    for (size_t i = 0; i < input_length;) {
        uint32_t octet_a = i < input_length ? static_cast<unsigned char>(data[i++]) : 0;
        uint32_t octet_b = i < input_length ? static_cast<unsigned char>(data[i++]) : 0;
        uint32_t octet_c = i < input_length ? static_cast<unsigned char>(data[i++]) : 0;

        uint32_t triple = (octet_a << 16) + (octet_b << 8) + octet_c;

        encoded_data += encoding_table[(triple >> 18) & 0x3F];
        encoded_data += encoding_table[(triple >> 12) & 0x3F];
        encoded_data += encoding_table[(triple >> 6) & 0x3F];
        encoded_data += encoding_table[triple & 0x3F];
    }

    for (int i = 0; i < mod_table[input_length % 3]; ++i) {
        encoded_data[output_length - 1 - i] = '=';
    }

    return encoded_data;
}

std::string base64_decode(const std::string& data) {
    size_t input_length = data.length();
    
    if (input_length % 4 != 0) {
        throw std::invalid_argument("Invalid base64 input length");
    }

    const auto& decoding_table = get_decoding_table();
    
    size_t output_length = input_length / 4 * 3;
    if (data[input_length - 1] == '=') --output_length;
    if (data[input_length - 2] == '=') --output_length;

    std::string decoded_data;
    decoded_data.reserve(output_length);

    for (size_t i = 0; i < input_length;) {
        uint32_t sextet_a = data[i] == '=' ? 0 : decoding_table[static_cast<unsigned char>(data[i])];
        ++i;
        uint32_t sextet_b = data[i] == '=' ? 0 : decoding_table[static_cast<unsigned char>(data[i])];
        ++i;
        uint32_t sextet_c = data[i] == '=' ? 0 : decoding_table[static_cast<unsigned char>(data[i])];
        ++i;
        uint32_t sextet_d = data[i] == '=' ? 0 : decoding_table[static_cast<unsigned char>(data[i])];
        ++i;

        uint32_t triple = (sextet_a << 18) + (sextet_b << 12) + (sextet_c << 6) + sextet_d;

        if (decoded_data.size() < output_length) {
            decoded_data.push_back(static_cast<char>((triple >> 16) & 0xFF));
        }
        if (decoded_data.size() < output_length) {
            decoded_data.push_back(static_cast<char>((triple >> 8) & 0xFF));
        }
        if (decoded_data.size() < output_length) {
            decoded_data.push_back(static_cast<char>(triple & 0xFF));
        }
    }

    return decoded_data;
}