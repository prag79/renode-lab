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
// Designed as a compact banner: eyebrow text, big title, subtitle,
// then a thick accent rule. No full-page cover -- the intro
// paragraph follows immediately on the same page.
$if(title)$
#block(above: 0pt, below: 0.8em)[
  #set text(size: 9pt, fill: muted, weight: "bold", tracking: 2pt)
  STUDENT HANDOUT
  #h(0.5em)
  #text(fill: accent)[•]
  #h(0.5em)
  SETUP & QUICK REFERENCE
  #v(0.5em)
  #text(size: 24pt, weight: "bold", fill: ink)[$title$]
  $if(subtitle)$
  #v(0.2em)
  #text(size: 11pt, fill: ink-soft)[$subtitle$]
  $endif$
  #v(0.8em)
  #block(height: 3pt, width: 100%, fill: accent)
]
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
