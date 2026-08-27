#include "config/Settings.h"

#include <algorithm>
#include <cctype>

namespace BPR::Config
{
    namespace
    {
        template <class Value>
        void ApplyBool(const IniDocument& document, std::string_view section, std::string_view key, Value& value)
        {
            bool parsed = false;
            if (const auto text = document.Find(section, key); text && ParseBool(*text, parsed)) {
                value = parsed;
            }
        }

        void ApplyFloat(
            const IniDocument& document,
            std::string_view section,
            std::string_view key,
            float& value,
            float minimum,
            float maximum)
        {
            float parsed = 0.0F;
            if (const auto text = document.Find(section, key); text && ParseFloat(*text, parsed)) {
                value = std::clamp(parsed, minimum, maximum);
            }
        }

        void ApplyCount(
            const IniDocument& document,
            std::string_view section,
            std::string_view key,
            std::uint32_t& value,
            std::uint32_t maximum)
        {
            std::uint32_t parsed = 0;
            if (const auto text = document.Find(section, key); text && ParseUnsigned(*text, parsed)) {
                value = std::min(parsed, maximum);
            }
        }

        std::pair<std::string_view, std::string_view> Split(std::string_view value) noexcept
        {
            const std::size_t delimiter = value.find(',');
            return delimiter == std::string_view::npos ?
                std::pair{ value, std::string_view{} } :
                std::pair{ value.substr(0, delimiter), value.substr(delimiter + 1) };
        }

        template <class Callback>
        void Fields(std::string_view text, Callback&& callback)
        {
            while (!text.empty()) {
                const auto [field, tail] = Split(text);
                const std::size_t delimiter = field.find(':');
                if (delimiter != std::string_view::npos) {
                    callback(
                        Lower(Trim(field.substr(0, delimiter))),
                        Trim(field.substr(delimiter + 1)));
                }
                text = tail;
            }
        }

        Core::SurfaceBehavior ParseBehavior(std::string_view value) noexcept
        {
            const std::string folded = Lower(Trim(value));
            if (folded == "soft") return Core::SurfaceBehavior::kSoft;
            if (folded == "flexible") return Core::SurfaceBehavior::kFlexible;
            if (folded == "frangible") return Core::SurfaceBehavior::kFrangible;
            if (folded == "rigid") return Core::SurfaceBehavior::kRigid;
            if (folded == "liquid") return Core::SurfaceBehavior::kLiquid;
            return Core::SurfaceBehavior::kSuppressRebound;
        }

        Core::ProjectileProfile ParseProjectile(std::string_view text, Core::ProjectileProfile value)
        {
            Fields(text, [&](const std::string& name, const std::string& field) {
                float number = 0.0F;
                bool flag = false;
                if (name == "depth" && ParseFloat(field, number)) {
                    value.penetrationScale = std::clamp(number, 0.0F, 8.0F);
                } else if (name == "glancing" && ParseFloat(field, number)) {
                    value.glancingTolerance = std::clamp(number, 0.05F, 4.0F);
                } else if (name == "reboundcost" && ParseFloat(field, number)) {
                    value.reboundCostScale = std::clamp(number, 0.0F, 8.0F);
                } else if (name == "energybeam" && ParseBool(field, flag)) {
                    value.energyBeam = flag;
                }
            });
            return value;
        }

        Core::SurfaceProfile ParseSurface(std::string_view text, Core::SurfaceProfile value)
        {
            Fields(text, [&](const std::string& name, const std::string& field) {
                float number = 0.0F;
                bool flag = false;
                if (name == "depth" && ParseFloat(field, number)) {
                    value.penetrationScale = std::clamp(number, 0.0F, 8.0F);
                } else if (name == "glancing" && ParseFloat(field, number)) {
                    value.glancingLimitDegrees = std::clamp(number, 0.0F, 90.0F);
                } else if (name == "reboundcost" && ParseFloat(field, number)) {
                    value.reboundCostScale = std::clamp(number, 0.0F, 8.0F);
                } else if (name == "behavior") {
                    value.behavior = ParseBehavior(field);
                } else if (name == "conductive" && ParseBool(field, flag)) {
                    value.conductive = flag;
                }
            });
            return value;
        }

        bool FoldedContains(std::string_view text, std::string_view pattern) noexcept
        {
            if (pattern.empty() || pattern.size() > text.size()) {
                return false;
            }
            const auto equalCharacter = [](char left, char right) {
                return std::tolower(static_cast<unsigned char>(left)) ==
                    std::tolower(static_cast<unsigned char>(right));
            };
            return std::search(text.begin(), text.end(), pattern.begin(), pattern.end(), equalCharacter) !=
                text.end();
        }
    }

    DataSettings BuiltInDefaults()
    {
        DataSettings settings;
        settings.projectileProfiles.emplace("standard", Core::ProjectileProfile{});
        settings.projectileProfiles.emplace("armor-piercing", Core::ProjectileProfile{ 1.30F, 0.88F, 1.08F, false });
        settings.projectileProfiles.emplace("expanding", Core::ProjectileProfile{ 0.68F, 1.10F, 1.20F, false });
        settings.projectileProfiles.emplace("shot-pellet", Core::ProjectileProfile{ 0.42F, 0.92F, 1.32F, false });
        settings.projectileProfiles.emplace("shot-slug", Core::ProjectileProfile{ 1.04F, 0.96F, 1.12F, false });
        settings.projectileProfiles.emplace("energy-beam", Core::ProjectileProfile{ 0.74F, 1.16F, 0.90F, true });

        settings.surfaceFamilies.emplace("general", Core::SurfaceProfile{ 0.84F, 17.0F, 1.05F, Core::SurfaceBehavior::kFlexible, false });
        settings.surfaceFamilies.emplace("textile", Core::SurfaceProfile{ 1.72F, 0.0F, 0.0F, Core::SurfaceBehavior::kSuppressRebound, false });
        settings.surfaceFamilies.emplace("glass", Core::SurfaceProfile{ 1.63F, 7.0F, 1.34F, Core::SurfaceBehavior::kFrangible, false });
        settings.surfaceFamilies.emplace("lumber", Core::SurfaceProfile{ 1.14F, 17.0F, 1.18F, Core::SurfaceBehavior::kFlexible, false });
        settings.surfaceFamilies.emplace("sheet-metal", Core::SurfaceProfile{ 0.93F, 12.0F, 0.92F, Core::SurfaceBehavior::kRigid, true });
        settings.surfaceFamilies.emplace("structural-metal", Core::SurfaceProfile{ 0.66F, 9.0F, 0.72F, Core::SurfaceBehavior::kRigid, true });
        settings.surfaceFamilies.emplace("armor-metal", Core::SurfaceProfile{ 0.32F, 5.0F, 0.48F, Core::SurfaceBehavior::kRigid, true });
        settings.surfaceFamilies.emplace("masonry", Core::SurfaceProfile{ 0.52F, 8.0F, 0.81F, Core::SurfaceBehavior::kRigid, false });
        settings.surfaceFamilies.emplace("heavy-stone", Core::SurfaceProfile{ 0.34F, 5.0F, 0.66F, Core::SurfaceBehavior::kRigid, false });
        settings.surfaceFamilies.emplace("flesh", Core::SurfaceProfile{ 1.41F, 0.0F, 0.0F, Core::SurfaceBehavior::kSuppressRebound, false });
        settings.surfaceFamilies.emplace("chitin", Core::SurfaceProfile{ 0.77F, 10.0F, 1.22F, Core::SurfaceBehavior::kFlexible, false });
        settings.surfaceFamilies.emplace("soil", Core::SurfaceProfile{ 0.54F, 0.0F, 0.0F, Core::SurfaceBehavior::kSuppressRebound, false });
        settings.surfaceFamilies.emplace("plastic", Core::SurfaceProfile{ 1.26F, 15.0F, 1.35F, Core::SurfaceBehavior::kFlexible, false });
        settings.surfaceFamilies.emplace("rubber", Core::SurfaceProfile{ 0.64F, 0.0F, 0.0F, Core::SurfaceBehavior::kSuppressRebound, false });
        settings.surfaceFamilies.emplace("liquid", Core::SurfaceProfile{ 0.12F, 0.0F, 0.0F, Core::SurfaceBehavior::kLiquid, false });
        return settings;
    }

    void ApplyDocument(DataSettings& settings, const IniDocument& document)
    {
        auto& runtime = settings.runtime;
        ApplyBool(document, "Diagnostics", "DetailedLogging", runtime.detailedLogging);
        ApplyBool(document, "Visuals", "SuppressProjectileTrails", runtime.suppressProjectileTrails);
        ApplyBool(document, "Penetration", "PreventRepeatActor", runtime.preventRepeatActor);
        ApplyFloat(document, "Penetration", "DamageFalloffExponent", runtime.damageFalloffExponent, 2.0F, 5.0F);
        ApplyFloat(document, "Penetration", "VariationDegrees", runtime.penetrationVariationDegrees, 0.0F, 12.0F);
        ApplyFloat(document, "Defaults", "UnknownAmmoDepth", runtime.defaultAmmoDepth, 0.0F, 10000.0F);
        if (const auto value = document.Find("Defaults", "ProjectileProfile")) {
            runtime.defaultProjectileProfile = Lower(Trim(*value));
        }
        if (const auto value = document.Find("Defaults", "SurfaceFamily")) {
            runtime.defaultSurfaceFamily = Lower(Trim(*value));
        }

        ApplyBool(document, "Receiver", "Enabled", runtime.receiver.enabled);
        ApplyFloat(document, "Receiver", "Influence", runtime.receiver.exponent, 0.0F, 4.0F);
        ApplyFloat(document, "Receiver", "Minimum", runtime.receiver.minimum, 0.01F, 100.0F);
        ApplyFloat(document, "Receiver", "Maximum", runtime.receiver.maximum, 0.01F, 100.0F);
        if (runtime.receiver.minimum > runtime.receiver.maximum) {
            std::swap(runtime.receiver.minimum, runtime.receiver.maximum);
        }

        auto applyRicochet = [&](std::string_view section) {
            ApplyBool(document, section, "Enabled", runtime.rebound.enabled);
            ApplyFloat(document, section, "ChancePercent", runtime.rebound.chancePercent, 0.0F, 100.0F);
            ApplyFloat(document, section, "HeadOnExclusionDegrees", runtime.rebound.headOnExclusionDegrees, 0.0F, 90.0F);
            ApplyFloat(document, section, "BaseEnergyCost", runtime.rebound.baseEnergyCost, 0.0F, 1000.0F);
            ApplyFloat(document, section, "IncidenceEnergyCost", runtime.rebound.incidenceEnergyCost, 0.0F, 1000.0F);
            ApplyFloat(document, section, "RepeatPenalty", runtime.rebound.repeatPenalty, 0.0F, 10.0F);
            ApplyFloat(document, section, "VariationDegrees", runtime.rebound.variationDegrees, 0.0F, 12.0F);
            ApplyBool(document, section, "AllowProps", runtime.impactPolicy.allowPropRicochets);
        };
        // Legacy 3.0 prerelease spelling remains accepted. The user-facing
        // Ricochet section wins when both spellings occur in the same layer.
        applyRicochet("Rebound");
        applyRicochet("Ricochet");

        ApplyFloat(document, "Geometry", "EntrySeparation", runtime.thickness.entrySeparation, 0.01F, 5.0F);
        ApplyFloat(document, "Geometry", "DuplicateSeparation", runtime.thickness.duplicateSeparation, 0.001F, 2.0F);
        ApplyFloat(document, "Geometry", "OutwardAlignment", runtime.thickness.outwardAlignment, 0.0F, 1.0F);
        ApplyFloat(document, "Geometry", "ReverseAgreement", runtime.thickness.reverseAgreement, 0.01F, 5.0F);

        auto applyShooter = [&](std::string_view section, Core::ShooterControlSettings& control) {
            ApplyBool(document, section, "Penetration", control.enablePenetration);
            ApplyBool(document, section, "Rebound", control.enableRicochet);
            ApplyBool(document, section, "Ricochet", control.enableRicochet);
            ApplyCount(document, section, "PenetrationLimit", control.maxPenetrations, 100);
            ApplyCount(document, section, "ReboundLimit", control.maxRicochets, 100);
            ApplyCount(document, section, "RicochetLimit", control.maxRicochets, 100);
        };
        applyShooter("Player", runtime.impactPolicy.player);
        applyShooter("NPC", runtime.impactPolicy.npc);
        applyShooter("UnknownShooter", runtime.impactPolicy.unknown);

        if (const auto* profiles = document.FindSection("ProjectileProfiles")) {
            for (const auto& [name, value] : *profiles) {
                settings.projectileProfiles[Lower(name)] = ParseProjectile(value, settings.projectileProfiles[Lower(name)]);
            }
        }
        if (const auto* surfaces = document.FindSection("SurfaceFamilies")) {
            for (const auto& [name, value] : *surfaces) {
                settings.surfaceFamilies[Lower(name)] = ParseSurface(value, settings.surfaceFamilies[Lower(name)]);
            }
        }
        if (const auto* assignments = document.FindSection("SurfaceRecordAssignments")) {
            for (const auto& [key, value] : *assignments) {
                settings.surfaceRecordAssignments[key] = Lower(Trim(value));
            }
        }
        if (const auto* assignments = document.FindSection("SurfaceAssignments")) {
            for (const auto& [name, value] : *assignments) {
                settings.surfaceAssignments[name] = Lower(Trim(value));
            }
        }
        if (const auto* aliases = document.FindSection("SurfaceAliases")) {
            for (const auto& [name, value] : *aliases) {
                settings.surfaceAliases[name] = Trim(value);
            }
        }
        if (const auto* patterns = document.FindSection("SurfacePatterns")) {
            for (const auto& [pattern, value] : *patterns) {
                const std::string key = Lower(Trim(pattern));
                if (!key.empty()) {
                    settings.surfacePatterns[key] = Lower(Trim(value));
                }
            }
        }
    }

    std::optional<SurfacePatternMatch> MatchSurfacePattern(
        const DataSettings& settings,
        std::string_view identifier) noexcept
    {
        const std::string* bestFamily = nullptr;
        std::string_view bestPattern;
        for (const auto& [pattern, family] : settings.surfacePatterns) {
            if (!settings.surfaceFamilies.contains(family) || !FoldedContains(identifier, pattern)) {
                continue;
            }
            if (pattern.size() > bestPattern.size() ||
                (pattern.size() == bestPattern.size() && pattern < bestPattern)) {
                bestPattern = pattern;
                bestFamily = &family;
            }
        }
        return bestFamily ?
            std::optional<SurfacePatternMatch>(SurfacePatternMatch{ bestPattern, *bestFamily }) :
            std::nullopt;
    }
}
