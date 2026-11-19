# Installing Alpha Monster

## macOS (10.15 or later, Apple Silicon and Intel)

1. Download `AlphaMonster-x.y.z-macOS.pkg`.
2. The package is not signed with an Apple certificate, so macOS blocks it the first time. To open it:
   - **right-click → Open** the `.pkg` file and confirm;
   - or go to **System Settings → Privacy & Security** and press **Open Anyway**.
3. Choose what to install:
   - **Apps**: *Alpha Monster Mix* (with the AI model) and *Alpha Monster Master* standalone, in `/Applications/Alpha Monster`;
   - **Audio Unit**: in `/Library/Audio/Plug-Ins/Components`;
   - **VST3**: in `/Library/Audio/Plug-Ins/VST3`.
4. If an app is blocked at its first launch, use **right-click → Open** again. From Terminal you can also run:
   ```
   xattr -dr com.apple.quarantine "/Applications/Alpha Monster"
   ```
5. Restart your DAW and rescan plug-ins if needed. In Logic Pro the plug-in appears under *Audio Units → Alpha Monster*.

Alternatively use the `.zip`: it contains the same bundles to copy by hand into the folders above.

## Windows 10/11 (64-bit)

1. Download `AlphaMonster-x.y.z-Windows-Setup.exe` and run it.
2. If SmartScreen appears ("Windows protected your PC"), press **More info → Run anyway**: the installer is not signed with a commercial certificate.
3. Installed components:
   - *Alpha Monster Mix* and the AI model in `C:\Program Files\Alpha Monster`;
   - the VST3 plug-in in `C:\Program Files\Common Files\VST3\Alpha Monster Master.vst3`;
   - *Alpha Monster Master* standalone.
4. Restart your DAW and rescan VST3 plug-ins if needed.

## First use

### Alpha Monster Mix

1. **LOAD REFERENCE** (or drag the file into the window).
2. **ANALYSE**: separates the instruments with AI. Everything runs on your computer (no cloud): a 4-minute song takes roughly 3 to 10 minutes depending on the processor. Apple Silicon is faster.
3. **SEPARATE TRACKS → ADD TRACKS** with the stems of your song, or **STEREO MIX → LOAD STEREO MIX**.
4. **MIX LIKE THE REFERENCE**.
5. **PLAYER**: listen to and compare the reference, your original song and the result. Mute/solo any lane and click a waveform to jump there.
6. **EXPORT**.
7. **SAVE ANALYSIS** saves the reference analysis (`.amref` file), so you can reuse it without separating again.

### Alpha Monster Master (Italian interface)

Insert it as the last plug-in on the master bus. Then:

1. **CARICA REF** (load reference);
2. **LEARN** while your song plays (the whole song is best), then press again to stop;
3. **MATCH** during playback;
4. adjust **Match EQ**, **Glue**, **Width** and **Ceiling** by ear.

Use **ASCOLTA REF** (listen to reference) for A/B comparison.
