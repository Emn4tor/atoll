# Packaging

- `PKGBUILD` builds a tagged release; `PKGBUILD-git` builds the default branch.
  Both install the same files, so only one of them can be on a machine at once.
- `atoll.service` is a user service, not a system one. It is installed to
  `/usr/lib/systemd/user` and started per session with
  `systemctl --user enable --now atoll.service`.
- `atollctl` is a plain shell wrapper around the D-Bus interface, so scripts
  and key bindings do not have to spell out `gdbus` invocations.

## Publishing to the AUR

Each AUR package is its own git repository holding a `PKGBUILD` and a
`.SRCINFO`. From a checkout of the AUR repository:

```sh
cp path/to/atoll/packaging/PKGBUILD .    # or PKGBUILD-git, renamed
makepkg --printsrcinfo > .SRCINFO
git commit -am "atoll 0.1.0" && git push
```

`makepkg -si` in the same directory builds and installs it locally first,
which is worth doing before every push: the AUR runs nothing on your behalf.

## The assistant's client

`claude-code` is an optional dependency rather than a required one. Atoll runs
without it - the assistant can also be pointed at an API key, and everything
else about the island is unaffected - but it is what makes the assistant work
with nothing to configure, so it is worth installing alongside.
