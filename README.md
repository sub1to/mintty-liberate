# mintty-liberate

I built this proxy DLL, because on Windows (10) all the git-bash windows stick together on the taskbar. You can move them as a group, but not individual windows, which also means you can't re-order your windows on the taskbar.

This behavior is caused by all instances of `mintty` sharing the same `AppID`. This project forces unique IDs for every instance, allowing them to be managed individually.

## How it Works

The DLL masquerades as `winmm.dll` and proxies all API calls to the real system `winmm.dll`.

1. **Injection:** When `DllMain` is called with `DLL_PROCESS_ATTACH`, the payload executes.
2. **Safety Check:** It checks if the current process is `mintty.exe`. If not, it does nothing.
3. **Hijacking:** Inside `mintty.exe`, the DLL hijacks the entrypoint by setting a debug breakpoint and adding a Vectored Exception Handler (VEH).
4. **AppID Modification:**
* From the VEH, it generates a random UUID.
* It hijacks `GetProcAddress` to intercept `SetCurrentProcessExplicitAppUserModelID`.
* It calls `SetCurrentProcessExplicitAppUserModelID` immediately with the random UUID to decouple the window from the group.


## Prerequisites

This solution requires you to bypass the standard `git-bash.exe` wrapper and launch `mintty` directly.

* **Tested Environment:** Windows 10 IoT Enterprise 21H2
* **Mintty Version:** 3.8.1


## Installation & Usage

### 1. Build the Project

1. Run `build.bat`.
2. Navigate to `.projects/vs2022` and open `mintty_librate.sln`.
3. Build the **Release** version.

### 2. Install the DLL

1. Locate the generated `winmm.dll`.
2. Drop it into the directory containing `mintty.exe` (usually `C:\Program Files\Git\usr\bin\`).
3. Test that it works by starting `mintty.exe` directly.

### 3. Update Shortcuts

The standard `git-bash.exe` launcher will still glue windows together. You must create a shortcut that starts `mintty` directly using `/usr/bin/env`.

Create a new shortcut with the following properties:

* **Target:**
```text
"C:\Program Files\Git\usr\bin\mintty.exe" -e /usr/bin/env.exe MSYSTEM=MINGW64 /usr/bin/bash.exe --login
```


* **Start in:**
```text
%HOMEDRIVE%%HOMEPATH%
```

### 4. Update Context Menu ("Git Bash Here")

To make the right-click context menu work with separate windows, you need to update the registry command keys.

**For Directories:**
Navigate to: `Computer\HKEY_CLASSES_ROOT\Directory\shell\git_shell\command`
Set the value to:

```text
"C:\Program Files\Git\usr\bin\mintty.exe" --dir "%1" -e /usr/bin/env.exe MSYSTEM=MINGW64 /usr/bin/bash.exe --login
```

**For Backgrounds:**
Navigate to: `Computer\HKEY_CLASSES_ROOT\Directory\background\shell\git_shell\command`
Set the value to:

```text
"C:\Program Files\Git\usr\bin\mintty.exe" --dir "%V" -e /usr/bin/env.exe MSYSTEM=MINGW64 /usr/bin/bash.exe --login
```



## License

MIT