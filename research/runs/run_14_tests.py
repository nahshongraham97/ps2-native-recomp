import subprocess, sys, shutil, tempfile
from pathlib import Path

root = Path(r"C:\Users\nahsh\mk port\Armageddon-backup\armageddon-reimplementation\external\forks\retail-main-22")
build = root / "b"

gamma_json = root / "research" / "runs" / "native-xbox-renderer-0406-implementation-20261122" / "gamma-one-expected.json"

temp_streams = root / "temp_streams_disposable"
if temp_streams.exists():
    shutil.rmtree(temp_streams)

temp_images = root / "temp_images_disposable"
if temp_images.exists():
    shutil.rmtree(temp_images)

tests = [
    ("allocator", ["test_native_retail_renderer_allocator.exe", "."]),
    ("core", ["test_native_retail_renderer_core.exe", "research/runs/parallel-renderer-engine-init-20261122"]),
    ("matrix", ["test_native_retail_renderer_matrix.exe", "."]),
    ("frames", ["test_native_retail_renderer_frames.exe", "."]),
    ("streams", ["test_native_retail_renderer_streams.exe", str(temp_streams)]),
    ("cameras", ["test_native_retail_renderer_cameras.exe", ".", "research/runs/native-xbox-renderer-0405-20261122/depth-expected.tsv"]),
    ("images", ["test_native_retail_renderer_images.exe", str(temp_images), str(gamma_json)]),
    ("raster_reg", ["test_native_retail_renderer_raster_registration.exe", "."]),
    ("rasters", ["test_native_retail_renderer_rasters.exe", "."]),
    ("textures", ["test_native_retail_renderer_textures.exe", "."]),
    ("dictionaries", ["test_native_retail_renderer_texture_dictionaries.exe", "."]),
    ("world", ["test_native_retail_renderer_world.exe", "."]),
    ("materials", ["test_native_retail_materials.exe" if False else "test_native_retail_renderer_materials.exe", "."]),
    ("geometry", ["test_native_retail_renderer_geometry.exe"]),
    ("display_movie", ["test_native_retail_display_movie_presentation.exe"]),
]

passed = 0
for name, args in tests:
    exe = build / args[0]
    p = subprocess.run([str(exe), *args[1:]], cwd=root, capture_output=True, text=True)
    if p.returncode == 0:
        print(f"PASS: {name:15s} (exit {p.returncode})")
        passed += 1
    else:
        print(f"FAIL: {name:15s} (exit {p.returncode})")
        print("  stdout:", p.stdout.strip())
        print("  stderr:", p.stderr.strip())

# Clean up temporary directories
if temp_streams.exists():
    shutil.rmtree(temp_streams)
if temp_images.exists():
    shutil.rmtree(temp_images)

print(f"\nTotal: {passed}/{len(tests)} passed.")
if passed != len(tests):
    sys.exit(1)
