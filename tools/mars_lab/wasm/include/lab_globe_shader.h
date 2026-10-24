/**
 * @file lab_globe_shader.h
 * @brief Native shader sources for the globe's thin browser rendering backend.
 *
 * These application shaders combine natural imagery, vector coverage, rivers
 * and the optional graticule. WebGL compilation and resource lifetime remain
 * browser responsibilities; shader selection is a one-time native operation.
 */
#ifndef LAB_GLOBE_SHADER_H
#define LAB_GLOBE_SHADER_H

/** Return the vertex (0) or fragment (1) source, enabling derivatives only when the browser reports support. */
void lab_globe_shader(unsigned fragment, unsigned derivatives);

#endif
