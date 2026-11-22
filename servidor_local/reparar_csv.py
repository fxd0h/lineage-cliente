#!/usr/bin/env python3
"""Rebuild rows that l1jdb_363.sql imported wrongly.

The dump was generated from db/csv/ko without honouring the CSV escape character, so every
field holding an escaped comma ("20009\\,20099") was split and the following columns shifted.
That broke all armor sets (only the first piece was kept, the bonuses landed in the wrong columns)
and 15 spirit-guardian NPCs. This script re-reads those CSVs correctly and prints REPLACE
statements for the affected rows; instalar.sh pipes it into MariaDB before corregir_db.sql.
"""
import csv
import pathlib
import sys

CSV = pathlib.Path(__file__).resolve().parent / "l1j-jp/db/csv/ko"


def literal(v: str) -> str:
    if v == "\\N":
        return "NULL"
    return "'" + v.replace("\\", "\\\\").replace("'", "\\'") + "'"


def replace_rows(table: str, only_with_commas: bool) -> None:
    with open(CSV / f"{table}.csv", encoding="utf-8", newline="") as f:
        rows = list(csv.reader(f, escapechar="\\"))
    head, body = rows[0], rows[1:]
    cols = ", ".join(f"`{c}`" for c in head)
    for r in body:
        if len(r) != len(head):
            sys.exit(f"{table}: row {r[:2]} has {len(r)} fields, expected {len(head)}")
        if only_with_commas and not any("," in c for c in r):
            continue
        print(f"REPLACE INTO `{table}` ({cols}) VALUES ({', '.join(literal(c) for c in r)});")


print("SET NAMES utf8mb4;")
replace_rows("armor_sets", only_with_commas=False)
replace_rows("npcs", only_with_commas=True)
