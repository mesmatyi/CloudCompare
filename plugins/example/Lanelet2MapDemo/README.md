# Lanelet2 Map Demo plugin

This example plugin adds a small CloudCompare dialog that exports a minimal Lanelet2 `.osm` file from the selected point cloud.

## Build

Enable the plugin at CMake configure time:

```bash
cmake -S /home/runner/work/CloudCompare/CloudCompare -B build -DPLUGIN_EXAMPLE_LANELET2_MAP_DEMO=ON
cmake --build build --parallel
```

## Behavior

- The plugin uses the selected cloud's **global/original** coordinates (`getOwnGlobalBB`) so existing global shift / scale values are preserved.
- The generated lanelet is a simple straight lane aligned to the cloud's longest XY extent (or the axis chosen in the dialog).
- The output mirrors the Lanelet2 Python tutorial structure: points → left/right ways → lanelet relation.
- The dialog asks for a geographic origin because Lanelet2 `.osm` files store WGS84 coordinates. To recover the same local frame, load the map with the same **LocalCartesian** origin you entered during export.

## Limits

- This is a quick demo exporter, not a full lane-drawing editor.
- It assumes the point cloud frame is local Cartesian ENU (`X=east`, `Y=north`, `Z=up`) when converting to Lanelet2's WGS84 storage format.
