# ResolumeLayerControl

現在はWebSocket方式の検証用実装「Layer Control」を開発中です。Windows Releaseビルドと単体・通信試験は実施済みですが、実機受入は未完了です。[仕様](docs/SPECIFICATION.md)、[ビルド・導入](docs/SETUP.md)、[共通基盤と固有処理の分離](docs/ARCHITECTURE.md)、[検証記録](docs/VALIDATION.md)を参照してください。旧OSC Probeは研究記録として保持しています。

ResolumePlugin is a custom FFGL plugin that lets you control a specified layer’s transition settings and mask mode using an Effect Clip.

## Setup

### Download DLL

⬇️ [Download the latest release](https://github.com/zukio/ResolumeLayerControl/releases/new)

### Use in Resolume

1️⃣ Copy the DLL to `%USERPROFILE%\Documents\Resolume Arena\Extra Effects` (or add its folder in Resolume preferences),
2️⃣ restart Resolume,
3️⃣ add **Layer Ctrl Probe** as an Effect Clip,
4️⃣ and Reload the window list once.

## Build

The current build target is **LayerControlProbe**, a diagnostic FFGL plug-in displayed as **Layer Ctrl Probe** in Resolume. It does not yet change Layer Transition settings or Mask Mode.

Install Visual Studio 2022 with **Desktop development with C++**, the **MSVC v143** toolset, and a **Windows SDK**. The project uses the bundled FFGL sources in `source\lib` and GLEW in `deps\glew-2.1.0` through `deps\glew.props`.

Open `build\windows\FFGLPlugins.sln`, select `Release|x64` (or `Debug|x64`), then right-click **effects > LayerControlProbe** in Solution Explorer and select **Build**.

Alternatively, run the following from the repository root in **Developer PowerShell for VS 2022**:

```powershell
msbuild .\build\windows\LayerControlProbe.vcxproj /p:Configuration=Release /p:Platform=x64
```

Use `/p:Configuration=Debug` for a debug build. The output DLL is:

```text
binaries\x64\<Configuration>\LayerControlProbe.dll
```

These instructions reflect the checked-in Visual Studio project; the command above has not been verified in this documentation update. See [the validation notes](docs/VALIDATION.md) for installation, diagnostic tests, and recorded results. Building the DLL does not establish that Column-triggered control works.

## Dependency

The official Resolume FFGL SDK is vendored under `third_party\ffgl` from <https://github.com/resolume/ffgl>.
