# Daily fiction quote pack rights and provenance

The runtime pack contains 366 short passages drawn from the six Project Gutenberg ebooks listed in `daily-quotes-sources.tsv`. Project Gutenberg identifies these ebook editions as public-domain material in the United States. The manifest preserves a SHA-256 and source sentence index for every compiled record so a release audit can trace each passage back to its edition.

The firmware stores only quote/character/title/author strings. Source URLs and hashes remain in this release-audit manifest and are not loaded at runtime. Any future pack change must regenerate the manifest and pass the exact 366-record/no-repeat tests.
