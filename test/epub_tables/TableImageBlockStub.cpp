#include <Epub/blocks/ImageBlock.h>

void* ImageBlock::extractCtx = nullptr;
ImageBlock::ExtractFn ImageBlock::extractFn = nullptr;

ImageBlock::ImageBlock(const std::string& imagePath, const std::string& srcPath, int16_t width, int16_t height)
    : imagePath(imagePath), srcPath(srcPath), width(width), height(height) {}
bool ImageBlock::imageExists() const { return false; }
bool ImageBlock::hasValidCache() const { return false; }
bool ImageBlock::needsDecode() const { return false; }
void ImageBlock::renderPlaceholder(GfxRenderer&, int, int) const {}
void ImageBlock::clearSessionRenderFailures() {}
void ImageBlock::releaseRenderCache() {}
void ImageBlock::setExtractor(void*, ExtractFn) {}
void ImageBlock::render(GfxRenderer&, int, int) {}
bool ImageBlock::serialize(HalFile&) { return false; }
std::unique_ptr<ImageBlock> ImageBlock::deserialize(HalFile&) { return nullptr; }
