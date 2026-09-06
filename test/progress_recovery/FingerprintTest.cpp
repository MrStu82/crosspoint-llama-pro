#include <cassert>
#include <cstdio>
#include "src/util/BookReadingRate.h"
#include "lib/Epub/Epub/ReaderRenderSpec.h"
int main(){ReaderRenderSpec s; auto base=BookReadingRate::renderSpecFingerprint(s,0);
#define CHECK(field,value) {auto changed=s;changed.field=value;assert(BookReadingRate::renderSpecFingerprint(changed,0)!=base);}
CHECK(fontId,1);CHECK(lineCompression,0.9f);CHECK(extraParagraphSpacing,true);CHECK(paragraphAlignment,1);
CHECK(viewportWidth,480);CHECK(viewportHeight,760);CHECK(hyphenationEnabled,true);CHECK(embeddedStyle,false);
CHECK(imageRendering,1);CHECK(focusReadingEnabled,true);CHECK(guideReadingEnabled,true);CHECK(forceParagraphIndents,true);
assert(BookReadingRate::renderSpecFingerprint(s,1)!=base);
puts("PASS 13 pagination/orientation identity mutations");}
