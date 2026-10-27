// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Independent Julian Date / Modified Julian Date executable clock resources.
 * Unix microseconds remain canonical; day/fraction presentation is computed
 * entirely here using checked-range exact integer partition, never a calendar
 * application ABI or a duplicated Common legacy clock formatter.
 */
#include <infiltratr/temporal_clock_resource.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define DAY_US UINT64_C(86400000000)
#define HALF_DAY_US UINT64_C(43200000000)

#define CLOCK_ID "modified-julian"
#define CLOCK_LABEL "MJD"
#define EPOCH_DAY INT64_C(40587)

/* Floor-division independent of the host's handling of negative remainders. */
static void split_unix_day(int64_t unix_us, int64_t *days, uint64_t *phase)
{
    int64_t quotient = unix_us / (int64_t)DAY_US;
    int64_t remainder = unix_us % (int64_t)DAY_US;
    if (remainder < 0) {
        --quotient;
        remainder += (int64_t)DAY_US;
    }
    *days = quotient;
    *phase = (uint64_t)remainder;
}

/*
 * JD rolls over at UTC noon, whereas MJD rolls over at UTC midnight.
 * Precision is 3 or 5 decimal day digits, truncated toward the earlier
 * boundary rather than rounded into a future instant.
 */
static uint64_t day_fraction_phase(uint64_t phase)
{
    return phase;
}

static bool format_clock(const InfiltratrTemporalClockProvider *provider,
                         const char *locale, int64_t unix_us,
                         int32_t utc_offset_seconds, bool show_seconds,
                         bool vertical, double latitude, double longitude,
                         char *buffer, size_t capacity, size_t *length)
{
    int64_t days;
    uint64_t phase;
    uint64_t fraction_phase;
    const uint64_t units = show_seconds ? UINT64_C(100000) : UINT64_C(1000);
    char text[96];
    int written;

    (void)provider; (void)locale; (void)utc_offset_seconds;
    (void)latitude; (void)longitude;
    if (buffer == NULL || capacity == 0U) return false;
    split_unix_day(unix_us, &days, &phase);
    fraction_phase = day_fraction_phase(phase);
    const int64_t whole_day = EPOCH_DAY + days;
    const uint64_t fractional_tick = fraction_phase * units / DAY_US;
    /* Format the signed scaled value, not a negative floor-day followed
     * by a positive fraction: floor-day -1 plus fraction .5 is -0.5.
     * The representable Unix-microsecond domain keeps this product well
     * within int64_t, including five fractional day digits. */
    const int64_t scaled_day = whole_day * (int64_t)units +
                               (int64_t)fractional_tick;
    const bool negative = scaled_day < 0;
    const uint64_t magnitude = negative ? (uint64_t)(-scaled_day)
                                        : (uint64_t)scaled_day;
    written = snprintf(text, sizeof(text), "%s%s%s%llu.%0*llu",
        CLOCK_LABEL, vertical ? "\n" : " ", negative ? "-" : "",
        (unsigned long long)(magnitude / units),
        show_seconds ? 5 : 3, (unsigned long long)(magnitude % units));
    if (written <= 0 || (size_t)written >= sizeof(text) ||
        (size_t)written >= capacity) return false;
    memcpy(buffer, text, (size_t)written + 1U);
    if (length != NULL) *length = (size_t)written;
    return true;
}

static bool next_clock_tick(const InfiltratrTemporalClockProvider *provider,
                            int64_t unix_us, int32_t utc_offset_seconds,
                            bool show_seconds, double latitude,
                            double longitude, uint32_t *milliseconds)
{
    int64_t days;
    uint64_t phase;
    uint64_t tick;
    uint64_t boundary;
    uint64_t remaining;
    const uint64_t units = show_seconds ? UINT64_C(100000) : UINT64_C(1000);
    (void)provider; (void)utc_offset_seconds;
    (void)latitude; (void)longitude;
    if (milliseconds == NULL) return false;
    split_unix_day(unix_us, &days, &phase);
    (void)days;
    phase = day_fraction_phase(phase);
    tick = phase * units / DAY_US;
    boundary = ((tick + UINT64_C(1)) * DAY_US + units - UINT64_C(1)) / units;
    remaining = boundary > phase ? boundary - phase : UINT64_C(1);
    const uint64_t rounded_ms =
        (remaining + UINT64_C(999)) / UINT64_C(1000);
    if (rounded_ms == 0U || rounded_ms > UINT32_MAX) return false;
    *milliseconds = (uint32_t)rounded_ms;
    return true;
}

static const InfiltratrTemporalResourceMetadata metadata = {
    sizeof(InfiltratrTemporalResourceMetadata),
    INFILTRATR_TEMPORAL_RESOURCE_METADATA_ABI,
    INFILTRATR_TEMPORAL_RESOURCE_CLOCK,
    CLOCK_ID, INFILTRATR_TEMPORAL_DISPLAY_NAME, true, false, false
};
static const InfiltratrTemporalClockProvider provider = {
    sizeof(InfiltratrTemporalClockProvider),
    INFILTRATR_TEMPORAL_CLOCK_PROVIDER_ABI,
    CLOCK_ID, format_clock, next_clock_tick
};

#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT __attribute__((visibility("default")))
#endif
EXPORT const InfiltratrTemporalResourceMetadata *
infiltratr_temporal_resource_metadata_v1(void);
EXPORT const InfiltratrTemporalClockProvider *
infiltratr_temporal_clock_provider_v1(void);
EXPORT const InfiltratrTemporalResourceMetadata *
infiltratr_temporal_resource_metadata_v1(void) { return &metadata; }
EXPORT const InfiltratrTemporalClockProvider *
infiltratr_temporal_clock_provider_v1(void) { return &provider; }
