#include "support/folia_test.hpp"
import folia.math.util.entities;

namespace {
using namespace boost::ut;
using namespace std::literals;
using folia::math::entities::EntityEntry;
using folia::math::entities::EntityModule;

struct EntityRow {
    std::u16string_view file;
    std::u16string_view name;
    std::u16string_view value;
};
struct EntityShard {
    std::u16string_view name;
    std::size_t tableSize;
};
struct EntityNumber {
    std::u16string_view input;
    std::u16string_view output;
    bool error;
};
struct EntityTranslation {
    std::size_t module;
    bool loadMissing;
    std::u16string_view input;
    std::u16string_view output;
    bool error;
    std::vector<std::u16string_view> loaded;
};
struct EntityOverrideCase {
    std::u16string_view name;
    std::u16string_view shard;
    std::u16string_view expected;
    std::u16string_view afterOverrideReload;
    std::u16string_view afterRemoveReload;
};
struct EntityRetryCase {
    std::u16string_view shard;
    std::u16string_view name;
    std::u16string_view trigger;
    std::u16string_view input;
    std::u16string_view expected;
    std::vector<std::u16string_view> files;
};
struct EntityNumericRetryCase {
    std::u16string_view trigger;
    std::u16string_view input;
    std::vector<std::u16string_view> files;
};
#include "math/fixtures/entities.inc"

bool hasLoaded(EntityModule const &module, std::u16string_view file) {
    return module.loadedFiles().contains(file);
}
} // namespace

suite entities_tables = [] {
    "entities base table matches every independent Source row"_test = [] {
        EntityModule module;
        std::size_t expectedRows = 0;
        for (auto const &row : entityRows) {
            if (!row.file.empty())
                continue;
            ++expectedRows;
            auto found = module.table().find(row.name);
            expect(found != module.table().end());
            if (found != module.table().end())
                expect(found->second == row.value);
        }
        expect(expectedRows == 406_u);
        expect(module.table().size() == expectedRows);
        expect(module.loadedFiles().empty());
    };

    "entities all 29 shards match every raw Source row and load once"_test = [] {
        expect(entityShards.size() == 29_u);
        expect(entityRows.size() == 2128_u);
        std::size_t totalShardRows = 0;
        for (auto const &shard : entityShards) {
            EntityModule module;
            expect(module.table().size() == 406_u);
            expect(module.installShard(shard.name));
            expect(module.loadedFiles().size() == 1_u);
            expect(hasLoaded(module, shard.name));

            std::size_t rows = 0;
            for (auto const &row : entityRows) {
                if (row.file != shard.name)
                    continue;
                ++rows;
                auto found = module.table().find(row.name);
                expect(found != module.table().end());
                if (found != module.table().end())
                    expect(found->second == row.value);
            }
            expect(rows > 0_u);
            totalShardRows += rows;
            expect(module.table().size() == shard.tableSize);
            expect(module.installShard(shard.name));
            expect(module.loadedFiles().size() == 1_u);
            expect(module.table().size() == shard.tableSize);
        }
        expect(totalShardRows == entityRows.size() - 406u);
        EntityModule module;
        std::map<std::u16string, std::u16string, std::less<>> expected;
        for (auto const &row : entityRows)
            expected.insert_or_assign(std::u16string(row.name), std::u16string(row.value));
        for (auto const &shard : entityShards)
            expect(module.installShard(shard.name));
        expect(module.table().size() == expected.size());
        expect(module.table() == expected);
        expect(module.loadedFiles().size() == 29_u);
        expect(!module.installShard(u"unknown"));
        expect(module.loadedFiles().size() == 29_u);
    };
};

suite entities_numeric = [] {
    "entities numeric preserves exact UTF16 values and Source errors"_test = [] {
        expect(entityNumbers.size() == 21_u);
        for (auto const &entry : entityNumbers) {
            if (entry.error) {
                expect(throws<std::range_error>(
                    [&] { (void)folia::math::entities::numeric(entry.input); }));
            } else {
                expect(folia::math::entities::numeric(entry.input) == entry.output);
            }
        }
        // Source treats an uppercase X in numeric() as an invalid automatic-radix
        // number and ultimately raises RangeError; preserve that exact boundary.
        expect(throws<std::range_error>([] { (void)folia::math::entities::numeric(u"X41"); }));
    };
};

suite entities_translation = [] {
    "entities completed translations retain exact output and cumulative loads"_test = [] {
        std::array<EntityModule, 3> modules;
        expect(entityTranslations.size() == 19_u);
        for (auto const &entry : entityTranslations) {
            auto &module = modules[entry.module];
            module.options().loadMissingEntities = entry.loadMissing;
            if (entry.error) {
                expect(throws<std::range_error>([&] { (void)module.translate(entry.input); }));
            } else {
                expect(module.translate(entry.input) == entry.output);
            }
            std::size_t expectedLoaded = 0;
            for (auto file : entry.loaded) {
                if (file.empty())
                    continue;
                ++expectedLoaded;
                expect(hasLoaded(module, file));
            }
            expect(module.loadedFiles().size() == expectedLoaded);
        }
        expect(modules[0].loadedFiles().size() == 4_u);
        expect(modules[1].loadedFiles().empty());
        expect(modules[2].loadedFiles().size() == 3_u);
        expect(modules[0].table().size() != modules[1].table().size());
    };

    "entities completed retry rescans the full input after loading a shard"_test = [] {
        for (auto const &entry : entityRetryCases) {
            EntityModule module;
            const std::array overrideEntry{
                EntityEntry{std::u16string(entry.name), std::u16string(u"changed")}};
            module.add(overrideEntry, u"test-overrides");
            expect(module.table().at(std::u16string(entry.name)) == u"changed");
            expect(entry.input.find(entry.trigger) != std::u16string_view::npos);
            expect(module.translate(entry.input) == entry.expected);
            expect(!entry.trigger.empty());
            expect(hasLoaded(module, entry.shard));
            expect(module.loadedFiles().size() == entry.files.size());
            for (auto file : entry.files)
                expect(hasLoaded(module, file));
        }
    };

    "entities numeric RangeError preserves shard loads before and after trigger"_test = [] {
        expect(entityNumericRetryCases.size() == 2_u);
        for (std::size_t index = 0; index < entityNumericRetryCases.size(); ++index) {
            auto const &entry = entityNumericRetryCases[index];
            auto numericPosition = entry.input.find(u"&#1114112;");
            auto triggerPosition = entry.input.find(entry.trigger);
            expect(numericPosition != std::u16string_view::npos);
            expect(triggerPosition != std::u16string_view::npos);
            if (index == 0)
                expect(numericPosition < triggerPosition);
            else
                expect(triggerPosition < numericPosition);
            EntityModule module;
            expect(throws<std::range_error>([&] { (void)module.translate(entry.input); }));
            expect(module.loadedFiles().size() == entry.files.size());
            for (auto file : entry.files)
                expect(hasLoaded(module, file));
        }
    };
};

suite entities_mutation = [] {
    "entities typed add remove empty values and explicit shard reinstall"_test = [] {
        EntityModule module;
        const std::array additions{EntityEntry{u"zzCustom", u"custom"},
                                   EntityEntry{u"zzEmpty", u""},
                                   EntityEntry{u"ApplyFunction", u"override"}};
        module.add(additions, u"custom-shard");
        expect(module.table().at(u"zzCustom") == u"custom");
        expect(module.table().contains(u"zzEmpty"));
        expect(module.table().at(u"zzEmpty").empty());
        expect(module.table().at(u"ApplyFunction") == u"override");
        expect(module.loadedFiles().contains(u"custom-shard"));
        expect(module.translate(u"&zzEmpty;|&zzCustom;") == u"&zzEmpty;|custom");
        module.remove(u"zzEmpty");
        module.remove(u"zzCustom");
        expect(!module.table().contains(u"zzEmpty"));
        expect(!module.table().contains(u"zzCustom"));
        module.remove(u"doesNotExist");
        expect(module.table().at(u"ApplyFunction") == u"override");
    };

    "entities overlapping shard entries restore Source values after remove and reinstall"_test =
        [] {
            expect(entityOverrideCases.size() == 3_u);
            for (auto const &entry : entityOverrideCases) {
                EntityModule module;
                auto base = module.table().find(entry.name);
                expect(base != module.table().end());
                if (base == module.table().end())
                    continue;
                expect(base->second == entry.expected);
                const std::array overrideEntry{
                    EntityEntry{std::u16string(entry.name), std::u16string(u"changed")}};
                module.add(overrideEntry, u"test-overrides");
                auto changed = module.table().find(entry.name);
                expect(changed != module.table().end());
                if (changed != module.table().end())
                    expect(changed->second == u"changed");
                expect(module.installShard(entry.shard));
                auto overridden = module.table().find(entry.name);
                expect(overridden != module.table().end());
                if (overridden != module.table().end())
                    expect(overridden->second == entry.afterOverrideReload);
                expect(module.loadedFiles().contains(u"test-overrides"));
                expect(module.loadedFiles().contains(entry.shard));
                module.remove(entry.name);
                expect(!module.table().contains(entry.name));
                expect(module.installShard(entry.shard));
                auto reinstalled = module.table().find(entry.name);
                expect(reinstalled != module.table().end());
                if (reinstalled != module.table().end())
                    expect(reinstalled->second == entry.afterRemoveReload);
            }
        };
};

suite entities_isolation = [] {
    "entities independent modules do not share tables or loaded shards"_test = [] {
        EntityModule first;
        EntityModule second;
        expect(first.installShard(u"z"));
        expect(first.loadedFiles().contains(u"z"));
        expect(second.loadedFiles().empty());
        expect(first.table().size() > second.table().size());
        first.add(std::array{EntityEntry{u"moduleOnly", u"one"}}, u"manual");
        expect(first.table().contains(u"moduleOnly"));
        expect(!second.table().contains(u"moduleOnly"));
        expect(!second.loadedFiles().contains(u"manual"));
    };
};
