/**
 * @file lab_globe.h
 * @brief Display-only orthographic globe projection and gesture arithmetic.
 *
 * Coordinates are degrees and screen positions are fractions of the globe radius. Results are scoped
 * browser arrays; geographic classification and imagery remain in the native shoreline module.
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
 * @brief Interpolate a three-stage history flight: zoom out, shortest-path rotation, then restore zoom.
 * @param start Five finite numbers: unit quaternion x,y,z,w and zoom in 1..4096.
 * @param target Saved camera in the same format, including roll.
 * @param progress Elapsed fraction clamped to 0..1; endpoints preserve the exact supplied camera.
 * @return Via the bridge, a five-number camera, or null for malformed input.
 */
void lab_globe_flight(jsref start, jsref target, double progress);
#endif
