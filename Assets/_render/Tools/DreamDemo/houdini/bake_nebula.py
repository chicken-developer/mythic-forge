"""T_DreamNebula: 360 degree equirectangular nebula, EXR 16-bit half, linear.

Karma was not used: the nebula is 3D noise evaluated on the view direction at several
"distances" (frequencies), which is seamless by construction and bakes in seconds.
Up is +Z, so the top row of the image is the zenith and the lower half is below the horizon.
"""
import os

import numpy as np

import _bake_common as bc

# ---- parameters -------------------------------------------------------------
WIDTH = 4096
HEIGHT = 2048
SEED = 11.0
WARP = 0.55
COLOR_TEAL = "#7FD6D0"
COLOR_TEAL_DARK = "#2E5F7A"
COLOR_INDIGO = "#1F2A5A"
COLOR_INDIGO_DEEP = "#0E1230"
FILAMENTS = 0.55
DUST = 0.8           # strength of the dark dust lane
BRIGHTNESS = 1.0
# -----------------------------------------------------------------------------

SNIPPET = r"""
float fbm(vector p; int oct; float rough) {
    float a = 1.0; float s = 0.0; float n = 0.0; vector q = p;
    for (int i = 0; i < oct; i++) {
        s += a * (noise(q) * 2.0 - 1.0);
        n += a;
        q = q * 2.07 + set(13.1, 7.7, 3.3);
        a *= rough;
    }
    return s / n;
}

int W = chi("W"); int H = chi("H");
int ix = @ptnum % W; int iy = @ptnum / W;
float u = (ix + 0.5) / W; float v = (iy + 0.5) / H;
float lon = (u - 0.5) * 2.0 * PI;
float lat = (0.5 - v) * PI;
vector d = set(cos(lat) * cos(lon), cos(lat) * sin(lon), sin(lat));
vector so = set(chf("seed"), chf("seed") * 1.7, chf("seed") * 0.6);

vector w = set(fbm(d * 1.3 + so, 4, 0.5),
               fbm(d * 1.3 + so + 31.7, 4, 0.5),
               fbm(d * 1.3 + so + 73.1, 4, 0.5));
vector dw = d + w * chf("warp");

// three gas layers at different "distances": far = large and soft, near = small and crisp
float far = fbm(dw * 1.6 + set(5, 1, 9) + so, 5, 0.55);
float mid = fbm(dw * 3.1 + set(2, 8, 4) + so, 6, 0.55);
float near = fbm(dw * 5.5 + w * 0.8 + so, 6, 0.6);
float dFar = smooth(-0.12, 0.30, far);
float dMid = smooth(-0.02, 0.32, mid) * smooth(-0.25, 0.15, far);
float dNear = smooth(0.05, 0.36, near) * dMid;

// thin filaments along the edges of the mid layer
float ridge = 1.0 - clamp(abs(fbm(dw * 7.0 + so + 40.0, 5, 0.6)) / 0.22, 0.0, 1.0);
float fil = pow(ridge, 5.0) * clamp(dMid * (1.0 - dMid) * 4.0, 0.0, 1.0);

// dark dust lane along a tilted great circle
vector nb = normalize(set(0.25, -0.5, 0.83));
float lane = dot(d, nb) / 0.2 + 1.6 * fbm(dw * 2.2 + so + 90.0, 4, 0.5);
float dust = exp(-lane * lane) * smooth(-0.15, 0.25, fbm(dw * 4.3 + so + 120.0, 6, 0.6));

vector cTeal = chv("c_teal"); vector cTealDark = chv("c_teal_dark");
vector cIndigo = chv("c_indigo"); vector cDeep = chv("c_deep");

vector col = lerp(cDeep, cIndigo, smooth(-0.2, 0.9, d.z));
col = lerp(col, cTealDark, dFar * 0.85);
col += cTealDark * dMid * 0.9;
col = lerp(col, cTeal, dNear * 0.75);
col += cTeal * fil * chf("filaments");
col *= lerp(1.0, 0.18, clamp(dust * chf("dust"), 0.0, 1.0));
col *= lerp(0.6, 1.15, smooth(0.0, 1.0, d.z));

// below the horizon: fade to solid deep indigo
float below = 1.0 - smooth(-0.30, 0.08, d.z);
col = lerp(col, cDeep * 0.6, below);
v@col = col * chf("brightness");
"""


def main():
    timer = bc.BakeTimer("T_DreamNebula")
    img = bc.eval_vex_image(WIDTH, HEIGHT, SNIPPET, {
        "seed": SEED, "warp": WARP, "filaments": FILAMENTS, "dust": DUST,
        "brightness": BRIGHTNESS,
        "c_teal": bc.srgb_hex_to_linear(COLOR_TEAL),
        "c_teal_dark": bc.srgb_hex_to_linear(COLOR_TEAL_DARK),
        "c_indigo": bc.srgb_hex_to_linear(COLOR_INDIGO),
        "c_deep": bc.srgb_hex_to_linear(COLOR_INDIGO_DEEP),
    })
    out = os.path.join(bc.RAW_DIR, "T_DreamNebula.exr")
    bc.save_exr_half(out, img)
    timer.output(out)
    # Unreal only turns long-lat images into a TextureCube when they come in as Radiance .hdr.
    hdr = os.path.join(bc.RAW_DIR, "T_DreamNebula.hdr")
    bc.save_hdr(hdr, img)
    timer.output(hdr)
    preview = np.clip(img, 0, 1) ** (1 / 2.2)
    check = os.path.join(bc.CHECK_DIR, "T_DreamNebula_preview.png")
    bc.save_png(check, preview[::4, ::4])
    timer.output(check)
    seam = bc.seam_report(img)
    timer.note("seam_longitude", {"inner": seam["inner_step_x"], "wrap": seam["wrap_step_x"]})
    timer.note("max_value", float(img.max()))
    timer.note("mean_upper", float(img[: HEIGHT // 2].mean()))
    timer.note("mean_lower", float(img[HEIGHT // 2:].mean()))
    timer.finish()


main()
