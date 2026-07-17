#!/usr/bin/awk -f

function esc(value) {
    gsub(/</, "\&lt;", value)
    gsub(/>/, "\&gt;", value)
    return value
}

function text(x, y, value, size, anchor, rotate, weight, style,    transform) {
    transform = rotate == 0 ? "" : sprintf(" transform=\"rotate(%g %g %g)\"", rotate, x, y)
    printf "  <text x=\"%.2f\" y=\"%.2f\" font-size=\"%g\" text-anchor=\"%s\"%s font-weight=\"%s\" font-style=\"%s\" stroke=\"none\">%s</text>\n", \
        x, y, size, anchor, transform, weight, style, esc(value)
}

BEGIN {
    W = 1040
    H = 1500
    x0 = 125
    x1 = 745
    strip_x = x1 + 35
    x2 = 850
    y0 = 70
    y1 = 1430
    dx = (x1 - x0) / 20
    dy = (y1 - y0) / 60

    bands[1] = "X"; bands[2] = "W"; bands[3] = "V"; bands[4] = "U"
    bands[5] = "T"; bands[6] = "S"; bands[7] = "R"; bands[8] = "Q"
    bands[9] = "P"; bands[10] = "N"; bands[11] = "M"; bands[12] = "L"
    bands[13] = "K"; bands[14] = "J"; bands[15] = "H"; bands[16] = "G"
    bands[17] = "F"; bands[18] = "E"; bands[19] = "D"; bands[20] = "C"

    degrees[0] = "84&#176;"; degrees[1] = "72&#176;"; degrees[2] = "64&#176;"
    degrees[3] = "56&#176;"; degrees[4] = "48&#176;"; degrees[5] = "40&#176;"
    degrees[6] = "32&#176;"; degrees[7] = "24&#176;"; degrees[8] = "16&#176;"
    degrees[9] = "8&#176;"; degrees[10] = "0&#176;"; degrees[11] = "8&#176;"
    degrees[12] = "16&#176;"; degrees[13] = "24&#176;"; degrees[14] = "32&#176;"
    degrees[15] = "40&#176;"; degrees[16] = "48&#176;"; degrees[17] = "56&#176;"
    degrees[18] = "64&#176;"; degrees[19] = "72&#176;"; degrees[20] = "80&#176;"

    print "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    printf "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%d\" height=\"%d\" viewBox=\"0 0 %d %d\" role=\"img\" aria-labelledby=\"title description\">\n", W, H, W, H
    print "  <title id=\"title\">UTM and UPS grid zone designations</title>"
    print "  <desc id=\"description\">Universal Transverse Mercator zones from 80 degrees south to 84 degrees north, with Universal Polar Stereographic zone insets.</desc>"
    printf "  <rect width=\"%d\" height=\"%d\" fill=\"#0D1117\"/>\n", W, H
    print "  <g fill=\"#E6EDF3\" stroke=\"#E6EDF3\" stroke-width=\"1.35\" stroke-linecap=\"square\" font-family=\"Georgia, 'Times New Roman', serif\">"

    # Main UTM grid and the adjoining numbered strip.
    printf "  <rect x=\"%.2f\" y=\"%.2f\" width=\"%.2f\" height=\"%.2f\" fill=\"none\" stroke-width=\"2\"/>\n", x0, y0, x1-x0, y1-y0
    printf "  <rect x=\"%.2f\" y=\"%.2f\" width=\"%.2f\" height=\"%.2f\" fill=\"none\" stroke-width=\"2\"/>\n", strip_x, y0, x2-strip_x, y1-y0

    for (i = 1; i < 20; i++) {
        x = x0 + i * dx
        printf "  <line x1=\"%.2f\" y1=\"%.2f\" x2=\"%.2f\" y2=\"%.2f\"/>\n", x, y0, x, y1
    }
    for (i = 1; i < 60; i++) {
        y = y0 + i * dy
        printf "  <line x1=\"%.2f\" y1=\"%.2f\" x2=\"%.2f\" y2=\"%.2f\"/>\n", x0, y, x1, y
        printf "  <line x1=\"%.2f\" y1=\"%.2f\" x2=\"%.2f\" y2=\"%.2f\"/>\n", strip_x, y, x2, y
    }

    # Latitude-band and boundary labels across the head of the rotated chart.
    for (i = 1; i <= 20; i++) {
        x = x0 + (i - 0.5) * dx
        text(x, 42, bands[i], 15, "middle", -90, "bold", "normal")
    }
    for (i = 0; i <= 20; i++) {
        x = x0 + i * dx
        text(x, y0 - 3, degrees[i], 12, "start", -90, "normal", "normal")
    }

    # UTM zone numbers and six-degree longitude boundaries.
    for (i = 0; i < 60; i++) {
        y = y0 + (i + 0.5) * dy
        zone = 60 - i
        text((strip_x + x2) / 2, y + 4, zone, 12, "middle", -90, "normal", "normal")

    }
    for (i = 0; i <= 60; i++) {
        y = y0 + i * dy
        longitude = 180 - i * 6
        boundary = (longitude < 0 ? -longitude : longitude) "&#176;"
        label_x = x1 + 3
        label_y = y
        printf "  <text x=\"%.2f\" y=\"%.2f\" font-size=\"14\" text-anchor=\"middle\" transform=\"rotate(90 %.2f %.2f)\" font-family=\"'Times New Roman', Times, serif\" font-weight=\"normal\" font-style=\"normal\" stroke=\"none\">%s</text>\n", \
            label_x, label_y, label_x, label_y, esc(boundary)
    }

    # Exact stepped boundaries around the Norway/Svalbard exceptions. The first
    # band has the 33-, 21-, 9-, and 0-degree divisions; the adjacent 3-degree
    # cell returns the chart to the regular six-degree zone grid.
    sy = y0 + 23 * dy
    ey = y0 + 30 * dy
    printf "  <rect x=\"%.2f\" y=\"%.2f\" width=\"%.2f\" height=\"%.2f\" fill=\"#0D1117\" stroke=\"none\"/>\n", x0-2, sy-2, 4*dx+4, ey-sy+4

    for (j = 0; j <= 4; j++) {
        x = x0 + j * dx
        printf "  <line x1=\"%.2f\" y1=\"%.2f\" x2=\"%.2f\" y2=\"%.2f\"/>\n", x, sy, x, ey
    }

    # Regular rows to the right of the widened first band.
    for (j = 23; j <= 28; j++) {
        y = y0 + j * dy
        printf "  <line x1=\"%.2f\" y1=\"%.2f\" x2=\"%.2f\" y2=\"%.2f\"/>\n", x0+dx, y, x0+4*dx, y
    }
    printf "  <line x1=\"%.2f\" y1=\"%.2f\" x2=\"%.2f\" y2=\"%.2f\"/>\n", x0+dx, y0+29*dy, x0+2*dx, y0+29*dy
    printf "  <line x1=\"%.2f\" y1=\"%.2f\" x2=\"%.2f\" y2=\"%.2f\"/>\n", x0+3*dx, y0+29*dy, x0+4*dx, y0+29*dy
    printf "  <line x1=\"%.2f\" y1=\"%.2f\" x2=\"%.2f\" y2=\"%.2f\"/>\n", x0+2*dx, y0+29.5*dy, x0+3*dx, y0+29.5*dy
    printf "  <line x1=\"%.2f\" y1=\"%.2f\" x2=\"%.2f\" y2=\"%.2f\"/>\n", x0+dx, ey, x0+4*dx, ey

    # Widened degree boundaries in the first band.
    special_y[1] = y0 + 24.5 * dy
    special_y[2] = y0 + 26.5 * dy
    special_y[3] = y0 + 28.5 * dy
    special_y[4] = ey
    special_label[1] = "33&#176;"
    special_label[2] = "21&#176;"
    special_label[3] = "9&#176;"
    special_label[4] = "0&#176;"
    for (j = 1; j <= 4; j++) {
        printf "  <line x1=\"%.2f\" y1=\"%.2f\" x2=\"%.2f\" y2=\"%.2f\"/>\n", x0, special_y[j], x0+dx, special_y[j]
        text(x0-8, special_y[j]+3, special_label[j], 11, "middle", -90, "normal", "normal")
    }
    text(x0+1.5*dx, y0+29.55*dy, "3&#176;", 11, "middle", -90, "normal", "normal")

    # UPS polar-zone insets. SVG circles guarantee undistorted geometry.
    print "  <g fill=\"none\" stroke-width=\"1.8\">"
    print "    <circle cx=\"57\" cy=\"750\" r=\"46\"/>"
    print "    <line x1=\"11\" y1=\"750\" x2=\"103\" y2=\"750\"/>"
    print "    <line x1=\"57\" y1=\"704\" x2=\"57\" y2=\"796\"/>"
    print "    <circle cx=\"955\" cy=\"750\" r=\"72\"/>"
    print "    <line x1=\"883\" y1=\"750\" x2=\"1027\" y2=\"750\"/>"
    print "    <line x1=\"955\" y1=\"678\" x2=\"955\" y2=\"822\"/>"
    print "  </g>"
    text(57, 738, "Z", 15, "middle", 0, "bold", "normal")
    text(57, 775, "Y", 15, "middle", 0, "bold", "normal")
    text(955, 738, "B", 15, "middle", 0, "bold", "normal")
    text(955, 775, "A", 15, "middle", 0, "bold", "normal")
    text(57, 687, "90&#176; E", 11, "middle", -90, "normal", "normal")
    text(57, 813, "90&#176; W", 11, "middle", -90, "normal", "normal")
    text(7, 750, "180&#176;", 11, "middle", -90, "normal", "normal")
    text(119, 750, "0&#176;", 11, "middle", -90, "normal", "normal")
    text(955, 663, "90&#176; E", 11, "middle", -90, "normal", "normal")
    text(955, 837, "90&#176; W", 11, "middle", -90, "normal", "normal")
    text(877, 750, "0&#176;", 11, "middle", -90, "normal", "normal")
    text(1033, 750, "180&#176;", 11, "middle", -90, "normal", "normal")

    # The source's highlighted square and directional annotations.
    hx = x0 + 10.5 * dx
    hy = y0 + 57.5 * dy
    printf "  <rect x=\"%.2f\" y=\"%.2f\" width=\"%.2f\" height=\"%.2f\" fill=\"#E6EDF3\" stroke=\"#E6EDF3\"/>\n", hx-dx/2, hy-dy/2, dx, dy
    printf "  <line x1=\"%.2f\" y1=\"%.2f\" x2=\"%.2f\" y2=\"%.2f\" stroke-width=\"2\"/>\n", hx-55, hy-105, hx-8, hy-22
    printf "  <polygon points=\"%.2f,%.2f %.2f,%.2f %.2f,%.2f\" stroke=\"none\"/>\n", hx-8,hy-22,hx-20,hy-28,hx-13,hy-39
    text(hx-58, hy-112, "13N", 12, "middle", -90, "normal", "normal")
    printf "  <line x1=\"%.2f\" y1=\"%.2f\" x2=\"%.2f\" y2=\"%.2f\" stroke-width=\"2\"/>\n", hx+145, hy+4, hx+95, hy+4
    printf "  <polygon points=\"%.2f,%.2f %.2f,%.2f %.2f,%.2f\" stroke=\"none\"/>\n", hx+95,hy+4,hx+108,hy-3,hx+108,hy+11

    print "  </g>"
    print "</svg>"
}
