"""
PlatformIO build script for pre-filesystem tasks:
1. Update openapi.yaml version from git tags
2. Copy documentation files to data/docs/

Runs before building the LittleFS filesystem image.
"""
import subprocess
import re
import os
import shutil
import glob

Import("env")


def get_git_version():
    """Get version string from git tags or commit hash."""
    try:
        # Try to get a tag-based version (e.g., "v1.2.3" or "v1.2.3-5-gabcdef")
        version = subprocess.check_output(
            ["git", "describe", "--tags", "--always"],
            stderr=subprocess.DEVNULL
        ).decode().strip()

        # Strip leading 'v' if present (v1.2.3 -> 1.2.3)
        if version.startswith("v"):
            version = version[1:]

        return version
    except Exception:
        return "0.0.0-dev"


def update_openapi_version(source, target, env):
    """Update the version field in openapi.yaml before building filesystem."""
    version = get_git_version()

    # Path to openapi.yaml in the data directory
    project_dir = env.get("PROJECT_DIR", ".")
    openapi_path = os.path.join(project_dir, "data", "openapi.yaml")

    if not os.path.exists(openapi_path):
        print(f"Warning: {openapi_path} not found, skipping version update")
        return

    with open(openapi_path, "r") as f:
        content = f.read()

    # Replace the version line in the info section
    # Matches "  version: <anything>" and replaces with the git version
    new_content = re.sub(
        r"^(  version: ).+$",
        rf"\g<1>{version}",
        content,
        count=1,
        flags=re.MULTILINE
    )

    if new_content != content:
        with open(openapi_path, "w") as f:
            f.write(new_content)
        print(f"Updated openapi.yaml version to: {version}")
    else:
        print(f"openapi.yaml version already set or pattern not found")


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


def pre_build_filesystem(source, target, env):
    """Run all pre-build tasks for the filesystem."""
    update_openapi_version(source, target, env)
    copy_documentation(source, target, env)


# Register the callback to run before building the filesystem image
env.AddPreAction("$BUILD_DIR/littlefs.bin", pre_build_filesystem)
