#include <pdfio.h>

#include <cstring>
#include <iostream>
#include <fstream>
#include <sstream>

#include "i_image_stream.h"
#include "pdf_image_extractor.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION

#include <stb_image_write.h>

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        std::cout << "Usage: test_pdfio <img_file>" << std::endl;
        return 1;
    }
    try 
    {
        const unsigned int MAX_IMAGE_SIZE = 1000000;
        std::unique_ptr<uint8_t[]> buf(new uint8_t[MAX_IMAGE_SIZE]);
        const char* pdf_file_name = argv[1];
        IImageExtractorPtr extractor = CreatePfdImageExtractor(pdf_file_name);
        auto count = extractor->GetImagesCount();
        std::cout << "Found " << count << " images" << std::endl;
        std::cout << "Extracting images..." << std::endl;
        for (auto i = 0u; i < count; ++i)
        {
            auto imageNo = i + 1;
            IImageStream* stream = extractor->GetImageStream(i);
            auto width = stream->GetWidth();
            auto height = stream->GetHeight();
            std::cout << "Image #" << imageNo << " width:" << width << ", height:" << height << std::endl;
            auto imageSize = width * height;
            if (imageSize <= MAX_IMAGE_SIZE)
                stream->Extract(buf.get());
            else
                std::cout << "Image #" << imageNo << " is too huge, skip it" << std::endl;
            std::string out_name = pdf_file_name;
            out_name += "." + std::to_string(imageNo) + ".png";
            stbi_write_png(out_name.c_str(), stream->GetWidth(), stream->GetHeight(), 1, buf.get(), stream->GetWidth());
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    catch(...)
    {
        std::cerr << "Unknown error!" << std::endl;
        return 1;
    }
    return 0;
}