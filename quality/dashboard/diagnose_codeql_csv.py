#!/usr/bin/env python3
"""Diagnose CodeQL CSV format to help fix parsing issues."""

import csv
import pathlib
import sys

def diagnose_csv(csv_path: str) -> None:
    """Analyze CodeQL CSV structure and content."""
    path = pathlib.Path(csv_path)
    
    if not path.is_file():
        print(f"❌ File not found: {csv_path}")
        return
    
    print(f"📊 Diagnosing: {csv_path}\n")
    
    with path.open(encoding="utf-8", errors="replace", newline="") as f:
        reader = csv.DictReader(f)
        
        if not reader.fieldnames:
            print("❌ No columns found (empty file?)")
            return
        
        print(f"✅ CSV Columns: {reader.fieldnames}")
        print(f"   Total columns: {len(reader.fieldnames)}\n")
        
        severity_variants = {}
        severity_values = {}
        row_count = 0
        sample_rows = []
        
        for i, row in enumerate(reader):
            row_count += 1
            
            # Try all possible severity column names
            for col in reader.fieldnames:
                if "sever" in col.lower() or "level" in col.lower() or "grade" in col.lower():
                    val = row.get(col, "").strip()
                    severity_values[col] = severity_values.get(col, {})
                    severity_values[col][val] = severity_values[col].get(val, 0) + 1
            
            # Save first 3 rows as samples
            if i < 3:
                sample_rows.append(row)
        
        print(f"📈 Total rows: {row_count}\n")
        
        if severity_values:
            print("🔍 Severity-like columns found:")
            for col, values in severity_values.items():
                print(f"\n   Column: '{col}'")
                for val, count in sorted(values.items(), key=lambda x: x[1], reverse=True):
                    print(f"      '{val}' → {count} occurrences")
        else:
            print("⚠️  No severity/level/grade columns found!")
            print("   Available columns to check:")
            for col in reader.fieldnames:
                print(f"      - {col}")
        
        print(f"\n📋 Sample rows (first 3):")
        for i, row in enumerate(sample_rows, 1):
            print(f"\n   Row {i}:")
            for col, val in row.items():
                print(f"      {col}: {val[:60]}")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python3 diagnose_codeql_csv.py <path_to_csv>")
        print("\nExample:")
        print("  python3 diagnose_codeql_csv.py _quality/codeql_findings.csv")
        sys.exit(1)
    
    diagnose_csv(sys.argv[1])
