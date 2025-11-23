#!/usr/bin/env python3

import json
import sys
import argparse
from pathlib import Path
from jsonschema import validate, ValidationError

def arg_parse():
    parser = argparse.ArgumentParser(description="Validate JSON files against a JSON schema.")
    parser.add_argument(
        "-s", "--schema_path",
        default="../pf2e_engine/schemas/schema.json",
        help="Path to the JSON schema file (default: schemas/schema.json)"
    )
    parser.add_argument(
        "-t", "--target_path",
        default="../pf2e_engine/data",
        help="Path to a JSON file or a directory with JSON files (default: data)"
    )
    return parser.parse_args()

def load_schema(schema_path):
    with open(schema_path, 'r', encoding='utf-8') as f:
        return json.load(f)

def validate_file(json_path, schema):
    with open(json_path, 'r', encoding='utf-8') as f:
        data = json.load(f)
    validate(instance=data, schema=schema)

def main():
    args = arg_parse()
    schema_path = Path(args.schema_path)
    target = Path(args.target_path)

    if not schema_path.is_file():
        print(f"Schema file not found: {schema_path}")
        sys.exit(2)
    if not target.exists():
        print(f"Target path not found: {target}")
        sys.exit(2)

    schema = load_schema(schema_path)
    errors = []

    if target.is_file():
        try:
            validate_file(target, schema)
            print(f"OK: {target}")
        except ValidationError as e:
            errors.append((target, e.message))
    else:
        for file_path in target.rglob("*.json"):
            try:
                validate_file(file_path, schema)
                print(f"OK: {file_path}")
            except ValidationError as e:
                errors.append((file_path, e.message))

    if errors:
        print("\nErrors found:")
        for file_path, msg in errors:
            print(f"{file_path}: {msg}")
        sys.exit(1)
    else:
        print("\nAll JSON files are valid!")

if __name__ == "__main__":
    main()
