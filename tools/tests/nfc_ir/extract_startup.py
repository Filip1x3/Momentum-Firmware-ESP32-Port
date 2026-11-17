"""Extract production indexing/parser-dispatch bodies for a fake-platform host test."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT.parent / ".diagnostics" / "nfc-ir-tests"


def extract(path, signature):
    source = (ROOT / path).read_text(encoding="utf-8")
    span = source[source.index(signature):]
    masked = re.sub(r"/\*[\s\S]*?\*/|//[^\n]*", lambda m: " " * len(m[0]), span)
    start = masked.index("{")
    depth, end = 1, start + 1
    while depth:
        depth += (masked[end] == "{") - (masked[end] == "}")
        end += 1
    return span[:end]


def main():
    nfc = "applications/main/nfc/helpers/nfc_supported_cards.c"
    ir = "lib/infrared/signal/infrared_brute_force.c"
    functions = [
        (nfc, "static bool nfc_supported_cards_get_next_name("),
        (nfc, "static void nfc_supported_cards_cache_metadata("),
        (nfc, "void nfc_supported_cards_load_cache("),
        (nfc, "bool nfc_supported_cards_read("),
        (nfc, "bool nfc_supported_cards_parse("),
        (ir, "InfraredErrorCode infrared_brute_force_calculate_messages("),
        (ir, "bool infrared_brute_force_send("),
    ]
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "startup_under_test.h").write_text(
        "/* Generated directly from current production sources. */\n" +
        "\n\n".join(extract(path, signature) for path, signature in functions) + "\n",
        encoding="utf-8",
    )
    print("Extracted NFC parser dispatch and IR index/send from production")


if __name__ == "__main__":
    main()
