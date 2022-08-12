/*
 * Copyright 2020-2022 AVSystem <avsystem@avsystem.com>
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

#ifndef SERIAL_MENU_H
#define SERIAL_MENU_H

#include <functional>
#include <initializer_list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

struct SerialConfigMenuEntry {
    enum class MenuLoopAction { CONTINUE, EXIT };
    const char *label;
    std::function<MenuLoopAction()> on_select;
    std::function<std::string()> value_getter;

    SerialConfigMenuEntry(const char *label,
                          std::function<MenuLoopAction()> &&on_select,
                          std::function<std::string()> &&value_getter = {})
            : label(label), on_select(on_select), value_getter(value_getter) {}
};

class SerialConfigMenu {
    const char *label_;
    std::vector<SerialConfigMenuEntry> entries_;

public:
    SerialConfigMenu(const char *label,
                     std::vector<SerialConfigMenuEntry> &&entries)
            : label_(label), entries_(std::move(entries)) {}
    SerialConfigMenu(const char *label,
                     std::initializer_list<SerialConfigMenuEntry> entries)
            : label_(label), entries_(entries) {}
    static char get_char();
    static std::string read_string(const char *label);
    static unsigned read_uint(const char *label);
    void keep_showing_until_exit_command() const;
};

template <typename EnumType>
class EnumSelectMenu {
    using EnumNamePair = std::pair<EnumType, const char *>;
    EnumNamePair selected_value_;
    SerialConfigMenu menu_;

    static EnumNamePair init_selected_value(
            EnumType initial_value,
            std::initializer_list<EnumNamePair> enum_names_mapping) {
        for (EnumNamePair pair : enum_names_mapping) {
            if (pair.first == initial_value) {
                return pair;
            }
        }
        return EnumNamePair(initial_value, "???");
    }

    SerialConfigMenu
    init_menu(const char *label,
              std::initializer_list<EnumNamePair> enum_names_mapping) {
        std::vector<SerialConfigMenuEntry> entries;
        for (EnumNamePair pair : enum_names_mapping) {
            entries.emplace_back(pair.second, [this, pair]() {
                selected_value_ = pair;
                return SerialConfigMenuEntry::MenuLoopAction::EXIT;
            });
        }
        entries.emplace_back("Back", []() {
            return SerialConfigMenuEntry::MenuLoopAction::EXIT;
        });
        return SerialConfigMenu(label, std::move(entries));
    }

public:
    EnumSelectMenu(const char *label,
                   EnumType initial_value,
                   std::initializer_list<EnumNamePair> enum_names_mapping)
            : selected_value_(
                    init_selected_value(initial_value, enum_names_mapping)),
              menu_(init_menu(label, enum_names_mapping)) {}

    EnumType show_and_get_value() {
        menu_.keep_showing_until_exit_command();
        return selected_value_.first;
    }

    const char *get_value_as_string() {
        return selected_value_.second;
    }
};

#endif // SERIAL_MENU_H
