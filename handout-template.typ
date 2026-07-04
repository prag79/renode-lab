// Custom pandoc typst template for HANDOUT.pdf.
//
// Replaces pandoc's default template (which wraps everything in a
// `conf(...)` function that sets its own page size and margins,
// causing a page break between any include-in-header content and
// the body). This template has no conf wrapper -- it sets the
// page once, applies the show rules from handout-style.typ, then
// renders the body inline.

// Pandoc's default template defines this; we need to too since
// the rendered body may reference it (e.g. for thematic breaks).
#let horizontalrule = line(start: (25%,0%), end: (75%,0%))

#show terms: it => {
  it.children
    .map(child => [
      #strong[#child.term]
      #block(inset: (left: 1.5em, top: -0.4em))[#child.description]
      ])
    .join()
}

$if(highlighting-definitions)$
// syntax highlighting functions from skylining:
$highlighting-definitions$

$endif$

$for(header-includes)$
// === begin header-includes ===
$header-includes$
// === end header-includes ===

$endfor$

// Title block (rendered inline, before the body, on the same page).
$if(title)$
  #set text(size: 10pt, fill: rgb("#2d3748"), weight: "bold", tracking: 0.8pt)
  STUDENT HANDOUT • SETUP & QUICK REFERENCE \
  #v(0.4em)
  #text(size: 22pt, weight: "bold", fill: rgb("#1a202c"))[$title$] \
  $if(subtitle)$
  #v(0.3em)
  #text(size: 11pt, fill: rgb("#4a5568"), tracking: 0pt)[$subtitle$] \
  $endif$
  #v(1.2em)
  #line(length: 100%, stroke: 0.6pt + rgb("#2f3b52"))
  #v(0.6em)
$endif$

$for(include-before)$
$include-before$

$endfor$
$if(toc)$
#outline(
  title: auto,
  depth: $toc-depth$
);
$endif$

$body$

$if(citations)$
$for(nocite-ids)$
#cite(label("${it}"), form: none)
$endfor$
$if(csl)$

#set bibliography(style: "$csl$")
$elseif(bibliographystyle)$

#set bibliography(style: "$bibliographystyle$")
$endif$
$if(bibliography)$

#bibliography($for(bibliography)$"$bibliography$"$sep$,$endfor$$if(full-bibliography)$, full: true$endif$)
$endif$
$endif$
$for(include-after)$

$include-after$
$endfor$
