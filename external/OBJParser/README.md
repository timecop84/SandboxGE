# OBJParser

Tiny OBJ loader that converts Wavefront OBJ files into simple position/normal/uv/index buffers. It is header-only on the interface side (`include/io/ObjParser.h`) with a single implementation file in `src/`.

## Build

```
cmake -S . -B build
cmake --build build
```

The CMake target is `OBJParser`. Link it and add `OBJParser/include` to your include paths.
