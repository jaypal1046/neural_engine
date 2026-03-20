# Local LLM Architecture Roadmap

## Goal

Build Neural Studio into a private local code-intelligence system that can:

- explain the project structure, flow, and architecture
- review code with exact file and line references
- modify code safely using local models
- keep source code and project memory local
- answer short natural prompts like "explain this" or "fix this function" without losing context

The product is not just a chat window. It is a local development brain made of:

- a context system
- a task router
- a safe action loop
- an editor interface

## Product Principles

1. Local first
All code understanding, memory, retrieval, and task routing should work without cloud dependency.

2. Retrieval before training
Current project knowledge must come from local retrieval, not model memory.

3. Exact context over large context
Send the minimum exact files, symbols, selections, and analysis results needed for the task.

4. Source-backed answers
Every explain/review answer should say which files and lines were used.

5. Safe modification loop
Any code change path should support build/test feedback before presenting a final answer.

## Current Architecture

- `C++`
  - core brain and local engine
  - direct Ollama path
  - agent execution and deeper task handling

- `Python`
  - routing and context orchestration
  - local context provider
  - local task preparation and workspace analysis
  - HTTP API for the desktop app

- `React/Electron`
  - editor and chat UI
  - workspace activity capture
  - project memory and active editor context capture
  - user-facing sources and actions

## What Exists Now

### Context and routing

- `server/context_provider.py`
  - workspace file retrieval
  - recent file and command memory support
  - active editor selection support

- `server/task_intelligence.py`
  - local task intent classification
  - per-workspace local storage in `AppData\\Local\\NeuralStudio`
  - relevant local analysis before deeper tasks

- `server/main.py`
  - fast chat path
  - local project context path
  - deep agent fallback
  - local-first routing

### Desktop memory and editor context

- `desktop_app/electron/main.ts`
  - workspace activity memory
  - active editor context storage

- `desktop_app/src/components/ProjectMemoryPanel.tsx`
  - UI for local project memory

- `desktop_app/src/components/MonacoEditor.tsx`
  - active file, cursor, and selected code syncing

## Target Architecture

### Layer 1: Workspace State

Purpose:
Track what the user is actually working on right now.

Stored locally outside the repo:

- workspace profile
- task router config
- analysis cache
- task log
- project activity memory
- active editor context

Paths:

- `C:\\Users\\<user>\\AppData\\Local\\NeuralStudio\\...`
- `C:\\Users\\<user>\\AppData\\Roaming\\desktop_app\\project-memory\\...`

### Layer 2: Context Provider

Purpose:
Build the exact context payload for the model.

Inputs:

- user message
- active file
- selected code
- recent files
- recent commands
- indexed workspace files
- local build/test results

Outputs:

- task summary
- relevant snippets
- exact sources
- fallback local answer when model response is weak

### Layer 3: Task Router

Purpose:
Detect what the user really means.

Main routes:

- `fast_local_chat`
- `context_chat`
- `deep_agent`
- `web_lookup` only for true general knowledge

Examples:

- "hi" -> fast local chat
- "what is java" -> general lookup
- "explain this file" -> context chat with active file
- "fix this function" -> deep agent with active selection
- "review this code" -> context chat or review pipeline

### Layer 4: Safe Action Engine

Purpose:
Execute local coding tasks safely.

Required loop:

1. Retrieve exact context
2. Generate proposal
3. Run relevant local checks
4. Feed failures back into the model
5. Show final result with files, diffs, and confidence

### Layer 5: Editor Interface

Purpose:
Make the system usable with normal developer language.

Required UI features:

- chat with sources
- open referenced files
- inline review findings
- active selection awareness
- proposed patches and commands
- project memory viewer

## Build Phases

## Phase 1: Context Foundation

Status: in progress

Must-have:

- workspace memory outside the repo
- active editor file and selection capture
- local task routing
- source-backed context answers
- web disabled by default for project work

Primary files:

- `server/context_provider.py`
- `server/task_intelligence.py`
- `server/main.py`
- `desktop_app/electron/main.ts`
- `desktop_app/src/components/MonacoEditor.tsx`

## Phase 2: Precise Code Understanding

Next target

Must-have:

- file symbol extraction
- function/class level retrieval
- better target detection for short prompts
- review mode with structured findings
- explain mode with flow summaries

Recommended approach:

- start with lightweight symbol extraction per language
- add Tree-sitter after the basic symbol pipeline works

## Phase 3: Safe Modification Loop

Must-have:

- patch proposal format
- apply confirmation
- local build/test/lint runner
- self-repair loop on failing changes
- diff-aware review after modification

Primary outcome:

The user can ask for changes and the assistant can validate locally before finalizing.

## Phase 4: Retrieval Depth

Must-have:

- local vector search for code/document chunks
- structured project topology
- import/call relationship mapping
- better multi-file flow retrieval

Recommended storage:

- start simple with local JSON and indexed chunks
- add ChromaDB or Qdrant only when keyword/symbol retrieval becomes the bottleneck

## Phase 5: Advanced Intelligence

Later, not first

Optional:

- git-history memory
- bug regression guard
- background autonomous reviewer
- LoRA fine-tuning for local style adaptation
- dependency graph visualization

## Model Strategy

Use models by task, not one model for everything.

- fast router / short chat
  - small fast local model

- context explain / review
  - stronger local instruction model

- code modification
  - strongest local coding model available through Ollama or another local adapter

Important:

- "best in market" matters less than exact context quality
- a smaller model with exact local context often beats a stronger model with weak retrieval
- if a model is not truly local, treat it as optional and separate from the private-core workflow

## Recommended Immediate Build Order

1. active editor context to local retrieval
2. symbol-aware context selection
3. review mode with structured findings
4. patch generation plus apply loop
5. local build/test feedback integration per task type
6. richer graph and vector retrieval later

## What Not To Do First

- do not start with fine-tuning
- do not build a 3D graph before the explain/review flow is reliable
- do not send the whole repo into prompts
- do not rely on web search for project questions

## Success Criteria

The system is working well when:

- "explain this" opens the right file/function context automatically
- code review answers cite the exact files and lines used
- modification requests run local checks before returning results
- general knowledge stays separate from project knowledge
- the user does not need to write perfect prompts to get precise answers
