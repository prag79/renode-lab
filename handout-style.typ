// Pandoc header-includes snippet for HANDOUT.pdf.
//
// Design language:
//   - Accent color: #2563eb (GitHub link blue) for hyperlinks and
//     section accent rules.
//   - Ink color:    #1a202c for headings, #2d3748 for body.
//   - Muted color:  #4a5568 for footer/header metadata.
//   - Code bg:      #f4f5f7 (cool light grey).
//   - Table header: #2f3b52 (dark slate) with white bold text.
//
// All colors are chosen to print well on B&W printers too -- the
// accent rules and link underlines survive the desaturation.

#let accent   = rgb("#2563eb")
#let ink      = rgb("#1a202c")
#let ink-soft = rgb("#2d3748")
#let muted    = rgb("#4a5568")
#let code-bg  = rgb("#f4f5f7")
#let table-hd = rgb("#2f3b52")
#let rule-clr = rgb("#2f3b52")

#set page(
  paper: "a4",
  margin: (top: 2.4cm, bottom: 2.0cm, left: 2.0cm, right: 2.0cm),
  header: context {
    let p = counter(page).at(here()).first()
    set text(size: 9pt, fill: muted)
    if p == 1 {
      // No running header on the title page.
    } else {
      grid(
        columns: (1fr, 1fr),
        align(left)[
          #text(fill: ink-soft, weight: "bold")[Renode Lab]
          #h(0.5em)
          #text(fill: muted)[| GitHub Codespaces Setup]
        ],
        align(right)[#text(fill: muted)[#p]],
      )
      v(-4pt)
      line(length: 100%, stroke: 0.4pt + rgb("#cbd5e0"))
    }
  },
  footer: context {
    let p = counter(page).at(here()).first()
    let total = counter(page).final().first()
    set text(size: 9pt, fill: muted)
    align(center)[
      #text(fill: ink-soft, weight: "bold")[prag79/renode-lab]
      #h(1em)
      Student Handout
      #h(1em)
      -- #p of #total --
    ]
  },
)

// ---- Hyperlinks: blue + underlined so they're visible in print ----
#show link: it => {
  set text(fill: accent)
  underline(offset: 2pt, stroke: 0.3pt + accent)[#it]
}
#show ref: it => {
  set text(fill: accent)
  underline(offset: 2pt, stroke: 0.3pt + accent)[#it]
}

// ---- Paragraph spacing: a bit more breathing room ----
#set par(leading: 0.7em, justify: false, first-line-indent: 0pt, spacing: 0.9em)

// ---- Section headings with a colored left accent bar ----
#let first-h1-state = state("first-h1-state", true)
#show heading.where(level: 1): it => {
  context {
    if first-h1-state.get() {
      first-h1-state.update(false)
    } else {
      pagebreak(weak: true)
    }
  }
  v(0.4em)
  block(above: 1.2em, below: 0.5em)[
    #grid(
      columns: (3pt, 1fr),
      column-gutter: 8pt,
      align(top)[#block(width: 3pt, height: 1em, fill: accent)],
      align(top)[
        #text(size: 15pt, weight: "bold", fill: ink)[#it]
        #v(2pt)
        #line(length: 100%, stroke: 0.6pt + rgb("#cbd5e0"))
      ],
    )
  ]
}
#show heading.where(level: 2): it => {
  v(0.3em)
  text(size: 12pt, weight: "bold", fill: ink-soft)[#it]
  v(-0.2em)
}

// ---- Tables: dark header, zebra rows, clean grid lines ----
#set table(
  stroke: 0.4pt + rgb("#dde2eb"),
  inset: 7pt,
  fill: (_, y) => {
    if y == 0 { table-hd }
    else if calc.odd(y) { rgb("#eef1f6") }
    else { white }
  },
)
#show table.cell.where(y: 0): set text(fill: white, weight: "bold")
#show table.cell.where(y: 0): set par(leading: 0.5em)
#show figure: set block(breakable: true)

// ---- Code blocks: light grey bg + left accent border ----
#show raw.where(block: true): set block(
  fill: code-bg,
  inset: (x: 10pt, y: 8pt),
  radius: 3pt,
  width: 100%,
  breakable: true,
  stroke: (left: 2.5pt + accent),
)
// Inline code: subtle grey background, no border
#show raw.where(block: false): set text(fill: ink-soft)
#show raw.where(block: false): it => box(
  fill: code-bg,
  inset: (x: 2pt, y: 0pt),
  radius: 2pt,
)[#it]

// ---- Blockquotes: styled as left-accent callouts ----
#show quote.where(block: true): it => block(
  inset: (left: 12pt, right: 4pt, top: 4pt, bottom: 4pt),
  stroke: (left: 3pt + accent),
  fill: rgb("#eef5ff"),
  width: 100%,
  radius: (left: 0pt, right: 3pt),
)[#set text(fill: ink-soft); #it]

// ---- Bullet lists: accent-colored bullet markers ----
#set list(indent: 1.2em, body-indent: 0.5em, spacing: 0.6em)
#show list: it => {
  set list(marker: text(fill: accent)[•])
  it
}

// ---- Thematic break (--- in markdown): accent-colored rule ----
#show raw.where(block: true): it => it
