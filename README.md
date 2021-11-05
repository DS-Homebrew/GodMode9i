<p align="center">
	<img src="https://github.com/DS-Homebrew/GodMode9i/raw/master/resources/logo2_small.png"><br>
	<b>A full access file browser for the DS and DSi consoles :godmode:</b>
	<br>
	<a href="https://dev.azure.com/DS-Homebrew/Builds/_build?definitionId=14" style="padding-right: 5px;">
		<img src="https://dev.azure.com/DS-Homebrew/Builds/_apis/build/status/RocketRobz.GodMode9i?branchName=master" height="20">
	</a>
	<a href="https://discord.gg/yD3spjv" style="padding-left: 5px; padding-right: 5px;">
		<img src="https://img.shields.io/badge/Discord%20Server-%23GodMode9i-green.svg">
	</a>
	<a href="https://gbatemp.net/threads/release-godmode9i-all-access-file-browser-for-the-ds-i-and-3ds.520096/" style="padding-left: 5px;">
		<img src="https://img.shields.io/badge/GBAtemp-thread-blue.svg" height="20">
	</a>
</p>

GodMode9i is a full access file browser for the Nintendo DS, Nintendo DSi and the Nintendo 3DS's TWL_FIRM.

<div align="center">
	<img src="https://github.com/DS-Homebrew/GodMode9i/raw/master/resources/screenshots/drive-menu.png" alt="Drive menu">
	<img src="https://github.com/DS-Homebrew/GodMode9i/raw/master/resources/screenshots/file-list.png" alt="File list">
	<img src="https://github.com/DS-Homebrew/GodMode9i/raw/master/resources/screenshots/nds-file-menu.png" alt="NDS file menu">
</div>

## Features

- Dump GameBoy Advance cartridges on the original Nintendo DS and Nintendo DS Lite consoles.
- Dump Nintendo DS/DSi cartridges on Nintendo DSi and Nintendo 3DS consoles (if GodMode9i is ran on the console SD card).
     - They can also be dumped on the original Nintendo DS and Nintendo DS Lite consoles, if running from a Slot-2 flashcard.
- Copy, move, delete, rename files/folders and create folders.
- Mount the NitroFS of .nds files.
- Browse files on supported flashcards when running GM9i from the NAND or SD Card. (`AceKard 2(i)` & `R4 Ultra (r4ultra.com)`)
- Browse files on the SD Card when running GM9i from any DS-mode flashcard. (Requires a DS game launcher with SD access enabled.)

## Building
If you don't want to compile yourself but you still want to get the latest build, please use our [TWLBot github repository](https://github.com/TWLBot/Builds/blob/master/extras/GodMode9i.7z)

In order to compile this application on your own, you will need [devkitPro](https://devkitpro.org/) with the devkitARM toolchain, plus the necessary tools and libraries. devkitPro includes `dkp-pacman` for easy installation of all components:

```
 $ dkp-pacman -Syu devkitARM general-tools dstools ndstool libnds libfat-nds
```

Once everything is downloaded and installed, `git clone` this repository, navigate to the folder in which it was cloned, and run `make` to compile the application. If there is an error, let us know.

## Custom Fonts
GodMode9i uses the same FRF font files as [GodMode9](https://github.com/d0k3/GodMode9). To create an FRF font use [GodMode9's Python script](https://github.com/d0k3/GodMode9/blob/master/utils/fontriff.py) or you can find some in the `resources/fonts` folder in this repository.

When loading GodMode9i will try to load `/gm9i/font.frf` on your SD card and if that fails will load the default font. To change the default font when building GodMode9i, replace `data/font_default.frf` with your font.

## Credits
* [RocketRobz](https://github.com/RocketRobz): Main Developer.
* [Evie/Pk11](https://github.com/Epicpkmn11): Contributor.
* [zacchi4k](https://github.com/zacchi4k): Logo designer.
* [Edo9300](https://github.com/edo9300): Save reading code from his save manager tool.
* [endrift](https://github.com/endrift): GBA ROM dumping code from [duplo](https://github.com/endrift/duplo), used for 64MB ROMs.
* [JimmyZ](https://github.com/JimmyZ): NAND code from twlnf (with writing code stripped for safety reasons).
* [zoogie](https://github.com/zoogie): ConsoleID code (originating from dumpTool).
* [devkitPro](https://github.com/devkitPro): devkitARM, libnds, original nds-hb-menu code, and screenshot code.
* [d0k3](https://github.com/d0k3): Developer of GodMode9 for the Nintendo 3DS, which this is inspired by.
