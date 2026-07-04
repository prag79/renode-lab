PANDOC := pandoc
PDF_ENGINE := typst
TEMPLATE := handout-template.typ
STYLE := handout-style.typ
FILTER := strip-first-h1.lua
META := handout-meta.yaml
SRC := HANDOUT.md
OUT := HANDOUT.pdf

PANDOC_FLAGS := \
  --pdf-engine=$(PDF_ENGINE) \
  --template=$(TEMPLATE) \
  -V mainfont="Times New Roman" \
  -V sansfont="Helvetica" \
  -V monofont="Menlo" \
  -V fontsize=11pt \
  --metadata-file=$(META) \
  --include-in-header $(STYLE) \
  --lua-filter $(FILTER)

.PHONY: all clean

all: $(OUT)

$(OUT): $(SRC) $(TEMPLATE) $(STYLE) $(FILTER) $(META)
	$(PANDOC) $(SRC) -o $(OUT) $(PANDOC_FLAGS)
	@echo "Wrote $(OUT)"

clean:
	rm -f $(OUT)
