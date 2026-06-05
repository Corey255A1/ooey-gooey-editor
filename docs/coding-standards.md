# OOEY-GOOEY Editor Coding Standards

The **ooey-gooey-editor** project strictly adheres to the core **OOEY Project Coding Standards** (defined in [ooey coding standards](file:///home/corey/code/ooey/docs/coding-standards.md)).

## Clean Code & Extensibility Principles

To prevent structural rot and maintain high testability, the codebase enforces the following:

### 1. SOLID Design Principles
* **Single Responsibility Principle (SRP)**: Keep ViewModel classes focused solely on data bindings, commands, and layout-state mappings. All core business and domain logic (such as parsing, AST tree manipulations, and DSL serialization) must be decoupled into separate, standalone service classes (like `AstService`).
* **Open/Closed Principle (OCP)**: Extend UI interpretation dynamically via metadata-driven components (such as the `TypeRegistry` engine) rather than hardcoded `if-else` type-casting chains.
* **Dependency Inversion Principle (DIP)**: High-level UI layers and ViewModels must not directly depend on lower-level structures unless using proper abstractions and clean data structures (e.g. mapping internal property items to standard utility pairs).

### 2. Precise and Single-Purpose Methods
* Strive for small, precise methods. Functions and methods must focus on a single, well-defined task.
* Refactor large procedures with step-by-step logic into well-named private helper functions to ensure the code remains self-documenting.

### 3. Descriptive Variable Naming
* Prefer descriptive and clear variable names over short, cryptic, or ambiguous names (e.g., use `last_typing_record_time` rather than `t`).
* Symbols, variables, and parameters must clearly convey their intent and behavior to ensure readability for the whole team.
