#include "runtime/RuntimeConfig.h"

#include "config/IniDocument.h"
#include "core/ProjectileClassifier.h"
#include "pch.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <filesystem>
#include <set>
#include <unordered_map>

namespace BPR::Runtime
{
    namespace
    {
        constexpr std::size_t kMaximumAliasDepth = 8;
        constexpr std::size_t kMaximumParentDepth = 12;

        struct CachedSurface
        {
            ResolvedSurface value;
        };

        std::atomic<Configuration> g_configuration;

        bool ParseFormKey(std::string_view key, std::string& plugin, std::uint32_t& formID)
        {
            const std::size_t delimiter = key.find('|');
            if (delimiter == std::string_view::npos) {
                return false;
            }
            plugin = Config::Trim(key.substr(0, delimiter));
            std::string value = Config::Trim(key.substr(delimiter + 1));
            if (value.starts_with("0x") || value.starts_with("0X")) {
                value.erase(0, 2);
            }
            const auto result = std::from_chars(value.data(), value.data() + value.size(), formID, 16);
            return !plugin.empty() && !value.empty() && result.ec == std::errc{} &&
                result.ptr == value.data() + value.size();
        }

        void ApplyMCM(Config::DataSettings& settings, const Config::IniDocument& document)
        {
            auto boolValue = [&](std::string_view section, std::string_view key, bool& output) {
                bool parsed = false;
                if (const auto text = document.Find(section, key); text && Config::ParseBool(*text, parsed)) {
                    output = parsed;
                }
            };
            auto floatValue = [&](std::string_view section, std::string_view key, float& output, float minimum, float maximum) {
                float parsed = 0.0F;
                if (const auto text = document.Find(section, key); text && Config::ParseFloat(*text, parsed)) {
                    output = std::clamp(parsed, minimum, maximum);
                }
            };
            auto countValue = [&](std::string_view section, std::string_view key, std::uint32_t& output) {
                std::uint32_t parsed = 0;
                if (const auto text = document.Find(section, key); text && Config::ParseUnsigned(*text, parsed)) {
                    output = std::min(parsed, 100U);
                }
            };

            auto& value = settings.runtime;
            boolValue("Diagnostics", "bDetailedLogging", value.detailedLogging);
            boolValue("Visuals", "bSuppressProjectileTrails", value.suppressProjectileTrails);
            boolValue("Penetration", "bPreventRepeatActor", value.preventRepeatActor);
            floatValue("Penetration", "fDamageFalloffExponent", value.damageFalloffExponent, 2.0F, 5.0F);
            floatValue("Penetration", "fVariationDegrees", value.penetrationVariationDegrees, 0.0F, 12.0F);
            boolValue("Receiver", "bEnabled", value.receiver.enabled);
            floatValue("Receiver", "fInfluence", value.receiver.exponent, 0.0F, 4.0F);
            floatValue("Receiver", "fMinimum", value.receiver.minimum, 0.01F, 100.0F);
            floatValue("Receiver", "fMaximum", value.receiver.maximum, 0.01F, 100.0F);
            if (value.receiver.minimum > value.receiver.maximum) {
                std::swap(value.receiver.minimum, value.receiver.maximum);
            }
            boolValue("Rebound", "bEnabled", value.rebound.enabled);
            boolValue("Rebound", "bAllowProps", value.impactPolicy.allowPropRicochets);
            floatValue("Rebound", "fChancePercent", value.rebound.chancePercent, 0.0F, 100.0F);
            floatValue("Rebound", "fHeadOnExclusionDegrees", value.rebound.headOnExclusionDegrees, 0.0F, 90.0F);
            floatValue("Rebound", "fVariationDegrees", value.rebound.variationDegrees, 0.0F, 12.0F);
            auto shooter = [&](std::string_view section, Core::ShooterControlSettings& control) {
                boolValue(section, "bPenetration", control.enablePenetration);
                boolValue(section, "bRebound", control.enableRicochet);
                countValue(section, "iPenetrationLimit", control.maxPenetrations);
                countValue(section, "iReboundLimit", control.maxRicochets);
            };
            shooter("Player", value.impactPolicy.player);
            shooter("NPC", value.impactPolicy.npc);
        }

        ResolvedSurface ResolveSurfaceSlow(
            const Config::DataSettings& data,
            const RE::BGSMaterialType* material) noexcept
        {
            ResolvedSurface result;
            if (const auto fallback = data.surfaceFamilies.find(data.runtime.defaultSurfaceFamily);
                fallback != data.surfaceFamilies.end()) {
                result.profile = fallback->second;
                result.familyName = fallback->first;
            }

            std::array<std::string_view, kMaximumParentDepth * 2> identifiersSeen{};
            std::size_t identifierCount = 0;
            ResolvedSurface genericParent = result;
            for (std::size_t parentDepth = 0; material && parentDepth < kMaximumParentDepth;
                ++parentDepth, material = material->parentType) {
                const char* identifiers[]{ material->GetFormEditorID(), material->materialName.c_str() };
                for (const char* raw : identifiers) {
                    if (!raw || *raw == '\0') {
                        continue;
                    }
                    if (result.runtimeName.empty()) {
                        result.runtimeName = raw;
                    }
                    std::string_view identifier(raw);
                    if (identifierCount < identifiersSeen.size()) {
                        identifiersSeen[identifierCount++] = identifier;
                    }
                    for (std::size_t aliasDepth = 0; aliasDepth < kMaximumAliasDepth; ++aliasDepth) {
                        const auto assignment = data.surfaceAssignments.find(identifier);
                        const std::string_view familyName = assignment != data.surfaceAssignments.end() ?
                            std::string_view(assignment->second) : identifier;
                        if (const auto family = data.surfaceFamilies.find(familyName);
                            family != data.surfaceFamilies.end()) {
                            ResolvedSurface resolved{
                                family->second,
                                result.runtimeName,
                                family->first,
                                false,
                                false
                            };
                            const bool genericFamily = Config::FoldedEqual{}(
                                family->first, data.runtime.defaultSurfaceFamily);
                            if (parentDepth == 0 || !genericFamily) {
                                return resolved;
                            }
                            genericParent = resolved;
                            break;
                        }
                        const auto alias = data.surfaceAliases.find(identifier);
                        if (alias == data.surfaceAliases.end() ||
                            Config::FoldedEqual{}(identifier, alias->second)) {
                            break;
                        }
                        identifier = alias->second;
                    }
                }
            }

            std::optional<Config::SurfacePatternMatch> inferred;
            std::string_view inferredFrom;
            for (std::size_t index = 0; index < identifierCount; ++index) {
                const auto candidate = Config::MatchSurfacePattern(data, identifiersSeen[index]);
                if (candidate && (!inferred ||
                        candidate->pattern.size() > inferred->pattern.size() ||
                        (candidate->pattern.size() == inferred->pattern.size() &&
                            candidate->pattern < inferred->pattern))) {
                    inferred = candidate;
                    inferredFrom = identifiersSeen[index];
                }
            }
            if (inferred) {
                if (const auto family = data.surfaceFamilies.find(inferred->family);
                    family != data.surfaceFamilies.end()) {
                    return {
                        family->second,
                        result.runtimeName.empty() ? inferredFrom : result.runtimeName,
                        family->first,
                        false,
                        true
                    };
                }
            }
            return genericParent.fallback ? result : genericParent;
        }
    }

    struct ConfigurationSnapshot
    {
        Config::DataSettings data{ Config::BuiltInDefaults() };
        std::unordered_map<const RE::TESAmmo*, float> ammoDepth;
        std::unordered_map<const RE::TESAmmo*, Core::ProjectileProfile> ammoProfiles;
        std::unordered_map<const RE::BGSMaterialType*, CachedSurface> materialCache;
    };

    namespace
    {
        class SnapshotBuilder
        {
        public:
            explicit SnapshotBuilder(RE::TESDataHandler& dataHandler) :
                _dataHandler(dataHandler),
                _snapshot(std::make_shared<ConfigurationSnapshot>())
            {}

            std::shared_ptr<ConfigurationSnapshot> Build()
            {
                LoadDocuments();
                ResolveAmmoSections();
                ResolveSurfaceRecordSection();
                std::size_t inferredMaterials = 0;
                std::size_t fallbackMaterials = 0;
                for (RE::BGSMaterialType* material : _dataHandler.GetFormArray<RE::BGSMaterialType>()) {
                    if (material) {
                        if (_snapshot->materialCache.contains(material)) {
                            continue;
                        }
                        ResolvedSurface resolved = ResolveSurfaceSlow(_snapshot->data, material);
                        inferredMaterials += resolved.inferred ? 1U : 0U;
                        fallbackMaterials += resolved.fallback ? 1U : 0U;
                        _snapshot->materialCache.emplace(material, CachedSurface{ resolved });
                    }
                }
                _inferredMaterials = inferredMaterials;
                _fallbackMaterials = fallbackMaterials;
                for (const std::string& plugin : _missingPlugins) {
                    REX::INFO("BPR skipped optional configuration entries for missing plugin '{}'", plugin);
                }
                if (_invalidEntries != 0) {
                    REX::WARN("BPR ignored {} malformed or unresolved configuration entries", _invalidEntries);
                }
                return std::move(_snapshot);
            }

            [[nodiscard]] std::size_t InferredMaterials() const noexcept { return _inferredMaterials; }
            [[nodiscard]] std::size_t FallbackMaterials() const noexcept { return _fallbackMaterials; }
            [[nodiscard]] std::size_t RecordAssignedMaterials() const noexcept { return _recordAssignedMaterials; }

        private:
            void LoadDocuments()
            {
                Config::IniDocument main;
                std::string error;
                if (main.Load(R"(Data\F4SE\Plugins\BPR.ini)", &error)) {
                    Config::ApplyDocument(_snapshot->data, main);
                }

                const std::filesystem::path directory{ R"(Data\F4SE\Plugins\BPR)" };
                std::vector<std::filesystem::path> files;
                std::error_code filesystemError;
                if (std::filesystem::is_directory(directory, filesystemError)) {
                    for (std::filesystem::directory_iterator iterator(directory, filesystemError), end;
                        !filesystemError && iterator != end; iterator.increment(filesystemError)) {
                        if (iterator->is_regular_file(filesystemError) &&
                            Config::Lower(iterator->path().extension().string()) == ".ini") {
                            files.push_back(iterator->path());
                        }
                    }
                }
                std::sort(files.begin(), files.end(), [](const auto& left, const auto& right) {
                    return Config::Lower(left.filename().string()) < Config::Lower(right.filename().string());
                });
                for (const auto& file : files) {
                    Config::IniDocument layer;
                    if (layer.Load(file, &error)) {
                        Config::ApplyDocument(_snapshot->data, layer);
                        _layers.push_back(std::move(layer));
                    } else {
                        ++_invalidEntries;
                    }
                }

                Config::IniDocument mcm;
                if (mcm.Load(R"(Data\MCM\Settings\BPR.ini)", &error)) {
                    ApplyMCM(_snapshot->data, mcm);
                }
            }

            template <class Callback>
            void ForEachLayerEntry(std::string_view section, Callback&& callback)
            {
                for (const Config::IniDocument& document : _layers) {
                    if (const auto* values = document.FindSection(section)) {
                        for (const auto& [key, value] : *values) {
                            callback(key, value);
                        }
                    }
                }
            }

            RE::TESAmmo* ResolveAmmo(std::string_view key)
            {
                std::string plugin;
                std::uint32_t formID = 0;
                if (!ParseFormKey(key, plugin, formID)) {
                    ++_invalidEntries;
                    return nullptr;
                }
                if (!_dataHandler.LookupModByName(plugin)) {
                    _missingPlugins.insert(plugin);
                    return nullptr;
                }
                RE::TESAmmo* ammo = _dataHandler.LookupForm<RE::TESAmmo>(formID, plugin);
                if (!ammo) {
                    ++_invalidEntries;
                }
                return ammo;
            }

            void ResolveAmmoSections()
            {
                ForEachLayerEntry("AmmoDepth", [&](const std::string& key, const std::string& value) {
                    float depth = 0.0F;
                    RE::TESAmmo* ammo = ResolveAmmo(key);
                    if (ammo && Config::ParseFloat(value, depth)) {
                        _snapshot->ammoDepth[ammo] = std::clamp(depth, 0.0F, 10000.0F);
                    } else if (ammo) {
                        ++_invalidEntries;
                    }
                });
                ForEachLayerEntry("AmmoProfileAssignments", [&](const std::string& key, const std::string& value) {
                    RE::TESAmmo* ammo = ResolveAmmo(key);
                    const auto profile = _snapshot->data.projectileProfiles.find(Config::Lower(Config::Trim(value)));
                    if (ammo && profile != _snapshot->data.projectileProfiles.end()) {
                        _snapshot->ammoProfiles[ammo] = profile->second;
                    } else if (ammo) {
                        ++_invalidEntries;
                    }
                });
            }

            void ResolveSurfaceRecordSection()
            {
                for (const auto& [key, familyName] : _snapshot->data.surfaceRecordAssignments) {
                    std::string plugin;
                    std::uint32_t formID = 0;
                    if (!ParseFormKey(key, plugin, formID)) {
                        ++_invalidEntries;
                        continue;
                    }
                    if (!_dataHandler.LookupModByName(plugin)) {
                        _missingPlugins.insert(plugin);
                        continue;
                    }
                    RE::BGSMaterialType* material =
                        _dataHandler.LookupForm<RE::BGSMaterialType>(formID, plugin);
                    const auto family = _snapshot->data.surfaceFamilies.find(familyName);
                    if (!material || family == _snapshot->data.surfaceFamilies.end()) {
                        ++_invalidEntries;
                        continue;
                    }
                    const char* editorID = material->GetFormEditorID();
                    const char* runtimeName = editorID && *editorID != '\0' ?
                        editorID : material->materialName.c_str();
                    _snapshot->materialCache[material] = CachedSurface{ ResolvedSurface{
                        family->second,
                        runtimeName ? std::string_view(runtimeName) : std::string_view{},
                        family->first,
                        false,
                        false
                    } };
                }
                _recordAssignedMaterials = _snapshot->materialCache.size();
            }

            RE::TESDataHandler& _dataHandler;
            std::shared_ptr<ConfigurationSnapshot> _snapshot;
            std::vector<Config::IniDocument> _layers;
            std::set<std::string, std::less<>> _missingPlugins;
            std::size_t _invalidEntries{ 0 };
            std::size_t _inferredMaterials{ 0 };
            std::size_t _fallbackMaterials{ 0 };
            std::size_t _recordAssignedMaterials{ 0 };
        };
    }

    void ReloadConfiguration() noexcept
    {
        try {
            RE::TESDataHandler* handler = RE::TESDataHandler::GetSingleton();
            if (!handler) {
                return;
            }
            SnapshotBuilder builder(*handler);
            auto next = builder.Build();
            REX::INFO(
                "BPR configuration loaded: {} ammo depths, {} explicit ammo profiles, {} surface families, {} record assignments ({} loaded), {} identifier assignments, {} patterns, {} cached materials ({} inferred, {} fallback)",
                next->ammoDepth.size(), next->ammoProfiles.size(), next->data.surfaceFamilies.size(),
                next->data.surfaceRecordAssignments.size(), builder.RecordAssignedMaterials(),
                next->data.surfaceAssignments.size(), next->data.surfacePatterns.size(),
                next->materialCache.size(), builder.InferredMaterials(), builder.FallbackMaterials());
            REX::INFO("BPR projectile classification: exact ammo override first; runtime beam record fallback excludes ballistic Alt Trigger");
            const Config::RuntimeSettings& effective = next->data.runtime;
            REX::INFO(
                "BPR effective settings: diagnostics={} trailsSuppressed={} falloff={:.3f} penetrationVariation={:.2f} repeatActor={} receiver={}/{:.3f}[{:.3f},{:.3f}] rebound={} props={} chance={:.2f} ricochetAngle={:.2f} costs={:.2f}/{:.2f} repeat={:.3f} reboundVariation={:.2f}",
                effective.detailedLogging, effective.suppressProjectileTrails,
                effective.damageFalloffExponent, effective.penetrationVariationDegrees,
                effective.preventRepeatActor,
                effective.receiver.enabled, effective.receiver.exponent,
                effective.receiver.minimum, effective.receiver.maximum,
                effective.rebound.enabled, effective.impactPolicy.allowPropRicochets,
                effective.rebound.chancePercent, effective.rebound.headOnExclusionDegrees,
                effective.rebound.baseEnergyCost,
                effective.rebound.incidenceEnergyCost, effective.rebound.repeatPenalty,
                effective.rebound.variationDegrees);
            REX::INFO(
                "BPR effective shooter controls: player=P{} R{} limits={}/{} npc=P{} R{} limits={}/{} unknown=P{} R{} limits={}/{}",
                effective.impactPolicy.player.enablePenetration,
                effective.impactPolicy.player.enableRicochet,
                effective.impactPolicy.player.maxPenetrations,
                effective.impactPolicy.player.maxRicochets,
                effective.impactPolicy.npc.enablePenetration,
                effective.impactPolicy.npc.enableRicochet,
                effective.impactPolicy.npc.maxPenetrations,
                effective.impactPolicy.npc.maxRicochets,
                effective.impactPolicy.unknown.enablePenetration,
                effective.impactPolicy.unknown.enableRicochet,
                effective.impactPolicy.unknown.maxPenetrations,
                effective.impactPolicy.unknown.maxRicochets);
            g_configuration.store(std::move(next), std::memory_order_release);
        } catch (const std::exception& exception) {
            REX::ERROR("BPR configuration reload failed: {}", exception.what());
        } catch (...) {
            REX::ERROR("BPR configuration reload failed with an unknown error");
        }
    }

    Configuration AcquireConfiguration() noexcept
    {
        Configuration current = g_configuration.load(std::memory_order_acquire);
        if (current) {
            return current;
        }
        static const Configuration fallback = std::make_shared<ConfigurationSnapshot>();
        return fallback;
    }

    const Config::RuntimeSettings& GlobalSettings(const ConfigurationSnapshot& configuration) noexcept
    {
        return configuration.data.runtime;
    }

    Core::ProjectileProfile AmmoProfile(
        const ConfigurationSnapshot& configuration,
        const RE::TESAmmo* ammo,
        const RE::BGSProjectile* projectileBase) noexcept
    {
        if (const auto found = configuration.ammoProfiles.find(ammo);
            found != configuration.ammoProfiles.end()) {
            return found->second;
        }
        const RE::BGSProjectile* classifiedProjectile = projectileBase;
        if (!classifiedProjectile && ammo) {
            classifiedProjectile = ammo->data.projectile;
        }
        if (classifiedProjectile &&
            Core::UsesEnergyBeamProfile(classifiedProjectile->data.flags)) {
            if (const auto energy = configuration.data.projectileProfiles.find("energy-beam");
                energy != configuration.data.projectileProfiles.end()) {
                return energy->second;
            }
        }
        if (const auto fallback = configuration.data.projectileProfiles.find(
                configuration.data.runtime.defaultProjectileProfile);
            fallback != configuration.data.projectileProfiles.end()) {
            return fallback->second;
        }
        return {};
    }

    float AmmoDepth(const ConfigurationSnapshot& configuration, const RE::TESAmmo* ammo) noexcept
    {
        if (const auto found = configuration.ammoDepth.find(ammo); found != configuration.ammoDepth.end()) {
            return found->second;
        }
        return configuration.data.runtime.defaultAmmoDepth;
    }

    ResolvedSurface Surface(
        const ConfigurationSnapshot& configuration,
        const RE::BGSMaterialType* material) noexcept
    {
        if (const auto found = configuration.materialCache.find(material);
            found != configuration.materialCache.end()) {
            return found->second.value;
        }
        return ResolveSurfaceSlow(configuration.data, material);
    }
}
