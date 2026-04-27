#include "../../../Module/NvCodec/NvEncode/D3D11NvEncoder.h"
#include "../../../Module/NvCodec/NvDecode/D3D11NvDecoder.h"

#include <d3d11.h>
#include <dxgi.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#pragma comment(lib, "d3d11.lib")

namespace
{
	struct FrameSize
	{
		uint32_t width = 0;
		uint32_t height = 0;
	};

	constexpr uint32_t kEncodeBufferCount = 4;
	constexpr uint32_t kEncodeFrameCount = 1;
	constexpr uint64_t kDecodeTimeout_ms = 5000;
	constexpr uint64_t kDecodeIdleWait_ms = 250;

	void ReleaseTexture(ID3D11Texture2D* textures)
	{
		if (textures)
		{
			textures->Release();
			textures = nullptr;
		}
	}

	void ReleaseTextureVector(std::vector<ID3D11Texture2D*>& textures)
	{
		for (ID3D11Texture2D*& texture : textures)
		{
			if (texture)
			{
				texture->Release();
				texture = nullptr;
			}
		}
	}

	std::filesystem::path ResolvePath(const std::wstring& fileName)
	{
		namespace fs = std::filesystem;

		const fs::path current = fs::current_path();
		const std::array<fs::path, 5> candidates =
		{
			current / fileName,
			current.parent_path() / fileName,
			current.parent_path().parent_path() / fileName,
			current.parent_path().parent_path().parent_path() / fileName,
			current.parent_path().parent_path().parent_path().parent_path() / fileName
		};

		for (const fs::path& candidate : candidates)
		{
			std::error_code ec;
			if (fs::exists(candidate, ec))
			{
				return candidate;
			}
		}

		return {};
	}

	bool DetectFrameSize(size_t byteSize, FrameSize& frameSize)
	{
		constexpr std::array<FrameSize, 5> knownSizes =
		{
			FrameSize{1280, 720},
			FrameSize{1920, 1080},
			FrameSize{2560, 1440},
			FrameSize{3840, 2160},
			FrameSize{7680, 4320}
		};

		for (const FrameSize& candidate : knownSizes)
		{
			const uint64_t requiredBytes = static_cast<uint64_t>(candidate.width) * candidate.height * 4ull;
			if (requiredBytes == byteSize)
			{
				frameSize = candidate;
				return true;
			}
		}

		return false;
	}

	bool LoadBGRAImage(const std::filesystem::path& imagePath, FrameSize frameSize, std::vector<uint8_t>& buffer)
	{
		const size_t expectedSize = static_cast<size_t>(frameSize.width) * frameSize.height * 4ull;
		std::ifstream imageFile(imagePath, std::ios::binary);
		if (!imageFile.is_open())
		{
			wprintf(L"Failed to open input image: %ls\n", imagePath.c_str());
			return false;
		}

		buffer.resize(expectedSize);
		imageFile.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
		if (!imageFile || static_cast<size_t>(imageFile.gcount()) != expectedSize)
		{
			wprintf(L"Failed to read %zu bytes from: %ls\n", expectedSize, imagePath.c_str());
			return false;
		}

		return true;
	}
}

bool CreateD3D11(ID3D11Device** device, ID3D11DeviceContext** context);
bool DetectInputFrameSize(const std::filesystem::path& imagePath, FrameSize& frameSize);
bool DoEncode(ID3D11Device* device, ID3D11DeviceContext* context, const std::filesystem::path& inputPath, const std::filesystem::path& outputPath, FrameSize frameSize);
bool DoDecode(ID3D11Device* device, const std::filesystem::path& bitstreamPath);

int main()
{
	ID3D11Device* device = nullptr;
	ID3D11DeviceContext* context = nullptr;

	const std::filesystem::path inputPath = ResolvePath(L"QHDDesktop.bgra");
	if (inputPath.empty())
	{
		printf_s("Failed to locate bgra file\n");
		return 1;
	}

	FrameSize frameSize;
	if (!DetectInputFrameSize(inputPath, frameSize))
	{
		return 1;
	}

	const std::filesystem::path outputPath = inputPath.parent_path() / L"Encode.h264";

	if (!CreateD3D11(&device, &context))
	{
		return 1;
	}

	int exitCode = 0;

	if (!DoEncode(device, context, inputPath, outputPath, frameSize))
	{
		exitCode = 1;
	}
	else if (!DoDecode(device, outputPath))
	{
		exitCode = 1;
	}

	if (context)
	{
		context->Release();
		context = nullptr;
	}

	if (device)
	{
		device->Release();
		device = nullptr;
	}

	return exitCode;
}

bool CreateD3D11(ID3D11Device** device, ID3D11DeviceContext** context)
{
	if (!device || !context)
	{
		return false;
	}

	*device = nullptr;
	*context = nullptr;

	D3D_FEATURE_LEVEL createdFeatureLevel = D3D_FEATURE_LEVEL_11_0;
	const UINT createFlags = 0;

	const D3D_FEATURE_LEVEL primaryFeatureLevels[] =
	{
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0
	};

	HRESULT hr = D3D11CreateDevice(
		nullptr,
		D3D_DRIVER_TYPE_HARDWARE,
		nullptr,
		createFlags,
		primaryFeatureLevels,
		static_cast<UINT>(std::size(primaryFeatureLevels)),
		D3D11_SDK_VERSION,
		device,
		&createdFeatureLevel,
		context);

	if (hr == E_INVALIDARG)
	{
		const D3D_FEATURE_LEVEL fallbackFeatureLevels[] =
		{
			D3D_FEATURE_LEVEL_11_0
		};

		hr = D3D11CreateDevice(
			nullptr,
			D3D_DRIVER_TYPE_HARDWARE,
			nullptr,
			createFlags,
			fallbackFeatureLevels,
			static_cast<UINT>(std::size(fallbackFeatureLevels)),
			D3D11_SDK_VERSION,
			device,
			&createdFeatureLevel,
			context);
	}

	if (FAILED(hr))
	{
		printf_s("Failed to create D3D11 device. hr=0x%08X\n", static_cast<uint32_t>(hr));
		return false;
	}

	printf_s("Created D3D11 device. featureLevel=0x%X\n", static_cast<uint32_t>(createdFeatureLevel));
	return true;
}

bool DetectInputFrameSize(const std::filesystem::path& imagePath, FrameSize& frameSize)
{
	std::error_code ec;
	const uintmax_t fileSize = std::filesystem::file_size(imagePath, ec);
	if (ec)
	{
		wprintf(L"Failed to query file size: %ls\n", imagePath.c_str());
		return false;
	}

	if (!DetectFrameSize(static_cast<size_t>(fileSize), frameSize))
	{
		wprintf(L"Unsupported BGRA file size (%llu bytes): %ls\n", static_cast<unsigned long long>(fileSize), imagePath.c_str());
		return false;
	}

	printf_s("Detected input frame size: %ux%u\n", frameSize.width, frameSize.height);
	return true;
}

bool DoEncode(ID3D11Device* device, ID3D11DeviceContext* context, const std::filesystem::path& inputPath, const std::filesystem::path& outputPath, FrameSize frameSize)
{
	if (!device || !context)
	{
		return false;
	}

	std::vector<uint8_t> bgraBuffer;
	if (!LoadBGRAImage(inputPath, frameSize, bgraBuffer))
	{
		return false;
	}

	std::ofstream outputFile(outputPath, std::ios::binary | std::ios::trunc);
	if (!outputFile.is_open())
	{
		wprintf(L"Failed to open encode output: %ls\n", outputPath.c_str());
		return false;
	}

	ID3D11Texture2D* inputTexture = nullptr;

	HRESULT hr = S_OK;
	D3D11_TEXTURE2D_DESC texDesc = {};
	texDesc.Width = frameSize.width;
	texDesc.Height = frameSize.height;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	texDesc.SampleDesc.Count = 1;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	ID3D11Texture2D* texture = nullptr;
	hr = device->CreateTexture2D(&texDesc, nullptr, &texture);
	if (FAILED(hr))
	{
		printf_s("Texture creation failed. hr=0x%08X\n", static_cast<uint32_t>(hr));

		ReleaseTexture(inputTexture);
		return false;
	}

	context->UpdateSubresource(
		texture,
		0,
		nullptr,
		bgraBuffer.data(),
		frameSize.width * 4,
		0);

	auto encoder = std::make_unique<D3D11NvEncoder>();
	if (!encoder->Initialize(device, frameSize.width, frameSize.height, kEncodeBufferCount))
	{
		printf_s("Encoder initialization failed.\n");
		ReleaseTexture(inputTexture);
		return false;
	}

	bool success = true;
	uint32_t writtenPacketCount = 0;

	for (uint32_t frameIndex = 0; frameIndex < kEncodeFrameCount; ++frameIndex)
	{
		// Encode 수행될 버퍼 업데이트
		encoder->PrepareFrameForEncode(inputTexture);

		NvEncPacket packet = {};
		if (!encoder->DoEncode(packet))
		{
			printf_s("Encoding failed at frame %u.\n", frameIndex);
			success = false;
			break;
		}

		if (packet.data && packet.size > 0)
		{
			outputFile.write(reinterpret_cast<const char*>(packet.data), packet.size);
			if (!outputFile)
			{
				printf_s("Failed to write encoded packet to disk.\n");
				success = false;
				break;
			}

			++writtenPacketCount;
		}
	}

	outputFile.flush();
	if (!outputFile)
	{
		printf_s("Failed to finalize encode output file.\n");
		success = false;
	}

	encoder->Destroy();
	ReleaseTexture(inputTexture);

	if (!success)
	{
		return false;
	}

	if (writtenPacketCount == 0)
	{
		printf_s("Encoder succeeded but produced no output packets.\n");
		return false;
	}

	wprintf(L"Encode test succeeded. packets=%u output=%ls\n", writtenPacketCount, outputPath.c_str());
	return true;
}

bool DoDecode(ID3D11Device* device, const std::filesystem::path& bitstreamPath)
{
	if (!device)
	{
		return false;
	}

	std::ifstream inputFile(bitstreamPath, std::ios::binary | std::ios::ate);
	if (!inputFile.is_open())
	{
		wprintf(L"Failed to open bitstream: %ls\n", bitstreamPath.c_str());
		return false;
	}

	const std::streamsize streamSize = inputFile.tellg();
	if (streamSize <= 0)
	{
		wprintf(L"Encoded bitstream is empty: %ls\n", bitstreamPath.c_str());
		return false;
	}

	inputFile.seekg(0, std::ios::beg);

	std::vector<uint8_t> bitstream(static_cast<size_t>(streamSize));
	inputFile.read(reinterpret_cast<char*>(bitstream.data()), streamSize);
	if (!inputFile || inputFile.gcount() != streamSize)
	{
		printf_s("Failed to read encoded bitstream.\n");
		return false;
	}

	auto decoder = std::make_unique<D3D11NvDecoder>();
	if (!decoder->Initialize(device))
	{
		printf_s("Decoder initialization failed.\n");
		return false;
	}

	if (!decoder->Parse(bitstream.data(), static_cast<uint32_t>(bitstream.size()), false, true, false))
	{
		printf_s("Failed to submit bitstream to decoder.\n");
		decoder->ShutDown();
		return false;
	}

	uint32_t decodedFrameCount = 0;
	uint64_t startTick = ::GetTickCount64();
	uint64_t lastFrameTick = startTick;

	while (true)
	{
		if (D3D11NvDecoder::Frame* frame = decoder->GetFrame())
		{
			if (!frame->texture)
			{
				printf_s("Decoder returned a frame without texture.\n");
				decoder->ShutDown();
				return false;
			}

			++decodedFrameCount;
			lastFrameTick = ::GetTickCount64();
			continue;
		}

		const uint64_t now = ::GetTickCount64();
		const uint64_t totalElapsed = now - startTick;
		const uint64_t idleElapsed = now - lastFrameTick;

		if (decodedFrameCount > 0 && idleElapsed >= kDecodeIdleWait_ms)
		{
			break;
		}

		if (totalElapsed >= kDecodeTimeout_ms)
		{
			printf_s("Decode timed out waiting for frames.\n");
			decoder->ShutDown();
			return false;
		}

		::Sleep(10);
	}

	decoder->ShutDown();

	if (decodedFrameCount == 0)
	{
		printf_s("Decode completed but produced no frames.\n");
		return false;
	}

	printf_s("Decode test succeeded. frames=%u\n", decodedFrameCount);
	return true;
}
