#!/usr/bin/env bash
# Overlay lab dispatcher + labs/ from the cloned git repo onto the
# image-baked paths (/usr/local/bin/lab, /labs/).
#
# GitHub Codespaces often reuses a cached ghcr.io/...:latest image across
# "Rebuild Container", while the git checkout is always current. Running
# this on start keeps `lab list` and `lab 08` in sync with the repo
# after `git pull` — no need to wait for (or trust) a fresh GHCR pull.

set -e

find_repo_root() {
  if [[ -n "${GITHUB_WORKSPACE:-}" && -f "${GITHUB_WORKSPACE}/lab" && -d "${GITHUB_WORKSPACE}/labs" ]]; then
    echo "${GITHUB_WORKSPACE}"
    return 0
  fi
  local d
  for d in /workspaces/renode-lab /workspaces/*; do
    if [[ -f "$d/lab" && -d "$d/labs" ]]; then
      echo "$d"
      return 0
    fi
  done
  return 1
}

REPO="$(find_repo_root || true)"
if [[ -z "$REPO" ]]; then
  exit 0
fi

cp -ru "$REPO/labs/." /labs/

if [[ -w /usr/local/bin/lab ]]; then
  cp "$REPO/lab" /usr/local/bin/lab
  chmod +x /usr/local/bin/lab
else
  sudo cp "$REPO/lab" /usr/local/bin/lab
  sudo chmod +x /usr/local/bin/lab
fi
