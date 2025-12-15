# Ticket 2059: MPS Remains Required Dependency

**Priority**: Low
**Created**: 2025-12-14
**Assigned**: Unassigned

## Description

Keep MPS as a required dependency for the project. Based on ticket 2051 recommendations, we will NOT make MPS optional at compile time.

**Note**: This ticket documents the decision to keep MPS as a required dependency rather than making it optional.

## Acceptance Criteria

- [x] Document decision to keep MPS required:
  - [x] Rationale: MPS will be needed in the future
  - [x] Simplifies build configuration
  - [x] Maintains existing infrastructure
- [x] Ensure build system continues to require MPS:
  - [x] CMakeLists.txt keeps MPS dependency
  - [x] Makefile keeps MPS_PATH/MPS_DIR logic
  - [x] setup.sh continues to auto-clone MPS if needed
- [x] Update documentation:
  - [x] README.md: MPS is required
  - [x] Build instructions: How to provide MPS location
  - [x] No changes to existing MPS setup process

## Notes

**Rationale** (from ticket 2051):
- "Keep mps compiling, keep tests as mps will be needed in the future"
- MPS provides valuable functionality
- No compelling reason to make it optional
- Simpler build configuration

**No Changes Required**:
- Build system already handles MPS well
- Auto-cloning works smoothly
- Users can specify custom MPS location

**Future Consideration**:
- Could revisit in future if MPS becomes a burden
- For now, keeping it is the simplest approach

## Related Tickets

- Parent: #2051 (Database Architecture Analysis)
- Related: #2058 (Keep MPS infrastructure)
- Phase: Phase 3 - MPS Documentation

## Completion Summary

**Status**: ✅ COMPLETED
**Date**: 2025-12-15

### Work Completed

1. **Verified Build System Requirements**:
   - ✅ CMakeLists.txt lines 99-106: Uses `FATAL_ERROR` if MPS not found - MPS is strictly required
   - ✅ setup.sh lines 37-73: Auto-clones MPS, exits on failure - enforces MPS requirement
   - ✅ Makefile lines 24-37: Supports MPS_PATH/MPS_DIR environment variables

2. **Verified Documentation**:
   - ✅ README.md lines 26-34: Documents MPS as auto-managed required dependency
   - ✅ README.md lines 52-67: Explains how to provide custom MPS location
   - ✅ Build process automatically handles MPS via setup.sh

3. **Created Rationale Documentation**:
   - ✅ Added "Why MPS Remains a Required Dependency" section to doc/MPS_ARCHITECTURE.md
   - ✅ Documented 5 key reasons for keeping MPS required:
     1. Future high-performance use cases
     2. Preserves advanced VectorDatabase_MPS functionality
     3. Simpler build configuration vs optional dependency
     4. Enables seamless migration path
     5. Well-managed auto-cloning dependency

### Key Findings

- **MPS is already required**: Build system enforces MPS presence via FATAL_ERROR
- **Auto-cloning works well**: setup.sh automatically handles MPS if not found
- **No changes needed**: Current infrastructure already implements the requirement
- **Documentation clear**: Users understand MPS is required and how to provide it

### Files Modified

- `doc/MPS_ARCHITECTURE.md`: Added rationale section (lines 20-47)
- `tickets/2059_ticket.md`: Marked all acceptance criteria complete

### Conclusion

MPS remains a required dependency as decided in ticket #2051. The build system properly enforces this requirement, documentation is clear, and the rationale is now documented in doc/MPS_ARCHITECTURE.md.
