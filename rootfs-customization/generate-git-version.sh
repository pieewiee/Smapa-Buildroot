#!/bin/bash

# Generates the "git describe" version information string and dumps it into /smapa-version-info

# First, we need to cd out of the buildroot directory. We do not want the buildroot version but the smapa-image version instead.
cd ..
# The arguments ensure that the dirty flag is contained if the repo is dirty and that there is always at least the git sha1 hash included.
git describe --always --dirty=-dirty --long '--match=v[0-9]*.[0-9]*.[0-9]*' --abbrev=8 > $1/smapa-image-version-info
