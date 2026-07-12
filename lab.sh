# /etc/profile.d/lab.sh — make `lab` land the interactive shell in the
# lab's scratchpad (~/work/<lab>) after Renode quits.
#
# Why a function: `lab` itself is /usr/local/bin/lab, an external script.
# A child process cannot change its parent shell's working directory, so
# the script alone can never leave you in ~/work/<lab>. This wrapper runs
# the real script (via `command lab`, which bypasses this function) and
# then cd's THIS shell into the directory the script recorded in
# $LAB_LAST_DIR_FILE. The script clears that file at the start of every
# run and only writes it when it actually enters a lab scratchpad, so
# `lab list|monitor|shell|help` don't move you anywhere.
#
# Effect for students:
#   - `lab 04` from anywhere builds/runs the lab, and when you quit
#     Renode you are left in ~/work/04-sifive-fe310/.
#   - Edit files right there; re-run `lab 04` and your edits take effect
#     (cp -rn never clobbers them). No need to cd into /workspaces/... .

lab() {
    command lab "$@"
    local rc=$?
    local f="${LAB_LAST_DIR_FILE:-$HOME/.lab_last_dir}"
    local d=""
    [ -f "$f" ] && d="$(cat "$f" 2>/dev/null)"
    if [ -n "$d" ] && [ -d "$d" ]; then
        cd "$d" || true
    fi
    return "$rc"
}
