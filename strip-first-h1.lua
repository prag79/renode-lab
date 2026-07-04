-- Drop the first H1 in the document. The HANDOUT.md opens with
-- `# Student Handout — Renode Lab on GitHub Codespaces` as its
-- top-level title; the styled typst title block in
-- handout-style.typ already renders that, so without this filter
-- the PDF would show the title twice -- once as a big styled
-- banner and once as a plain H1 section heading.
--
-- Returns pandoc.Null() (which renders as nothing at all) rather
-- than an empty Div, because pandoc renders an empty Div as a
-- typst `#block[]` with default above/below spacing that
-- ironically pushes the intro paragraph onto page 2, leaving
-- page 1 with just the title block.
local first_h1_seen = false
function Header(el)
  if el.level == 1 and not first_h1_seen then
    first_h1_seen = true
    return {}  -- empty list = remove the element
  end
  return el
end
