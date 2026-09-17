# sam-yum-tt — design notes (alpha)

Source of truth: SAM monorepo `maestro/tracks/tt030-rpm-pipeline/plan.md` (locked 2026-09-14).
This file tracks what the TT client needs and what is proven.

## Planned client (from the plan)

- `yum install | remove | list | search | makecache`, `yum`-compatible verbs, not a port of yum.
- Fetches `http://mirror.sam.int/tt030/index.tsv` (one tab-separated line per package), resolves
  the dependency closure with rpmvercmp, downloads and SHA-256-checks the **whole** closure, then
  runs plain `rpm -i` (no `--force`).
- The index carries a monotonic serial and an asymmetric signature (`index.tsv.sig`); the card
  holds only the public key. Stale or tampered indexes are refused.
- A package missing from the mirror triggers a build request over MQTT
  (`sam/tt030/build/request`, replies on `sam/tt030/build/{queued,progress,done,failed}/<id>`),
  and `yum` prints the build's progress.
- Names only (`mirror.sam.int`, `mqtt.sam.int`) from the card's `/etc/hosts`; no IPs.
- The plan chose POSIX `sh` + `awk` for the client.

## Open question this alpha answers

Would a native C client that uses the T425 be faster?

| Work | T425? | Status |
|---|---|---|
| Index signature check | yes, if the signature is Ed25519 over a SHA-256 of the index | T425 Ed25519 verify exists (sam-ssh-tt) |
| SHA-256 of packages | maybe | **shabench** measures it |
| Dependency resolution | no | 415 lines is too little work |
| Unpack and install | no | done by `rpm` |

Constraints on any T425 use: the T425 is shared with `sam-ssh-tt` (requests take turns), and
some programs take it over, so every T425 path needs a 68030 fallback.

## Next

1. Run `shabench` on the real TT (board card `7d9cb80d`).
2. If the T425 wins clearly: propose a plan revision (native client). If not: keep `sh` + `awk`,
   and only pick Ed25519 for the index signature.
