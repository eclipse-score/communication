#!/bin/bash
set -e

# Configuration
PLANTUML_VERSION="1.2025.10"
PLANTUML_JAR="plantuml-${PLANTUML_VERSION}.jar"
PLANTUML_PATH="${HOME}/.cache/plantuml/${PLANTUML_JAR}"
SVG_OUTPUT_DIR="./generated/svg"

# Download PlantUML if needed
if [ ! -f "$PLANTUML_PATH" ]; then
    mkdir -p "$(dirname "$PLANTUML_PATH")"
    echo "Downloading PlantUML ${PLANTUML_VERSION}..."
    URL="https://github.com/plantuml/plantuml/releases/download/v${PLANTUML_VERSION}/${PLANTUML_JAR}"
    wget -q --show-progress -O "$PLANTUML_PATH" "$URL" || curl -L -o "$PLANTUML_PATH" "$URL"
fi

# Generate diagrams
rm -rf "$SVG_OUTPUT_DIR"
mkdir -p "$SVG_OUTPUT_DIR"
for file in ./*.puml; do
    [ -f "$file" ] && java -jar "$PLANTUML_PATH" -svg -charset UTF-8 -o "$SVG_OUTPUT_DIR" "$file"
done

# Fix line endings
find "$SVG_OUTPUT_DIR" -name "*.svg" -exec sed -i 's/\r$//' {} \; 2>/dev/null

echo "Diagrams generated with PlantUML ${PLANTUML_VERSION}."
