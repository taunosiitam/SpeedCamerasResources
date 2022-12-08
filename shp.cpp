#include "shp.h"

#include <cstring>

/// Mandatory default constructor
XY::XY() = default;

/// Init original point coordinate and calculate integer counterpart
XY::XY(double x, double y) : x(x), y(y) {
    ix = (int) ((x - FN) * 20.0);
    iy = (int) ((y - FE) * 20.0);
}

/// Init SHP object with given filenames
SHP::SHP(
        std::string pointsFilename,  ///< base name for points SHP/SHX/DBF files
        std::string polygonsFilename  ///< base name for polygons SHP/SHX/DBF files
) : pointsFilename(std::move(pointsFilename)), polygonsFilename(std::move(polygonsFilename)) {}

/// Read data from point and polygon SHP files
void SHP::read() {
    read(pointsFilename);
    read(polygonsFilename);
}

/// Read point or polygon data from a set of SHP/SHX/DBF files
void SHP::read(const std::string &filename) {
    shp.open(filename + ".shp", std::ifstream::binary);
    if (!shp.is_open()) throw std::runtime_error(filename + " SHP not found");
    shx.open(filename + ".shx", std::ifstream::binary);
    if (!shx.is_open()) throw std::runtime_error(filename + " SHX not found");
    dbf.open(filename + ".dbf", std::ifstream::binary);
    if (!dbf.is_open()) throw std::runtime_error(filename + " DBF not found");

    unsigned char *buf;

    // SHX header
    buf = new unsigned char[100];
    shx.read((char *) buf, 100);
    recordCount = ((buf[24] << 24u) + (buf[25] << 16u) + (buf[26] << 8u) + buf[27] - 50) / 4;
    shapeType = buf[32];
    delete[] buf;

    if (shapeType != POINT && shapeType != POLYGON) {
        throw std::runtime_error("Unsupported shape type: " + std::to_string(shapeType));
    }

    // SHP header
    buf = new unsigned char[100];
    shp.read((char *) buf, 100);
    unsigned shpDataLength = ((buf[24] << 24u) + (buf[25] << 16u) + (buf[26] << 8u) + buf[27]) * 2 - 100;
    delete[] buf;

    // DBF header
    buf = new unsigned char[32];
    dbf.read((char *) buf, 32);
    unsigned dbfRecordCount = (buf[7] << 24u) + (buf[6] << 16u) + (buf[5] << 8u) + buf[4];
    unsigned dbfFieldsLength = (buf[9] << 8u) + buf[8] - 32;
    dbfRecordLength = (buf[11] << 8u) + buf[10];
    delete[] buf;

    if (recordCount != dbfRecordCount) {
        throw std::runtime_error("SHP/DBF record count mismatch: " + std::to_string(recordCount) + " != " + std::to_string(dbfRecordCount));
    }

    // SHP index
    buf = new unsigned char[recordCount * 8];
    shx.read((char *) buf, recordCount * 8);
    offsets = new unsigned[recordCount];
    for (unsigned i = 0, j = 0; i < recordCount; i++, j += 8) {
        offsets[i] = ((buf[j] << 24u) + (buf[j + 1] << 16u) + (buf[j + 2] << 8u) + buf[j + 3]) * 2 - 100 + 8;
    }
    delete[] buf;

    // SHP records
    shpData = new unsigned char[shpDataLength];
    shp.read((char *) shpData, shpDataLength);

    // DBF fields
    buf = new unsigned char[dbfFieldsLength];
    dbf.read((char *) buf, dbfFieldsLength);
    dbfFieldCount = dbfFieldsLength / 32;
    dbfField = new DbfField[dbfFieldCount];
    for (unsigned i = 0, offset = 1, length; i < dbfFieldCount; i++, offset += length) {
        memcpy(dbfField[i].name, buf + i * 32, 11);
        dbfField[i].name[11] = 0;
        dbfField[i].offset = offset;
        length = buf[i * 32 + 16];
        dbfField[i].length = length;
    }
    delete[] buf;

    // DBF records
    dbfData = new unsigned char[recordCount * dbfRecordLength];
    dbf.read((char *) dbfData, recordCount * dbfRecordLength);

    if (shapeType == POINT) addPoints();
    else addPolygons();

    shx.close();
    shp.close();
    dbf.close();
    delete[] offsets;
    delete[] shpData;
    delete[] dbfField;
    delete[] dbfData;
}

/// Add point data to points list
void SHP::addPoints() {
    for (unsigned i = 0; i < recordCount; i++) {
        auto p = shpData + offsets[i];
        Point point = parsePointName(i);
        if (!point.name.empty()) {
            point.xy = XY(*(double *) (p + 12), *(double *) (p + 4));
            points.push_back(point);
        }
    }
}

/// Add polygon data to polygons list
void SHP::addPolygons() {
    for (unsigned i = 0; i < recordCount; i++) {
        auto p = shpData + offsets[i];
        auto partCount = (unsigned *) (p + 36);
        auto pointCount = (unsigned *) (p + 40);
        auto partIndex = new unsigned[*partCount];
        memcpy(partIndex, p + 44, *partCount * 4);
        std::vector<std::vector<XY>> rings;
        for (unsigned j = 0; j < *partCount; j++) {
            std::vector<XY> ring;
            unsigned end = j == *partCount - 1 ? *pointCount : partIndex[j + 1];
            for (unsigned k = partIndex[j]; k < end; k++) {
                ring.emplace_back(*(double *) (p + k * 16 + *partCount * 4 + 52), *(double *) (p + k * 16 + *partCount * 4 + 44));
            }
            rings.push_back(ring);
        }
        delete[] partIndex;
        Polygon polygon = parsePolygonData(i);
        polygon.rings = rings;
        polygons.push_back(polygon);
    }
}

/// Parses point name from DBF data
/// \returns Point object with the name field set
Point SHP::parsePointName(
        unsigned n  ///<[in] record position
) {
    Point p;
    for (unsigned i = 0; i < dbfFieldCount; i++) {
        char *s0 = (char *) dbfData + n * dbfRecordLength + dbfField[i].offset;
        char *s1 = s0 + dbfField[i].length;
        char c = *s1;  // backup
        *s1 = 0;
        while (*s0 == ' ' && s0 < s1) s0++;  // trim leading spaces
        while (*(--s1) == ' ' && s0 < s1) *s1 = 0;  // trim trailing spaces
        if (strcmp("TextString", dbfField[i].name) == 0) p.name = s0;
        else if (strcmp("KIRJELDUS", dbfField[i].name) == 0 && (*s0 != 'M' || *(s0 + 1) != 'a')) p.name = "";  // include only "Maaüksuse nimi"
        *(s0 + dbfField[i].length) = c;  // restore
    }
    return p;
}

/// Parses polygon type and names from DBF data
/// \returns Point object with the type and names fields set
Polygon SHP::parsePolygonData(unsigned n) {
    Polygon p;
    for (unsigned i = 0; i < dbfFieldCount; i++) {
        char *s0 = (char *) dbfData + n * dbfRecordLength + dbfField[i].offset;
        char *s1 = s0 + dbfField[i].length;
        char c = *s1;  // backup
        *s1 = 0;
        while (*s0 == ' ' && s0 < s1) s0++;  // trim leading spaces
        while (*(--s1) == ' ' && s0 < s1) *s1 = 0;  // trim trailing spaces
        if (strcmp("TYYP", dbfField[i].name) == 0) p.type = strtoul(s0, &s1, 10);
        else if (strcmp("MNIMI", dbfField[i].name) == 0) p.names[0] = s0;
        else if (strcmp("ONIMI", dbfField[i].name) == 0) p.names[1] = s0;
        else if (strcmp("ANIMI", dbfField[i].name) == 0) p.names[2] = s0;
        *(s0 + dbfField[i].length) = c;  // restore
    }
    return p;
}
