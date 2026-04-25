# D3D11NvCodecApp
D3D11-based application for testing the NvCodec DLL

# Info
A sample D3D11 application for integrating and testing the `NvCodec` DLL.
This project is used to validate encode/decode workflows built on Direct3D 11 and the NVIDIA Video Codec SDK.

# Dependencies
- [Core](./Modules/Core) as a submodule
- [NvCodec](./Modules/NvCodec) as a submodule

# Build Environment
- C++20
- MSVC (Visual Studio 2022)
- Windows 10/11 x64
- Direct3D 11

# Notes
- This repository uses `Core` and `NvCodec` as submodules.
- `NvCodec` also depends on CUDA Toolkit and NVIDIA Video Codec SDK.
- Make sure all submodules are initialized before building.

# Clone
- Clone submodules:
```bash
git clone --recurse-submodules https://github.com/KommyButterCream/D3D11NvCodecApp.git
'''
- If already cloned without submodules:
```bash
git submodule update --init --recursive
'''
