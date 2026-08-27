#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace BPR::Config
{
    struct FoldedHash
    {
        using is_transparent = void;
        [[nodiscard]] std::size_t operator()(std::string_view value) const noexcept;
    };

    struct FoldedEqual
    {
        using is_transparent = void;
        [[nodiscard]] bool operator()(std::string_view left, std::string_view right) const noexcept;
    };

    template <class Value>
    using FoldedMap = std::unordered_map<std::string, Value, FoldedHash, FoldedEqual>;

    class IniDocument
    {
    public:
        using Section = FoldedMap<std::string>;

        [[nodiscard]] bool Parse(std::string_view text, std::string* error = nullptr);
        [[nodiscard]] bool Load(const std::filesystem::path& path, std::string* error = nullptr);
        void Overlay(const IniDocument& later);

        [[nodiscard]] std::optional<std::string_view> Find(
            std::string_view section,
            std::string_view key) const noexcept;
        [[nodiscard]] const Section* FindSection(std::string_view section) const noexcept;

    private:
        FoldedMap<Section> _sections;
    };

    [[nodiscard]] std::string Trim(std::string_view value);
    [[nodiscard]] std::string Lower(std::string value);
    [[nodiscard]] bool ParseBool(std::string_view value, bool& output) noexcept;
    [[nodiscard]] bool ParseFloat(std::string_view value, float& output) noexcept;
    [[nodiscard]] bool ParseUnsigned(std::string_view value, std::uint32_t& output) noexcept;
}
