---
name: no-emojis
description: >-
  Strict engineering and coding directive that prohibits the use of emojis across all source code, comments, documentation, commit messages, and assistant responses. Use whenever writing, reviewing, refactoring, or generating any code, markdown, or technical text.
---

# No Emojis Coding Standard

## Objective
Enforce classical engineering rigor (Electrical, Telecommunications, and Systems Engineering standards) by completely banning the generation of emojis, pictorial icons, or informal ideograms in source code, technical documentation, logs, user interfaces, commit messages, and conversational responses.

## Core Rules

1. **Source Code and Comments**:
   - Never insert emojis into source files (`.cpp`, `.hpp`, `.c`, `.h`, `.rs`, `.py`, `.js`, etc.).
   - Never insert emojis into inline comments, block comments, docstrings, Doxygen tags, or variable names.
   - String literals must not contain emojis (except when explicitly creating unit test vectors to verify UTF-8 byte boundary decoding).

2. **Documentation and Markdown**:
   - `README.md`, technical specifications, and guides must use formal, professional technical vocabulary.
   - Replace any icons with clean ASCII diagrams, formal Markdown tables, LaTeX equations, or standard technical terminology.

3. **Commit Messages and Git Logs**:
   - Use standard Conventional Commits or classical engineering commit messages (e.g., `feat:`, `fix:`, `refactor:`, `docs:`) without gitmoji or emojis.

4. **Conversational Responses**:
   - All explanations and communications with the user must maintain a formal, concise, and academic engineering tone without emojis.
