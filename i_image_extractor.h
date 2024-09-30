#pragma once

#include <cstddef>

class IImageStream;

class IImageExtractor
{
public:
    virtual ~IImageExtractor() {}
    virtual size_t GetImagesCount() const = 0;
    virtual IImageStream* GetImageStream(unsigned int index) const = 0;
};
