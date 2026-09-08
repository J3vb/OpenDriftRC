// Logs one record through the real BlackboxLogger and checks the CSV row:
// column count matches the header, the battery fields land at the end, and
// the row fits the download buffer with margin.
#include "BlackboxLogger.h"

#include <cstdio>
#include <cstring>
#include <string>

unsigned long fakeMillis = 0;
uint32_t fakeAdcMillivolts = 0;
int fakePinModeCalls = 0;

namespace
{
    int failures = 0;
    int checks = 0;

    #define CHECK(condition, ...) \
        do \
        { \
            checks++; \
            if(!(condition)) \
            { \
                failures++; \
                std::printf("FAIL %s:%d: ", __FILE__, __LINE__); \
                std::printf(__VA_ARGS__); \
                std::printf("\n"); \
            } \
        } while(0)

    int countColumns(const char* text)
    {
        int columns = 1;
        for(const char* c = text; *c && *c != '\n'; c++)
        {
            if(*c == ',') columns++;
        }
        return columns;
    }
}

int main()
{
    BlackboxLogger blackbox;

    CHECK(blackbox.begin(), "begin failed");
    CHECK(blackbox.isReady(), "not ready");

    // Large-magnitude values so the row length is a pessimistic estimate.
    blackbox.log(
        4294967295UL,
        -123.456f, -123.456f, -123.456f, -123.456f,
        -12.3456f, -12.3456f, -12.3456f, 12.3456f, 12.3456f,
        -123.456f, 1.0f,
        -1000, -1000, -1000, true,
        2200, 2200, 2200, 50, 2200, 2200,
        6.00f, 200.00f, 100, 1.000f, 2,
        20.000f, 500, -500,
        100, 100, 100,
        -123.456f, -123.456f, -123.456f, 1.000f, 1.000f, -1234.567f,
        -1000,
        -1234.567f, 1.000f, 1.000f, -12345.678f,
        3, 1.000f, 1.000f,
        true, true, true, true,
        100, -1.000f, 1.000f, 3.600f, 1.000f, 1.000f, 1.000f, -123.456f, -1234.567f,
        99, 1.000f, 100, 123.456f, 3.600f,
        8.201f, 8.150f, 8.180f, 4.3f, 1737
    );

    CHECK(blackbox.getRecordCount() == 1, "record count %zu", blackbox.getRecordCount());
    CHECK(blackbox.getSize() == 264, "record size %zu", blackbox.getSize());

    const int headerColumns = countColumns(blackbox.getCsvHeader());
    CHECK(headerColumns == 70, "header columns %d", headerColumns);

    char line[800];
    size_t length = blackbox.formatCsvRecord(0, line, sizeof(line));

    CHECK(length > 0 && length < sizeof(line) - 1, "row length %zu", length);
    CHECK(line[length - 1] == '\n', "row lacks newline");
    CHECK(countColumns(line) == headerColumns, "row columns %d vs header %d", countColumns(line), headerColumns);

    std::string row(line);
    const char* tail = ",8.201,8.150,8.180,4.3,1737\n";
    CHECK(row.size() >= std::strlen(tail) && row.compare(row.size() - std::strlen(tail), std::strlen(tail), tail) == 0, "battery columns missing from row tail: %s", row.c_str() + (row.size() > 80 ? row.size() - 80 : 0));

    std::string header(blackbox.getCsvHeader());
    const char* headerTail = ",battery_raw_v,battery_filtered_v,battery_resting_v,battery_comp_pct,throttle_out_us";
    CHECK(header.compare(header.size() - std::strlen(headerTail), std::strlen(headerTail), headerTail) == 0, "header tail wrong");

    std::printf("worst-case row length %zu of %zu bytes; %d columns\n", length, sizeof(line), headerColumns);
    std::printf("%d checks, %d failures\n", checks, failures);

    return failures == 0 ? 0 : 1;
}
