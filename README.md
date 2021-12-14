# [initrdg](https://github.com/Biswa96/initrdg)

initrd binary to experiment with WSLg in Windows 10

:warning: **Disclaimer:** At time of this writing, [WSLg] (GUI support in WSL)
is not available in Windows 10. This project is just to experiment with that.
This is not a replacement of actual WSLg shipped in Windows 11. Please use this
only for fun and experimental purposes. Also backup your WSL setup before using
this program. This may totally destroy your current WSL environment.

[WSLg]: https://github.com/microsoft/wslg

## Table of contents

* [Assumptions](#assumptions)
* [Preparation](#preparation)
* [How to use](#how-to-use)
* [Differences with initrd](#differences-with-initrd)
* [Caveats](#caveats)
* [Acknowledgments](#acknowledgments)
* [License](#license)

## Assumptions

This program assumes some certain aspects by default which are as following:

* WSL msix is extracted at `C:\WSL` path. Feel free to change the folder path.
But you also have to change the path in [SystemDistro.reg](SystemDistro.reg)
file and the environment variable listed in [how to use](#how-to-use) section.

* This project has been tested in Windows 10 21H1 Version 10.0.19043.928.
The messages between initrd and Lxss service are varied with Windows versions.
Feel free to modify this project for your own Windows version.

* Every binaries are compiled in Alpine Linux and with musl libc & gcc.
Feel free to use any compiler toolchain and libc.

## Preparation

:warning: **Warning:** The following steps requires read/write permission in
System32 folder. Any type of wrong step may make your entire WSL setup or even
the operating system unusable.

* Download WSL msixbundle package from [WSL Release page]. In that msixbundle,
extract the ARM64 or X64 msix according to your CPU architecture. Extract the
msix in `C:\WSL` path (default) or at your choice (need modification). Use any
program that can extract from ZIP format e.g. unzip, 7zip etc.

[WSL Release page]: https://github.com/microsoft/WSL/releases

* In `C:\WSL` directory, remove the following two lines from wslg.rdp file.
This enables full desktop mode instead of separate application window mode.

```
remoteapplicationmode:i:1
remoteapplicationprogram:s:dummy-entry
```

* In `C:\WSL` directory, convert the system.vhd to ext4.vhdx file. Because Lxss
service expects the WSL distribution in exactly ext4.vhdx named file. Use this
command in Powershell.

```
Convert-VHD -Path "system.vhd" -DestinationPath "ext4.vhdx"
```

* Double click on the [SystemDistro.reg](SystemDistro.reg) file to register
the ext4.vhdx file with "system" named distribution in WSL2 environment.
Make sure there is no distribution with "system" name before that.

* Backup and Rename the initrd.img file in `C:\Windows\System32\lxss\tools`
directory to something else. Clone this repository. Run `make` command in any
GNU/Linux distribution to compile the initrd.img file. Copy the newly compiled
initrd.img file to that `C:\Windows\System32\lxss\tools` directory.

* Check out Win10 branch in [my fork of wslg](https://github.com/Biswa96/wslg).
Run `make` command in WSLGd directory to compile WSLGd static executable.
Remember the full path of that for later use.

## How to use

* Run your preferred GNU/Linux distribution in WSL2.

* Execute `wsl.exe -d system` to run the newly registered "system" distribution.

* Run the following commands in both your running distribution and "system"
distribution as well. This step may not required in future, just thoughts.
The `<guid>` is retrieved from `hcsdiag.exe list` command output.

```sh
export WAYLAND_DISPLAY=wayland-0
export DISPLAY=:0
export XDG_RUNTIME_DIR=/mnt/wsl/runtime-dir
export PULSE_SERVER=/mnt/wsl/PulseServer
export WSL2_INSTALL_PATH="C:\\WSL"
export WSL2_WESTON_SHELL_OVERRIDE=desktop-shell
export WSL2_VM_ID=<guid>
```

* Run previously compiled WSLGd binary in "system" distribution. The WSLGd will
invoke `C:\WSL\msrdc.exe` executable and show a warning window to accept network
connections. Click on "Connect" button to allow it.

* Now run any GUI program in your distribution :tada:

## Differences with initrd

This project is not an replacement of initrd binary which already exists in
Windows 10 and 11 systems. This does NOT do:

* Import or export distributions in WSL2.
* Convert distributions from WSL1 to WSL2 or vice-versa.
* Create or attach swap file.
* Execute localhost or telemetry processes.
* Compact memory.

## Caveats

* Every GUI applications are in one big window. Because Wayland apps become
unresponsive in rdprail-shell which is used by-default to show apps in separate
windows.

## Acknowledgments

Everyone in the discussions:

* [wslg#82](https://github.com/microsoft/wslg/discussions/82)
* [wslg#347](https://github.com/microsoft/wslg/issues/347)
* [wslg#441](https://github.com/microsoft/wslg/issues/414)
* [wslg#446](https://github.com/microsoft/wslg/issues/446)

Free and Open source projects:

* [musl libc](https://git.musl-libc.org/cgit/musl)
* [ArchLinux's mkinitcpio](https://github.com/archlinux/mkinitcpio)

## License

This project is licensed under GNU General Public License version 3 or any later
version at your option. See [LICENSE](LICENSE) file for further details.
