#pragma once

#include <utility>
#include <cmath>
#include "config/Settings.h"

// =============================================================================
// CoordinateTransform
//
// Converts between Voronoi space and image/scene space.
//
// Coordinate Systems:
//   Voronoi space : origin at image center, X→right, Y→down (same orientation
//                   as image space — see voronoiToImagePixel()/voronoiToScene()
//                   below: row = vy + height/2 - offset, i.e. increasing vy
//                   increases row, which is downward)
//   Image space   : origin at top-left,     col→right (X), row→down (Y)
//   Scene space   : same as image space but floating-point
//
// CRITICAL FIX applied here:
//   Original Python code used shape[0] (height) for X and shape[1] (width) for Y.
//   Correct mapping: shape[1]=width → X(col), shape[0]=height → Y(row)
// =============================================================================

class CoordinateTransform {
public:
    // Returns (row, col) in image pixel space
    static std::pair<int, int> voronoiToImagePixel(
        double vx, double vy,
        int imageHeight, int imageWidth)
    {
        int col = static_cast<int>(vx + imageWidth  / 2.0 - COORDINATE_OFFSET);
        int row = static_cast<int>(vy + imageHeight / 2.0 - COORDINATE_OFFSET);
        return {row, col};
    }

    // Returns (scene_x, scene_y) for Qt graphics scene
    static std::pair<double, double> voronoiToScene(
        double vx, double vy,
        int imageHeight, int imageWidth)
    {
        double sx = vx + imageWidth  / 2.0 - COORDINATE_OFFSET;
        double sy = vy + imageHeight / 2.0 - COORDINATE_OFFSET;
        return {sx, sy};
    }

    static bool isValidPixel(int row, int col, int imageHeight, int imageWidth)
    {
        return row >= 0 && row < imageHeight && col >= 0 && col < imageWidth;
    }

    static std::pair<int, int> clipToBounds(int row, int col, int imageHeight, int imageWidth)
    {
        row = std::max(0, std::min(row, imageHeight - 1));
        col = std::max(0, std::min(col, imageWidth  - 1));
        return {row, col};
    }
};
