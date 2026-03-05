# Export Checklist (Kali VM style deliverable)

## Before Export
- [ ] Kali VM boots to desktop successfully
- [ ] `Custom VM Interface` appears in app menu
- [ ] Auto-launch works on login (if required)
- [ ] `vm_cli` commands work inside VM terminal:
  - [ ] `./vm_cli help`
  - [ ] `./vm_cli fact 5`
  - [ ] `./vm_cli asm-build sample_factorial.asm sample_factorial.bin`
  - [ ] `./vm_cli run-bin sample_factorial.bin --trace trace.jsonl`
- [ ] `viewer.html` opens and runs
- [ ] `kali_ui.html` opens and accepts commands

## Clean-up Before Submission
- [ ] Remove generated traces/binaries not needed
- [ ] Keep source + docs + sample asm files
- [ ] Add screenshots of VM desktop + app running

## Export
- [ ] Power off VM (not suspend)
- [ ] Export appliance as OVA from hypervisor
- [ ] Name file: `custom-vm-kali.ova`
- [ ] Test import once on another machine (if possible)

## Attach in Submission
- [ ] Source folder zip
- [ ] OVA file (or link)
- [ ] Quick run guide (2–3 commands)
- [ ] Architecture + instruction docs
