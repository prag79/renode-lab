// Professional table design for the Demo build-internals PDF.
// Visible row/column separators, a filled header row, and light
// zebra striping for readability.
#set table(
  stroke: 0.6pt + rgb("#9aa3b2"),
  inset: 7pt,
  fill: (_, y) => {
    if y == 0 { rgb("#2f3b52") }
    else if calc.odd(y) { rgb("#eef1f6") }
    else { white }
  },
)

// Header row: bold white text on the dark fill set above.
#show table.cell.where(y: 0): set text(fill: white, weight: "bold")

// Pandoc wraps tables in a figure; figures don't break across pages by
// default, so a long table overflows the page. Allow them to break.
#show figure: set block(breakable: true)
