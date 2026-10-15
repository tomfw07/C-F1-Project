#include "telemetry.hpp"
#include <cstdlib>

struct Stats {
    double minDistance = 0, maxDistance = 0, maxSpeed = 0, meanSpeed = 0;
    double meanThrottle = 0, brakePercent = 0, maxRpm = 0;
    std::size_t samples = 0, brakeSamples = 0;
    std::map<int, std::size_t> gearCounts;
};

Stats calculateStats(const std::vector<Sample>& data, double brakeThreshold) {
    Stats st;
    st.samples = data.size();
    st.minDistance = data.front().distance;
    st.maxDistance = data.back().distance;
    double speedSum = 0, throttleSum = 0;
    std::size_t throttleN = 0, rpmN = 0, brakeN = 0;
    for (const auto& p : data) {
        speedSum += p.speed;
        st.maxSpeed = std::max(st.maxSpeed, p.speed);
        if (p.hasThrottle) { throttleSum += p.throttle; ++throttleN; }
        if (p.hasBrake) {
            ++brakeN;
            if (p.brake >= brakeThreshold) ++st.brakeSamples;
        }
        if (p.hasRpm) { st.maxRpm = std::max(st.maxRpm, p.rpm); ++rpmN; }
        if (p.hasGear) ++st.gearCounts[p.gear];
    }
    st.meanSpeed = speedSum / static_cast<double>(data.size());
    if (throttleN) st.meanThrottle = throttleSum / static_cast<double>(throttleN);
    if (brakeN) st.brakePercent = 100.0 * static_cast<double>(st.brakeSamples) /
                                  static_cast<double>(brakeN);
    return st;
}

void printStats(const std::string& label, const std::vector<Sample>& data,
                double threshold) {
    const Stats s = calculateStats(data, threshold);
    std::cout << "\n=== " << label << " ===\n";
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Samples:                 " << s.samples << '\n'
              << "Distance range:          " << s.minDistance << " - "
              << s.maxDistance << " m\n"
              << "Distance covered:        " << (s.maxDistance - s.minDistance) << " m\n"
              << "Maximum speed:           " << s.maxSpeed << " km/h\n"
              << "Mean sampled speed:      " << s.meanSpeed << " km/h\n";
    bool hasThrottle = std::any_of(data.begin(), data.end(), [](const Sample& p){return p.hasThrottle;});
    bool hasBrake = std::any_of(data.begin(), data.end(), [](const Sample& p){return p.hasBrake;});
    bool hasRpm = std::any_of(data.begin(), data.end(), [](const Sample& p){return p.hasRpm;});
    bool hasGear = std::any_of(data.begin(), data.end(), [](const Sample& p){return p.hasGear;});
    if (hasThrottle) std::cout << "Mean throttle:           " << s.meanThrottle << " %\n";
    if (hasBrake) std::cout << "Braking samples:         " << s.brakeSamples
                            << " (" << s.brakePercent << "% of brake samples, threshold "
                            << threshold << ")\n";
    if (hasRpm) std::cout << "Maximum RPM:             " << s.maxRpm << '\n';
    if (hasGear) {
        std::cout << "Gear sample counts:      ";
        bool first = true;
        for (const auto& kv : s.gearCounts) {
            if (!first) std::cout << ", ";
            std::cout << kv.first << ": " << kv.second;
            first = false;
        }
        std::cout << '\n';
    }
    if (hasBrake) {
        std::cout << "Likely braking zones (start - end distance):\n";
        bool active = false;
        double start = 0, end = 0;
        for (const auto& p : data) {
            const bool braking = p.hasBrake && p.brake >= threshold;
            if (braking && !active) { start = p.distance; active = true; }
            if (braking) end = p.distance;
            if (!braking && active) {
                std::cout << "  " << start << " - " << end << " m\n";
                active = false;
            }
        }
        if (active) std::cout << "  " << start << " - " << end << " m\n";
    }
}

double interpolateSpeed(const std::vector<Sample>& data, double distance) {
    auto it = std::lower_bound(data.begin(), data.end(), distance,
        [](const Sample& p, double d) { return p.distance < d; });
    if (it == data.begin()) return it->speed;
    if (it == data.end()) return data.back().speed;
    const auto& b = *it;
    const auto& a = *(it - 1);
    const double span = b.distance - a.distance;
    if (std::abs(span) < 1e-9) return a.speed;
    const double t = (distance - a.distance) / span;
    return a.speed + t * (b.speed - a.speed);
}

void compareLaps(const std::vector<Sample>& a, const std::vector<Sample>& b,
                 const std::string& outPath) {
    std::ofstream out(outPath);
    if (!out) throw std::runtime_error("Could not write comparison file: " + outPath);
    const double start = std::max(a.front().distance, b.front().distance);
    const double end = std::min(a.back().distance, b.back().distance);
    if (end <= start) throw std::runtime_error("The laps have no overlapping distance range.");
    out << "Distance_m,Lap1_Speed_kmh,Lap2_Speed_kmh,Speed_Delta_kmh\n";
    double totalAbsDelta = 0;
    std::size_t count = 0;
    constexpr double step = 5.0;
    for (double d = start; d <= end; d += step) {
        const double va = interpolateSpeed(a, d);
        const double vb = interpolateSpeed(b, d);
        out << std::fixed << std::setprecision(2) << d << ',' << va << ',' << vb
            << ',' << (vb - va) << '\n';
        totalAbsDelta += std::abs(vb - va);
        ++count;
    }
    std::cout << "\n=== Lap comparison ===\n"
              << "Wrote distance-aligned comparison to: " << outPath << '\n'
              << "Mean absolute speed difference: "
              << (count ? totalAbsDelta / static_cast<double>(count) : 0.0)
              << " km/h (sampled every 5 m)\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "F1 Telemetry Analyzer\n\n"
                  << "Usage:\n"
                  << "  f1_telemetry lap.csv [--brake-threshold 10]\n"
                  << "  f1_telemetry lap1.csv lap2.csv --compare comparison.csv\n";
        return 0;
    }

    try {
        std::string file1 = argv[1];
        std::string file2;
        std::string compareOut;
        double brakeThreshold = 10.0;

        for (int i = 2; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--brake-threshold" && i + 1 < argc) {
                brakeThreshold = std::stod(argv[++i]);
            } else if (arg == "--compare" && i + 1 < argc) {
                compareOut = argv[++i];
            } else if (!arg.empty() && arg[0] != '-') {
                if (file2.empty()) file2 = arg;
                else throw std::runtime_error("Unexpected extra argument: " + arg);
            } else {
                throw std::runtime_error("Unknown or incomplete option: " + arg);
            }
        }
        if (brakeThreshold < 0 || brakeThreshold > 100)
            throw std::runtime_error("Brake threshold must be between 0 and 100.");

        auto lap1 = loadTelemetry(file1);
        printStats(file1, lap1, brakeThreshold);

        if (!file2.empty()) {
            auto lap2 = loadTelemetry(file2);
            printStats(file2, lap2, brakeThreshold);
            if (compareOut.empty()) compareOut = "lap_comparison.csv";
            compareLaps(lap1, lap2, compareOut);
        } else if (!compareOut.empty()) {
            throw std::runtime_error("--compare requires two input CSV files.");
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
