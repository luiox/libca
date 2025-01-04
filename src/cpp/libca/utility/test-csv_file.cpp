#include <doctest/doctest.h>
#include <libca/utility/csv_file.hpp>

using namespace libca::utility;
using namespace std;

void write_test()
{
    CsvFile csv;

    vector<string> title   = {"id", "name", "year"};
    vector<string> record1 = {"001", "test name1", "2024"};
    vector<string> record2 = {"002", "test name2", "2025"};
    vector<string> record3 = {"003", "test name3", "2026"};
    csv.addTitle(title);
    csv.addRecord(record1);
    csv.addRecord(record2);
    csv.addRecord(record3);

    CHECK_FALSE(!csv.wrtie("test.csv"));
}

void read_test()
{
    CsvFile csv("test.csv", true);
    csv.load();
    CHECK_EQ(csv.getRecord(0)[0], "001");
    CHECK_EQ(csv.getRecord(1)[1], "test name2");
}

TEST_CASE("libca::utility::CsvFile")
{
    write_test();
    read_test();
}
