---
name: project-intelligence
description: >
  Use this skill to manage, update, and query project-level knowledge stored in the .leaning directory.
  This skill provides the AI with a "memory" of the project structure, tech stack, and logic flows.
  Triggers include: "how is this project structured", "what is this project's stack", 
  "update project knowledge", "index project", "explain project architecture".
intents:
  - ANALYZE_PROJECT
  - UPDATE_KNOWLEDGE
  - QUERY_STRUCTURE
---

# Project Intelligence Skill

Manage the project-level knowledge base in `.leaning/`.

## Key Files in .leaning/
- `identity_manifest.md`: Project name, root path, and core purpose.
- `project_blueprint.md`: Technical stack, directory structure, and module relationships.
- `logic_flows.md`: Critical data paths and business logic flows.
- `api_surface.md`: Exposed endpoints (FastAPI) and Bridge functions (Electron).

## Guidelines
1. **Consistency**: Always ensure the data in `.leaning/` matches the actual state of the code.
2. **Conciseness**: Keep documentation high-level but accurate. Link to specific files for details.
3. **Accessibility**: This data is intended for the AI to "jump-start" its understanding of the project.

## How to Update
When major changes are made:
1. Run a project-wide scan (LIST_DIR, PROJECT_SEARCH).
2. Update the relevant files in `.leaning/`.
3. Use the `NEURAL_ANALYZE` tool where needed for deep insights.
