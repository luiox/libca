#include "libca/csv/csv_document.hpp"

#include <string>
#include <utility>

#include <gtest/gtest.h>

using namespace ca::csv;

TEST(CsvDocumentTest, ConstHeaderAccessKeepsAbsentHeader) {
    CsvDocument document;
    const auto& view = document;

    EXPECT_TRUE(view.header().empty());
    EXPECT_FALSE(view.has_header());

    document.header();
    EXPECT_TRUE(view.header().empty());
    EXPECT_TRUE(view.has_header());

    document.set_header({"name", "value"});
    ASSERT_EQ(view.header().size(), 2u);
    EXPECT_EQ(view.header()[1], "value");

    document.clear_header();
    EXPECT_TRUE(view.header().empty());
    EXPECT_FALSE(view.has_header());
}

TEST(CsvDocumentTest, MoveAssignmentPreservesArenaBackedFields) {
    CsvDocument target;
    target.set_header({"replaced"});

    {
        CsvDocument source;
        source.set_header({"id"});
        CsvRow row;
        EXPECT_TRUE(row.empty());
        {
            const std::string value(256, 'x');
            row.push_back(source.intern_field(
                reinterpret_cast<const ca::u8*>(value.data()), value.size()));
        }
        row.push_back(source.intern_field(reinterpret_cast<const ca::u8*>(""), 0));
        source.add_row(std::move(row));
        target = std::move(source);
    }

    const auto& view = target;
    ASSERT_TRUE(view.has_header());
    ASSERT_EQ(view.header().size(), 1u);
    EXPECT_EQ(view.header()[0], "id");
    ASSERT_EQ(view.rows().size(), 1u);
    const auto& row = view.rows().front();
    ASSERT_EQ(row.size(), 2u);
    EXPECT_EQ(row.fields()[0].to_std_string(), std::string(256, 'x'));
    EXPECT_EQ(row[1], "");
}

TEST(CsvDocumentTest, ClearAllowsReuseWithoutOldRowsOrHeader) {
    CsvDocument document;
    document.set_header({"old"});
    document.add_row(CsvRow({document.intern_field(reinterpret_cast<const ca::u8*>("old"), 3)}));

    document.clear();

    const auto& view = document;
    EXPECT_FALSE(view.has_header());
    EXPECT_TRUE(view.header().empty());
    EXPECT_TRUE(view.rows().empty());

    document.add_row(CsvRow({document.intern_field(reinterpret_cast<const ca::u8*>("new"), 3)}));
    ASSERT_EQ(view.rows().size(), 1u);
    ASSERT_EQ(view.rows()[0].size(), 1u);
    EXPECT_EQ(view.rows()[0][0], "new");
}
