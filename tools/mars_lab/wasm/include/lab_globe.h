/**
 * @file lab_globe.h
 * @brief Display-only orthographic globe projection and gesture arithmetic.
 *
 * Coordinates are degrees and screen positions are fractions of the globe radius. Results are scoped
 * browser arrays; geographic classification and imagery remain in the native shoreline module.
 * Great-circle camera interpolation and bounded display-clock advancement keep travel distance-aware,
 * retain destination visibility and limit screen motion independently of browser frame delays.
 */
#ifndef LAB_GLOBE_H
#define LAB_GLOBE_H

#include "lab_surface_math.h"

/**
 * @brief Construct a north-up quaternion centred on an observer.
 * @param latitude Observer latitude in degrees, from -90 to 90.
 * @param longitude Observer longitude in degrees.
 * @return Via the bridge, a normalised [x,y,z,w] quaternion, or null for invalid inputs.
 */
void lab_globe_centre(double latitude, double longitude);

/**
 * @brief Compose a screen-space gesture with the persistent globe quaternion.
 * @param orientation Current world-to-camera quaternion.
 * @param zoom Current positive magnification controlling gesture sensitivity.
 * @param dx Horizontal gesture divided by the unzoomed globe radius.
 * @param dy Downward gesture divided by the unzoomed globe radius.
 * @param roll Anticlockwise screen roll divided by the unzoomed globe radius, independent of zoom.
 * @return Via the bridge, a normalised quaternion, or null for invalid gestures.
 */
void lab_globe_rotate(quaternion_jsref orientation, double zoom, double dx, double dy, double roll);

/**
 * @brief Project a geographic point onto an orthographic globe.
 * @param latitude Point latitude in degrees.
 * @param longitude Point longitude in degrees.
 * @param orientation Current world-to-camera quaternion.
 * @return Via the browser bridge, [x,y,depth], with y increasing downwards and negative depth hidden.
 */
void lab_globe_project(double latitude, double longitude, quaternion_jsref orientation);

/**
 * @brief Return a constant-cost local affine reprojection of retained SVG geometry.
 * Frames contain [quaternion x,y,z,w, zoom, CSS size]. Returns SVG matrix coefficients
 * [a,b,c,d,e,f], or null for invalid frames or a centre rotated behind the globe.
 * This tangent-plane preview is for close views; worker projection supplies exact updates.
 */
void lab_globe_frame_transform(jsref previous, jsref current);

/** @brief Test whether a completed nearby-town query covers a centre within one degree, including the date line. */
bool lab_globe_nearby_covers(double latitude, double longitude, double cached_latitude, double cached_longitude);

/**
 * @brief Resolve a visible globe position to geographic coordinates.
 * @param x Horizontal displacement divided by globe radius.
 * @param y Downward displacement divided by globe radius.
 * @param orientation Current world-to-camera quaternion.
 * @return Via the bridge, [latitude,longitude], or null outside the globe.
 */
void lab_globe_pick(double x, double y, quaternion_jsref orientation);

/**
 * @brief Identify displayed graticule lines near a visible geographic point.
 * @param latitude Picked latitude in degrees.
 * @param longitude Picked longitude in degrees.
 * @param zoom Camera magnification, from 1 to 4096.
 * @param size Canvas width in CSS pixels.
 * @return Via the bridge, [nearest latitude,nearest longitude,flags], with bits 1 and 2 identifying nearby lines.
 */
void lab_globe_grid(double latitude, double longitude, double zoom, double size);

/**
 * @brief Extract the quaternion's centre and compute a conservative visible texture window.
 * @param orientation Current world-to-camera quaternion; never reconstructed from the returned centre.
 * @param zoom Current magnification, clamped to 1..4096.
 * @param factor Positive zoom multiplier.
 * @return Via the bridge, [latitude,longitude,zoom,west,south,east,north].
 */
void lab_globe_view(quaternion_jsref orientation, double zoom, double factor);

/**
 * @brief Plan a bounded two-by-two photographic tile patch around a camera centre.
 * Returns [first column, second column, first row, west, south, east, north].
 * Columns wrap at the date line; rows clamp at the poles. Each tile spans thirty
 * degrees and contains 1800 source pixels. Invalid coordinates return null.
 */
void lab_globe_photo_patch(double latitude, double longitude);

/**
 * @brief Validate a displayed camera without recentering it on an observer.
 * @param camera Saved unit quaternion and magnification in 1..4096.
 * @return Via the bridge, the unchanged five camera numbers, or null for invalid data.
 * @details Startup restores the last displayed view, including a deliberate pan away from the observer.
 */
void lab_globe_saved_camera(jsref camera);

/**
 * @brief Validate a saved camera against the geographic observer it accompanies.
 * @param camera Saved quaternion and zoom, or null for a legacy record without a camera.
 * @param latitude Observer latitude in degrees, within -90..90.
 * @param longitude Observer longitude in degrees, within -180..180.
 * @param zoom Fallback magnification in 1..4096 when the saved camera is invalid.
 * @details A valid camera which keeps the observer visible retains its pan, roll and zoom.
 * Otherwise the result faces the observer north-up, retaining a valid saved zoom. This repairs
 * mismatched legacy records without modifying their location, calendar or history stacks.
 * @return Via the bridge, five camera numbers, or null for an invalid observer or fallback zoom.
 */
void lab_globe_history_camera(jsref camera, double latitude, double longitude, double zoom);

/**
 * @brief Bound automatic arrival magnification to three CSS pixels per source-photo pixel at the equator.
 * @param zoom Requested magnification; manual gestures remain independent of this automatic framing limit.
 * @param viewport View width in CSS pixels.
 * @param pixels Width of the full-resolution equirectangular photograph.
 * @return Via the bridge, bounded zoom or null for invalid inputs.
 */
void lab_globe_arrival_zoom(double zoom, double viewport, double pixels);

/**
 * @brief Face the route, follow its shortest great circle, then restore the destination's roll.
 * @param start Five finite numbers: unit quaternion x,y,z,w and zoom in 1..4096.
 * @param target Saved camera in the same format, including roll.
 * @param progress Display-clock fraction clamped to 0..1; endpoints preserve the exact supplied camera.
 * @param redirect Continue from an interrupted camera without repeating the departure-heading phase.
 * @details The first eighth gently aligns the departure heading with the route. Travel occupies the
 * middle three quarters, parallel-transporting that heading without north-up correction. Zoom reaches
 * its minimum at the geographic midpoint and returns to the target zoom on arrival. A visible target
 * needs no zoom-out or departure roll; other routes fit the remaining half-route rather than forcing
 * a full overview. Reciprocal zoom interpolation keeps the approaching destination in view. Heading
 * alignment is also suppressed near viewport corners to prevent a visible target rotating out of view.
 * The final eighth restores the target roll without moving the centre. Quintic easing gives zero
 * velocity and acceleration at phase joins.
 * Redirected travel begins immediately from the supplied orientation, parallel-transports that
 * orientation without a new departure turn, and retains the separate arrival-roll phase.
 * History navigation supplies the current viewing scale as the destination zoom.
 * Antipodal centres use the quaternion module's deterministic shortest great circle.
 * @return Via the bridge, a five-number camera, or null for malformed input.
 */
void lab_globe_flight(jsref start, jsref target, double progress, bool redirect);

/**
 * @brief Advance the display clock at a distance-dependent, screen-motion-limited rate.
 * @param start Departure camera: unit quaternion x,y,z,w and zoom in 1..4096.
 * @param target Destination camera in the same format.
 * @param progress Current display-clock fraction in 0..1.
 * @param elapsed_ms Available animation time, non-negative and capped internally at 50 milliseconds.
 * @param viewport Positive viewport width in CSS pixels.
 * @param redirect Whether the interrupted route skips departure-heading alignment.
 * @details Heading and arrival turns take nominally 1.2 and 1.5 seconds. The travel phase takes two
 * seconds plus six seconds per radian, so longer arcs take longer without lengthening local turns.
 * Translation, rotation and magnification share
 * a 360 CSS-pixel-per-second motion budget, capped at six pixels per frame even under load.
 * A fixed-iteration search limits progress without retaining
 * state. Callers may withhold elapsed time while imagery loads; delayed frames never cause catch-up.
 * @return Via the bridge, the next progress fraction, or null for malformed input.
 */
void lab_globe_flight_advance(jsref start, jsref target, double progress, double elapsed_ms, double viewport, bool redirect);
#endif
