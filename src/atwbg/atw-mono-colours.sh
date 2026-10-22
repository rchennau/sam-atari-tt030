#!/bin/sh
# Black background, white text in 1-plane modes only (operator 2026-10-20). Run from xaaes.cnf autorun.
# 256-colour modes keep their DEFAULT.PAL look. Undo: delete this file and its xaaes.cnf run line.
set -- $(/usr/bin/atwres)
[ "$3" = 1 ] && exec /usr/bin/atwpal 0 0 0 1000 1000 1000
exit 0
