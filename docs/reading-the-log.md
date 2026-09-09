# Reading the log

## Reading the log

Everything the plugin does is written to `plugins/texoverride.log`. The file starts fresh on every
launch, and the previous session's log is kept next to it as `texoverride.log.old`, so if the game
crashed, the log from the crashed session is still there.

Every line has the same shape:

```
[19:04:22] [INFO] [CLAIM] REDIRECT mp_m_freemode_01/uppr_012_r.ydd -> tex_overrides/...
```

The level is `INFO`, `WARN` or `ERROR`. If something did not work, search the file for `WARN` and
`ERROR` first, because those two carry the reason. The category says which part of the plugin
spoke: `CORE`, `SCAN`, `COLLECTION`, `AUDIT`, `CLAIM`, `VERIFY`, `LIVE`, `TATTOO` or `UPDATE`.

There is a fourth level, `DEBUG`, which is always on. It adds internal detail that is mostly
useful when someone is helping you work out a problem, and it is there from the first launch so
nobody has to ask you to turn something on and send the log again.

| Line | What it means |
|---|---|
| `texoverride x.y.z ...` | The plugin is in and running |
| `Loaded N override(s) in Ns` | Your files were found |
| indented `Collections` and `Root Assets` lines | How your files were grouped |
| `Pack cost when fully loaded: ...` | What your files cost the game in memory |
| `HEAVY x MB file` | That file is oversized; shrink it to avoid texture loss |
| `HUGE file - x MB` | Over 32 MB; it is loaded, but shrink it first if you start crashing |
| `UNREADABLE file` | The file could not be opened, so it was not loaded |
| `SKIP file` | The name does not fit any rule; the reason is on the line |
| `PREFERRED slot - a takes the slot from b` | A file in an `_override` folder won over another copy |
| `DUPLICATE slot - ...` | Two folders hold the same file; this copy was not used |
| `IGNORED file` | Not a type the game can be handed this way; the reason is on the line |
| `CRASH SAVER: ...` | Last run died on a file; it is skipped this launch so you can get in |
| `QUARANTINED file` | Skipped after a crash; delete `_quarantine.txt` to try it again |
| `Texture budget: Sized to this PC ...` | The texture budget was raised to fit your card |
| `Texture budget: a -> b GB` | The raise was written into the game |
| `Loaded placement file: ...` | Your edited `.xml` was read |
| `... layout solved` | The `.xml` matched the game; changes can be applied |
| `Streaming manager @ ...` | Internal: found what it needs to keep overrides in place |
| `registerRawStreamingFile @ ...` | Internal: found the function it works through |
| `MH_EnableHook: MH_OK` | Internal: ready |
| `OVERRIDE-REG: slot <- file` | Your file took over that item |
| `OVERRIDE-TAKEOVER: slot <- file` | The slot already existed, so its handle was replaced |
| `OVERRIDE-WAIT: slot <- file` | The file is ready and will bind when its target slot appears |
| `OVERRIDE-FAILED: slot <- file` | Registration failed and produced no usable entry |
| `LATE-BIND: slot ...` | A previously missing target appeared and was attached |
| `RECLAIM: slot (old -> ours)` | The game tried to take an item back; the plugin re-took it |
| `LOADED: slot from the GAME file` | The game read its own copy of that file before the plugin held the slot. Yours shows once the game reloads it, or after a restart |
| `MOVED: slot no longer lives at id=...` | The game freed that slot and reused the index; the plugin follows the name instead of writing into it |
| `REDIRECT name -> file` | A server file was swapped for yours |
| `PLACEMENT: ...` | A tattoo position change was applied |
| `LIVE-ADD` / `LIVE-TAKEOVER` / `LIVE-UPDATE` | A file you changed while playing was picked up |
| `Server collection: name kind [tag]` | A collection the server uses, what it is, and whether it is reachable |
| `Server file: name [overridable...]` | A loose file the server streams that you can replace |
| `Other server files ... counted, not listed` | How many streamed files the plugin can never touch |
| `Update available` / `Plugin is up to date` | Whether you have the newest version |
| `Heartbeat (beat N) ...` | The plugin is still running |
| `pattern NOT FOUND` | The game updated; the plugin needs an update |

The three tags on a `Server collection` line mean:

- `overridable`, make a folder with that name and your files will be used.
- `depends on the file names inside`, the collection itself is fine, but each file still has to be
  named the way GTA names ped parts.
- `OTHER - never touched`, a story or ambient character. The plugin refuses these on purpose.

Collections are always listed, refused ones included, because that list is how new character names
get found. Loose files are listed too, every one of them, so you can find the exact name of one
particular server prop. Everything else the server streams (car parts, map pieces, often tens of
thousands of files) is written at `DEBUG` level under `OTHER - never touched`, and the heartbeat
line carries the count.

[Back to the README](../README.md)
