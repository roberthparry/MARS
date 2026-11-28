/**
 * @file test_almanac_readme.h
 * @brief Private declarations for the almanac suite's documented observing programmes.
 *
 * The almanac test driver calls these examples after its ordinary regression checks, using the configured
 * catalogue loaded by its suite setup. Implementations own and release all local engines, dates and results.
 * This header is test-only; applications use almanac-t.h and the programmes in docs/almanac.md.
 */
#ifndef MARS_TEST_ALMANAC_README_H
#define MARS_TEST_ALMANAC_README_H

#if !defined(MARS_TEST_ALMANAC_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_TEST_ALMANAC_INTERNAL_ACCESS."
#endif

/**
 * @brief Run the London Sun, Moon, five-planet and named-star position README programme.
 * @return Nothing; reports a failed programme through the test harness and prints the documented table.
 * @note Takes no arguments. Releases every local entry, date and engine; requires configured ephemeris data.
 */
void example_almanac_sky_positions(void);

/**
 * @brief Run the README query for bright catalogue stars above London's geometric horizon.
 * @return Nothing; reports a failed programme through the test harness and prints the documented catalogue rows.
 * @note Takes no arguments. Releases the local snapshot, date and engine; entries remain snapshot-owned.
 */
void example_almanac_bright_stars(void);

/**
 * @brief Run the Sydney lunar position, refraction, phase, distance and angular-size README programme.
 * @return Nothing; reports a failed programme through the test harness and prints the documented Moon report.
 * @note Takes no arguments. Releases the local entry, date and engine; requires configured ephemeris data.
 */
void example_almanac_moon_details(void);

#endif
