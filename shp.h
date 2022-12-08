#pragma once

#include <fstream>
#include <string>
#include <vector>

#define POINT 1
#define POLYGON 5

#define FN 6375000
#define FE 500000

/// DBF file field information
struct DbfField {
    char name[12];  ///< field name
    unsigned offset;  ///< field byte offset in a data record
    unsigned length;  ///< field byte length in a data record
};

/// Point EPSG:3301 coordinate in original floating point and converted integer format.
struct XY {
    double x = 0;  ///< original northing floating point value
    double y = 0;  ///< original easting floating point value
    int ix = 0;  ///< northing integer value = 20 * (x - false northing)
    int iy = 0;  ///< easting integer value = 20 * (y - false easting)

    XY();
    XY(double x, double y);
};

/// Point coordinate and name
struct Point {
    XY xy;  ///< coordinate in EPSG:3301
    std::string name;  ///< name in Windows-1252 encoding
};

/// Polygon rings, type and 3 names (county, town/parish, settlement)
struct Polygon {
    std::vector<std::vector<XY>> rings;  ///< polygon rings
    unsigned char type = 0;  ///< 0 - county, 1 - rural municipality, 3 - town, 4 - city, 5 - city without municipal status, 6 - city district, 7 - small town, 8 - village
    std::string names[3];  ///< names for county, town/parish, and settlement
};

class SHP {
public:
    SHP(std::string pointsFilename, std::string polygonsFilename);

    void read();

    std::vector<Point> points;  ///< list of points
    std::vector<Polygon> polygons;  ///< list of polygons

private:
    void read(const std::string &filename);

    void addPoints();

    void addPolygons();

    Point parsePointName(unsigned n);

    Polygon parsePolygonData(unsigned n);

    std::string pointsFilename;
    std::string polygonsFilename;
    unsigned recordCount = 0;
    unsigned shapeType = 0;
    unsigned *offsets = nullptr;
    unsigned dbfFieldCount = 0;
    DbfField *dbfField = nullptr;
    std::ifstream shx, shp, dbf;
    unsigned char *shpData{};
    unsigned dbfRecordLength{};
    unsigned char *dbfData{};
};
