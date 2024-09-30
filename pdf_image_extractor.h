#pragma once

#include <memory>

#include "i_image_extractor.h"

typedef std::unique_ptr<IImageExtractor> IImageExtractorPtr;

IImageExtractorPtr CreatePfdImageExtractor(const char* file_name);
