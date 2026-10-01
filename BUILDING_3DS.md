# Building Dark Forces 3DS v1.0.0

The public tree keeps the final TFE source snapshot and the Nintendo 3DS
platform layer in their release layout:

```text
TFE/
Platform3DS/
```

Use a current devkitPro/devkitARM Nintendo 3DS toolchain and the 3DS libraries
referenced by `Platform3DS/Makefile` (including libctru/Citro3D).

From `Platform3DS/`:

```bash
make clean
make -j4
```

Expected primary output:

```text
DarkForces3DS.3dsx
```

The GitHub preparation helper intentionally does **not** rebuild the public
release asset. The v1.0.0 Release uses the exact hardware-tested
`DarkForces3DS_RC1.3dsx` copied byte-for-byte from the final development tree.
