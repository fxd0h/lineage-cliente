#!/usr/bin/env python3
"""Append the teleports listed in teleports_extra.tsv to Whale's teleport_list.yaml (idempotent)."""
import sys

yaml_path, tsv_path = sys.argv[1], sys.argv[2]
text = open(yaml_path, encoding="utf-8").read()
added = 0
for line in open(tsv_path, encoding="utf-8"):
    if not line.strip() or line.startswith("#"):
        continue
    action, npc, x, y, m, h, price = line.rstrip("\n").split("\t")[:7]
    key = f"- action: {action}\n      npc_id: {npc}\n"
    if key in text:
        continue
    text = text.rstrip("\n") + (f"\n    - action: {action}\n      npc_id: {npc}\n      x: {x}\n      \"y\": {y}\n"
                                 f"      map_id: {m}\n      heading: {h}\n      price: {price}\n")
    added += 1
open(yaml_path, "w", encoding="utf-8").write(text)
print(f"teleports added: {added}")
