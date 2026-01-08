#!/bin/bash

# Check if project name is provided
if [ -z "$1" ]; then
    echo "Usage: $0 <project_name>"
    exit 1
fi

PROJECT_NAME=$1

# Create root folder
mkdir -p "$PROJECT_NAME"

# Create subfolders
mkdir -p "$PROJECT_NAME/config"
mkdir -p "$PROJECT_NAME/docs"
mkdir -p "$PROJECT_NAME/include/config"
mkdir -p "$PROJECT_NAME/include/core"
mkdir -p "$PROJECT_NAME/include/drivers"
mkdir -p "$PROJECT_NAME/include/utils"
mkdir -p "$PROJECT_NAME/script"
mkdir -p "$PROJECT_NAME/src/core"
mkdir -p "$PROJECT_NAME/src/drivers"
mkdir -p "$PROJECT_NAME/src/utils"
mkdir -p "$PROJECT_NAME/test"

# Create files
touch "$PROJECT_NAME/CMakeLists.txt"
touch "$PROJECT_NAME/Makefile"
touch "$PROJECT_NAME/README.md"
touch "$PROJECT_NAME/docs/architecture.md"
touch "$PROJECT_NAME/docs/build_guide.md"
touch "$PROJECT_NAME/docs/driver_guide.md"

echo "✅ Project structure created under '$PROJECT_NAME'"
