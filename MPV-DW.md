<p align="center">
<img src="/mpvnet.png" height="150">
</p>

<h1 align="center"> mpv.net-DW </h1>

<h3 align="center"> mpv.net_CM的💗DW版本💗 </h3>

<h3 align="center"> 定制了播放界面、右键菜单、缩略图、视频滤镜和着色器 </h3>

<p align="center">
<a href="https://visitorbadge.io/status?path=https%3A%2F%2Fgithub.com%2Fdiana7127%2Fmpv.net-DW"><img src="https://api.visitorbadge.io/api/combined?path=https%3A%2F%2Fgithub.com%2Fdiana7127%2Fmpv.net-DW&label=Hello%20Visitors&countColor=%23e7818b" height="25"></a>
<a href="https://github.com/diana7127/mpv.net-DW/releases"><img src="https://img.shields.io/github/v/release/diana7127/mpv.net-DW?label=Latest%20Release&&style=for-the-badge&color=%238cc7ff" height="25"></a>
</p>

<p align="center">
<img src="https://img.shields.io/github/downloads/diana7127/mpv.net-DW/total?style=for-the-badge&color=%23e94556&logo=github" height="25">
<img src="https://img.shields.io/github/stars/diana7127/mpv.net-DW?&style=for-the-badge&color=%23756dab&logo=readme&logoColor=white" height="25">
<img src="https://img.shields.io/badge/Support-Windows%20x64-%230094ff?style=for-the-badge&logo=Windows" height="25">
</p>

## 目录
- [📊 简介](#-简介-)
- [🔲 DW版本特点](#-dw版本特点-)
- [🔳 上游版本特点](#-上游版本特点-)
- [📺 界面预览](#-界面预览-)
- [📖 使用说明](#-使用说明-)
- [🧰 更新](#-更新-)
- [🔑 安装](#-安装-)
- [🔗 下载](#-下载-)
- [💌 鸣谢](#-鸣谢-)

## 📊 简介 [🔝](#目录)
🔲[mpv.net-DW_vX.0](https://github.com/diana7127/mpv.net-DW/tree/mpv.net-v6)：基于mpv.net v6版和mpv.net_CM的个人定制版

🔲[mpv.net-DW_v202X.0](https://github.com/diana7127/mpv.net-DW/tree/mpv.net-v7)：基于mpv.net v7版和mpv_lazy的个人定制版

🔳mpv.net：基于mpv开源播放器的Windows媒体播放器

## 🔲 DW版本特点 [🔝](#目录)
> 基于上游，做出以下个人定制款

✳️修改播放界面为ModernX（zydezu/ModernX）

✳️集成缩略图引擎thumbfast（po5/thumbfast）

✳️集成SVP补帧滤镜（hooke007/MPV_lazy）

✳️集成2x_AnimeJaNai V3和HFA2kCompact超分滤镜（hooke007/MPV_lazy）

✳️支持多种渲染样式显示次字幕

✳️修改右键菜单  
- 调整排列顺序
- 修改部分翻译（参考potplayer）
- 更多vs滤镜和着色器方案选择，兼容更多显卡
		
✳️修改部分mpv设置  
- 默认硬件解码为auto-copy
- 默认开启列表循环
- 默认OSD显示为建议方案
- 默认音量为100
- 默认开启interpolation
- 默认开启icc色彩管理
- 默认模糊识别音频与字幕
- 修改srt字幕样式
  
✳️修改部分快捷键设置（参考potplayer）  
- 文件详细信息 — TAB
- 打开其他音轨 — a
- 打开其他字幕 — l

✳️自解压安装包封装

✳️一些其他细节

## 🔳 上游版本特点 [🔝](#目录)
> 引用自hooke007/mpv.net_CM；mpvnet-player/mpv.net

### 🔘mpv.net_CM
✔️界面汉化

✔️编辑器选项修改

✔️菜单条目+初始快捷键修改

✔️集成Python+VapourSynth便携式组件

✔️预设脚本与着色器

✔️操作习惯移植自mpv-lazy

✔️有更友好的界面操作性

### 🔘mpv.net
#### ✳️v6.0版
✔️几乎所有mpv的功能都可用，除去一些窗口行为

✔️兼容几乎所有mpv脚本/着色器，除去部分具有特定依赖的项目

✔️单实例/多实例切换

✔️支持基础操作的播放列表、音轨字幕轨列表面板

✔️图形化的设置修改与快捷键编辑器

✔️可自定义的右键菜单

✔️音量、窗口尺寸的退出时记忆

#### ✳️v7.0+版
✔️.NET 6运行库

✔️改进了mpv.conf编辑器的布局，选项超过3项，就会使用全新的组合框控件（下拉菜单）

✔️mpv.conf编辑器根据mpv手册中的选项类别进行了重组，新增libplacebo选项

✔️支持MPVNET_HOME环境变量，允许自定义配置目录位置

✔️支持编码模式和thumbfast   

❌移除蓝色mpv.net徽标，以提高OSC兼容性

❌由于与.NET 6平台的兼容性问题，移除.NET查看面板。其他用户脚本也有类似的功能：
  - [command_palette](https://github.com/stax76/mpv-scripts#command_palette)
  - [search_menu](https://github.com/stax76/mpv-scripts#search_menu)


## 📺 界面预览 [🔝](#目录)
### 🔘osc显示预览
![预览图01](https://user-images.githubusercontent.com/125502871/220343125-74366dd1-af9e-41a3-81e4-f1f06328a881.jpg)

### 🔘srt字幕预览
![预览图02](https://user-images.githubusercontent.com/125502871/224633362-a53dc9d0-aee6-4802-8213-daa1383febc9.jpg)

### 🔘右键菜单
![预览图03](https://user-images.githubusercontent.com/125502871/221334160-ce3310fa-b8bb-4258-a76c-992cd1467f39.jpg)
![预览图04](https://user-images.githubusercontent.com/125502871/230541653-8119588e-d46c-4f2e-9eef-9c1cf917db3f.jpg)
![预览图05](https://user-images.githubusercontent.com/125502871/222751934-2ffc0619-381e-454c-93c7-82fd8b005300.jpg)

### 🔘设置选项
<details>
<summary>- ❣️X.0 (mpv.net v6.0)版❣️界面</summary>
	
![预览图06](https://user-images.githubusercontent.com/125502871/220125827-6a33ee6d-14a9-40fa-ae0c-733f6760f7b4.jpg)
![预览图07](https://user-images.githubusercontent.com/125502871/220144657-50817726-37f3-41c5-87be-9e50ca5a4cca.jpg)
![预览图08](https://user-images.githubusercontent.com/125502871/224598109-f36722d6-ebca-4d44-9113-63fadb079be3.jpg)
</details>
<details>
<summary>- ❣️202X.0 (mpv.net v7.0+)版❣️界面</summary>

![预览图09](https://raw.githubusercontent.com/mpvnet-player/mpv.net/main/docs/img/ConfEditor.webp)

</details>

## 📖 使用说明 [🔝](#目录)
### 🔘OSC
> [!NOTE]
> 单击鼠标中键与Shift + 鼠标左键功能相同，可单手操作

✳️进度条  
- 鼠标左键：定位到选定的位置
- 鼠标右键：定位到所选章节的开头

✳️播放列表的后退/前进按钮  
- 鼠标左键：播放上一个/下一个文件
- 鼠标右键：显示播放列表
- Shift + 鼠标左键：播放上一个/下一个文件并显示播放列表
- Shift + 鼠标右键：显示播放列表

✳️章节的后退/前进按钮  
- 鼠标左键：转到上一章/下一章
- 鼠标右键：显示章节列表
- Shift + 鼠标左键：转到上一/下一章并显示播放列表
- Shift + 鼠标右键：显示章节列表

✳️向前/向后跳转按钮  
- 鼠标左键： 向前/向后跳转5秒
- 鼠标右键： 向前/向后跳转1分钟
- Shift + 鼠标左键：跳到上一帧/下一帧

✳️音频/字幕按钮  
- 鼠标左键/鼠标右键：循环到下一个/上一个轨道
- Shift + 鼠标左键：循环到下一个/上一个轨道并显示轨道列表
- Shift + 鼠标右键：显示轨道列表

✳️音量按钮  
- 鼠标左键：静音/取消静音
- 鼠标滚轮：改变音量

✳️置顶按钮  
- 鼠标左键：切换置顶，并取消边框
- 鼠标右键：切换置顶，不更改边框

✳️播放时间（左侧）  
- 鼠标左键：切换为以毫秒显示时间

✳️持续时间（右侧）  
- 鼠标左键：切换为剩余时间，而不是总时间

### 🔘右键菜单
> [!NOTE]
> 右键菜单中的所有选择仅限当前窗口，窗口关闭后不会保存设置，如需保存个人设置，请前往`mpv设置`界面更改

✳️超分滤镜
- 2x_AnimeJaNai V2L1要求显卡RTX3060及以上；HFA2kCompact要求显卡RTX4080及以上

### 🔘设置选项
> [!NOTE]
> 打开右键菜单`mpv设置`即为`mpv.conf编辑器`，可保存个人设置

✳️自动挂载滤镜/着色器

- ❣️X.0 (mpv.net v6.0)版❣️在`mpv.conf编辑器`的`高级`列表里，将其路径填入`vf`（视频滤镜）或`glsl-shaders`（着色器）的选项框中
  > 示例：   
  > - 在`vf`选项框中填入
  > ```editorconfig
  > vapoursynth=~~/vs/MEMC_MVT_STD.vpy
  > ```
  > ```editorconfig
  > fps=fps=60/1.001
  > ```
  > - 在`glsl-shaders`选项框中填入
  > ```editorconfig
  > ~~/shaders/Anime4K_Clamp_Highlights.glsl;~~/shaders/Anime4K_Restore_CNN_M.glsl
  > ```
- ❣️202X.0 (mpv.net v7.0+)版❣️在`portable_config`文件夹中的`mpv.conf`，写入vf（视频滤镜）或glsl-shaders（着色器）的路径
  > 示例：
  > ```editorconfig
  > vf='vapoursynth=~~/vs/MEMC_MVT_STD.vpy'
  > ```
  > ```editorconfig
  > vf='fps=fps=60/1.001'
  > ```
  > ```editorconfig
  > glsl-shaders='~~/shaders/Anime4K_Clamp_Highlights.glsl;~~/shaders/Anime4K_Restore_CNN_M.glsl'
  > ```

### 🔘进阶说明
✳️mpv-player
- [mpv官方手册](https://mpv.io/manual/stable)
- [mpv官方手册汉化版](https://hooke007.github.io/#mpv)
- [mpv非官方使用引导](https://hooke007.github.io/#id1)

✳️其他组件
- [VS滤镜介绍](https://github.com/hooke007/MPV_lazy/wiki/3_K7sfunc)
- [第三方着色器介绍](https://hooke007.github.io/unofficial/mpv_shaders.html)
- [osc-ModernX配置说明](https://github.com/zydezu/ModernX#configuration)

## 🧰 更新 [🔝](#目录)
> 更新频率：半年更

- mpv-dev-x86_64-v3 [<shinchiro_20240121>](https://sourceforge.net/projects/mpv-player-windows/files/libmpv/)
- mpv.net_CM [git_mpvnet-CM-obs](https://github.com/hooke007/mpv.net_CM/releases)/mpv.net [git_v7.1.0.0](https://github.com/mpvnet-player/mpv.net/releases)
- MediaInfo [<v23.11>](https://mediaarea.net/en/MediaInfo/Download/Windows)
- yt-dlp [<git_2023.12.30>](https://github.com/yt-dlp/yt-dlp/releases)
- ModernX [<git_v0.2.5>](https://github.com/zydezu/ModernX/releases)
- Thumbfast [<git_20231209>](https://github.com/po5/thumbfast)
- Python-embed-amd64 [<3.11.7>](https://www.python.org/downloads)
- VapourSynth-portable [<git_R_65>](https://github.com/vapoursynth/vapoursynth/releases)
- 视频滤镜与着色器 [<git_mpv-lazy-2024V0>](https://github.com/hooke007/MPV_lazy/releases)

## 🔑 安装 [🔝](#目录)
> [!IMPORTANT]
> 不可使用覆盖旧版文件的形式进行更新，请提前做好个人配置的备份

> [!NOTE]
> ❣️X.0 (mpv.net v6.0)版❣️下载安装[.NET framework 4.8](https://support.microsoft.com/zh-cn/topic/%E9%80%82%E7%94%A8%E4%BA%8E-windows-%E7%9A%84-microsoft-net-framework-4-8-%E8%84%B1%E6%9C%BA%E5%AE%89%E8%A3%85%E7%A8%8B%E5%BA%8F-9d23f658-3b97-68ab-d013-aa3c3e7495e0)运行库（win10 1903及之后版本的系统内已集成）   
> ❣️202X.0 (mpv.net v7.0+)版❣️下载安装[.NET Desktop Runtime 6.0](https://dotnet.microsoft.com/zh-cn/download/dotnet/6.0)运行库
- 删除原有mpv.net-DW
- 下载安装最新版mpv.net-DW
- 运行mpvnet.exe

## 🔗 下载 [🔝](#目录)
见网页端右侧Releases或移动端下方Releases

## 💌 鸣谢 [🔝](#目录)
- mpvnet-player/mpv.net
- hooke007/mpv.net_CM, MPV_lazy
- zydezu/ModernX
- po5/thumbfast
- tsl0922/ImPlay