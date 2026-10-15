# F1 Telemetry Analyzer (C++)

A small C++17 command-line project for analysing Formula 1 lap telemetry exported to CSV. It reports speed and driver-input statistics, identifies likely braking zones, and can compare two laps by distance.

## Features
- Reads telemetry CSV files with a header row.
- Reports distance covered, maximum and mean speed, maximum RPM, average throttle, brake usage, and gear usage.
- Detects likely braking zones (brake input above a configurable threshold).
- Optionally compares two laps sampled at matching distance intervals and writes a comparison CSV.
- Uses only the C++ standard library; no extra dependencies.

## Build

### Windows (Visual Studio / CMake)
Install a C++ compiler and CMake, then from this folder run:

```powershell
cmake -S . -B build
cmake --build build --config Release
```

### Linux / macOS
```bash
cmake -S . -B build
cmake --build build -j
```

The executable will be in `build/` (on some Windows generators, under `build/Release/`).

## CSV format

The program accepts case-insensitive column names. Required:
- `Distance` (metres)
- `Speed` (km/h)

Optional:
- `Throttle` (0–100)
- `Brake` (0–100, or 0/1)
- `nGear` (or `Gear`)
- `RPM`

A typical CSV header is:

```csv
Distance,Speed,Throttle,Brake,nGear,RPM
0,122,35,0,3,9800
10,130,52,0,4,10100
20,141,76,0,5,10800
30,149,94,0,5,11200
40,145,30,45,4,10400
```

### Using FastF1
FastF1 is a Python package, while this project is C++. You can use FastF1 to export a lap's telemetry to CSV, then analyse that CSV here. Example Python export:

```python
import fastf1

session = fastf1.get_session(2024, "Monza", "Q")
session.load()
lap = session.laps.pick_fastest()
telemetry = lap.get_telemetry()
telemetry.to_csv("lap.csv", index=False)
```

FastF1's exported telemetry may use names such as `Distance`, `Speed`, `Throttle`, `Brake`, `nGear`, and `RPM`. The parser ignores extra columns such as time, X/Y coordinates, and driver metadata.

## Run

Analyse one lap:

```bash
f1_telemetry lap.csv
```

Set a braking threshold (default 10):

```bash
f1_telemetry lap.csv --brake-threshold 15
```

Compare two laps and save a distance-aligned CSV:

```bash
f1_telemetry lap1.csv lap2.csv --compare comparison.csv
```

Comparison output contains distance, each lap's interpolated speed, and speed delta (`lap2 - lap1`). Positive delta means lap 2 is faster at that distance.

## Notes and limitations
- This is a learning project, not an official FIA timing tool.
- Braking zones are estimated from the Brake channel and threshold; they are not corner labels.
- The comparison uses linear interpolation between telemetry samples. It compares channels at matching distance, not matching elapsed time.
- Speed averages are sample averages, not time-weighted averages.
- Always check the source data's units and channel names.
