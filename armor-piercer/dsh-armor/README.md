# DeepSeek Harness (dsh) armor

Armor for `deepseek-ai/deepseek-harness` using its **official patch layer** —
no binary patching.

## Why this one is clean
- dsh assembles the system prompt from ordered sections (`@deepseek-ai/dsh-system-prompt`).
- The deployment persona is a first-class config slot:
  `system-prompt` row -> `personaPrefix` (order 0) / `personaSuffix` (order 10200).
- dsh ships **no content-refusal section**; SAFETY.md is docs only. Approval and
  sandboxing are execution-level services, not prompt text.
- So the whole armor surface is one config row, and every first-party tool
  section, harness identity, runtime context and tool schema stays untouched.

## Files
- `dsh-armor.patch.yml` - patch overlay (replaces the `system-prompt` row config)
- `install_dsh_armor.ps1` - installs to `$DSH_HOME/cordis.patch.yml` (default `~/.dsh`), backups + safe merge
- `dump-config.txt` - verification: `--patch` layer assembled
- `dump-config-home.txt` - verification: home layer assembled without `--patch`

## Usage
One run:
    dsh web --patch <path>\dsh-armor.patch.yml

Persistent:
    powershell -ExecutionPolicy Bypass -File install_dsh_armor.ps1
    # writes ~/.dsh/cordis.patch.yml (home layer, applies to every profile)

## Verification (observed)
- `dsh --profile web --patch ... --dump-config` -> row provenance shows
  `patched by ... dsh-armor.patch.yml`, doctrine present.
- After install, `dsh --profile web --dump-config` (no --patch) -> provenance
  shows `patched by ... C:\Users\ASUS\.dsh\cordis.patch.yml`, doctrine present.

## Rollback
Delete `~/.dsh/cordis.patch.yml` (or restore the `.bak-<stamp>` the installer left).
