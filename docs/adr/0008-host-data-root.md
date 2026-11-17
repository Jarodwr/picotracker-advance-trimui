# ADR 0008: Map firmware paths into a desktop data root

Status: Accepted (2026-11-15)

## Context

Advance firmware addresses projects and samples with absolute paths such as `/projects`. On a desktop these paths must not address the host filesystem root. The upstream `FileSystem` interface now lists absolute directories and returns owned `FileHandle` objects.

## Decision

Install a wrapper-owned `HostFileSystem`. Resolve every firmware path beneath one desktop data directory. The default is SDL's application preference path and `--data-dir` selects another location. Reject parent components, alternate host path syntax, and all symlink components. Apply the same resolver to reading, writing, listing, copying, moving, and deleting. Use `I_File` handles backed by standard C files.

## Consequences

The scaffold can provide filesystem storage without touching the upstream submodule. A focused check covers ordinary operations, traversal, and a dangling symlink escape. This is an application-level guard against accidental path escape; concurrent hostile filesystem mutation could still create a time-of-check/time-of-use race. Sandboxed or untrusted multiuser execution would need OS-level directory-handle operations or process isolation.
