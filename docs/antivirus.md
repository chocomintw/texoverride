# Why your antivirus may call it a trojan

## Why your antivirus may call it a trojan

It happens, and the honest answer is that the plugin does the things antivirus software watches
for. Not by accident, and not hidden: it is what a game mod that changes what the game draws has
to do.

- It writes five bytes into the running game to redirect one function. That is the same technique
  every trainer, overlay and mod loader uses, and scanners class it as code injection.
- It allocates a small piece of memory that is both writable and executable, to hold the original
  copy of that function. Generic detections weigh this heavily on its own.
- It scans the game's memory for byte patterns to find the functions it needs.
- It is an unsigned file, loaded into another program, that almost nobody has run yet. Microsoft
  Defender scores new unsigned files partly on how many people have seen them, so a fresh release
  starts with a bad score no matter what is in it.

Names like `Wacatac`, `Injector`, `HackTool` or `Trojan:Win32/Wacatac.B!ml` mean a heuristic fired,
not that something was found. The `!ml` on the end literally means a machine learning guess.

What you can do:

- Check it yourself. Upload the file to [VirusTotal](https://www.virustotal.com). A handful of
  engines flagging it while the majority do not is what a false positive looks like.
- Compare the file. Every release is built by GitHub Actions from the source in this repository,
  and the release notes list the SHA-256 of the file so you can check the one you downloaded is
  the one that was built. You do not have to take that on trust either. Each release is signed
  with build provenance, so with [GitHub CLI](https://cli.github.com) installed you can ask for
  proof that this exact file came out of this repository:

  ```
  gh attestation verify texoverride.asi --repo blancodagoat/texoverride
  ```

  If someone hands you a `texoverride.asi` from anywhere else and that command fails, do not run
  it. That is the check worth doing, because a tampered copy is the one real risk here.
- Build it yourself. `build.bat` needs only the free Visual Studio Build Tools. Then the file on
  your disk is one you made.
- Report it. If Defender flagged it, submitting it at
  [Microsoft's false positive form](https://www.microsoft.com/en-us/wdsi/filesubmission) usually
  gets it cleared within a few days, for everyone.
- Add an exclusion for your FiveM `plugins` folder, if you are comfortable doing that and you
  trust where you got the file.

## A new release can score worse than the old one, with the same code in it

Numbers from 2026-09-13, both files scanned on VirusTotal seventeen minutes apart:

| | 0.8.27 | 0.8.26 |
|---|---|---|
| total | 4 of 71 engines | 2 of 71 engines |
| Microsoft Defender | `Trojan:Win32/Wacatac.C!ml` | did not flag it |
| Bkav | `W32.Malware.C8C9EC41` | `W32.Malware.BD7E04A6` |
| Cynet | `Malicious (score: 100)` | `Malicious (score: 100)` |
| Trapmine | `suspicious.low.ml.score` | did not flag it |

Those two files are the same plugin. Set the version number in 0.8.27's source back to 0.8.26,
build it, and you get the file 0.8.26 shipped, hash for hash. The only difference between them is
six characters of version text, so nothing in the file explains why one was called a trojan and
the other was not.

The detection names give it away. Anything ending in `!ml` or `.ml.score`, and any bare score, is
a machine learning guess rather than a match on known code. Part of that guess is how many
machines have already seen the exact file, and a release that came out yesterday has been seen by
almost nobody. Every new version starts that count at zero, which is why the build you have been
running looks clean while the one that just auto-updated does not.

So an older release will usually score better than a newer one, and that is about the age of the
file rather than what is in it. It also means comparing two releases on VirusTotal tells you
nothing about which one is safer, unless the code between them changed.

## What the scanners object to

You can check the whole list yourself with `dumpbin /imports texoverride.asi`. These are the
Windows functions the plugin calls, and what an antivirus tends to read them as:

| Windows functions used | what that looks like |
|---|---|
| `VirtualAlloc`, `VirtualProtect`, `FlushInstructionCache` | allocating executable memory and rewriting code in a running program |
| ten `WinHttp*` calls, `BCrypt*` hashing, `ShellExecuteA` | download a file, hash it, run it |
| `GetAsyncKeyState`, `GetForegroundWindow` | reading the keyboard and checking which window is in front |
| `LoadLibraryA`, `GetProcAddress`, `GetModuleHandle*` | looking up Windows functions at runtime instead of declaring them |

Every row has a plain reason behind it. The first is the hook, which is the entire point of the
plugin. The second is the update check, which fetches the release list over HTTPS, checks the
SHA-256 of what it downloaded, and asks Windows to run the new file. The third is the screenshot
key for `hide_overlay`, plus the test that FiveM is the window in front so that keys are ignored
while you are in another program. The fourth is how FiveM's own exports get found, because a
normal import on a FiveM DLL would turn a renamed export into a plugin that refuses to load at
all.

Individually none of that is unusual. Together it is close to the shape of a program that
downloads something and watches the keyboard, and a scanner trained on that shape scores the
combination.

Two things that could be on that list are not:

- The plugin never calls game natives, so it does not touch the script engine.
- It does not enumerate or suspend threads. MinHook normally freezes every other thread while it
  writes the patch, through `CreateToolhelp32Snapshot`, `SuspendThread` and `SetThreadContext`,
  which is one of the loudest code injection signatures a scanner looks for. That code never ran
  under FiveM, which blocks the thread snapshot, and it was never needed: the patch happens before
  the game's entry point, so no thread is running the code being changed. Version 0.8.28 deleted
  it from `minhook/src/hook.c` instead of leaving it unused, so those six functions are not in the
  file at all.

**If FiveM shows "Couldn't load texoverride.asi" and there is no `texoverride.log` next to the
file**, something refused the file before any of the plugin's own code ran. Two things have
caused that so far: Smart App Control, and McAfee. Both block unsigned files silently, neither
shows anything in Windows Security, and the file looks perfectly fine. Work through
[the four checks in the README](../README.md#it-says-couldnt-load-texoverrideasi), which name
both and end with a test that reports the actual Windows error code.

Windows Security settings and the plugin, if you want to check yours:

- Keep on: real-time protection, cloud-delivered protection, firewall, reputation-based
  protection (PUA blocking and SmartScreen), core isolation. None of these stop the plugin. At
  worst one flags a fresh download; allow it in Protection history.
- Must stay off: Smart App Control. It blocks every unsigned file, this one included. Once it is
  off it cannot be turned back on without reinstalling Windows.
- Leave at defaults: Exploit protection program settings (an entry for FiveM with Code integrity
  guard or Arbitrary code guard would block the plugin) and Controlled folder access (if it is on
  and the log never appears, allow FiveM there).
- Do not run a second antivirus next to Defender. McAfee, Norton, Avast, AVG and Kaspersky trials
  switch Defender off and can veto the plugin loading with no notice, which is the case above.

To find out what is blocking it when nothing admits to it, use Process Monitor:

1. Download Procmon from Microsoft (search "Sysinternals Process Monitor") and run it as
   administrator. Close FiveM first.
2. In Procmon press Ctrl+L, add the filter `Path` `contains` `texoverride.asi`, click Add, OK.
3. Launch FiveM and wait for the "Couldn't load" dialog. Go back to Procmon.
4. Read the Result column. `SUCCESS` all the way down and still no log means a driver vetoed the
   load; look at the Process Name column for anything that is not FiveM (an antivirus service
   touching the file right before the failure is the answer). `ACCESS DENIED` names the blocker
   directly. `NAME NOT FOUND` means FiveM is looking in a different plugins folder than the one
   you put the file in.
5. To keep the evidence: File > Save, PML format, and attach it to a GitHub issue.

What this project will not do is obfuscate, pack, or otherwise dress the file up to slip past
scanners. That is what actual malware does, it makes detections worse rather than better, and it
would destroy the one thing that makes a mod like this trustworthy: that you can read every line
of what it does.

[Back to the README](../README.md)
