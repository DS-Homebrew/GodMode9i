<p align="center">
	<img src="https://github.com/RocketRobz/GodMode9i/blob/master/resources/logo2_small.png"><br>
	<b>A full access file browser for the DS and DSi consoles :godmode:</b>
	<br>
	<a href="https://dev.azure.com/DS-Homebrew/Builds/_build?definitionId=14" style="padding-right: 5px;">
		<img src="https://dev.azure.com/DS-Homebrew/Builds/_apis/build/status/RocketRobz.GodMode9i?branchName=master" height="20">
	</a>
	<a href="https://discord.gg/yqSut8c" style="padding-left: 5px; padding-right: 5px;">
		<img src="https://img.shields.io/badge/Discord%20Server-%23GodMode9i-green.svg">
	</a>
	<a href="https://gbatemp.net/threads/release-godmode9i-all-access-file-browser-for-the-ds-i-and-3ds.520096/" style="padding-left: 5px;">
		<img src="https://img.shields.io/badge/GBATemp-thread-blue.svg" height="20">
	</a>
</p>

GodMode9i is a full access file browser for the Nintendo DS, Nintendo DSi and the Nintendo 3DS's TWL_FIRM.

![](https://gbatemp.b-cdn.net/attachments/snap_191051-png.195366/) ![](https://gbatemp.b-cdn.net/attachments/snap_191132-png.195368/) ![](https://gbatemp.b-cdn.net/attachments/file-options-v2-1-0-png.195367/)

## Features

- Dump GameBoy Advance cartridges on the original Nintendo DS and Nintendo DS Lite consoles.
- Dump Nintendo DS/DSi cartridges on Nintendo DSi and Nintendo 3DS consoles (if GodMode9i is ran on the console SD card).
- Copy, move, delete, rename files/folders and create folders.
- Mount the NitroFS of .nds files.
- Browse files on supported flashcards when running GM9i from the NAND or SD Card. (`AceKard 2(i)` & `R4 Ultra (r4ultra.com)`)

## Building
If you don't want to compile yourself but you still want to get the latest build, please use our [TWLBot github repository](https://github.com/TWLBot/Builds/blob/master/extras/GodMode9i.7z)

In order to compile this application on your own, you will need [devkitPro](https://devkitpro.org/) with the devkitARM toolchain, plus the necessary tools and libraries. devkitPro includes `dkp-pacman` for easy installation of all components:

```
 $ dkp-pacman -Syu devkitARM general-tools dstools ndstool libnds libfat-nds
```

Once everything is downloaded and installed, `git clone` this repository, navigate to the folder in which it was cloned, and run `make` to compile the application. If there is an error, let us know.

## Credits
* [RocketRobz](https://github.com/RocketRobz): Main Developer.
* [zacchi4k](https://github.com/zacchi4k): Logo designer.
* [Edo9300](https://github.com/edo9300): Save reading code from his save manager tool.
* [JimmyZ](https://github.com/JimmyZ): NAND code from twlnf (with writing code stripped for safety reasons).
* [zoogie](https://github.com/zoogie): ConsoleID code (originating from dumpTool).
* [devkitPro](https://github.com/devkitPro): devkitARM, libnds, original nds-hb-menu code, and screenshot code.
* [d0k3](https://github.com/d0k3): Developer of GodMode9 for the Nintendo 3DS, which this is inspired by.
