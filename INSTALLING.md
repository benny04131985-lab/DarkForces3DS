# Installing Dark Forces 3DS v1.0.0

Final hardware validation targeted **New Nintendo 3DS** hardware.

## SD-card layout

Copy the GitHub Release executable to:

```text
sdmc:/3ds/DarkForces3DS/DarkForces3DS.3dsx
```

Provide your own legally obtained Dark Forces data under:

```text
sdmc:/3ds/DarkForces3DS/data/
```

The final installation layout is:

```text
sdmc:/3ds/DarkForces3DS/
    DarkForces3DS.3dsx
    data/
        DARK.GOB
        SOUNDS.GOB
        TEXTURES.GOB
        SPRITES.GOB
        LFD/
            ...complete original LFD folder...
    saves/
    config/
    logs/
```

Copy the original game data from your own installation; preserving the complete
`LFD/` directory is important. If your retail installation contains additional
Dark Forces data files used by TFE, keep them with the other game data rather
than deleting them just because they are not listed above.

The release ZIP contains the homebrew executable and empty runtime-folder
structure/documentation only. It deliberately contains **no commercial game
data**.
