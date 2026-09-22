# Developer Documentation Template

Use this structure for all module-level developer docs.

## 1. Purpose and Scope

- What the module does
- where it is used in this project
- Explicit scope boundaries

## 2. Responsibilities and Non-Goals

- Responsibilities (what this module guarantees)
- Non-goals (what it does not attempt to solve)

## 3. Public API Summary

- Table of public functions/macros/types
- One-line description per API

## 4. Core Concepts and Data Model

- Core data structures and key fields
- Enums/types that define behavior
- Internal model invariants

## 5. Lifecycle and Operational Flow

- Typical call sequence
- State transitions and start/stop rules

## 6. Concurrency and Synchronization

- Thread model
- Locking/atomic usage
- Caller-side concurrency requirements

## 7. Ownership and Memory Management

- Who owns which buffers/objects
- Allocation/free rules
- Callback-based cleanup behavior

## 8. Behavioral Semantics and Guarantees

- Ordering rules
- Dispatch/lookup semantics
- Core behavior contracts

## 9. Error Handling and Return Codes

- Return values and their meaning
- Validation/failure paths
- Logging/error propagation behavior

## 10. Limits and Configuration

- Compile-time limits
- Runtime configuration
- Performance implications and sizing guidance

## 11. Usage Examples

- Minimal valid usage
- One realistic usage pattern
- Edge-case or advanced usage where relevant

## 12. Testing and Verification Guidance

- Existing test coverage areas
- Recommended additional tests for future changes

## 13. Known Pitfalls

- Common misuse patterns
- Operational gotchas

## 14. Source References

- Public header path(s)
- Implementation path(s)
- Test file path(s)

## Summary

- 2-4 lines describing module value and constraints
