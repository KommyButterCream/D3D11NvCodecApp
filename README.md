# D3D11NvCodecApp
D3D11-based application for testing the NvCodec DLL

# Info
A sample D3D11 application for integrating and testing the `NvCodec` DLL.
This project is used to validate encode/decode workflows built on Direct3D 11 and the NVIDIA Video Codec SDK.

# Dependencies
- [Core](./Modules/Core)
- [NvCodec](./Modules/NvCodec)

# Build Environment
- C++20
- MSVC (Visual Studio 2022)
- Windows 10/11 x64
- Direct3D 11

# Notes
- `NvCodec` depends on CUDA Toolkit and NVIDIA Video Codec SDK.

# Repository Layout
This project expects `NvCodec` and `Core` to be placed under the same parent directory.

Example:
```text
Module/
├─ Core/
└─ NvCodec/
