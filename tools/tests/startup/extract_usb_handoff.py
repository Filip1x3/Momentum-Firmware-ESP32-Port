"""Mechanically extract the small production handoff functions for host tests.

Only platform includes are replaced by the test driver's fake platform.
No production function bodies are copied/reimplemented in the test driver.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT.parent / ".diagnostics" / "startup-tests"
OUT.mkdir(parents=True, exist_ok=True)


def extract(relative: str, signature: str) -> str:
    source = (ROOT / relative).read_text(encoding="utf-8")
    start = source.index(signature)
    # Ignore comments for brace counting (not their content in the result).
    span = source[start:]
    masked = re.sub(r"/\*[\s\S]*?\*/|//[^\n]*", lambda m: " " * len(m[0]), span)
    first = masked.index("{")
    depth = 1
    end = first + 1
    while depth:
        depth += (masked[end] == "{") - (masked[end] == "}")
        end += 1
    return span[:end]


functions = [
    ("applications/services/storage/storages/storage_ext.c", "FS_Error sd_unmount_card("),
    ("applications/services/storage/storage_processing.c", "static FS_Error storage_process_sd_unmount("),
    ("applications/services/desktop/scenes/desktop_scene_usb_storage.c", "static bool desktop_usb_storage_enter("),
    ("applications/services/desktop/scenes/desktop_scene_usb_storage.c", "static void desktop_usb_storage_leave("),
]
(OUT / "usb_backend_under_test.h").write_text(
    "/* Generated directly from current production sources. */\n" +
    "\n\n".join(extract(path, signature) for path, signature in functions) + "\n",
    encoding="utf-8",
)
print("Extracted active FAM Storage + Desktop MSC backend for native tests")
