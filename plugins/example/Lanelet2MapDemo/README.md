# Lanelet2 Map Demo plugin

This example plugin adds a small CloudCompare catalog dialog that lets you collect **lanes** and **objects** from selected point clouds and export them as a minimal Lanelet2 `.osm` map.

## Build

Enable the plugin at CMake configure time:

```bash
cmake -S /home/runner/work/CloudCompare/CloudCompare -B build -DPLUGIN_EXAMPLE_LANELET2_MAP_DEMO=ON
cmake --build build --parallel
```

## Behavior

- The dialog stays open as a small catalog where you can add **lane** entries and **object** entries from the currently selected point cloud.
- Lanes are exported as left/right boundaries plus a Lanelet2 `lanelet` relation.
- Objects are exported as simple rectangular `multipolygon` relations.
- The plugin uses the selected cloud's **global/original** coordinates (`getOwnGlobalBB`) so existing global shift / scale values are preserved.
- The dialog asks for a geographic origin because Lanelet2 `.osm` files store WGS84 coordinates. To recover the same local frame, load the map with the same **LocalCartesian** origin you entered during export.

## Linux AppImage

After configuring and building CloudCompare, you can package an AppImage with:

```bash
cmake --build build --target appimage
```

or directly:

```bash
/home/runner/work/CloudCompare/CloudCompare/scripts/linux/build-appimage.sh /absolute/path/to/build
```

This requires `linuxdeploy` and its Qt plugin to be installed on the build machine.

## Limits

- This is a quick demo catalog/exporter, not a full lane-drawing editor.
- Object export currently uses simple rectangular footprints derived from point-cloud bounding boxes.
- It assumes the point cloud frame is local Cartesian ENU (`X=east`, `Y=north`, `Z=up`) when converting to Lanelet2's WGS84 storage format.
