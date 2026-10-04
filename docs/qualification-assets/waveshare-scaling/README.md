# Waveshare scaling qualification assets

These PNG files are deterministic visual probes for the Waveshare scaling qualification. Load the file that matches the logical iDotMatrix profile exposed by the firmware.

- `waveshare-scaling-16x16.png`: each logical source pixel must occupy exactly 4x4 physical pixels on the 64x64 HUB75 panel.
- `waveshare-scaling-32x32.png`: each logical source pixel must occupy exactly 2x2 physical pixels.
- `waveshare-scaling-64x64.png`: exact 1:1 output.

All patterns contain a one-pixel white border, red/green/blue/yellow inner corner markers, a magenta horizontal center line, a cyan vertical center line and a dim one-pixel checkerboard background. Any mirror, rotation, off-by-one or wrong replication factor should therefore be immediately visible.
