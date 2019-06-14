<p align="center">
	<img src="https://github.com/RocketRobz/GodMode9i/blob/master/resources/logo2_small.png"><br>
	<b>A full access file browser for the DS and DSi consoles :godmode:</b>
</p>

<p align="center">
	<a href="https://dev.azure.com/DS-Homebrew/Builds/_build?definitionId=14" style="padding-right: 5px;">
		<img src="https://dev.azure.com/DS-Homebrew/Builds/_apis/build/status/RocketRobz.GodMode9i?branchName=master" height="20">
	</a>
	<a href="https://discord.gg/yqSut8c" style="padding-left: 5px; padding-right: 5px;">
		<img src="https://img.shields.io/badge/Discord-Server-blue.svg" height="20">
	</a>
	<a href="https://gbatemp.net/threads/release-godmode9i-all-access-file-browser-for-the-ds-i-and-3ds.520096/" style="padding-left: 5px;">
		<img src="https://img.shields.io/badge/GBATemp-thread-blue.svg" height="20">
	</a>
</p>

GodMode9i is a full access file browser for the Nintendo DS, Nintendo DSi and the Nintendo 3DS library of consoles. It works on any console that either supports a flashcard or has Custom Firmware.

## Features

- Dump GameBoy Advance cartridges on the original Nintendo DS and Nintendo DS Lite consoles.
- Copy, move, delete, rename files/folders and create folders.
- Mount the NitroFS for .nds files (allowing you to browse through them), including retail nds files.
- View files on supported flashcards when running GM9i from the SD Card. (`AceKard 2(i)` `R4 Ultra (r4ultra.com)`)

## Building
If you don't want to compile yourself but you still want to get the latest build, please use our [TWLBot github repository](https://github.com/TWLBot/Builds/blob/master/extras/GodMode9i.7z)

In order to compile this application on your own, you will need [devkitPro](https://devkitpro.org/) with the devkitARM toolchain, plus the necessary tools and libraries. devkitPro includes `dkp-pacman` for easy installation of all components:

```
 $ dkp-pacman -Syu devkitARM general-tools dstools ndstool libnds libfat-nds
```

Once everything is downloaded and installed, `git clone` this repository, navigate to the folder in which it was cloned, and run `make` to compile the application. If there is an error, let us know.

## Screenshots

![](https://gbatemp.b-cdn.net/attachments/snap_212809-png.147117/)![](https://gbatemp.b-cdn.net/attachments/snap_211051-png.147114/)![](https://gbatemp.b-cdn.net/attachments/file-options-v1-3-0-no-border-png.147118/)

## Credits
* RocketRobz: Creator of GodMode9i.
* zacchi4k: Creator of the GodMode9i logo used in v1.3.1 and onwards.
* devkitPro/WinterMute: devkitARM, libnds, original nds-hb-menu code, and screenshot code.
* d0k3: Original GM9 app and name for the Nintendo 3DS, which this is inspired by.
