PANDOC := pandoc
PDF_ENGINE := typst
STYLE := handout-style.typ
FILTER := strip-first-h1.lua
SRC := HANDOUT.md
OUT := HANDOUT.pdf

PANDOC_FLAGS := \
  --pdf-engine=$(PDF_ENGINE) \
  -V mainfont="Times New Roman" \
  -V sansfont="Helvetica" \
  -V monofont="Menlo" \
  -V fontsize=11pt \
  --include-in-header $(STYLE) \
  --lua-filter $(FILTER)

.PHONY: all clean

all: $(OUT)

$(OUT): $(SRC) $(STYLE) $(FILTER)
	$(PANDOC) $(SRC) -o $(OUT) $(PANDOC_FLAGS)
	@echo "Wrote $(OUT)"

clean:
	rm -f $(OUT)
