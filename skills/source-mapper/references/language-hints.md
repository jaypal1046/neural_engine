# Language-Specific Hints for Source Mapper

## JavaScript / TypeScript
- Entry points: `index.js`, `main.ts`, `app.js`, `server.js`
- Watch for: callback hell, promise chains, async/await misuse
- Key patterns: middleware chains (Express), hooks (React), modules (ESM vs CJS)
- Common issues: missing `await`, implicit `any` in TS, unhandled promise rejections
- Diagram tip: use `sequenceDiagram` for async flows, `graph` for component trees

## Python
- Entry points: `main.py`, `__main__.py`, `app.py`, `manage.py` (Django)
- Watch for: mutable default arguments, bare `except`, global state
- Key patterns: decorators, context managers, generators, dataclasses
- Common issues: circular imports, missing type hints, improper exception handling
- Diagram tip: use `classDiagram` for OOP, `graph` for pipeline/data flow

## Java / Kotlin
- Entry points: class with `main()`, Spring Boot `@SpringBootApplication`
- Watch for: null pointer risks (Java), checked exception swallowing
- Key patterns: design patterns (Builder, Factory, Singleton), dependency injection
- Common issues: resource leaks (unclosed streams), thread safety, God classes
- Diagram tip: `classDiagram` for inheritance, `sequenceDiagram` for DI flows

## Go
- Entry points: `main.go`, `func main()`
- Watch for: unhandled errors (`_`), goroutine leaks, race conditions
- Key patterns: interfaces, channels, defer, struct embedding
- Common issues: ignoring error returns, improper channel usage, missing mutex
- Diagram tip: `graph` for package dependencies, `sequenceDiagram` for goroutines

## Rust
- Entry points: `main.rs`, `lib.rs`
- Watch for: unwrap() calls (panic risk), lifetime complexity
- Key patterns: ownership, borrowing, traits, Result/Option chaining
- Common issues: excessive cloning, blocking in async context
- Diagram tip: `graph` for module/crate structure

## C / C++
- Entry points: `main()` function
- Watch for: buffer overflows, memory leaks (malloc without free), dangling pointers
- Key patterns: header/source split, RAII (C++), templates (C++)
- Common issues: undefined behavior, missing null checks, integer overflow
- Diagram tip: `graph` for include dependencies, `classDiagram` for C++ class hierarchies

## Ruby
- Entry points: `app.rb`, `config.ru`, `Gemfile`
- Watch for: monkey patching, missing nil checks, N+1 queries (Rails)
- Key patterns: mixins, blocks/procs/lambdas, ActiveRecord (Rails)
- Common issues: symbol vs string confusion, mutable frozen objects

## PHP
- Entry points: `index.php`, `public/index.php`
- Watch for: SQL injection, XSS vulnerabilities, `$_GET`/`$_POST` without sanitization
- Key patterns: PSR standards, Composer autoloading, MVC (Laravel)
- Common issues: direct DB queries, mixed HTML/logic, deprecated functions

## Swift
- Entry points: `AppDelegate.swift`, `@main` struct
- Watch for: force unwrap (`!`), retain cycles in closures
- Key patterns: protocols, extensions, Combine, SwiftUI vs UIKit
- Common issues: strong reference cycles, main thread UI updates

## SQL
- Watch for: N+1 patterns, missing indexes, SELECT *, non-parameterized queries
- Key patterns: JOINs, subqueries, CTEs, window functions
- Common issues: implicit type casting, missing WHERE in UPDATE/DELETE
- Diagram tip: use `erDiagram` for table relationships

## Bash / Shell
- Entry points: shebang line (`#!/bin/bash`)
- Watch for: unquoted variables, missing error checks (`set -e`), command injection
- Key patterns: pipes, subshells, heredocs, trap
- Common issues: whitespace in filenames, missing exit codes

## React / Vue / Angular (Frontend Frameworks)
- Entry points: `App.jsx`, `main.tsx`, `index.html`
- Watch for: prop drilling, missing keys in lists, useEffect dependency arrays (React)
- Key patterns: component composition, state management (Redux/Zustand/Pinia), routing
- Common issues: stale closures, memory leaks in effects, unnecessary re-renders
- Diagram tip: `graph TD` for component tree hierarchy

## Docker / Infrastructure (HCL, YAML)
- Watch for: running as root, hardcoded secrets, latest tags
- Key patterns: multi-stage builds, health checks, environment variables
- Diagram tip: `graph` for service dependencies (docker-compose)
