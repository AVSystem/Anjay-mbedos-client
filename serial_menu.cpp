/*
 * Copyright 2020-2021 AVSystem <avsystem@avsystem.com>
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

#include "serial_menu.h"
#include <cctype>
#include <cstdio>
#include <mbed.h>

using namespace std;

namespace {

int parse_unsigned(const string &unsigned_as_str, unsigned *result) {
    if (unsigned_as_str.empty()) {
        return -1;
    }
    for (char c : unsigned_as_str) {
        if (!isdigit(c)) {
            return -1;
        }
    }
    return sscanf(unsigned_as_str.c_str(), "%u", result) == 1 ? 0 : -1;
}

bool isbackspace(char character) {
    return character == '\b' || character == 0x7F;
}

} // namespace

char SerialConfigMenu::get_char() {
    char result;
    while (mbed_file_handle(STDIN_FILENO)->read(&result, sizeof(result))
           != sizeof(result)) {
    }
    return result;
}

#if MBED_CONF_APP_SERIAL_MENU_ECHO
#define print_character_if_echo_enabled(character) \
    do {                                           \
        putchar(character);                        \
        fflush(stdout);                            \
    } while (0)
#define print_string_if_echo_enabled(string) \
    do {                                     \
        printf(string);                      \
        fflush(stdout);                      \
    } while (0)
#else // MBED_CONF_APP_SERIAL_MENU_ECHO
#define print_character_if_echo_enabled(character)
#define print_string_if_echo_enabled(string)
#endif // MBED_CONF_APP_SERIAL_MENU_ECHO

string SerialConfigMenu::read_string(const char *label) {
    // Default end of line strings:
    // mbed sterm: CRLF
    // picocom: CR

    printf("%s: ", label);
    fflush(stdout);
    string line;
    char character;
    while ((character = get_char()) != '\r') {
        if (isprint(character) || isblank(character)) {
            print_character_if_echo_enabled(character);
            line.push_back(character);
        } else if (isbackspace(character) && line.length()) {
            print_string_if_echo_enabled("\b \b");
            line.pop_back();
        }
    }
    fflush(stdin);
    print_string_if_echo_enabled("\r\n");
    return line;
}

unsigned SerialConfigMenu::read_uint(const char *label) {
    unsigned result;
    while (parse_unsigned(read_string(label), &result)) {
        printf("Could not parse input as unsigned integer. Try again.\n");
    }
    return result;
}

void SerialConfigMenu::keep_showing_until_exit_command() const {
    for (;;) {
        printf("\n### %s ###\n", label_);
        for (size_t entry_index = 0; entry_index < entries_.size();
             ++entry_index) {
            const SerialConfigMenuEntry &entry = entries_[entry_index];
            printf("%3d) %s", entry_index + 1, entry.label);
            if (entry.value_getter) {
                printf(" (%s)", entry.value_getter().c_str());
            }
            printf("\n");
        }

        while (true) {
            unsigned selected_entry = read_uint("Option number");
            if (1 <= selected_entry && selected_entry <= entries_.size()) {
                if (entries_[selected_entry - 1].on_select()
                    == SerialConfigMenuEntry::MenuLoopAction::EXIT) {
                    return;
                }
                break;
            } else {
                printf("Incorrect option number. Try again.\n");
            }
        }
    }
}
