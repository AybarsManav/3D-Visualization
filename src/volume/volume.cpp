#include "volume.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cctype> // isspace
#include <chrono>
#include <filesystem>
#include <fstream>
#include <glm/glm.hpp>
#include <gsl/span>
#include <iostream>
#include <string>

struct Header {
    glm::ivec3 dim;
    size_t elementSize;
};
static Header readHeader(std::ifstream& ifs, const volume::FileExtension& fileExtension);
static Header readVolumeHeader_fld(std::ifstream& ifs);
static Header readVolumeHeader_dat(std::ifstream& ifs);

static float computeMinimum(gsl::span<const float> data);
static float computeMaximum(gsl::span<const float> data);
static std::vector<int> computeHistogram(gsl::span<const float> data);

namespace volume {

Volume::Volume(const std::filesystem::path& file)
    : m_fileName(file.string())
{
    using clock = std::chrono::high_resolution_clock;
    auto start = clock::now();
    loadFile(file);
    auto end = clock::now();
    std::cout << "Time to load: " << std::chrono::duration<double, std::milli>(end - start).count() << "ms" << std::endl;

    if (m_data.size() > 0) {
        m_minimum = computeMinimum(m_data);
        m_maximum = computeMaximum(m_data);
        m_histogram = computeHistogram(m_data);
    }
}

Volume::Volume(std::vector<float> data, const glm::ivec3& dim)
    : m_fileName()
    , m_elementSize(2)
    , m_dim(dim)
    , m_data(std::move(data))
    , m_minimum(computeMinimum(m_data))
    , m_maximum(computeMaximum(m_data))
    , m_histogram(computeHistogram(m_data))
{
}

float Volume::minimum() const
{
    return m_minimum;
}

float Volume::maximum() const
{
    return m_maximum;
}

std::vector<int> Volume::histogram() const
{
    return m_histogram;
}

glm::ivec3 Volume::dims() const
{
    return m_dim;
}

std::string_view Volume::fileName() const
{
    return m_fileName;
}

float Volume::getVoxel(int x, int y, int z) const
{
    const size_t i = size_t(x + m_dim.x * (y + m_dim.y * z));
    return static_cast<float>(m_data[i]);
}

// This function returns a value based on the current interpolation mode
float Volume::getSampleInterpolate(const glm::vec3& coord) const
{
    switch (interpolationMode) {
    case InterpolationMode::NearestNeighbour: {
        return getSampleNearestNeighbourInterpolation(coord);
    }
    case InterpolationMode::Linear: {
        return getSampleTriLinearInterpolation(coord);
    }
    case InterpolationMode::Cubic: {
        return getSampleTriCubicInterpolation(coord);
    }
    default: {
        throw std::exception();
    }
    }
}

// This function returns the nearest neighbour value at the continuous 3D position given by coord.
// Notice that in this framework we assume that the distance between neighbouring voxels is 1 in all directions
float Volume::getSampleNearestNeighbourInterpolation(const glm::vec3& coord) const
{
    // check if the coordinate is within volume boundaries, since we only look at direct neighbours we only need to check within 0.5
    if (glm::any(glm::lessThan(coord + 0.5f, glm::vec3(0))) || glm::any(glm::greaterThanEqual(coord + 0.5f, glm::vec3(m_dim))))
        return 0.0f;

    // nearest neighbour simply rounds to the closest voxel positions
    auto roundToPositiveInt = [](float f) {
        // rounding is equal to adding 0.5 and cutting off the fractional part
        return static_cast<int>(f + 0.5f);
    };

    return getVoxel(roundToPositiveInt(coord.x), roundToPositiveInt(coord.y), roundToPositiveInt(coord.z));
}

// ======= TODO : IMPLEMENT the functions below for tri-linear interpolation ========
// ======= Consider using the linearInterpolate and biLinearInterpolate functions ===
// This function returns the trilinear interpolated value at the continuous 3D position given by coord.
float Volume::getSampleTriLinearInterpolation(const glm::vec3& coord) const
{
    if (glm::any(glm::lessThan(coord, glm::vec3(0))) || glm::any(glm::greaterThanEqual(coord + 1.0f, glm::vec3(m_dim)))) {
        return 0.0f;
    }

    // For tri-linear interpolation, we need to basically do the same as bi-linear interpolation
    // Given the continuous xy coordinate we need to compute that for the floor and ceil of z and
    // interpolate those two results
    const int floor_z = static_cast<int>(floorf(coord.z));
    const int ceil_z = floor_z + 1;
    const glm::vec2 xy_coord = glm::vec2(coord.x, coord.y);
    const float z_factor = coord.z - floor_z;

    const float interpolated_floor = this->biLinearInterpolate(xy_coord, floor_z);
    const float interpolated_ceil = this->biLinearInterpolate(xy_coord, ceil_z);

    const float interpolated_result = Volume::linearInterpolate(interpolated_floor, interpolated_ceil, z_factor);
    return interpolated_result;
}

// This function linearly interpolates the value at X using incoming values g0 and g1 given a factor (equal to the positon of x in 1D)
//
// g0--X--------g1
//   factor
float Volume::linearInterpolate(float g0, float g1, float factor)
{
    return std::lerp(g0, g1, factor);
}

// This function bi-linearly interpolates the value at the given continuous 2D XY coordinate for a fixed integer z coordinate.
float Volume::biLinearInterpolate(const glm::vec2& xyCoord, int z) const
{
    // To bi-linearly interpolate we need to get the interpolated value of min_y, min_x and min_y and max_x
    // This value gets interpolated with the interpolated value of max_y, min_x and max_y, max_x
    const int min_y = static_cast<int>(floorf(xyCoord.y));
    const int max_y = min_y + 1;
    const int min_x = static_cast<int>(floorf(xyCoord.x));
    const int max_x = min_x + 1;

    const float x_factor = xyCoord.x - min_x;
    const float y_factor = xyCoord.y - min_y;

    const float a = this->getVoxel(min_x, min_y, z);
    const float b = this->getVoxel(max_x, min_y, z);
    const float c = this->getVoxel(min_x, max_y, z);
    const float d = this->getVoxel(max_x, max_y, z);

    const float interpolate_ab = Volume::linearInterpolate(a, b, x_factor);
    const float interpolate_cd = Volume::linearInterpolate(c, d, x_factor);

    const float interpolated_result = Volume::linearInterpolate(interpolate_ab, interpolate_cd, y_factor);

    return interpolated_result;
}


// ======= OPTIONAL : This functions can be used to implement cubic interpolation ========
// This function represents the h(x) function, which returns the weight of the cubic interpolation kernel for a given position x
float Volume::weight(float x)
{
    return 0.0f;
}

// ======= OPTIONAL : This functions can be used to implement cubic interpolation ========
// This functions returns the results of a cubic interpolation using 4 values and a factor
float Volume::cubicInterpolate(float g0, float g1, float g2, float g3, float factor)
{
    return 0.0f;
}

// ======= OPTIONAL : This functions can be used to implement cubic interpolation ========
// This function returns the value of a bicubic interpolation
float Volume::biCubicInterpolate(const glm::vec2& xyCoord, int z) const
{
    return 0.0f;
}

// ======= OPTIONAL : This functions can be used to implement cubic interpolation ========
// This function computes the tricubic interpolation at coord
float Volume::getSampleTriCubicInterpolation(const glm::vec3& coord) const
{
    return 0.0f;
}

// Load an fld volume data file
// First read and parse the header, then the volume data can be directly converted from bytes to uint16_ts
void Volume::loadFile(const std::filesystem::path& file)
{
    assert(std::filesystem::exists(file));
    std::ifstream ifs(file, std::ios::binary);
    assert(ifs.is_open());

    // Normalize file extension to lowercase
    std::string extension = file.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

    // Check file type
    if (extension == ".fld") {
        m_fileExtension = FileExtension::FLD;
    }
    else if (extension == ".dat") {
        m_fileExtension = FileExtension::DAT;
    }
    else {
        std::cerr << "Unsupported file extension: " << extension << "\n";
        return;
    }

    const auto header = readHeader(ifs, m_fileExtension);
    m_dim = header.dim;
    m_elementSize = header.elementSize;

    loadVolumeData(ifs);
}

void Volume::loadVolumeData(std::ifstream& ifs)
{
    const size_t voxelCount = static_cast<size_t>(m_dim.x * m_dim.y * m_dim.z);
    const size_t byteCount = voxelCount * m_elementSize;
    std::vector<char> buffer(byteCount);
    // Data section is separated from header by two /f characters.
    if (m_fileExtension == FileExtension::FLD) ifs.seekg(2, std::ios::cur);
    ifs.read(buffer.data(), std::streamsize(byteCount));

    m_data.resize(voxelCount);
    if (m_elementSize == 1) { // Bytes.
        for (size_t i = 0; i < byteCount; i++) {
            m_data[i] = static_cast<float>(buffer[i] & 0xFF);
        }
    } else if (m_elementSize == 2) { // uint16_ts.
        for (size_t i = 0; i < byteCount; i += 2) {
            m_data[i / 2] = static_cast<float>((buffer[i] & 0xFF) + (buffer[i + 1] & 0xFF) * 256);
        }
    }
}
}

static Header readHeader(std::ifstream& ifs, const volume::FileExtension& fileExtension)
{
    if (fileExtension == volume::FileExtension::FLD) {
        return readVolumeHeader_fld(ifs);
    } else if (fileExtension == volume::FileExtension::DAT) {
        return readVolumeHeader_dat(ifs);
    } else {
        return {};
    }
}

Header readVolumeHeader_dat(std::ifstream& ifs)
{
    Header out {};

    unsigned short sizeX, sizeY, sizeZ;
    const std::size_t nbytes = 2;
    char buff[nbytes];
    ifs.read(buff, nbytes);
    std::memcpy(&sizeX, buff, sizeof(int));
    ifs.read(buff, nbytes);
    std::memcpy(&sizeY, buff, sizeof(int));
    ifs.read(buff, nbytes);
    std::memcpy(&sizeZ, buff, sizeof(int));
    out.dim.x = sizeX;
    out.dim.y = sizeY;
    out.dim.z = sizeZ;
    out.elementSize = 2;
    return out;
}

Header readVolumeHeader_fld(std::ifstream& ifs)
{
    Header out {};

    // Read input until the data section starts.
    std::string line;
    while (ifs.peek() != '\f' && !ifs.eof()) {
        std::getline(ifs, line);
        // Remove comments.
        line = line.substr(0, line.find('#'));
        // Remove any spaces from the string.
        // https://stackoverflow.com/questions/83439/remove-spaces-from-stdstring-in-c
        line.erase(std::remove_if(std::begin(line), std::end(line), ::isspace), std::end(line));
        if (line.empty())
            continue;

        const auto separator = line.find('=');
        const auto key = line.substr(0, separator);
        const auto value = line.substr(separator + 1);

        if (key == "ndim") {
            if (std::stoi(value) != 3) {
                std::cout << "Only 3D files supported\n";
            }
        } else if (key == "dim1") {
            out.dim.x = std::stoi(value);
        } else if (key == "dim2") {
            out.dim.y = std::stoi(value);
        } else if (key == "dim3") {
            out.dim.z = std::stoi(value);
        } else if (key == "nspace") {
        } else if (key == "veclen") {
            if (std::stoi(value) != 1)
                std::cerr << "Only scalar m_data are supported" << std::endl;
        } else if (key == "data") {
            if (value == "byte") {
                out.elementSize = 1;
            } else if (value == "short") {
                out.elementSize = 2;
            } else {
                std::cerr << "Data type " << value << " not recognized" << std::endl;
            }
        } else if (key == "field") {
            if (value != "uniform")
                std::cerr << "Only uniform m_data are supported" << std::endl;
        } else if (key == "#") {
            // Comment.
        } else {
            std::cerr << "Invalid AVS keyword " << key << " in file" << std::endl;
        }
    }
    return out;
}

static float computeMinimum(gsl::span<const float> data)
{
    return float(*std::min_element(std::begin(data), std::end(data)));
}

static float computeMaximum(gsl::span<const float> data)
{
    return float(*std::max_element(std::begin(data), std::end(data)));
}

static std::vector<int> computeHistogram(gsl::span<const float> data)
{
    std::vector<int> histogram(size_t(*std::max_element(std::begin(data), std::end(data)) + 1), 0);
    for (const auto v : data)
        histogram[v]++;
    return histogram;
}
