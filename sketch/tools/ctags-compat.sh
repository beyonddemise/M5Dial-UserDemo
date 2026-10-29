#!/bin/bash
# ctags-compat.sh - make universal-ctags look like arduino-cli's bundled ctags 5.8-arduino11.
#
# Installed by setup.sh (Apple Silicon without Rosetta only) as
#   .arduino/data/packages/builtin/tools/ctags/<ver>/ctags
# with @UCTAGS@ replaced by the absolute path of a universal-ctags binary. arduino-cli
# runs it as:  ctags -u --language-force=c++ -f - --c++-kinds=svpf --fields=KSTtzns
#              --line-directives <filtered-sketch.cpp>
#
# why: arduino-cli's prototype generator (internal/.../preprocessor/internal/ctags) only
# understands the Exuberant/Arduino output. Raw universal-ctags differs in two ways that
# silently break .ino preprocessing - prototypes become `setup();` call statements and are
# inserted inside setup()'s body, so setup() recurses forever at boot:
#   1. the return type is `typeref:typename:void`; arduino-cli reads only `returntype:`.
#   2. `line:` is the physical line of the filtered temp file; universal-ctags applies
#      `#line`/`# N "file"` markers to the file name only, not to the line number.
# This wrapper runs universal-ctags with the same arguments and rewrites each tag:
#   - line:N           -> line number in the original file per the last preceding marker
#   - typeref:K:T      -> returntype:T (K=typename) or returntype:K T (struct/union/...)
#                         for function/prototype tags; templates get the
#                         "template <...> " prefix Arduino's ctags puts there.
# Exit status is universal-ctags' own.
set -o pipefail

real="@UCTAGS@"
src="${*: -1}"  # arduino-cli passes the input file last

"$real" "--fields-C++=+{template}" "$@" | awk -v src="$src" '
# Build the list of line markers (physical line -> logical line of the next line).
BEGIN {
  FS = OFS = "\t"; n = 0; phys = 0
  while ((getline l < src) > 0) {
    phys++
    if (l ~ /^[ \t]*#[ \t]*(line[ \t]+)?[0-9]+([ \t]|$)/) {
      sub(/^[ \t]*#[ \t]*(line[ \t]+)?/, "", l); sub(/[^0-9].*$/, "", l)
      n++; dpos[n] = phys; dnum[n] = l + 0
    }
  }
  close(src)
}
# logical(p): binary search for the last marker before physical line p.
function logical(p,   lo, hi, mid) {
  lo = 0; hi = n
  while (lo < hi) { mid = int((lo + hi + 1) / 2); if (dpos[mid] < p) lo = mid; else hi = mid - 1 }
  return lo ? dnum[lo] + (p - dpos[lo] - 1) : p
}
{
  # Extension fields start after the pattern terminator `;"` (last one on the row).
  i = 0; for (k = NF; k > 3; k--) if ($(k - 1) ~ /;"$/) { i = k; break }
  if (!i) { print; next }
  kind = ""; tref = ""; tmpl = ""
  for (k = i; k <= NF; k++) {
    if ($k ~ /^kind:/) kind = substr($k, 6)
    else if ($k ~ /^typeref:/) tref = substr($k, 9)
    else if ($k ~ /^template:/) tmpl = substr($k, 10)
  }
  fn = (kind == "function" || kind == "prototype")
  out = $1; for (k = 2; k < i; k++) out = out OFS $k
  for (k = i; k <= NF; k++) {
    f = $k
    if (f ~ /^line:[0-9]+$/) f = "line:" logical(substr(f, 6) + 0)
    else if (f ~ /^template:/) continue
    else if (fn && f ~ /^typeref:/) {
      c = index(tref, ":"); t = substr(tref, c + 1)
      if (substr(tref, 1, c - 1) != "typename") t = substr(tref, 1, c - 1) " " t
      f = "returntype:" (tmpl != "" ? "template " tmpl " " : "") t
    }
    out = out OFS f
  }
  print out
}'
