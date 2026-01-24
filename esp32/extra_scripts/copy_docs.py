"""
PlatformIO build script to copy documentation files to data/docs/.

Runs before building the LittleFS filesystem image.
"""
import os
import shutil
import glob

Import("env")


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
copy_documentation(None, None, env)
