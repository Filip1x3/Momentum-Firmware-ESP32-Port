"""Extract production receive lifecycle functions for native, non-radio tests."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[3]
output = root.parent / ".diagnostics" / "receive-rfid-tests"
output.mkdir(parents=True, exist_ok=True)
functions = []
for path, names in (
    ("applications_user/protopirate/views/protopirate_receiver.c", (
        "protopirate_view_receiver_lock_history", "protopirate_view_receiver_unlock_history")),
    ("applications_user/protopirate/protopirate_history.c", ("protopirate_history_get_item",)),
    ("applications_user/protopirate/scenes/protopirate_scene_receiver.c", (
        "protopirate_scene_receiver_callback",)),
    ("applications_user/protopirate/helpers/protopirate_txrx.c", (
        "protopirate_rx_end", "protopirate_rx_stack_teardown_for_registry_switch")),
):
    source = (root / path).read_text(encoding="utf-8")
    for name in names:
        match = re.search(r"^[\w* \t]+\b" + re.escape(name) + r"\([^;{}]*\)\s*\{", source, re.M)
        if not match:
            raise RuntimeError(name)
        position = source.index("{", match.start())
        depth, end = 1, position + 1
        while depth:
            depth += (source[end] == "{") - (source[end] == "}")
            end += 1
        functions.append(source[match.start():end])
(output / "production_receive.h").write_text("\n\n".join(functions), encoding="utf-8")
source = (root / "components/furi_hal/furi_hal_rfid.c").read_text(encoding="utf-8")
capability = re.search(r"bool furi_hal_rfid_is_supported\(void\)\s*\{[^}]*\}", source)
if not capability:
    raise RuntimeError("furi_hal_rfid_is_supported")
(output / "production_capability.h").write_text(capability[0], encoding="utf-8")
