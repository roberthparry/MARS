/**
 * @file lab_evaluation_weather.h
 * @brief Private weather-response completion policy for the MARS Lab browser.
 *
 * Publishes ordered browser-owned service plans for the DateTime weather request.
 * The host retains request ownership checks, asynchronous transport and rendering.
 * Native weather sections remain opaque and are merged only by the deferred
 * installation service. This freestanding main-thread interface retains neither
 * scoped handles nor request state and is not an installed MARS library API.
 */
#ifndef LAB_WASM_EVALUATION_WEATHER_H
#define LAB_WASM_EVALUATION_WEATHER_H

/**
 * @brief Publish weather installation and status services through lab_dom_return.
 * @param outcome Stale 0, rejected response 1, success 2 or caught exception 4.
 * @param overview Scoped original DateTime response, passed unchanged to the installation service.
 * @param data Scoped weather response, passed unchanged to the installation service.
 * @details Stale and invalid outcomes publish null. Failure and exception plans set
 * Weather unavailable without reading either response. Success installs the weather
 * before setting Ready; installation exceptions remain the host's responsibility.
 * Plans contain browser values, never retained handle numbers, and creation has no effects.
 */
void lab_evaluation_weather_response(unsigned outcome, int overview, int data);

#endif
