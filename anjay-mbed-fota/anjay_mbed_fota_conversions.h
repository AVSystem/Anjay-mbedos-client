/*
 * Copyright 2020-2025 AVSystem <avsystem@avsystem.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ANJAY_MBED_FOTA_PEM2DER_H
#define ANJAY_MBED_FOTA_PEM2DER_H

#include <array>
#include <cstddef>

namespace avs {

namespace constexpr_conversions {

constexpr const char BASE64_CHARS[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

template <typename T, size_t N, size_t... Indexes>
constexpr std::array<T, N> as_array(const T (&tab)[N],
                                    std::index_sequence<Indexes...>) {
    return { tab[Indexes]... };
}

template <typename T, size_t N>
constexpr std::array<T, N> as_array(const T (&tab)[N]) {
    return as_array<T, N>(tab, std::make_index_sequence<N>());
}

constexpr int base64_char_index(char ch) {
    for (int i = 0; i < (int) sizeof(BASE64_CHARS) - 1; ++i) {
        if (BASE64_CHARS[i] == ch) {
            return i;
        }
    }
    return -1;
}

template <size_t N, const char Str[N]>
constexpr size_t count_active_pem_chars() {
    size_t result = 0;
    for (size_t i = 0; i < N; ++i) {
        if (Str[i] == '-') {
            // Skip lines starting with '-'
            while (i < N
                   && !(Str[i] == '\n' || Str[i] == '\'' || Str[i] == '"')) {
                ++i;
            }
        } else if (base64_char_index(Str[i]) >= 0) {
            ++result;
        }
    }
    return result;
}

template <size_t N,
          const char Str[N],
          size_t OutSize = count_active_pem_chars<N, Str>()>
constexpr auto strip_non_pem_chars() {
    char result[OutSize] = {};
    size_t out = 0;
    for (size_t i = 0; i < N; ++i) {
        if (Str[i] == '-') {
            // Skip lines starting with '-'
            while (i < N
                   && !(Str[i] == '\n' || Str[i] == '\'' || Str[i] == '"')) {
                ++i;
            }
        } else if (base64_char_index(Str[i]) >= 0) {
            result[out++] = Str[i];
        }
    }
    return as_array<char, OutSize>(result);
}

template <size_t N,
          const char Str[N],
          size_t OutSize = (count_active_pem_chars<N, Str>() * 3) / 4>
constexpr auto pem2der() {
    uint8_t result[OutSize] = {};
    size_t out = 0;
    const auto input = strip_non_pem_chars<N, Str>();
    uint8_t bits = 0;
    uint32_t accumulator = 0;
    for (size_t i = 0; i < input.size(); ++i) {
        bits += 6;
        accumulator <<= 6;
        accumulator |= base64_char_index(input[i]);
        if (bits >= 8) {
            bits -= 8;
            result[out++] = (uint8_t) ((accumulator >> bits) & 0xFFu);
        }
    }
    return as_array<uint8_t, OutSize>(result);
}

template <size_t N, const char Str[N]>
constexpr size_t count_valid_hex_chars() {
    size_t result = 0;
    for (size_t i = 0; i < N; ++i) {
        if ((Str[i] >= '0' && Str[i] <= '9') || (Str[i] >= 'a' && Str[i] <= 'f')
            || (Str[i] >= 'A' && Str[i] <= 'F')) {
            ++result;
        }
    }
    return result;
}

template <size_t N,
          const char Str[N],
          size_t ValidChars = count_valid_hex_chars<N, Str>()>
constexpr auto unhexlify() {
    static_assert(ValidChars % 2 == 0, "Odd number of hex characters in input");
    uint8_t result[ValidChars / 2] = {};
    size_t out = 0;
    uint8_t nibbles = 0;
    uint8_t accumulator = 0;
    for (size_t i = 0; i < N; ++i) {
        int16_t nibble = -1;
        if (Str[i] >= '0' && Str[i] <= '9') {
            nibble = Str[i] - '0';
        } else if (Str[i] >= 'a' && Str[i] <= 'f') {
            nibble = Str[i] - 'a' + 10;
        } else if (Str[i] >= 'A' && Str[i] <= 'F') {
            nibble = Str[i] - 'A' + 10;
        }
        if (nibble >= 0) {
            accumulator <<= 4;
            accumulator |= nibble;
            ++nibbles;
        }
        if (nibbles == 2) {
            result[out++] = accumulator;
            accumulator = 0;
            nibbles = 0;
        }
    }
    return as_array<uint8_t, ValidChars / 2>(result);
}

} // namespace constexpr_conversions

} // namespace avs

#endif /* ANJAY_MBED_FOTA_PEM2DER_H */
