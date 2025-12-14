# Ticket 2059: MPS Remains Required Dependency

**Priority**: Low
**Created**: 2025-12-14
**Assigned**: Unassigned

## Description

Keep MPS as a required dependency for the project. Based on ticket 2051 recommendations, we will NOT make MPS optional at compile time.

**Note**: This ticket documents the decision to keep MPS as a required dependency rather than making it optional.

## Acceptance Criteria

- [ ] Document decision to keep MPS required:
  - [ ] Rationale: MPS will be needed in the future
  - [ ] Simplifies build configuration
  - [ ] Maintains existing infrastructure
- [ ] Ensure build system continues to require MPS:
  - [ ] CMakeLists.txt keeps MPS dependency
  - [ ] Makefile keeps MPS_PATH/MPS_DIR logic
  - [ ] setup.sh continues to auto-clone MPS if needed
- [ ] Update documentation:
  - [ ] README.md: MPS is required
  - [ ] Build instructions: How to provide MPS location
  - [ ] No changes to existing MPS setup process

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
