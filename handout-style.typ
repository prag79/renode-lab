// Pandoc header-includes snippet for HANDOUT.pdf
// Restores the header/footer/title-block style of the original
// 2026-06-13 PDF, so pandoc HANDOUT.md -> HANDOUT.pdf via typst
// produces a professionally formatted document, not a plain one.

#set page(
  paper: "a4",
  margin: (top: 2.2cm, bottom: 2.0cm, left: 2.0cm, right: 2.0cm),
  header: context {
    let p = counter(page).at(here()).first()
    set text(size: 9pt, fill: rgb("#4a5568"))
    if p == 1 {
      // No running header on the title page.
    } else {
      grid(
        columns: (1fr, 1fr),
        align(left)[Renode Lab | GitHub Codespaces Setup],
        align(right)[#p],
      )
      line(length: 100%, stroke: 0.4pt + rgb("#cbd5e0"))
    }
  },
  footer: context {
    let p = counter(page).at(here()).first()
    let total = counter(page).final().first()
    set text(size: 9pt, fill: rgb("#4a5568"))
    align(center)[
      #text(fill: rgb("#2d3748"), weight: "bold")[prag79/renode-lab]
      #h(1em)
      Student Handout
      #h(1em)
      -- #p of #total --
    ]
  },
)

// Slightly looser line spacing for readability.
#set par(leading: 0.65em, justify: false)

// Styled section headings.
// Skip the pagebreak before the FIRST H1 so the title block on
// page 1 and the first section ("1. What you need before
// starting") share the same page. Without this, page 1 ends up
// with just the title block and the rest of the document starts
// on page 2.
#let first-h1-state = state("first-h1-state", true)
#show heading.where(level: 1): it => {
  context {
    if first-h1-state.get() {
      first-h1-state.update(false)
    } else {
      pagebreak(weak: true)
    }
  }
  v(0.6em)
  text(size: 14pt, weight: "bold", fill: rgb("#1a202c"))[#it]
  v(0.2em)
  line(length: 30%, stroke: 1.2pt + rgb("#2f3b52"))
  v(0.4em)
}
#show heading.where(level: 2): it => {
  v(0.4em)
  text(size: 12pt, weight: "bold", fill: rgb("#2d3748"))[#it]
  v(0.1em)
}

// Tables: match labs/00-Demo/table-style.typ for consistency.
#set table(
  stroke: 0.6pt + rgb("#9aa3b2"),
  inset: 7pt,
  fill: (_, y) => {
    if y == 0 { rgb("#2f3b52") }
    else if calc.odd(y) { rgb("#eef1f6") }
    else { white }
  },
)
#show table.cell.where(y: 0): set text(fill: white, weight: "bold")
#show figure: set block(breakable: true)

// Code blocks: light grey background, monospace. Use set rules
// instead of a block() wrapper so the raw block can break across
// pages naturally. A block() wrapper -- even with breakable: true
// -- was silently dropping the final code block in the handout
// (the quick-reference card) because it couldn't fit in the
// remaining space on the last page.
#show raw.where(block: true): set block(
  fill: rgb("#f4f5f7"),
  inset: 8pt,
  radius: 3pt,
  width: 100%,
  breakable: true,
)
