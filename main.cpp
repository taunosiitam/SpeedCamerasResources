// SHP files from:
// https://geoportaal.maaamet.ee/est/Ruumiandmed/Eesti-topograafia-andmekogu/Laadi-ETAK-andmed-alla-p609.html
// https://geoportaal.maaamet.ee/est/Ruumiandmed/Topokaardid-ja-aluskaardid/Eesti-pohikaart-1-10000/Laadi-pohikaart-alla-p612.html
// https://geoportaal.maaamet.ee/est/Ruumiandmed/Haldus-ja-asustusjaotus-p119.html

#include <boost/geometry/algorithms/correct.hpp>
#include <boost/geometry/algorithms/simplify.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/ring.hpp>
#include <boost/geometry/io/wkt/write.hpp>

#include "shp.h"

namespace bg = boost::geometry;
namespace bgi = boost::geometry::index;

using point_t = bg::model::d2::point_xy<double>;
using ring_t = bg::model::ring<point_t>;

std::set<std::string> pointNames;  ///< point unique names list
std::map<std::string, unsigned> pointIndex;  ///< point names index

std::set<std::string> polygonNames[3];  ///< polygon unique names list
std::map<std::string, unsigned> polygonIndex[3];  ///< polygon names index

/// Pack and export an alphabetically ordered set of points
void exportNames(
        std::ofstream &f,  ///< output file stream
        std::set<std::string> &names,  ///< set of names
        unsigned numberOfCountBytes  ///< either 1 for small sets, or 2 for slightly larger ones
) {
    unsigned namesCount = names.size();
    f.write((const char *) &namesCount, numberOfCountBytes);
    std::string prev;
    for (auto &name: names) {
        unsigned char commonLength = 0;
        while (name[commonLength] == prev[commonLength]) commonLength++;
        f.write((const char *) &commonLength, 1);
        unsigned char nameLength = name.length() - commonLength;
        f.write((const char *) &nameLength, 1);
        f.write(name.c_str() + commonLength, nameLength);
        prev = name;
    }
}

/// Export a list of points to a binary output file
void exportPoints(
        SHP &shp,  /// SHP object containing imported geometry data
        const char *filename  ///< output file stream
) {
    std::ofstream f(filename, std::ios::out | std::ios::binary);
    if (!f) return;

    // create names index
    for (auto &point: shp.points) pointNames.insert(point.name);
    unsigned i = 0;
    for (auto &name: pointNames) pointIndex[name] = i++;

    // export names
    exportNames(f, pointNames, 2);

    // export points
    unsigned pointsCount = shp.points.size();
    f.write((const char *) &pointsCount, 4);
    for (auto &point: shp.points) {
        f.write((const char *) &point.xy.ix, 3);
        f.write((const char *) &point.xy.iy, 3);
        f.write((const char *) &pointIndex[point.name], 2);
    }

    f.close();
}

/// Simplify and export a list of polygons to a binary output file
void exportPolygons(
        SHP &shp,  ///<[in] SHP object containing imported geometry data
        const char *filename,  ///<[in] output file stream
        int simplifyDistance  ///<[in] point distance in meters to other segments to be removed (skip simplify if negative)
) {
    std::ofstream f(filename, std::ios::out | std::ios::binary);
    if (!f) return;

    // create names index
    for (auto &polygon: shp.polygons) for (unsigned i = 0; i < 3; i++) polygonNames[i].insert(polygon.names[i]);
    for (unsigned i = 0; i < 3; i++) {
        unsigned j = 0;
        for (auto &name: polygonNames[i]) polygonIndex[i][name] = j++;
    }

    // export names
    for (unsigned i = 0; i < 3; i++) exportNames(f, polygonNames[i], i < 2 ? 1 : 2);

    // export polygons
    unsigned polygonsCount = shp.polygons.size();
    f.write((const char *) &polygonsCount, 2);  // 2 bytes for number of polygons
    for (auto &polygon: shp.polygons) {
        // create simplified boost rings
        std::vector<ring_t> rings;
        for (auto &polygonRing: polygon.rings) {
            ring_t ring;
            for (auto &xy: polygonRing) ring.emplace_back(xy.y, xy.x);  // swap boost cartesian axes

            // simplify
            bg::correct(ring);
            ring_t simplified;
            if (simplifyDistance < 0 || ring.size() < 20) rings.push_back(ring);
            else {
                bg::simplify(ring, simplified, simplifyDistance);
                rings.push_back(simplified);
            }
        }
        if (rings.empty()) continue;

        f.write((const char *) &polygon.type, 1);  // 1 byte for multi-polygon type
        for (unsigned i = 0; i < 3; i++) f.write((const char *) &polygonIndex[i][polygon.names[i]], i < 2 ? 1 : 2);  // name indexes

        unsigned ringsCount = rings.size();
        f.write((const char *) &ringsCount, 1);  // 1 byte for number of rings
        for (auto &ring: rings) {
            unsigned short ringSize = ring.size();
            f.write((const char *) &ringSize, 2);  // 2 bytes for number of points in a ring
            int lastX = 0, lastY = 0;
            int diffX, diffY;
            for (auto &xy: ring) {
                auto x = (int) ((xy.y() - FN) * 20), y = (int) ((xy.x() - FE) * 20);  // swap boost cartesian axes
                diffX = (int) (x - lastX);
                diffY = (int) (y - lastY);
                f.write((const char *) &diffX, 3);  // 3 bytes for x diff
                f.write((const char *) &diffY, 3);  // 3 bytes for y diff
                lastX = x;
                lastY = y;
            }
        }
    }

    f.close();
}

int main(int argc, char *argv[]) {
    int simplifyDistance;
    simplifyDistance = argc > 1 ? strtol(argv[1], nullptr, 10) : -1;

    std::cout << "Reading SHP ..." << std::endl;
    SHP shp("kohanimi", "asustusyksus");
    shp.read();

    std::cout << "Exporting points ..." << std::endl;
    exportPoints(shp, "points.dat");

    std::cout << "Exporting polygons ..." << std::endl;
    exportPolygons(shp, "polygons.dat", simplifyDistance);

    return 0;
}
