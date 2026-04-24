# GTAV_recorder

A project to record Grand Theft Auto V (GTAV) gameplay data for training world models.

---

## What it Records

The tool captures synchronized gameplay data including:

- **RGB frames**: Screenshot images from the game rendered at 1080p.
- **Camera pose**: Per-frame extrinsic camera parameters (position and orientation in world coordinates).
- **Camera intrinsics**: Fixed intrinsic calibration for the first-person view camera.
- **Gamepad input**: Per-frame controller state (thumbsticks, triggers, and button masks).

---

## Output Format

The recorder produces the following files for each session:

```
{timestamp}/
├── {timestamp}_images.txt      # Camera extrinsics (COLMAP format)
├── {timestamp}_cameras.txt      # Camera intrinsics (COLMAP format)
├── {timestamp}_input.json       # Per-frame gamepad input
└── images/
    ├── 0000001.jpg
    ├── 0000002.jpg
    └── ...
```

All metadata files (`images.txt`, `cameras.txt`, `input.json`) are prefixed with the session timestamp to distinguish between different recording sessions.

### `{timestamp}_images.txt`

Camera extrinsic parameters in **standard COLMAP text format**. Each image occupies exactly **two lines**.

**File header:**
```text
# Image list with two lines of data per image:
#   IMAGE_ID, QW, QX, QY, QZ, TX, TY, TZ, CAMERA_ID, NAME
#   POINTS2D[] as (X, Y, POINT3D_ID)
# Number of images: {N}, mean observations per image: 0
```

**Line 1 — Image metadata and pose (10 space-separated fields):**

| # | Field | Type | Example | Description |
|---|-------|------|---------|-------------|
| 1 | `IMAGE_ID` | `int` | `1` | Frame index, starting from 1 |
| 2 | `QW` | `float` | `-0.243976` | Quaternion scalar (w) |
| 3 | `QX` | `float` | `-0.050536` | Quaternion vector (x) |
| 4 | `QY` | `float` | `-0.196433` | Quaternion vector (y) |
| 5 | `QZ` | `float` | `0.948333` | Quaternion vector (z) |
| 6 | `TX` | `float` | `-1007.907166` | Translation vector x |
| 7 | `TY` | `float` | `-1060.606812` | Translation vector y |
| 8 | `TZ` | `float` | `1033.181519` | Translation vector z |
| 9 | `CAMERA_ID` | `int` | `1` | Camera ID referencing `cameras.txt` |
| 10 | `NAME` | `string` | `images/0000001.jpg` | Image filename, relative to session root (7-digit zero-padded) |

**Line 2 — 2D observations (`POINTS2D[]`):**

Currently left **empty** (a blank line). Since this is raw gameplay recording without feature extraction or SfM reconstruction, there are no 2D–3D point correspondences yet.

**Example:**
```text
1 -0.243976 -0.050536 -0.196433 0.948333 -1007.907166 -1060.606812 1033.181519 1 images/0000001.jpg

2 -0.243976 -0.050536 -0.196433 0.948333 -1007.907166 -1060.606812 1033.181519 1 images/0000002.jpg

3 -0.271967 -0.061919 -0.213029 0.936386 -970.967957 -1154.840820 967.123169 1 images/0000003.jpg
```

---

### `{timestamp}_cameras.txt`

Camera intrinsic parameters in **standard COLMAP text format**. The dataset uses a single camera, so the file contains exactly one data line.

**File header:**
```text
# Camera list with one line of data per camera:
#   CAMERA_ID, MODEL, WIDTH, HEIGHT, PARAMS[]
# Number of cameras: 1
```

**Data line (8 fields):**

| # | Field | Type | Value | Description |
|---|-------|------|-------|-------------|
| 1 | `CAMERA_ID` | `int` | `1` | Camera ID referenced by `images.txt` |
| 2 | `MODEL` | `string` | `PINHOLE` | Pinhole camera model |
| 3 | `WIDTH` | `int` | `1920` | Image width in pixels |
| 4 | `HEIGHT` | `int` | `1080` | Image height in pixels |
| 5 | `fx` | `float` | `2269.12` | Focal length along x-axis (px) |
| 6 | `fy` | `float` | `2269.12` | Focal length along y-axis (px) |
| 7 | `cx` | `float` | `960` | Principal point x-coordinate (px) |
| 8 | `cy` | `float` | `540` | Principal point y-coordinate (px) |

**Example:**
```text
1 PINHOLE 1920 1080 2269.12 2269.12 960 540
```

---

### `{timestamp}_input.json`

Per-frame gamepad controller state sampled synchronously with the video stream.

**Top-level structure:**

```json
{
  "session": "20260424_215625",
  "total_frames": 483,
  "data": [
    { ... },
    { ... }
  ]
}
```

| Field | Type | Description |
|-------|------|-------------|
| `session` | `string` | Recording session ID (`YYYYMMDD_HHMMSS`) |
| `total_frames` | `int` | Total number of frames. Must match the image count in `images.txt`. |
| `data` | `array` | Array of per-frame controller states. Length equals `total_frames`. |

**Per-frame element:**

```json
{
  "type": "gamepad",
  "start_time": 0.75,
  "end_time": 0.783333,
  "stick_lx": 0,
  "stick_ly": 0,
  "stick_rx": 0,
  "stick_ry": -0.456693,
  "trigger_l": 0,
  "trigger_r": 0,
  "buttons": 0
}
```

| Field | Type | Range | Description |
|-------|------|-------|-------------|
| `type` | `string` | `"gamepad"` | Input device type |
| `start_time` | `float` | ≥ 0 | Sampling interval start (seconds) |
| `end_time` | `float` | ≥ 0 | Sampling interval end (seconds) |
| `stick_lx` | `float` | `[-1.0, 1.0]` | Left stick X-axis (0 = neutral) |
| `stick_ly` | `float` | `[-1.0, 1.0]` | Left stick Y-axis |
| `stick_rx` | `float` | `[-1.0, 1.0]` | Right stick X-axis |
| `stick_ry` | `float` | `[-1.0, 1.0]` | Right stick Y-axis |
| `trigger_l` | `float` | `[0.0, 1.0]` | Left trigger (LT) |
| `trigger_r` | `float` | `[0.0, 1.0]` | Right trigger (RT) |
| `buttons` | `int` | ≥ 0 | Button bitmask (0 = no buttons pressed) |

---

### `images/`

Raw screenshot frames captured from the game.

- **Format:** `.jpg`
- **Resolution:** 1920 × 1080
- **Naming:** 7-digit zero-padded index (`0000001.jpg`, `0000002.jpg`, ..., `0000483.jpg`)
- **Color space:** RGB

---

## Coordinate System

The pose format follows the **COLMAP / OpenCV convention**:

- **Quaternion:** `q(w, x, y, z)` in **Hamilton convention** (same as Eigen).
- **Rotation `R`:** World → Camera rotation.
- **Translation `T`:** The transformation from world to camera coordinates is `X_cam = R · X_world + T`.
- **Camera center `C`:** The camera position in world coordinates is `C = -R^T · T`.
- **Camera axes:** X right, Y down, Z forward (into the screen).

---

## Notes

- **First-person view:** All recordings are captured in first-person perspective. Third-person support may be added later.
- **No GUI/HUD:** The in-game graphical interface is disabled during recording.
- **Raw stage:** `images.txt` currently contains no 2D–3D observations (second line is blank) because feature extraction and reconstruction are performed offline in a later stage.
