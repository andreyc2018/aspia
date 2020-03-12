//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "crypto/password_generator.h"

#include <algorithm>
#include <random>
#include <vector>

namespace crypto {

void PasswordGenerator::setCharacters(uint32_t value)
{
    if (value)
        characters_ = value;
}

void PasswordGenerator::setLength(size_t value)
{
    if (value)
        length_ = value;
}

std::string PasswordGenerator::result() const
{
    std::vector<char> table;

    if (characters_ & LOWER_CASE)
    {
        for (char i = 'a'; i < 'z'; ++i)
            table.emplace_back(i);
    }

    if (characters_ & UPPER_CASE)
    {
        for (char i = 'A'; i < 'Z'; ++i)
            table.emplace_back(i);
    }

    if (characters_ & DIGITS)
    {
        for (char i = '0'; i < '9'; ++i)
            table.emplace_back(i);
    }

    std::random_device random_device;
    std::mt19937 engine(random_device());

    std::shuffle(table.begin(), table.end(), engine);

    std::uniform_int_distribution<> uniform_distance(0, table.size() - 1);

    std::string result;
    result.resize(length_);

    for (size_t i = 0; i < length_; ++i)
        result[i] = table[uniform_distance(engine)];

    return result;
}

} // namespace crypto
