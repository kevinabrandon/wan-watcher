"""
PlatformIO build script for pre-filesystem tasks:
1. Copy documentation files to data/docs/
2. Generate version.json with version, git hash, and build time

Runs before building the LittleFS filesystem image.
"""
import os
import shutil
import glob
import subprocess
import re
import json
from datetime import datetime, timezone

Import("env")


def get_git_hash():
    """Get short git commit hash."""
    try:
        return subprocess.check_output(
            ["git", "rev-parse", "--short", "HEAD"],
            stderr=subprocess.DEVNULL
        ).decode().strip()
    except Exception:
        return "unknown"


def get_openapi_version(project_dir):
    """Read version from openapi.yaml."""
    openapi_path = os.path.join(project_dir, "data", "openapi.yaml")
    try:
        with open(openapi_path, "r") as f:
            content = f.read()
        match = re.search(r"^\s+version:\s+(.+)$", content, re.MULTILINE)
        if match:
            return match.group(1).strip()
    except Exception:
        pass
    return "0.0.0"


def generate_version_json(source, target, env):
    """Generate version.json with build info."""
    project_dir = env.get("PROJECT_DIR", ".")

    version_info = {
        "version": get_openapi_version(project_dir),
        "git_hash": get_git_hash(),
        "build_time": datetime.now(timezone.utc).strftime("%Y-%m-%d %H:%M UTC")
    }

    version_path = os.path.join(project_dir, "data", "version.json")
    with open(version_path, "w") as f:
        json.dump(version_info, f)

    print(f"Generated version.json: v{version_info['version']} ({version_info['git_hash']}) built {version_info['build_time']}")


def copy_documentation(source, target, env):
    """Copy documentation files to data/docs/ before building filesystem."""
    project_dir = env.get("PROJECT_DIR", ".")
    repo_root = os.path.dirname(project_dir)  # Go up from esp32/ to repo root
    docs_dest = os.path.join(project_dir, "data", "docs")

    # Clean existing docs directory
    if os.path.exists(docs_dest):
        shutil.rmtree(docs_dest)
    os.makedirs(docs_dest, exist_ok=True)

    copied = []

    # Copy README.md from repo root
    readme_src = os.path.join(repo_root, "README.md")
    if os.path.exists(readme_src):
        shutil.copy2(readme_src, os.path.join(docs_dest, "README.md"))
        copied.append("README.md")

    # Copy docs/*.md files
    docs_src = os.path.join(repo_root, "docs")
    if os.path.exists(docs_src):
        for md_file in glob.glob(os.path.join(docs_src, "*.md")):
            shutil.copy2(md_file, docs_dest)
            copied.append(os.path.basename(md_file))

    # Copy docs/diagrams/ (for circuit diagram SVG)
    diagrams_src = os.path.join(docs_src, "diagrams")
    diagrams_dest = os.path.join(docs_dest, "diagrams")
    if os.path.exists(diagrams_src):
        os.makedirs(diagrams_dest, exist_ok=True)
        for svg_file in glob.glob(os.path.join(diagrams_src, "*.svg")):
            shutil.copy2(svg_file, diagrams_dest)
            copied.append(f"diagrams/{os.path.basename(svg_file)}")

    # Copy images/ directory (for README screenshots)
    images_src = os.path.join(repo_root, "images")
    images_dest = os.path.join(docs_dest, "images")
    if os.path.exists(images_src):
        os.makedirs(images_dest, exist_ok=True)
        for img_file in glob.glob(os.path.join(images_src, "*")):
            if os.path.isfile(img_file):
                shutil.copy2(img_file, images_dest)
                copied.append(f"images/{os.path.basename(img_file)}")

    print(f"Copied {len(copied)} documentation files to data/docs/")


# Run when script is loaded (pre: prefix ensures this runs before build)
generate_version_json(None, None, env)
copy_documentation(None, None, env)
