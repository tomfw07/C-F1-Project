#pragma once
#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

struct Sample {
    double distance = 0.0;
    double speed = 0.0;
    double throttle = 0.0;
    double brake = 0.0;
    double rpm = 0.0;
    int gear = 0;
    bool hasThrottle = false;
    bool hasBrake = false;
    bool hasRpm = false;
    bool hasGear = false;
};

inline std::string trim(std::string s) {
    auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
    return s;
}

inline std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

// CSV splitter supporting quoted fields and escaped quotes ("").
inline std::vector<std::string> splitCsv(const std::string& line) {
    std::vector<std::string> fields;
    std::string field;
    bool quoted = false;
    for (std::size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (c == '"') {
            if (quoted && i + 1 < line.size() && line[i + 1] == '"') {
                field += '"';
                ++i;
            } else {
                quoted = !quoted;
            }
        } else if (c == ',' && !quoted) {
            fields.push_back(trim(field));
            field.clear();
        } else {
            field += c;
        }
    }
    fields.push_back(trim(field));
    return fields;
}

inline std::optional<double> toDouble(const std::string& raw) {
    const std::string s = trim(raw);
    if (s.empty()) return std::nullopt;
    try {
        std::size_t used = 0;
        double v = std::stod(s, &used);
        if (used != s.size() || !std::isfinite(v)) return std::nullopt;
        return v;
    } catch (...) {
        return std::nullopt;
    }
}

inline std::vector<Sample> loadTelemetry(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("Could not open file: " + path);

    std::string line;
    if (!std::getline(in, line)) throw std::runtime_error("CSV file is empty: " + path);
    if (line.size() >= 3 &&
        static_cast<unsigned char>(line[0]) == 0xEF &&
        static_cast<unsigned char>(line[1]) == 0xBB &&
        static_cast<unsigned char>(line[2]) == 0xBF) line.erase(0, 3);

    auto headers = splitCsv(line);
    std::map<std::string, std::size_t> col;
    for (std::size_t i = 0; i < headers.size(); ++i)
        col[lower(trim(headers[i]))] = i;

    auto findCol = [&](std::initializer_list<const char*> names) -> std::optional<std::size_t> {
        for (const char* name : names) {
            auto it = col.find(name);
            if (it != col.end()) return it->second;
        }
        return std::nullopt;
    };

    auto distanceCol = findCol({"distance"});
    auto speedCol = findCol({"speed"});
    if (!distanceCol || !speedCol)
        throw std::runtime_error("CSV needs 'Distance' and 'Speed' columns.");

    auto throttleCol = findCol({"throttle"});
    auto brakeCol = findCol({"brake"});
    auto gearCol = findCol({"ngear", "gear"});
    auto rpmCol = findCol({"rpm"});

    std::vector<Sample> data;
    std::size_t lineNo = 1;
    while (std::getline(in, line)) {
        ++lineNo;
        if (trim(line).empty()) continue;
        auto f = splitCsv(line);
        auto get = [&](std::size_t idx) -> std::string {
            return idx < f.size() ? f[idx] : "";
        };
        auto d = toDouble(get(*distanceCol));
        auto s = toDouble(get(*speedCol));
        if (!d || !s) continue; // Skip blank, metadata, or malformed rows.

        Sample p;
        p.distance = *d;
        p.speed = *s;
        if (throttleCol) {
            auto v = toDouble(get(*throttleCol));
            if (v) { p.throttle = *v; p.hasThrottle = true; }
        }
        if (brakeCol) {
            auto v = toDouble(get(*brakeCol));
            if (v) { p.brake = *v; p.hasBrake = true; }
        }
        if (gearCol) {
            auto v = toDouble(get(*gearCol));
            if (v) { p.gear = static_cast<int>(std::lround(*v)); p.hasGear = true; }
        }
        if (rpmCol) {
            auto v = toDouble(get(*rpmCol));
            if (v) { p.rpm = *v; p.hasRpm = true; }
        }
        data.push_back(p);
    }
    if (data.size() < 2)
        throw std::runtime_error("Fewer than two valid telemetry rows in: " + path);
    std::sort(data.begin(), data.end(),
              [](const Sample& a, const Sample& b) { return a.distance < b.distance; });
    return data;
}
