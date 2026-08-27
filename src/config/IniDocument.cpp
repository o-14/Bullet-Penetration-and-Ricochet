#include "config/IniDocument.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iterator>

namespace BPR::Config
{
    namespace
    {
        unsigned char Fold(unsigned char value) noexcept
        {
            return value >= 'A' && value <= 'Z' ?
                static_cast<unsigned char>(value + ('a' - 'A')) : value;
        }
    }

    std::size_t FoldedHash::operator()(std::string_view value) const noexcept
    {
        std::size_t hash = sizeof(std::size_t) == 8 ? 14695981039346656037ULL : 2166136261U;
        constexpr std::size_t prime64 = 1099511628211ULL;
        constexpr std::size_t prime32 = 16777619U;
        for (unsigned char character : value) {
            hash = (hash ^ Fold(character)) * (sizeof(std::size_t) == 8 ? prime64 : prime32);
        }
        return hash;
    }

    bool FoldedEqual::operator()(std::string_view left, std::string_view right) const noexcept
    {
        if (left.size() != right.size()) {
            return false;
        }
        for (std::size_t index = 0; index < left.size(); ++index) {
            if (Fold(static_cast<unsigned char>(left[index])) !=
                Fold(static_cast<unsigned char>(right[index]))) {
                return false;
            }
        }
        return true;
    }

    std::string Trim(std::string_view value)
    {
        while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())) != 0) {
            value.remove_prefix(1);
        }
        while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())) != 0) {
            value.remove_suffix(1);
        }
        return std::string(value);
    }

    std::string Lower(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
            return static_cast<char>(Fold(character));
        });
        return value;
    }

    bool IniDocument::Parse(std::string_view text, std::string* error)
    {
        FoldedMap<Section> parsed;
        std::string currentSection;
        std::size_t lineNumber = 0;
        while (!text.empty()) {
            ++lineNumber;
            const std::size_t end = text.find_first_of("\r\n");
            std::string line = Trim(text.substr(0, end));
            if (end == std::string_view::npos) {
                text = {};
            } else {
                const std::size_t consumed = end +
                    (text[end] == '\r' && end + 1 < text.size() && text[end + 1] == '\n' ? 2U : 1U);
                text.remove_prefix(consumed);
            }

            if (line.empty() || line[0] == ';' || line[0] == '#') {
                continue;
            }
            if (line.front() == '[' && line.back() == ']') {
                currentSection = Trim(std::string_view(line).substr(1, line.size() - 2));
                if (currentSection.empty()) {
                    if (error) {
                        *error = "empty section at line " + std::to_string(lineNumber);
                    }
                    return false;
                }
                continue;
            }
            const std::size_t delimiter = line.find('=');
            if (delimiter == std::string::npos || currentSection.empty()) {
                if (error) {
                    *error = "expected key=value inside a section at line " + std::to_string(lineNumber);
                }
                return false;
            }
            const std::string key = Trim(std::string_view(line).substr(0, delimiter));
            const std::string value = Trim(std::string_view(line).substr(delimiter + 1));
            if (key.empty()) {
                if (error) {
                    *error = "empty key at line " + std::to_string(lineNumber);
                }
                return false;
            }
            parsed[currentSection][key] = value;
        }
        _sections = std::move(parsed);
        return true;
    }

    bool IniDocument::Load(const std::filesystem::path& path, std::string* error)
    {
        std::ifstream stream(path, std::ios::binary);
        if (!stream) {
            if (error) {
                *error = "could not open " + path.string();
            }
            return false;
        }
        const std::string text{
            std::istreambuf_iterator<char>(stream),
            std::istreambuf_iterator<char>()
        };
        return Parse(text, error);
    }

    void IniDocument::Overlay(const IniDocument& later)
    {
        for (const auto& [sectionName, section] : later._sections) {
            Section& destination = _sections[sectionName];
            for (const auto& [key, value] : section) {
                destination[key] = value;
            }
        }
    }

    std::optional<std::string_view> IniDocument::Find(
        std::string_view section,
        std::string_view key) const noexcept
    {
        const auto foundSection = _sections.find(section);
        if (foundSection == _sections.end()) {
            return std::nullopt;
        }
        const auto foundValue = foundSection->second.find(key);
        if (foundValue == foundSection->second.end()) {
            return std::nullopt;
        }
        return foundValue->second;
    }

    const IniDocument::Section* IniDocument::FindSection(std::string_view section) const noexcept
    {
        const auto found = _sections.find(section);
        return found == _sections.end() ? nullptr : &found->second;
    }

    bool ParseBool(std::string_view value, bool& output) noexcept
    {
        const std::string folded = Lower(Trim(value));
        if (folded == "true" || folded == "yes" || folded == "on" || folded == "1") {
            output = true;
            return true;
        }
        if (folded == "false" || folded == "no" || folded == "off" || folded == "0") {
            output = false;
            return true;
        }
        return false;
    }

    bool ParseFloat(std::string_view value, float& output) noexcept
    {
        const std::string text = Trim(value);
        if (text.empty()) {
            return false;
        }
        char* end = nullptr;
        const float parsed = std::strtof(text.c_str(), &end);
        if (end != text.c_str() + text.size() || !std::isfinite(parsed)) {
            return false;
        }
        output = parsed;
        return true;
    }

    bool ParseUnsigned(std::string_view value, std::uint32_t& output) noexcept
    {
        const std::string text = Trim(value);
        if (text.empty()) {
            return false;
        }
        std::uint32_t parsed = 0;
        const auto result = std::from_chars(text.data(), text.data() + text.size(), parsed, 10);
        if (result.ec != std::errc{} || result.ptr != text.data() + text.size()) {
            return false;
        }
        output = parsed;
        return true;
    }
}
