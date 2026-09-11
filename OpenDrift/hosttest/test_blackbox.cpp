// Logs one record through the real BlackboxLogger and checks the CSV row:
// column count matches the header, the battery fields land at the end, the
// widest possible row fits the download buffer, and a row that does not fit
// a smaller buffer still ends with a newline.
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

    // Every integer at INT32_MIN and every float at a five-digit magnitude
    // (far beyond any real rate, angle, pulse or percentage in the log):
    // the widest plausible row. A wider one is cut by the newline guard.
    const int32_t I = -2147483647 - 1;
    const float B = -99999.5f;

    blackbox.log(
        4294967295UL,
        B, B, B, B,
        B, B, B, B, B,
        B, B,
        I, I, I, true,
        I, I, I, I, I, I,
        B, B, I, B, I,
        B, I, I,
        I, I, I,
        B, B, B, B, B, B,
        I,
        B, B, B, B,
        I, B, B,
        true, true, true, true,
        I, B, B, B, B, B, B, B, B,
        I, B, I, B, B,
        B, B, B, B, I
    );

    CHECK(blackbox.getRecordCount() == 1, "record count %zu", blackbox.getRecordCount());
    CHECK(blackbox.getSize() == 264, "record size %zu", blackbox.getSize());

    const int headerColumns = countColumns(blackbox.getCsvHeader());
    CHECK(headerColumns == 70, "header columns %d", headerColumns);

    char line[800];
    size_t length = blackbox.formatCsvRecord(0, line, sizeof(line));

    CHECK(length > 0 && length < sizeof(line) - 1, "widest row does not fit the 800-byte download buffer: %zu", length);
    CHECK(line[length - 1] == '\n' && line[length] == '\0', "row lacks newline");
    CHECK(countColumns(line) == headerColumns, "row columns %d vs header %d", countColumns(line), headerColumns);

    std::string row(line);
    const char* tail = ",-99999.500,-99999.500,-99999.500,-99999.5,-2147483648\n";
    CHECK(row.size() >= std::strlen(tail) && row.compare(row.size() - std::strlen(tail), std::strlen(tail), tail) == 0, "battery columns missing from row tail: %s", row.c_str() + (row.size() > 80 ? row.size() - 80 : 0));

    // A buffer too small for the row: truncated, but still one line.
    char small[100];
    size_t smallLength = blackbox.formatCsvRecord(0, small, sizeof(small));
    CHECK(smallLength == sizeof(small) - 1, "truncated length %zu", smallLength);
    CHECK(small[smallLength - 1] == '\n' && small[smallLength] == '\0', "truncated row lost its newline");
    CHECK(std::strncmp(small, line, smallLength - 1) == 0, "truncated row differs from the full row");

    std::string header(blackbox.getCsvHeader());
    const char* headerTail = ",battery_raw_v,battery_filtered_v,battery_resting_v,battery_comp_pct,throttle_out_us";
    CHECK(header.compare(header.size() - std::strlen(headerTail), std::strlen(headerTail), headerTail) == 0, "header tail wrong");

    std::printf("worst-case row length %zu of %zu bytes; %d columns\n", length, sizeof(line), headerColumns);
    std::printf("%d checks, %d failures\n", checks, failures);

    return failures == 0 ? 0 : 1;
}
