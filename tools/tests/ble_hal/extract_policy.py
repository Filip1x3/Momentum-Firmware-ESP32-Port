"""Extract the real btshim restore/stop policy; do not reimplement it in a test.

The complete production BLE HAL and bt_stack_deinit are compiled directly.
Only the large service's four small policy/API functions are extracted, with
their bodies unchanged, because the rest depends on the GUI/RPC services.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT.parent / ".diagnostics" / "ble-hal-tests"
OUT.mkdir(parents=True, exist_ok=True)
source = (ROOT / "components/btshim/btshim.c").read_text(encoding="utf-8")


def extract(signature: str) -> str:
    start = source.index(signature)
    span = source[start:]
    masked = re.sub(r'/\*[\s\S]*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"',
                    lambda match: " " * len(match[0]), span)
    first = masked.index("{")
    depth, end = 1, first + 1
    while depth:
        depth += (masked[end] == "{") - (masked[end] == "}")
        end += 1
    return span[:end]


signatures = [
    "static bool bt_handle_stop_stack(Bt* bt) {",
    "static void bt_handle_restore_default(Bt* bt, BtMessage* message) {",
    "bool bt_profile_restore_default(Bt* bt) {",
    "void bt_stop_stack(Bt* bt) {",
]
(OUT / "bt_policy_under_test.h").write_text(
    "/* Generated from current production btshim.c, function bodies unchanged. */\n" +
    "\n\n".join(extract(signature) for signature in signatures) + "\n",
    encoding="utf-8",
)
print("Extracted actual btshim stop/restore handlers and public API wrappers")

# The scanner's callbacks and GATT read/write functions are outside this test.
# Extract just its state declarations and lifecycle functions, unchanged.
source = (ROOT / "applications/main/ble_spam/ble_walk_hal.c").read_text(encoding="utf-8")
state_names = ["s_device_count", "s_scanning", "s_connected", "s_hal_started",
               "s_stack_owned", "s_gattc_if", "s_conn_id"]
states = []
for name in state_names:
    matches = re.findall(r"^static [^\n;]*\b" + re.escape(name) + r"\s*=[^;]*;", source, re.M)
    if len(matches) != 1:
        raise ValueError("Cannot identify unique scanner lifecycle state: " + name)
    states.append(matches[0])
(OUT / "walk_lifecycle_under_test.h").write_text(
    "/* Generated directly from current production BLE Walk HAL. */\n" +
    "\n".join(states) + "\n" +
    "\n\n".join(extract(signature) for signature in [
        "bool ble_walk_hal_start(void) {", "void ble_walk_hal_stop(void) {"])
    + "\n", encoding="utf-8")
print("Extracted actual BLE Walk HAL lifecycle functions and state")
